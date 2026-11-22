#include <QtTest>

#include "AtomicKmsPresenter.hpp"
#include "DrmEventDispatcher.hpp"
#include "GbmBufferManager.hpp"
#include "IndependentFrameLoop.hpp"
#include "KmsOutputDiscovery.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QSGRendererInterface>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

using namespace Systalia::Platform::Drm;

namespace {

class AuditMockGbm final : public IGbmBackend {
public:
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &, GbmExportedBuffer *) override { return true; }
    void destroyBuffer(GbmExportedBuffer *) override {}
};

int makeUnsignaledPipeFd(int *writeEndOut) {
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return -1;
    }
    *writeEndOut = fds[1];
    return fds[0];
}

} // namespace

class tst_VulkanDmabufAudit final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void auditsExternalMemoryAndDrmModifierCapabilitiesOnRadv();
    void runsSafeMatrixScenariosAandBWithDeterministicCleanup();
    void rejectsUnsignaledReleaseFenceAndIncompatibleModifiersSafely();
    void recyclesWaitSemaphoreWhenFrameAbortedAfterImport();
    void submissionTrackerDistinguishesSubmittedTimedOutAndRealGpuCompletion();
    void sharedPoolSlotsPreserveInternalOffscreenImageWithoutDoubleFree();
    void atomicPresenterNegotiatesModifiersAndHandlesCommitFailureAndLateFlip();
    void fiveFrameSlotAlternationWithReleaseFenceAndShutdownPending();
    void firstModesetStateMachineAndModeChangeContract();
    void inFormatsBlobParsingValidEmptyMalformedAndNoIntersection();
    void firstModesetModeBlobOutFenceReleaseAndQpaSync();
    void slotReuseScenariosAtoDAndShellRootWithReleaseFence();
};

void tst_VulkanDmabufAudit::auditsExternalMemoryAndDrmModifierCapabilitiesOnRadv() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));

    const auto caps = renderer.capabilities();
    QVERIFY(caps.supportsExternalMemoryFd);
    QVERIFY(caps.supportsExternalMemoryDmabuf);
    QVERIFY(caps.supportsDrmFormatModifier);
    QVERIFY(caps.supportsExternalSemaphoreFd);
    QVERIFY(caps.supportsSyncFdExport);
    QVERIFY(caps.supportsSyncFdImport);

    const auto modCaps = renderer.queryDrmModifierCapabilities();
    QVERIFY(!modCaps.isEmpty());

    const auto singlePlaneExportable = renderer.querySupportedDrmModifiers();
    QVERIFY(!singlePlaneExportable.isEmpty());
    QVERIFY(singlePlaneExportable.contains(DRM_FORMAT_MOD_LINEAR));

    for (const auto &entry : modCaps) {
        if (singlePlaneExportable.contains(entry.modifier)) {
            QCOMPARE(entry.planeCount, 1U);
            QVERIFY(entry.dmabufExportable);
        }
    }

    renderer.shutdown();
}

void tst_VulkanDmabufAudit::runsSafeMatrixScenariosAandBWithDeterministicCleanup() {
    // Cenário A: 128x128, Vulkan interno, efeitos desativados
    {
        VulkanQuickRenderer rendererA;
        QVERIFY2(rendererA.initialize(QSize(128, 128)), qPrintable(rendererA.lastError()));
        QVERIFY(rendererA.loadQmlSource(
            "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#102030' }\n"));
        for (int i = 0; i < 3; ++i) {
            QVERIFY(rendererA.renderFrame());
            QVERIFY(!rendererA.isDeviceLost());
        }
        rendererA.shutdown();
    }

    // Cenário B: 128x128, pool DMA-BUF (2 slots), efeitos desativados
    {
        VulkanQuickRenderer rendererB;
        QVERIFY2(rendererB.initialize(QSize(128, 128)), qPrintable(rendererB.lastError()));
        AuditMockGbm gbm;
        GbmBufferManager pool(gbm);
        QVERIFY(rendererB.createSharedPoolSlots(pool, QSize(128, 128), 2, {DRM_FORMAT_MOD_LINEAR}));
        QCOMPARE(pool.bufferCount(), 2);

        const int fd0 = pool.buffers().at(0).dmabufFd;
        const int fd1 = pool.buffers().at(1).dmabufFd;
        QCOMPARE(pool.buffers().at(0).modifier, DRM_FORMAT_MOD_LINEAR);
        QVERIFY(pool.buffers().at(0).stride >= 128U * 4U);

        QVERIFY(rendererB.loadQmlSource(
            "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#228866' }\n"));

        for (int i = 0; i < 3; ++i) {
            auto *slot = pool.acquireBufferForRendering();
            QVERIFY(slot != nullptr);
            QVERIFY(rendererB.renderIntoBuffer(*slot));
            QVERIFY(slot->acquireFenceFd >= 0);
            QVERIFY(VulkanQuickRenderer::isSyncFdSignaled(slot->acquireFenceFd, 2000));
            ::close(slot->acquireFenceFd);
            slot->acquireFenceFd = -1;
            QVERIFY(pool.releaseBufferToAvailable(slot->id));
        }

        rendererB.destroySharedPoolSlots(pool);
        pool.shutdown();
        rendererB.shutdown();

        QCOMPARE(::fcntl(fd0, F_GETFD), -1);
        QCOMPARE(::fcntl(fd1, F_GETFD), -1);
    }
}

