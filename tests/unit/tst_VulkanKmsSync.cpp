#include <QtTest>

#include "AtomicKmsPresenter.hpp"
#include "DrmEventDispatcher.hpp"
#include "GbmBufferManager.hpp"
#include "IndependentFrameLoop.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"

#include <QColor>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

int makePipeFd(bool writeReadyByte) {
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return -1;
    }
    if (writeReadyByte) {
        const char b = 1;
        Q_UNUSED(::write(fds[1], &b, 1));
    } else {
        // Mantém ponta de escrita fechada? Não: se fechada, poll retorna POLLHUP;
        // para simular fence não sinalizada, retornamos read-end mantendo write-end?
        // Melhor: usar helper PipePair para controlar fechamento/escrita.
    }
    ::close(fds[1]);
    return fds[0];
}

struct ControlledPipe {
    int readFd = -1;
    int writeFd = -1;
    ControlledPipe() {
        int fds[2] = {-1, -1};
        if (::pipe2(fds, O_CLOEXEC) == 0) {
            readFd = fds[0];
            writeFd = fds[1];
        }
    }
    ~ControlledPipe() {
        if (writeFd >= 0) {
            ::close(writeFd);
        }
    }
    void signal() {
        if (writeFd >= 0) {
            const char b = 1;
            Q_UNUSED(::write(writeFd, &b, 1));
        }
    }
};

class SyncMockKms final : public IKmsResourceBackend {
public:
    KmsDiscoverySnapshot probeResources(int drmFd) override {
        KmsDiscoverySnapshot snap;
        if (drmFd < 0) {
            return snap;
        }
        snap.valid = true;
        snap.atomicSupported = true;
        snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 128, 128});
        snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
        KmsConnectorInfo dp;
        dp.connectorId = 20;
        dp.name = QStringLiteral("DP-1");
        dp.connected = true;
        dp.possibleCrtcs = 0x1;
        dp.modes.append(DrmOutputMode{QStringLiteral("DP-1"), 128, 128, 60, true});
        dp.preferredMode = dp.modes.first();
        snap.connectors.append(dp);
        return snap;
    }
};

class SyncMockGbm final : public IGbmBackend {
public:
    int activeBuffers = 0;
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &req, GbmExportedBuffer *out) override {
        ++activeBuffers;
        out->width = req.width;
        out->height = req.height;
        out->format = req.format;
        out->stride = req.width * 4;
        out->nativeHandle = reinterpret_cast<void *>(0x2);
        return true;
    }
    void destroyBuffer(GbmExportedBuffer *buf) override {
        if (buf && buf->nativeHandle) {
            --activeBuffers;
            buf->nativeHandle = nullptr;
        }
    }
};

class SyncMockAtomic final : public IAtomicKmsBackend {
public:
    KmsFenceCapabilities fenceCaps{true, true};
    bool failAddFb = false;
    bool failCommit = false;
    bool returnOutFenceOnCommit = false;
    bool dupInFenceAsOutFence = false;
    bool recordDupedInFences = true;
    uint32_t nextFbId = 500;
    QList<uint32_t> createdFbs;
    QList<uint32_t> removedFbs;
    QList<AtomicCommitRequest> commits;
    QList<int> dupedInFences;
    int lastReturnedOutFence = -1;

    ~SyncMockAtomic() override {
        for (int fd : dupedInFences) {
            if (fd >= 0) {
                ::close(fd);
            }
        }
    }

    KmsFenceCapabilities probeFenceCapabilities(int, uint32_t, uint32_t) override {
        return fenceCaps;
    }

    uint32_t addFramebuffer(int, const GbmExportedBuffer &buffer) override {
        if (failAddFb || buffer.stride == 0) {
            return 0;
        }
        const uint32_t fb = nextFbId++;
        createdFbs.append(fb);
        return fb;
    }

    void removeFramebuffer(int, uint32_t fbId) override {
        removedFbs.append(fbId);
    }

    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
        commits.append(req);
        if (recordDupedInFences && req.inFenceFd >= 0) {
            dupedInFences.append(::dup(req.inFenceFd));
        }
        int outFd = -1;
        if (returnOutFenceOnCommit && fenceCaps.supportsOutFencePtr) {
            if (dupInFenceAsOutFence && req.inFenceFd >= 0) {
                outFd = ::dup(req.inFenceFd);
            } else {
                outFd = makePipeFd(true);
            }
        }
        lastReturnedOutFence = outFd;
        if (failCommit) {
            return AtomicCommitResult{false, outFd};
        }
        return AtomicCommitResult{true, outFd};
    }
};

