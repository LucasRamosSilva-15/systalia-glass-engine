#include <QtTest>

#include "AtomicKmsPresenter.hpp"
#include "DrmBackendController.hpp"
#include "DrmEventDispatcher.hpp"
#include "GbmBufferManager.hpp"
#include "IndependentFrameLoop.hpp"
#include "KmsOutputDiscovery.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

int makePipeReadFd() {
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return -1;
    }
    ::close(fds[1]);
    return fds[0];
}

class MockSessionProvider final : public ISessionDrmProvider {
public:
    int releaseCount = 0;
    bool isSessionActive() const override { return true; }
    std::optional<DrmDeviceDescriptor> acquireDrmDevice(const QString &path) override {
        const int fd = makePipeReadFd();
        if (fd < 0) {
            return std::nullopt;
        }
        return DrmDeviceDescriptor(fd, 226, 1, path, FdOwnership::OwnedDirect);
    }
    bool releaseDrmDevice(uint32_t, uint32_t, int fd) override {
        ++releaseCount;
        if (fd >= 0) {
            ::close(fd);
        }
        return true;
    }
};

class ConfigurableMockKms final : public IKmsResourceBackend {
public:
    bool hasConnectedMonitor = true;
    bool hasCompatibleCrtc = true;
    bool hasPrimaryPlane = true;

    KmsDiscoverySnapshot probeResources(int drmFd) override {
        KmsDiscoverySnapshot snap;
        if (drmFd < 0) {
            return snap;
        }
        snap.valid = true;
        snap.atomicSupported = true;
        snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 2560, 1440});
        if (hasPrimaryPlane) {
            snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
        }
        KmsConnectorInfo dp;
        dp.connectorId = 20;
        dp.name = QStringLiteral("DP-1");
        dp.connected = hasConnectedMonitor;
        dp.possibleCrtcs = hasCompatibleCrtc ? 0x1u : 0x0u;
        dp.modes.append(DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, false});
        dp.modes.append(DrmOutputMode{QStringLiteral("DP-1"), 2560, 1440, 144, true});
        dp.preferredMode = dp.modes.at(1);
        snap.connectors.append(dp);
        return snap;
    }
};

class TrackingMockGbm final : public IGbmBackend {
public:
    int allocateCalls = 0;
    int activeBuffers = 0;
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &req, GbmExportedBuffer *out) override {
        ++allocateCalls;
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

class RealisticMockAtomic final : public IAtomicKmsBackend {
public:
    bool failCommit = false;
    uint32_t nextFbId = 700;
    QList<uint32_t> removedFbs;
    QList<AtomicCommitRequest> commits;

    KmsFenceCapabilities probeFenceCapabilities(int, uint32_t, uint32_t) override {
        return KmsFenceCapabilities{true, true};
    }

    uint32_t addFramebuffer(int, const GbmExportedBuffer &buffer) override {
        if (buffer.stride == 0) {
            return 0;
        }
        return nextFbId++;
    }

    void removeFramebuffer(int, uint32_t fbId) override {
        removedFbs.append(fbId);
    }

    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
        commits.append(req);
        return AtomicCommitResult{!failCommit, -1};
    }
};

class MockSink final : public IWaylandOutputFrameSink {
public:
    int started = 0;
    int sent = 0;
    void setAutomaticCallbacks(bool) override {}
    void notifyFrameStarted() override { ++started; }
    void notifySendFrameCallbacks() override { ++sent; }
};

class MockRenderSource final : public IControlledRenderSource {
public:
    bool renderControlledFrame() override { return true; }
};

} // namespace

class tst_PageFlipAndUnifiedStartup final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void commitUserDataContextSequentialFlipsAndDuplicateRejection();
    void timeoutPreservesContextAndLateEventReleasesSafely();
    void suspendResumeRejectsOldGenerationEventsAndShutdownCleansUp();
    void dynamicModeSelectionAndFailClosedDiscovery();
    void unifiedVulkanPoolHasNoConcurrentGbmPoolAndCleanShutdownOrder();
};