void tst_VulkanDmabufAudit::rejectsUnsignaledReleaseFenceAndIncompatibleModifiersSafely() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));

    AuditMockGbm gbm;
    GbmBufferManager pool(gbm);

    // Modificador incompatível falha sem vazar slots
    QVERIFY(!renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {0xdeadbeefcafebabeULL}));
    QCOMPARE(pool.bufferCount(), 0);

    // Criação válida e verificação de isSyncFdSignaled em fence não sinalizada
    QVERIFY(renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {DRM_FORMAT_MOD_LINEAR}));
    int writeEnd = -1;
    const int unsignaledReadFd = makeUnsignaledPipeFd(&writeEnd);
    QVERIFY(unsignaledReadFd >= 0);
    QVERIFY(!VulkanQuickRenderer::isSyncFdSignaled(unsignaledReadFd, 5));
    ::close(writeEnd);
    ::close(unsignaledReadFd);

    renderer.destroySharedPoolSlots(pool);
    pool.shutdown();
    renderer.shutdown();
}

void tst_VulkanDmabufAudit::recyclesWaitSemaphoreWhenFrameAbortedAfterImport() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));

    AuditMockGbm gbm;
    GbmBufferManager pool(gbm);
    QVERIFY(renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {DRM_FORMAT_MOD_LINEAR}));
    QVERIFY(renderer.loadQmlSource(
        "import QtQuick 2.15\nRectangle { width: 64; height: 64; color: '#114477' }\n"));

    auto *slot0 = pool.acquireBufferForRendering();
    QVERIFY(slot0 != nullptr);
    QVERIFY(renderer.renderIntoBuffer(*slot0));
    const int realSyncFd = slot0->acquireFenceFd;
    QVERIFY(realSyncFd >= 0);
    slot0->acquireFenceFd = -1;
    QVERIFY(pool.releaseBufferToAvailable(slot0->id));

    // Simula importação de releaseFenceFd seguida de suspensão (frame abortado antes de submissão)
    slot0 = pool.acquireBufferForRendering();
    QVERIFY(slot0 != nullptr);
    slot0->releaseFenceFd = realSyncFd;
    const uint64_t origWaitSem = slot0->vkWaitSemaphoreHandle;
    QVERIFY(origWaitSem != 0);
    QVERIFY(renderer.renderIntoBuffer(*slot0));
    QCOMPARE(slot0->releaseFenceFd, -1);
    if (slot0->acquireFenceFd >= 0) {
        ::close(slot0->acquireFenceFd);
        slot0->acquireFenceFd = -1;
    }
    QCOMPARE(::fcntl(realSyncFd, F_GETFD), -1);

    renderer.destroySharedPoolSlots(pool);
    pool.shutdown();
    renderer.shutdown();
}

void tst_VulkanDmabufAudit::submissionTrackerDistinguishesSubmittedTimedOutAndRealGpuCompletion() {
    // 1. Transições inválidas são rejeitadas (impossível ir para Completed sem Submitted)
    GpuSubmissionTracker tracker;
    tracker.markRecorded(1, 10, 0x1234);
    QVERIFY(!tracker.markCompleted(1));
    QVERIFY(tracker.markSubmitted(1, 99));
    QCOMPARE(tracker.find(1)->state, GpuSubmissionState::Submitted);
    QVERIFY(tracker.markCompleted(1));
    QCOMPARE(tracker.find(1)->state, GpuSubmissionState::Completed);
    QVERIFY(!tracker.markTimedOut(1)); // Já concluído

    // 2. Observação real de SYNC_FD em RADV vs fence não sinalizada (timeout)
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));
    AuditMockGbm gbm;
    GbmBufferManager pool(gbm);
    QVERIFY(renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {DRM_FORMAT_MOD_LINEAR}));
    QVERIFY(renderer.loadQmlSource(
        "import QtQuick 2.15\nRectangle { width: 64; height: 64; color: '#226688' }\n"));

    auto *slot0 = pool.acquireBufferForRendering();
    QVERIFY(slot0 != nullptr);
    QVERIFY(renderer.renderIntoBuffer(*slot0));
    const uint64_t frameId1 = renderer.lastSubmittedFrameId();
    QCOMPARE(renderer.submissionTracker().find(frameId1)->state, GpuSubmissionState::Submitted);

    // Observa conclusão real no SYNC_FD exportado
    QVERIFY(renderer.observeSyncFdCompletion(frameId1, slot0->acquireFenceFd, 2000));
    QCOMPARE(renderer.submissionTracker().find(frameId1)->state, GpuSubmissionState::Completed);
    ::close(slot0->acquireFenceFd);
    slot0->acquireFenceFd = -1;
    QVERIFY(pool.releaseBufferToAvailable(slot0->id));

    // 3. Exportação presente, mas fence ainda não sinalizada -> CompletionTimedOut (não vira Completed)
    auto *slot1 = pool.acquireBufferForRendering();
    QVERIFY(slot1 != nullptr);
    QVERIFY(renderer.renderIntoBuffer(*slot1));
    const uint64_t frameId2 = renderer.lastSubmittedFrameId();
    int writeEnd = -1;
    const int unsignaledFd = makeUnsignaledPipeFd(&writeEnd);
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, unsignaledFd, 5));
    QCOMPARE(renderer.submissionTracker().find(frameId2)->state, GpuSubmissionState::CompletionTimedOut);

    // 4. POLLHUP (ponta de escrita fechada sem dados) e FD fechado/inválido (POLLNVAL) nunca viram Completed
    ::close(writeEnd);
    QVERIFY(!VulkanQuickRenderer::isSyncFdSignaled(unsignaledFd, 5));
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, unsignaledFd, 5));
    ::close(unsignaledFd);
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, unsignaledFd, 5)); // POLLNVAL
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, -1, 5));

    if (slot1->acquireFenceFd >= 0) {
        ::close(slot1->acquireFenceFd);
        slot1->acquireFenceFd = -1;
    }
    QVERIFY(pool.releaseBufferToAvailable(slot1->id));

    renderer.destroySharedPoolSlots(pool);
    pool.shutdown();
    renderer.shutdown();
}