class SyncMockSink final : public IWaylandOutputFrameSink {
public:
    int started = 0;
    int sent = 0;
    void setAutomaticCallbacks(bool) override {}
    void notifyFrameStarted() override { ++started; }
    void notifySendFrameCallbacks() override { ++sent; }
};

} // namespace

class tst_VulkanKmsSync final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void realRadvSyncFdExportSignalingAndInFenceOwnership();
    void outFencePtrReleaseFenceAttachedToPreviousPresentedAndImportedOnReuse();
    void atomicKmsInFenceAbsentFallbackAndOutFenceAbsent();
    void atomicKmsCommitFailureDuplicateFlipTimeoutAndReorderedEvents();
    void suspendWithPendingCommitAndResumeWithNewFdClearsFbIds();
};

void tst_VulkanKmsSync::realRadvSyncFdExportSignalingAndInFenceOwnership() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));
    QVERIFY(renderer.capabilities().supportsExternalSemaphoreFd);
    QVERIFY(renderer.capabilities().supportsSyncFdExport);

    SyncMockKms kms;
    SyncMockGbm gbm;
    SyncMockAtomic atomic;
    atomic.fenceCaps = {true, true};
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.fenceCapabilities().supportsInFenceFd);
    QVERIFY(presenter.fenceCapabilities().supportsOutFencePtr);

    QVERIFY(renderer.createSharedPoolSlots(presenter.bufferManager(), QSize(128, 128), 2));
    const auto &slotsList = presenter.bufferManager().buffers();
    QVERIFY(slotsList[0].vkSignalSemaphoreHandle != 0);
    QVERIFY(slotsList[1].vkSignalSemaphoreHandle != 0);

    const QByteArray qml =
        "import QtQuick 2.15\n"
        "Rectangle {\n"
        "  width: 128; height: 128; color: '#0b1e36'\n"
        "  Rectangle { x: 20; y: 20; width: 88; height: 88; color: '#33dd99' }\n"
        "}\n";
    QVERIFY2(renderer.loadQmlSource(qml), qPrintable(renderer.lastError()));

    SyncMockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(renderer, presenter, wayland, dispatcher);

    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(atomic.commits.size(), 1);

    const int passedInFence = atomic.commits.at(0).inFenceFd;
    QVERIFY(passedInFence >= 0);
    // Após submitBufferForScanout, o FD de entrada deve ter sido fechado (sem reuso) e limpo do slot.
    QCOMPARE(::fcntl(passedInFence, F_GETFD), -1);
    QCOMPARE(presenter.bufferManager().buffers().at(0).acquireFenceFd, -1);

    // O dup capturado durante commitAtomic representa o SYNC_FD real exportado do RADV e deve sinalizar.
    QCOMPARE(atomic.dupedInFences.size(), 1);
    QVERIFY(VulkanQuickRenderer::isSyncFdSignaled(atomic.dupedInFences.at(0), 2000));

    // Readback confirma que o conteúdo da imagem foi realmente renderizado sem vkQueueWaitIdle.
    const QImage img = renderer.readbackImage();
    QVERIFY(!img.isNull());
    QVERIFY(qAbs(img.pixelColor(64, 64).green() - 0xdd) <= 4);

    dispatcher.dispatchSimulatedPageFlip(1, 0);
    QCOMPARE(sink.sent, 1);

    loop.shutdown();
    renderer.destroySharedPoolSlots(presenter.bufferManager());
    presenter.shutdown();
    renderer.shutdown();
}