void tst_PageFlipAndUnifiedStartup::commitUserDataContextSequentialFlipsAndDuplicateRejection() {
    ConfigurableMockKms kms;
    TrackingMockGbm gbm;
    RealisticMockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 1280, 720, 60, true}));

    MockRenderSource render;
    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(render, presenter, wayland, dispatcher);

    // Commit 1 com user_data != nullptr e contexto estável
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(atomic.commits.size(), 1);
    void *ctx1 = atomic.commits.at(0).userData;
    QVERIFY(ctx1 != nullptr);
    QCOMPARE(presenter.flipContextRegistry()->liveContextCount(), size_t(1));

    // Entrega realista via page_flip_handler2 com ctx1
    dispatcher.dispatchKernelPageFlipForTest(101, 55, ctx1);
    QCOMPARE(sink.sent, 1);
    QCOMPARE(loop.presentedCount(), 1ULL);
    QCOMPARE(presenter.flipContextRegistry()->liveContextCount(), size_t(0));

    // Callback duplicado com o mesmo ponteiro ctx1 é ignorado com segurança
    dispatcher.dispatchKernelPageFlipForTest(101, 55, ctx1);
    QCOMPARE(sink.sent, 1);
    QCOMPARE(loop.presentedCount(), 1ULL);

    // Commit 2 gera novo contexto
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(atomic.commits.size(), 2);
    void *ctx2 = atomic.commits.at(1).userData;
    QVERIFY(ctx2 != nullptr);

    dispatcher.dispatchKernelPageFlipForTest(102, 55, ctx2);
    QCOMPARE(sink.sent, 2);
    QCOMPARE(loop.presentedCount(), 2ULL);

    loop.shutdown();
    presenter.shutdown();
}

void tst_PageFlipAndUnifiedStartup::timeoutPreservesContextAndLateEventReleasesSafely() {
    ConfigurableMockKms kms;
    TrackingMockGbm gbm;
    RealisticMockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 1280, 720, 60, true}));

    MockRenderSource render;
    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(render, presenter, wayland, dispatcher);
    loop.setFlipTimeoutMs(25);

    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    void *ctx1 = atomic.commits.at(0).userData;
    QVERIFY(ctx1 != nullptr);

    QTRY_COMPARE(loop.timedOutCount(), 1ULL);
    QVERIFY(!loop.isFlipInFlight());
    QCOMPARE(presenter.bufferManager().countInState(GbmBufferState::TimedOutInScanout), 1);
    // Contexto preservado para reconhecer o evento tardio
    QCOMPARE(presenter.flipContextRegistry()->liveContextCount(), size_t(1));

    // Evento tardio da mesma geração libera buffer TimedOutInScanout para Available sem enviar callback duplicado
    dispatcher.dispatchKernelPageFlipForTest(201, 55, ctx1);
    QCOMPARE(sink.sent, 0);
    QCOMPARE(presenter.bufferManager().countInState(GbmBufferState::TimedOutInScanout), 0);
    QCOMPARE(presenter.bufferManager().countInState(GbmBufferState::Available), 2);
    QCOMPARE(presenter.flipContextRegistry()->liveContextCount(), size_t(0));

    loop.shutdown();
    presenter.shutdown();
}

void tst_PageFlipAndUnifiedStartup::suspendResumeRejectsOldGenerationEventsAndShutdownCleansUp() {
    ConfigurableMockKms kms;
    TrackingMockGbm gbm;
    RealisticMockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 1280, 720, 60, true}));

    MockRenderSource render;
    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(render, presenter, wayland, dispatcher);

    // Frame 1 da geração 1
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    void *oldGenCtx = atomic.commits.at(0).userData;
    const uint64_t genBeforeSuspend = presenter.flipContextRegistry()->currentGeneration();

    loop.suspend();
    QVERIFY(presenter.flipContextRegistry()->currentGeneration() > genBeforeSuspend);

    const int newFd = makePipeReadFd();
    QVERIFY(newFd >= 0);
    loop.resume(newFd);
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(atomic.commits.size(), 2);
    void *newGenCtx = atomic.commits.at(1).userData;

    // Evento da geração antiga não deve concluir o frame novo!
    dispatcher.dispatchKernelPageFlipForTest(301, 55, oldGenCtx);
    QVERIFY(loop.isFlipInFlight());
    QCOMPARE(sink.sent, 0);

    // Evento da geração atual conclui o frame novo
    dispatcher.dispatchKernelPageFlipForTest(302, 55, newGenCtx);
    QVERIFY(!loop.isFlipInFlight());
    QCOMPARE(sink.sent, 1);

    // Shutdown com commit pendente limpa contextos com segurança
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    loop.shutdown();
    presenter.shutdown();
    QCOMPARE(presenter.flipContextRegistry()->liveContextCount(), size_t(0));
    QCOMPARE(gbm.activeBuffers, 0);
    ::close(newFd);
}