void tst_VulkanDmabufAudit::sharedPoolSlotsPreserveInternalOffscreenImageWithoutDoubleFree() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));
    const VkImage initialColorImage = renderer.colorImage();
    QVERIFY(initialColorImage != VK_NULL_HANDLE);
    QVERIFY(renderer.loadQmlSource(
        "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#0b1e36' }\n"));

    AuditMockGbm gbm;
    GbmBufferManager pool(gbm);
    QVERIFY(renderer.createSharedPoolSlots(pool, QSize(128, 128), 2, {DRM_FORMAT_MOD_LINEAR}));
    auto *slot0 = pool.acquireBufferForRendering();
    QVERIFY(slot0 != nullptr);
    QVERIFY(renderer.renderIntoBuffer(*slot0));
    // O slot externo não deve sobrescrever m_colorImage interno (evita leak + double-free no shutdown)
    QCOMPARE(renderer.colorImage(), initialColorImage);

    if (slot0->acquireFenceFd >= 0) {
        ::close(slot0->acquireFenceFd);
        slot0->acquireFenceFd = -1;
    }
    QVERIFY(pool.releaseBufferToAvailable(slot0->id));

    renderer.destroySharedPoolSlots(pool);
    pool.shutdown();
    renderer.shutdown();
    renderer.shutdown(); // Shutdown idempotente
}

void tst_VulkanDmabufAudit::atomicPresenterNegotiatesModifiersAndHandlesCommitFailureAndLateFlip() {
    class ParityMockKms final : public IKmsResourceBackend {
    public:
        KmsDiscoverySnapshot probeResources(int drmFd) override {
            KmsDiscoverySnapshot snap;
            if (drmFd < 0) return snap;
            snap.valid = true;
            snap.atomicSupported = true;
            snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 1920, 1080});
            snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
            KmsConnectorInfo hdmi;
            hdmi.connectorId = 20;
            hdmi.name = QStringLiteral("HDMI-A-1");
            hdmi.connected = true;
            hdmi.possibleCrtcs = 0x1;
            hdmi.modes.append(DrmOutputMode{QStringLiteral("HDMI-A-1"), 1920, 1080, 60, true});
            hdmi.preferredMode = hdmi.modes.first();
            snap.connectors.append(hdmi);
            return snap;
        }
    };
    class ParityMockAtomic final : public IAtomicKmsBackend {
    public:
        bool failCommits = false;
        uint32_t nextFb = 500;
        uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override {
            return b.stride > 0 ? nextFb++ : 0;
        }
        void removeFramebuffer(int, uint32_t) override {}
        AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &) override {
            return { !failCommits, -1 };
        }
    };

    ParityMockKms kms;
    AuditMockGbm gbm;
    ParityMockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    presenter.setUseExternalVulkanPool(true);
    QVERIFY(presenter.initializeOnFd(10));

    const auto badNeg = GbmBufferManager::negotiateModifiers(
        DRM_FORMAT_XRGB8888, {DRM_FORMAT_MOD_LINEAR}, {DRM_FORMAT_MOD_INVALID});
    QVERIFY(!badNeg.compatible);

    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("HDMI-A-1"), 1920, 1080, 60, true}));
    GbmExportedBuffer extSlot;
    extSlot.width = 1920;
    extSlot.height = 1080;
    extSlot.format = DRM_FORMAT_XRGB8888;
    extSlot.stride = 1920 * 4;
    extSlot.modifier = DRM_FORMAT_MOD_LINEAR;
    auto *registered = presenter.bufferManager().registerExternalSlot(extSlot);
    QVERIFY(registered != nullptr);

    int pipeFds[2] = {-1, -1};
    QCOMPARE(::pipe(pipeFds), 0);
    ::close(pipeFds[1]); // sinalizado/fechado apenas como FD válido para ownership check
    auto *acquired = presenter.bufferManager().acquireBufferForRendering();
    QVERIFY(acquired != nullptr);
    QVERIFY(presenter.bufferManager().markBufferReadyForScanout(acquired->id, pipeFds[0]));

    atomic.failCommits = true;
    QVERIFY(!presenter.submitBufferForScanout(acquired->id));
    QCOMPARE(acquired->state, GbmBufferState::Available);
    QCOMPARE(::fcntl(pipeFds[0], F_GETFD), -1); // FD fechado em falha de commit

    // Page-flip tardio ou com userData inválido é ignorado com segurança
    DrmEventDispatcher dispatcher;
    dispatcher.setFlipContextRegistry(presenter.flipContextRegistry());
    dispatcher.dispatchKernelPageFlipForTest(1, 55, nullptr);
    QCOMPARE(presenter.flipContextRegistry()->liveContextCount(), size_t(0));

    presenter.shutdown();
}