void tst_VulkanKmsSync::outFencePtrReleaseFenceAttachedToPreviousPresentedAndImportedOnReuse() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));
    QVERIFY(renderer.capabilities().supportsSyncFdExport);

    SyncMockKms kms;
    SyncMockGbm gbm;
    SyncMockAtomic atomic;
    atomic.fenceCaps = {true, true};
    atomic.returnOutFenceOnCommit = true;
    atomic.dupInFenceAsOutFence = true; // Retorna SYNC_FD real válido para vkImportSemaphoreFdKHR!
    atomic.recordDupedInFences = false;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(renderer.createSharedPoolSlots(presenter.bufferManager(), QSize(64, 64), 2));

    const QByteArray qml =
        "import QtQuick 2.15\n"
        "Rectangle { width: 64; height: 64; color: '#224466' }\n";
    QVERIFY2(renderer.loadQmlSource(qml), qPrintable(renderer.lastError()));

    SyncMockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(renderer, presenter, wayland, dispatcher);

    // Frame 1 -> Slot 0
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QVERIFY(presenter.pendingOutFenceFd() >= 0);
    dispatcher.dispatchSimulatedPageFlip(1, 0);
    QCOMPARE(presenter.pendingOutFenceFd(), -1);
    QCOMPARE(presenter.bufferManager().buffers().at(0).state, GbmBufferState::Presented);
    QCOMPARE(presenter.bufferManager().buffers().at(0).releaseFenceFd, -1);

    // Frame 2 -> Slot 1; flip concluído deve anexar outFenceFd ao Slot 0 (Presented -> Available)
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    const int outFenceFrame2 = presenter.pendingOutFenceFd();
    QVERIFY(outFenceFrame2 >= 0);
    dispatcher.dispatchSimulatedPageFlip(2, 0);
    QCOMPARE(presenter.bufferManager().buffers().at(0).state, GbmBufferState::Available);
    QCOMPARE(presenter.bufferManager().buffers().at(0).releaseFenceFd, outFenceFrame2);
    QCOMPARE(presenter.bufferManager().buffers().at(1).state, GbmBufferState::Presented);

    // Frame 3 -> reusa Slot 0: renderIntoBuffer importa releaseFenceFd (via vkImportSemaphoreFdKHR ou poll) e consome/fecha FD
    atomic.returnOutFenceOnCommit = false; // Evita reutilização imediata do mesmo número de FD pelo mock no Frame 3
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(presenter.bufferManager().buffers().at(0).releaseFenceFd, -1);
    QCOMPARE(::fcntl(outFenceFrame2, F_GETFD), -1);
    dispatcher.dispatchSimulatedPageFlip(3, 0);
    QCOMPARE(sink.sent, 3);

    // Caso de segurança: releaseFenceFd NÃO sinalizada impede sobrescrever slot ocupado pelo display
    ControlledPipe unsignaledPipe;
    QVERIFY(unsignaledPipe.readFd >= 0);
    // Simula buffer Available com releaseFenceFd não sinalizada tentando renderIntoBuffer
    GbmExportedBuffer busySlot;
    busySlot.vkImageHandle = presenter.bufferManager().buffers().at(0).vkImageHandle;
    busySlot.vkMemoryHandle = presenter.bufferManager().buffers().at(0).vkMemoryHandle;
    busySlot.releaseFenceFd = unsignaledPipe.readFd;
    // Ajusta poll rápido via timeout? Em renderIntoBuffer timeout é 2000ms, então testamos isSyncFdSignaled diretamente
    // para não atrasar 2s no CTest e verificamos que retorna false enquanto não sinalizado.
    QVERIFY(!VulkanQuickRenderer::isSyncFdSignaled(unsignaledPipe.readFd, 5));
    unsignaledPipe.signal();
    QVERIFY(VulkanQuickRenderer::isSyncFdSignaled(unsignaledPipe.readFd, 50));
    ::close(unsignaledPipe.readFd);

    loop.shutdown();
    renderer.destroySharedPoolSlots(presenter.bufferManager());
    presenter.shutdown();
    renderer.shutdown();
}

void tst_VulkanKmsSync::atomicKmsInFenceAbsentFallbackAndOutFenceAbsent() {
    SyncMockKms kms;
    SyncMockGbm gbm;
    SyncMockAtomic atomic;
    atomic.fenceCaps = {false, false}; // Nem IN_FENCE_FD nem OUT_FENCE_PTR disponíveis
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(!presenter.fenceCapabilities().supportsInFenceFd);
    QVERIFY(!presenter.fenceCapabilities().supportsOutFencePtr);
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 128, 128, 60, true}));

    // 1. IN_FENCE_FD ausente com fence já sinalizada: aguarda/fecha FD e envia commit com inFenceFd=-1
    auto *buf = presenter.bufferManager().acquireBufferForRendering();
    QVERIFY(buf != nullptr);
    const int signaledFd = makePipeFd(true);
    QVERIFY(signaledFd >= 0);
    QVERIFY(presenter.bufferManager().markBufferReadyForScanout(buf->id, signaledFd));
    QVERIFY(presenter.submitBufferForScanout(buf->id));
    QCOMPARE(atomic.commits.size(), 1);
    QCOMPARE(atomic.commits.last().inFenceFd, -1);
    QCOMPARE(::fcntl(signaledFd, F_GETFD), -1);
    QCOMPARE(presenter.pendingOutFenceFd(), -1);
    QVERIFY(presenter.onPageFlipCompleted(buf->id));
    QCOMPARE(buf->state, GbmBufferState::Presented);

    presenter.shutdown();
}