void tst_PageFlipAndUnifiedStartup::dynamicModeSelectionAndFailClosedDiscovery() {
    // 1. Modo preferencial dinâmico (2560x1440@144)
    {
        MockSessionProvider session;
        ConfigurableMockKms kms;
        TrackingMockGbm gbm;
        RealisticMockAtomic atomic;
        AtomicKmsPresenter presenter(kms, gbm, atomic);
        presenter.setUseExternalVulkanPool(true);
        DrmBackendController controller(session, presenter);

        QVERIFY(controller.initialize(QStringLiteral("/dev/dri/card1"), {}));
        QCOMPARE(presenter.activeMode().width, 2560);
        QCOMPARE(presenter.activeMode().height, 1440);
        QCOMPARE(presenter.activeMode().refreshHz, 144);
        QCOMPARE(gbm.allocateCalls, 0); // Zero pool GBM paralelo!
        controller.shutdown();
    }

    // 2. Ausência de monitor conectado -> fail-closed
    {
        MockSessionProvider session;
        ConfigurableMockKms kms;
        kms.hasConnectedMonitor = false;
        TrackingMockGbm gbm;
        RealisticMockAtomic atomic;
        AtomicKmsPresenter presenter(kms, gbm, atomic);
        DrmBackendController controller(session, presenter);

        QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), {}));
        QCOMPARE(controller.state(), DrmOwnershipState::Failed);
        QCOMPARE(session.releaseCount, 1);
    }

    // 3. Ausência de CRTC compatível -> fail-closed
    {
        MockSessionProvider session;
        ConfigurableMockKms kms;
        kms.hasCompatibleCrtc = false;
        TrackingMockGbm gbm;
        RealisticMockAtomic atomic;
        AtomicKmsPresenter presenter(kms, gbm, atomic);
        DrmBackendController controller(session, presenter);

        QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), {}));
        QCOMPARE(controller.state(), DrmOwnershipState::Failed);
        QCOMPARE(session.releaseCount, 1);
    }

    // 4. Ausência de plano primário -> fail-closed
    {
        MockSessionProvider session;
        ConfigurableMockKms kms;
        kms.hasPrimaryPlane = false;
        TrackingMockGbm gbm;
        RealisticMockAtomic atomic;
        AtomicKmsPresenter presenter(kms, gbm, atomic);
        DrmBackendController controller(session, presenter);

        QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), {}));
        QCOMPARE(controller.state(), DrmOwnershipState::Failed);
        QCOMPARE(session.releaseCount, 1);
    }
}

void tst_PageFlipAndUnifiedStartup::unifiedVulkanPoolHasNoConcurrentGbmPoolAndCleanShutdownOrder() {
    MockSessionProvider session;
    ConfigurableMockKms kms;
    TrackingMockGbm gbm;
    RealisticMockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    presenter.setUseExternalVulkanPool(true);
    DrmBackendController controller(session, presenter);

    QVERIFY(controller.initialize(QStringLiteral("/dev/dri/card1"), {}));
    QCOMPARE(gbm.allocateCalls, 0);
    QCOMPARE(presenter.bufferManager().bufferCount(), 0);

    const QSize poolSize(128, 128);
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(poolSize), qPrintable(renderer.lastError()));

    // Falha na negociação de modificadores não vaza slots
    QVERIFY(!renderer.createSharedPoolSlots(presenter.bufferManager(), poolSize, 2, {0xdeadbeefcafebabeULL}));
    QCOMPARE(presenter.bufferManager().bufferCount(), 0);

    // Criação do pool unificado Vulkan->DMA-BUF no mesmo GbmBufferManager do presenter
    QVERIFY(renderer.createSharedPoolSlots(presenter.bufferManager(), poolSize, 2));
    QCOMPARE(presenter.bufferManager().bufferCount(), 2);
    QCOMPARE(gbm.allocateCalls, 0);

    const int fd0 = presenter.bufferManager().buffers().at(0).dmabufFd;
    const int fd1 = presenter.bufferManager().buffers().at(1).dmabufFd;
    QVERIFY(fd0 >= 0 && fd1 >= 0);

    const QByteArray qml = "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#22aa88' }\n";
    QVERIFY(renderer.loadQmlSource(qml));

    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    dispatcher.attachFd(controller.activeFd());
    IndependentFrameLoop loop(renderer, presenter, wayland, dispatcher);

    // 2 frames sequenciais via contexto page_flip_handler2
    for (int f = 0; f < 2; ++f) {
        loop.requestFrame();
        QTRY_VERIFY(loop.isFlipInFlight());
        void *ctx = atomic.commits.last().userData;
        QVERIFY(ctx != nullptr);
        dispatcher.dispatchKernelPageFlipForTest(static_cast<uint64_t>(f + 1), 55, ctx);
    }
    QCOMPARE(sink.sent, 2);

    // Ordem exata de destruição idêntica à do main.cpp
    loop.shutdown();
    presenter.releaseKmsFramebuffers();
    renderer.destroySharedPoolSlots(presenter.bufferManager());
    renderer.shutdown();
    controller.shutdown();

    QCOMPARE(atomic.removedFbs.size(), 2);
    QCOMPARE(::fcntl(fd0, F_GETFD), -1);
    QCOMPARE(::fcntl(fd1, F_GETFD), -1);
    QCOMPARE(session.releaseCount, 1);
}

QTEST_MAIN(tst_PageFlipAndUnifiedStartup)
#include "tst_PageFlipAndUnifiedStartup.moc"