void tst_VulkanDmabufAudit::fiveFrameSlotAlternationWithReleaseFenceAndShutdownPending() {
    class MultiFrameKms final : public IKmsResourceBackend {
    public:
        KmsDiscoverySnapshot probeResources(int drmFd) override {
            KmsDiscoverySnapshot snap;
            if (drmFd < 0) return snap;
            snap.valid = true;
            snap.atomicSupported = true;
            snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 1920, 1080});
            snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
            KmsConnectorInfo hdmi;
            hdmi.connectorId = 20;
            hdmi.name = QStringLiteral("HDMI-A-1");
            hdmi.connected = true;
            hdmi.possibleCrtcs = 0x1;
            hdmi.modes.append(DrmOutputMode{QStringLiteral("HDMI-A-1"), 1920, 1080, 60, true});
            hdmi.preferredMode = hdmi.modes.first();
            snap.connectors.append(hdmi);
            return snap;
        }
    };
    class MultiFrameAtomic final : public IAtomicKmsBackend {
    public:
        QList<AtomicCommitRequest> requests;
        uint32_t nextFb = 700;
        uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override {
            return b.stride > 0 ? nextFb++ : 0;
        }
        void removeFramebuffer(int, uint32_t) override {}
        AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
            requests.append(req);
            int p[2] = {-1, -1};
            if (::pipe(p) == 0) {
                ::close(p[1]); // sinalizado imediatamente como outFence simulado
                return {true, p[0]};
            }
            return {true, -1};
        }
    };
    class MultiFrameRenderSource final : public IControlledRenderSource {
    public:
        QList<uint64_t> renderedSlots;
        bool renderControlledFrame() override { return true; }
        bool renderIntoBuffer(GbmExportedBuffer &slot) override {
            renderedSlots.append(slot.id);
            if (slot.releaseFenceFd >= 0) {
                ::close(slot.releaseFenceFd);
                slot.releaseFenceFd = -1;
            }
            return true;
        }
    };
    class MultiFrameSink final : public IWaylandOutputFrameSink {
    public:
        int callbacksSent = 0;
        void setAutomaticCallbacks(bool) override {}
        void notifyFrameStarted() override {}
        void notifySendFrameCallbacks() override { ++callbacksSent; }
    };

    MultiFrameKms kms;
    AuditMockGbm gbm;
    MultiFrameAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    presenter.setUseExternalVulkanPool(true);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("HDMI-A-1"), 1920, 1080, 60, true}));

    for (int i = 0; i < 2; ++i) {
        GbmExportedBuffer slot;
        slot.width = 1920;
        slot.height = 1080;
        slot.format = DRM_FORMAT_XRGB8888;
        slot.stride = 1920 * 4;
        slot.modifier = DRM_FORMAT_MOD_LINEAR;
        QVERIFY(presenter.bufferManager().registerExternalSlot(slot) != nullptr);
    }

    MultiFrameRenderSource renderSource;
    MultiFrameSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(renderSource, presenter, wayland, dispatcher);

    for (int f = 0; f < 5; ++f) {
        loop.requestFrame();
        QVERIFY(loop.processNextFrame());
        const uint64_t activeSlot = renderSource.renderedSlots.last();
        void *userData = atomic.requests.last().userData;

        // Flip duplicado não deve causar erro nem double-release
        dispatcher.dispatchKernelPageFlipForTest(static_cast<uint64_t>(f + 1), 55, userData);
        dispatcher.dispatchKernelPageFlipForTest(static_cast<uint64_t>(f + 1), 55, userData);

        QCOMPARE(presenter.bufferManager().findBuffer(activeSlot)->state, GbmBufferState::Presented);
        if (f >= 1) {
            const uint64_t prevSlot = renderSource.renderedSlots.at(f - 1);
            QVERIFY(prevSlot != activeSlot);
            QCOMPARE(presenter.bufferManager().findBuffer(prevSlot)->state, GbmBufferState::Available);
        }
    }
    QCOMPARE(renderSource.renderedSlots.size(), 5);
    QCOMPARE(loop.presentedCount(), 5ULL);
    QCOMPARE(sink.callbacksSent, 5);

    // Shutdown com commit pendente
    loop.requestFrame();
    QVERIFY(loop.processNextFrame());
    QVERIFY(loop.isFlipInFlight());
    loop.shutdown();
    presenter.releaseKmsFramebuffers();
    presenter.shutdown();
}