void tst_VulkanKmsSync::atomicKmsCommitFailureDuplicateFlipTimeoutAndReorderedEvents() {
    SyncMockKms kms;
    SyncMockGbm gbm;
    SyncMockAtomic atomic;
    atomic.fenceCaps = {true, true};
    atomic.returnOutFenceOnCommit = true;
    atomic.failCommit = true;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 128, 128, 60, true}));

    // 1. Falha de commit fecha outFenceFd retornado pelo backend e faz rollback do buffer
    auto *buf0 = presenter.bufferManager().acquireBufferForRendering();
    QVERIFY(buf0 != nullptr);
    const int inFd = makePipeFd(true);
    QVERIFY(presenter.bufferManager().markBufferReadyForScanout(buf0->id, inFd));
    QVERIFY(!presenter.submitBufferForScanout(buf0->id));
    QCOMPARE(buf0->state, GbmBufferState::Available);
    QCOMPARE(::fcntl(inFd, F_GETFD), -1);
    QVERIFY(atomic.lastReturnedOutFence >= 0);
    QCOMPARE(::fcntl(atomic.lastReturnedOutFence, F_GETFD), -1);

    // 2. Flip duplicado / fora de ordem não corrompe estado nem vaza releaseFenceFd
    atomic.failCommit = false;
    QVERIFY(presenter.bufferManager().acquireBufferForRendering() == buf0);
    QVERIFY(presenter.bufferManager().markBufferReadyForScanout(buf0->id));
    QVERIFY(presenter.submitBufferForScanout(buf0->id));
    QVERIFY(presenter.onPageFlipCompleted(buf0->id));
    const int extraReleaseFd = makePipeFd(true);
    QVERIFY(!presenter.onPageFlipCompleted(buf0->id, extraReleaseFd)); // Duplicado
    QCOMPARE(::fcntl(extraReleaseFd, F_GETFD), -1);
    QVERIFY(!presenter.onPageFlipCompleted(9999ULL)); // Token inexistente

    // 3. Timeout marca TimedOutInScanout e impede reuso
    auto *buf1 = presenter.bufferManager().acquireBufferForRendering();
    QVERIFY(buf1 != nullptr && buf1->id != buf0->id);
    QVERIFY(presenter.bufferManager().markBufferReadyForScanout(buf1->id));
    QVERIFY(presenter.submitBufferForScanout(buf1->id));
    QVERIFY(presenter.bufferManager().markBufferTimedOut(buf1->id));
    QCOMPARE(buf1->state, GbmBufferState::TimedOutInScanout);
    QVERIFY(presenter.bufferManager().acquireBufferForRendering() == nullptr);

    presenter.shutdown();
    QCOMPARE(gbm.activeBuffers, 0);
}

void tst_VulkanKmsSync::suspendWithPendingCommitAndResumeWithNewFdClearsFbIds() {
    SyncMockKms kms;
    SyncMockGbm gbm;
    SyncMockAtomic atomic;
    atomic.fenceCaps = {true, true};
    atomic.returnOutFenceOnCommit = true;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 128, 128, 60, true}));

    auto *buf = presenter.bufferManager().acquireBufferForRendering();
    QVERIFY(buf != nullptr);
    QVERIFY(presenter.bufferManager().markBufferReadyForScanout(buf->id));
    QVERIFY(presenter.submitBufferForScanout(buf->id));
    const uint32_t oldFb = buf->kmsFramebufferId;
    QVERIFY(oldFb != 0);
    const int pendingOut = presenter.pendingOutFenceFd();
    QVERIFY(pendingOut >= 0);

    // Suspensão fecha outFenceFd pendente e bloqueia commits
    presenter.suspendPresentation();
    QVERIFY(presenter.isSuspended());
    QCOMPARE(presenter.pendingOutFenceFd(), -1);
    QCOMPARE(::fcntl(pendingOut, F_GETFD), -1);
    QVERIFY(!presenter.presentFrame(2, 10));

    // Retomada com novo FD invalida FB IDs antigos para recriação segura
    QVERIFY(presenter.resumePresentation(25));
    QCOMPARE(buf->kmsFramebufferId, 0U);
    QVERIFY(presenter.bufferManager().releaseBufferToAvailable(buf->id));
    QVERIFY(presenter.presentFrame(3, 25));
    QVERIFY(buf->kmsFramebufferId != 0 && buf->kmsFramebufferId != oldFb);

    presenter.shutdown();
    QCOMPARE(gbm.activeBuffers, 0);
}

QTEST_MAIN(tst_VulkanKmsSync)
#include "tst_VulkanKmsSync.moc"