void tst_VulkanDmabufAudit::firstModesetStateMachineAndModeChangeContract() {
    class ModesetMockKms final : public IKmsResourceBackend {
    public:
        KmsDiscoverySnapshot probeResources(int drmFd) override {
            KmsDiscoverySnapshot snap;
            if (drmFd < 0) return snap;
            snap.valid = true;
            snap.atomicSupported = true;
            snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 1920, 1080});
            snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
            KmsConnectorInfo hdmi;
            hdmi.connectorId = 20;
            hdmi.name = QStringLiteral("HDMI-A-1");
            hdmi.connected = true;
            hdmi.possibleCrtcs = 0x1;
            hdmi.modes.append(DrmOutputMode{QStringLiteral("HDMI-A-1"), 1920, 1080, 60, true});
            hdmi.preferredMode = hdmi.modes.first();
            snap.connectors.append(hdmi);
            return snap;
        }
    };
    class ModesetMockAtomic final : public IAtomicKmsBackend {
    public:
        bool failNextCommit = false;
        QList<bool> allowModesetFlags;
        uint32_t addFramebuffer(int, const GbmExportedBuffer &) override { return 900; }
        void removeFramebuffer(int, uint32_t) override {}
        AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
            allowModesetFlags.append(req.allowModeset);
            if (failNextCommit) {
                failNextCommit = false;
                return {false, -1};
            }
            return {true, -1};
        }
    };

    ModesetMockKms kms;
    AuditMockGbm gbm;
    ModesetMockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QCOMPARE(presenter.modesetState(), KmsModesetState::Unconfigured);

    QVERIFY(presenter.initializeOnFd(10));
    QCOMPARE(presenter.modesetState(), KmsModesetState::InitialModesetPending);
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("HDMI-A-1"), 1920, 1080, 60, true}));
    QCOMPARE(presenter.modesetState(), KmsModesetState::InitialModesetPending);

    // Falha no primeiro modeset mantém InitialModesetPending
    atomic.failNextCommit = true;
    QVERIFY(!presenter.presentFrame(1, 10));
    QCOMPARE(presenter.modesetState(), KmsModesetState::InitialModesetPending);
    QCOMPARE(atomic.allowModesetFlags.last(), true);

    // Sucesso no primeiro modeset transiciona para Active
    QVERIFY(presenter.presentFrame(2, 10));
    QCOMPARE(presenter.modesetState(), KmsModesetState::Active);
    QCOMPARE(atomic.allowModesetFlags.last(), true);

    // Segundo frame em Active não usa allowModeset
    const auto buf2 = presenter.bufferManager().buffers().at(1).id;
    QVERIFY(presenter.onPageFlipCompleted(presenter.bufferManager().buffers().at(0).id));
    QVERIFY(presenter.presentFrame(3, 10));
    QCOMPARE(atomic.allowModesetFlags.last(), false);
    QVERIFY(presenter.onPageFlipCompleted(buf2));

    // Mudança de modo após inicialização retorna para InitialModesetPending
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("HDMI-A-1"), 1280, 720, 60, false}));
    QCOMPARE(presenter.modesetState(), KmsModesetState::InitialModesetPending);
    QVERIFY(presenter.presentFrame(4, 10));
    QCOMPARE(atomic.allowModesetFlags.last(), true);
    QCOMPARE(presenter.modesetState(), KmsModesetState::Active);

    // Suspensão e retomada
    presenter.suspendPresentation();
    QCOMPARE(presenter.modesetState(), KmsModesetState::Suspended);
    QVERIFY(presenter.resumePresentation(11));
    QCOMPARE(presenter.modesetState(), KmsModesetState::InitialModesetPending);
    presenter.shutdown();
    QCOMPARE(presenter.modesetState(), KmsModesetState::Unconfigured);
}

void tst_VulkanDmabufAudit::inFormatsBlobParsingValidEmptyMalformedAndNoIntersection() {
    // 1. Blob nulo ou menor que o cabeçalho
    QVERIFY(KmsOutputDiscovery::parseInFormatsBlob(nullptr, 0, DRM_FORMAT_XRGB8888).isEmpty());
    const uint8_t tiny[4] = {1, 2, 3, 4};
    QVERIFY(KmsOutputDiscovery::parseInFormatsBlob(tiny, sizeof(tiny), DRM_FORMAT_XRGB8888).isEmpty());

    // 2. Blob válido com XRGB8888 (index 1) suportando LINEAR e modifier tiled 0x200000000401a01
    struct ValidBlob {
        drm_format_modifier_blob hdr{};
        uint32_t formats[2]{};
        drm_format_modifier mods[2]{};
    } valid;
    valid.hdr.version = FORMAT_BLOB_CURRENT;
    valid.hdr.count_formats = 2;
    valid.hdr.formats_offset = offsetof(ValidBlob, formats);
    valid.hdr.count_modifiers = 2;
    valid.hdr.modifiers_offset = offsetof(ValidBlob, mods);
    valid.formats[0] = DRM_FORMAT_RGB565;
    valid.formats[1] = DRM_FORMAT_XRGB8888;
    valid.mods[0] = drm_format_modifier{0x1ULL, 0, 0, DRM_FORMAT_MOD_LINEAR};          // só RGB565
    valid.mods[1] = drm_format_modifier{0x2ULL, 0, 0, 0x200000000401a01ULL};         // só XRGB8888

    const auto parsed = KmsOutputDiscovery::parseInFormatsBlob(&valid, sizeof(valid), DRM_FORMAT_XRGB8888);
    QCOMPARE(parsed, QList<uint64_t>({0x200000000401a01ULL}));

    // 3. Blob malformado com offsets/contagens fora dos limites
    ValidBlob malformed = valid;
    malformed.hdr.count_formats = 999999U;
    QVERIFY(KmsOutputDiscovery::parseInFormatsBlob(&malformed, sizeof(malformed), DRM_FORMAT_XRGB8888).isEmpty());

    malformed = valid;
    malformed.hdr.modifiers_offset = sizeof(valid) + 64U;
    QVERIFY(KmsOutputDiscovery::parseInFormatsBlob(&malformed, sizeof(malformed), DRM_FORMAT_XRGB8888).isEmpty());

    // 4. Sem interseção entre KMS e Vulkan não faz fallback silencioso
    const auto neg = GbmBufferManager::negotiateModifiers(
        DRM_FORMAT_XRGB8888, parsed, {DRM_FORMAT_MOD_LINEAR});
    QVERIFY(!neg.compatible);
    QVERIFY(neg.negotiatedModifiers.isEmpty());
}

void tst_VulkanDmabufAudit::firstModesetModeBlobOutFenceReleaseAndQpaSync() {
    // 1. Paridade QPA vs. modo físico descoberto (sem recriar QGuiApplication/Vulkan)
    if (QGuiApplication::platformName() == QLatin1String("sge_vulkan_offscreen")) {
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen != nullptr);
        const QSize origSize = screen->size();
        const qreal origHz = screen->refreshRate();
        qGuiApp->setProperty("sgeKmsOutputRefreshHz", 144);
        qGuiApp->setProperty("sgeKmsOutputSize", QSize(2560, 1440));
        QTRY_COMPARE(screen->size(), QSize(2560, 1440));
        QCOMPARE(screen->refreshRate(), 144.0);
        qGuiApp->setProperty("sgeKmsOutputRefreshHz", static_cast<int>(origHz));
        qGuiApp->setProperty("sgeKmsOutputSize", origSize);
        QTRY_COMPARE(screen->size(), origSize);
    }

    // 2. Mode blob no primeiro modeset + OUT_FENCE_PTR + releaseFenceFd não sinalizada + flip atrasado
    class BlobMockKms final : public IKmsResourceBackend {
    public:
        KmsDiscoverySnapshot probeResources(int drmFd) override {
            KmsDiscoverySnapshot snap;
            if (drmFd < 0) return snap;
            snap.valid = true;
            snap.atomicSupported = true;
            snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 2560, 1440});
            snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
            KmsConnectorInfo dp;
            dp.connectorId = 20;
            dp.name = QStringLiteral("DP-1");
            dp.connected = true;
            dp.possibleCrtcs = 0x1;
            const DrmOutputMode mode{QStringLiteral("DP-1"), 2560, 1440, 144, true};
            dp.modes.append(mode);
            drm_mode_modeinfo raw{};
            raw.hdisplay = 2560;
            raw.vdisplay = 1440;
            dp.rawModeInfos.append(QByteArray(reinterpret_cast<const char *>(&raw), sizeof(raw)));
            dp.preferredMode = mode;
            snap.connectors.append(dp);
            return snap;
        }
    };
    class OutFenceMockAtomic final : public IAtomicKmsBackend {
    public:
        QList<AtomicCommitRequest> commits;
        int nextOutFenceFd = -1;
        bool failCommit = false;
        uint32_t addFramebuffer(int, const GbmExportedBuffer &) override { return 901; }
        void removeFramebuffer(int, uint32_t) override {}
        AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
            commits.append(req);
            if (failCommit) {
                const int leakedCandidate = nextOutFenceFd;
                nextOutFenceFd = -1;
                return {false, leakedCandidate};
            }
            const int outFd = nextOutFenceFd;
            nextOutFenceFd = -1;
            return {true, outFd};
        }
    };

    BlobMockKms kms;
    AuditMockGbm gbm;
    OutFenceMockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 2560, 1440, 144, true}));

    // Falha simulada (ex.: perda de DRM master) fecha outFenceFd retornado e mantém InitialModesetPending
    int failPipe[2] = {-1, -1};
    QCOMPARE(::pipe(failPipe), 0);
    ::close(failPipe[1]);
    atomic.failCommit = true;
    atomic.nextOutFenceFd = failPipe[0];
    QVERIFY(!presenter.presentFrame(1, 10));
    QCOMPARE(::fcntl(failPipe[0], F_GETFD), -1);
    QCOMPARE(presenter.modesetState(), KmsModesetState::InitialModesetPending);

    // Commit 1 (modeset): deve incluir modeInfoBlob
    atomic.failCommit = false;
    QVERIFY(presenter.presentFrame(2, 10));
    QCOMPARE(atomic.commits.last().allowModeset, true);
    QCOMPARE(atomic.commits.last().modeInfoBlob.size(), static_cast<qsizetype>(sizeof(drm_mode_modeinfo)));
    const uint64_t buf1Id = presenter.bufferManager().buffers().at(0).id;
    QVERIFY(presenter.onPageFlipCompleted(buf1Id));
    QCOMPARE(presenter.bufferManager().findBuffer(buf1Id)->state, GbmBufferState::Presented);

    // Commit 2 (Active): allowModeset=false, modeInfoBlob vazio, OUT_FENCE_PTR não sinalizada
    int unsignaledPipe[2] = {-1, -1};
    QCOMPARE(::pipe(unsignaledPipe), 0);
    atomic.nextOutFenceFd = unsignaledPipe[0];
    QVERIFY(presenter.presentFrame(3, 10));
    QCOMPARE(atomic.commits.last().allowModeset, false);
    QVERIFY(atomic.commits.last().modeInfoBlob.isEmpty());
    const uint64_t buf2Id = presenter.bufferManager().buffers().at(1).id;
    QVERIFY(presenter.onPageFlipCompleted(buf2Id));

    // buf1 volta para Available carregando unsignaledPipe[0] como releaseFenceFd (ainda não sinalizada)
    auto *buf1 = presenter.bufferManager().findBuffer(buf1Id);
    QCOMPARE(buf1->state, GbmBufferState::Available);
    QCOMPARE(buf1->releaseFenceFd, unsignaledPipe[0]);
    QVERIFY(!VulkanQuickRenderer::isSyncFdSignaled(buf1->releaseFenceFd, 5));

    // Sinaliza outFence e verifica que agora é observada como sinalizada
    const char sig = 1;
    QCOMPARE(::write(unsignaledPipe[1], &sig, 1), 1);
    QVERIFY(VulkanQuickRenderer::isSyncFdSignaled(buf1->releaseFenceFd, 50));
    ::close(unsignaledPipe[1]);

    // Flip atrasado após timeout emite latePageFlipCompleted sem liberar buffer ativo
    DrmEventDispatcher dispatcher;
    dispatcher.setFlipContextRegistry(presenter.flipContextRegistry());
    QSignalSpy lateSpy(&dispatcher, &DrmEventDispatcher::latePageFlipCompleted);
    QVERIFY(presenter.presentFrame(4, 10));
    void *timedOutUserData = atomic.commits.last().userData;
    presenter.markFlipTimedOut(buf1Id);
    dispatcher.dispatchKernelPageFlipForTest(99, 55, timedOutUserData);
    QCOMPARE(lateSpy.count(), 1);
    QCOMPARE(presenter.bufferManager().findBuffer(buf2Id)->state, GbmBufferState::Presented);

    presenter.shutdown();
    QCOMPARE(::fcntl(unsignaledPipe[0], F_GETFD), -1);
}

void tst_VulkanDmabufAudit::slotReuseScenariosAtoDAndShellRootWithReleaseFence() {
    // Cenário A: Reutilização Slot 1 -> Slot 2 -> Slot 1 sem fence externa (releaseFenceFd = -1)
    {
        VulkanQuickRenderer renderer;
        QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));
        AuditMockGbm gbm;
        GbmBufferManager pool(gbm);
        QVERIFY(renderer.createSharedPoolSlots(pool, QSize(128, 128), 2, {DRM_FORMAT_MOD_LINEAR}));
        QVERIFY(renderer.loadQmlSource(
            "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#123456' }\n"));

        auto *slot1 = pool.acquireBufferForRendering();
        QVERIFY(slot1 != nullptr);
        QVERIFY(renderer.renderIntoBuffer(*slot1));
        QVERIFY(pool.markBufferReadyForFlip(slot1->id, slot1->acquireFenceFd));
        slot1->acquireFenceFd = -1;
        QVERIFY(pool.markBufferPresented(slot1->id, 100, -1));

        auto *slot2 = pool.acquireBufferForRendering();
        QVERIFY(slot2 != nullptr && slot2->id != slot1->id);
        QVERIFY(renderer.renderIntoBuffer(*slot2));
        QVERIFY(pool.markBufferReadyForFlip(slot2->id, slot2->acquireFenceFd));
        slot2->acquireFenceFd = -1;
        QVERIFY(pool.markBufferPresented(slot2->id, 101, -1));
        QCOMPARE(slot1->state, GbmBufferState::Available);
        QCOMPARE(slot1->releaseFenceFd, -1);

        auto *reused = pool.acquireBufferForRendering();
        QCOMPARE(reused->id, slot1->id);
        QVERIFY(renderer.renderIntoBuffer(*reused));
        QVERIFY(!renderer.isDeviceLost());
        if (reused->acquireFenceFd >= 0) {
            ::close(reused->acquireFenceFd);
            reused->acquireFenceFd = -1;
        }
        QVERIFY(pool.releaseBufferToAvailable(reused->id));

        renderer.destroySharedPoolSlots(pool);
        pool.shutdown();
        renderer.shutdown();
    }

    // Cenário B: Slot 1 -> Slot 2 -> Slot 1 com releaseFenceFd real sinalizada
    {
        VulkanQuickRenderer renderer;
        QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));
        AuditMockGbm gbm;
        GbmBufferManager pool(gbm);
        QVERIFY(renderer.createSharedPoolSlots(pool, QSize(128, 128), 2));
        QVERIFY(renderer.loadQmlSource(
            "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#2468ac' }\n"));

        auto *slot1 = pool.acquireBufferForRendering();
        QVERIFY(slot1 != nullptr);
        QVERIFY(renderer.renderIntoBuffer(*slot1));
        QVERIFY(pool.markBufferReadyForFlip(slot1->id, slot1->acquireFenceFd));
        slot1->acquireFenceFd = -1;
        QVERIFY(pool.markBufferPresented(slot1->id, 200, -1));

        auto *slot2 = pool.acquireBufferForRendering();
        QVERIFY(slot2 != nullptr && slot2->id != slot1->id);
        QVERIFY(renderer.renderIntoBuffer(*slot2));
        const int syncFd2 = slot2->acquireFenceFd;
        QVERIFY(syncFd2 >= 0);
        const int releaseFdForSlot1 = ::dup(syncFd2);
        QVERIFY(releaseFdForSlot1 >= 0);
        QVERIFY(pool.markBufferReadyForFlip(slot2->id, syncFd2));
        slot2->acquireFenceFd = -1;
        QVERIFY(pool.markBufferPresented(slot2->id, 201, releaseFdForSlot1));
        QCOMPARE(slot1->state, GbmBufferState::Available);
        QCOMPARE(slot1->releaseFenceFd, releaseFdForSlot1);

        auto *reused = pool.acquireBufferForRendering();
        QCOMPARE(reused->id, slot1->id);
        QVERIFY2(renderer.renderIntoBuffer(*reused), qPrintable(renderer.lastError()));
        QVERIFY(!renderer.isDeviceLost());
        QCOMPARE(reused->releaseFenceFd, -1);
        if (reused->acquireFenceFd >= 0) {
            ::close(reused->acquireFenceFd);
            reused->acquireFenceFd = -1;
        }
        QCOMPARE(::fcntl(releaseFdForSlot1, F_GETFD), -1);
        QVERIFY(pool.releaseBufferToAvailable(reused->id));

        renderer.destroySharedPoolSlots(pool);
        pool.shutdown();
        renderer.shutdown();
    }

    // Cenário C: Release fence pendente bloqueia escrita até sinalização
    {
        int writeEnd = -1;
        const int pendingFd = makeUnsignaledPipeFd(&writeEnd);
        QVERIFY(pendingFd >= 0);
        QVERIFY(!VulkanQuickRenderer::isSyncFdSignaled(pendingFd, 5));
        const char sig = 1;
        QCOMPARE(::write(writeEnd, &sig, 1), 1);
        QVERIFY(VulkanQuickRenderer::isSyncFdSignaled(pendingFd, 50));
        ::close(writeEnd);
        ::close(pendingFd);
    }

    // Cenário D: Falha de importação / fence não sinalizada (POLLHUP/timeout) propaga erro e não retorna Available
    {
        VulkanQuickRenderer renderer;
        QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));
        AuditMockGbm gbm;
        GbmBufferManager pool(gbm);
        QVERIFY(renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {DRM_FORMAT_MOD_LINEAR}));
        QVERIFY(renderer.loadQmlSource(
            "import QtQuick 2.15\nRectangle { width: 64; height: 64; color: '#654321' }\n"));

        auto *slot1 = pool.acquireBufferForRendering();
        QVERIFY(slot1 != nullptr);
        int fds[2] = {-1, -1};
        QCOMPARE(::pipe2(fds, O_CLOEXEC), 0);
        ::close(fds[1]); // POLLHUP sem dados -> falha em vkImportSemaphoreFdKHR e em isSyncFdSignaled
        slot1->releaseFenceFd = fds[0];

        QVERIFY(!renderer.renderIntoBuffer(*slot1));
        QCOMPARE(slot1->releaseFenceFd, -1);
        QCOMPARE(::fcntl(fds[0], F_GETFD), -1);
        QCOMPARE(slot1->state, GbmBufferState::Rendering);

        QVERIFY(pool.releaseBufferToAvailable(slot1->id));
        renderer.destroySharedPoolSlots(pool);
        pool.shutdown();
        renderer.shutdown();
    }
}

QTEST_MAIN(tst_VulkanDmabufAudit)
#include "tst_VulkanDmabufAudit.moc"
