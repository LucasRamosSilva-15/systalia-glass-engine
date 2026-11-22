#include <QtTest>

#include "AppRegistryService.hpp"
#include "AtomicKmsPresenter.hpp"
#include "DrmEventDispatcher.hpp"
#include "IndependentFrameLoop.hpp"
#include "InputDeviceManager.hpp"
#include "LiquidTilingService.hpp"
#include "LogindSessionService.hpp"
#include "NotificationServer.hpp"
#include "OutputManagerService.hpp"
#include "ProcessLauncherService.hpp"
#include "SystaliaCompositor.hpp"
#include "SystemControlService.hpp"
#include "SystemTelemetryService.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"
#include "WorkspaceManagerService.hpp"

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWaylandQuickOutput>
#include <drm_fourcc.h>

using namespace Systalia::Platform::Drm;

namespace {

class MockKms final : public IKmsResourceBackend {
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
        dp.name = QStringLiteral("HDMI-A-1");
        dp.connected = true;
        dp.possibleCrtcs = 0x1;
        dp.modes.append(DrmOutputMode{QStringLiteral("HDMI-A-1"), 128, 128, 60, true});
        dp.preferredMode = dp.modes.first();
        snap.connectors.append(dp);
        return snap;
    }
};

class MockGbm final : public IGbmBackend {
public:
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &, GbmExportedBuffer *) override { return true; }
    void destroyBuffer(GbmExportedBuffer *) override {}
};

class MockAtomic final : public IAtomicKmsBackend {
public:
    int commitCalls = 0;
    uint32_t nextFb = 900;
    uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override {
        return b.stride > 0 ? nextFb++ : 0;
    }
    void removeFramebuffer(int, uint32_t) override {}
    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &) override {
        ++commitCalls;
        return AtomicCommitResult{true, -1};
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

void registerShellTypes(NotificationServer *notif,
                        SystemControlService *control,
                        WorkspaceManagerService *workspaces,
                        LiquidTilingService *tiling,
                        OutputManagerService *outputs,
                        LogindSessionService *session,
                        InputDeviceManager *input) {
    qmlRegisterType<SystaliaCompositor>("Systalia.Compositor", 1, 0, "SystaliaCompositor");
    qmlRegisterSingletonType<SystemTelemetryService>(
        "Systalia.System", 1, 0, "SystemTelemetry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemTelemetryService(); });
    qmlRegisterSingletonType<ProcessLauncherService>(
        "Systalia.System", 1, 0, "ProcessLauncher",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new ProcessLauncherService(); });
    qmlRegisterSingletonType<AppRegistryService>(
        "Systalia.System", 1, 0, "AppRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new AppRegistryService(); });
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "Notifications", notif);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "SystemControl", control);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "WorkspaceManager", workspaces);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LiquidTiling", tiling);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "OutputManager", outputs);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LogindSession", session);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "InputDeviceManager", input);
}

} // namespace

class tst_DeviceLostAndCompositorLifecycle final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void deviceLostBlocksSubmissionsAndShutsDownFailClosed();
    void waylandCompositorOutputTeardownClearsWindowSafely();

private:
    QTemporaryDir m_runtimeDir;
    NotificationServer m_notif;
    SystemControlService m_control;
    WorkspaceManagerService m_workspaces;
    LiquidTilingService m_tiling;
    OutputManagerService m_outputs;
    LogindSessionService m_session;
    InputDeviceManager m_input;
};

void tst_DeviceLostAndCompositorLifecycle::initTestCase() {
    QVERIFY(m_runtimeDir.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtimeDir.path().toUtf8());
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    registerShellTypes(&m_notif, &m_control, &m_workspaces, &m_tiling, &m_outputs, &m_session, &m_input);
}

void tst_DeviceLostAndCompositorLifecycle::deviceLostBlocksSubmissionsAndShutsDownFailClosed() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));

    MockKms kms;
    MockGbm gbm;
    MockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    presenter.setUseExternalVulkanPool(true);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("HDMI-A-1"), 128, 128, 60, true}));
    QVERIFY(renderer.createSharedPoolSlots(presenter.bufferManager(), QSize(128, 128), 2));

    const QByteArray qml = "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#113355' }\n";
    QVERIFY(renderer.loadQmlSource(qml));

    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    dispatcher.attachFd(10);
    IndependentFrameLoop loop(renderer, presenter, wayland, dispatcher);

    QObject::connect(&renderer, &VulkanQuickRenderer::deviceLost,
                     &loop, &IndependentFrameLoop::handleDeviceLost);
    QSignalSpy lostSpy(&renderer, &VulkanQuickRenderer::deviceLost);

    // Simula perda do dispositivo Vulkan
    renderer.simulateDeviceLostForTest(QStringLiteral("Simulated VK_ERROR_DEVICE_LOST"));
    QCOMPARE(lostSpy.count(), 1);
    QVERIFY(renderer.isDeviceLost());
    QVERIFY(loop.isDeviceLost());
    QVERIFY(loop.isSuspended());
    QVERIFY(presenter.isSuspended());
    QVERIFY(!dispatcher.isAttached());

    // Novas tentativas de renderização, commit ou resume são bloqueadas
    QVERIFY(!renderer.renderFrame());
    QVERIFY(!renderer.renderIntoBuffer(presenter.bufferManager().buffers().first()));
    loop.requestFrame();
    QVERIFY(!loop.processNextFrame());
    loop.resume(10);
    QVERIFY(loop.isDeviceLost());
    QCOMPARE(atomic.commitCalls, 0);

    // Teardown seguro sem travar em vkDeviceWaitIdle
    loop.shutdown();
    presenter.releaseKmsFramebuffers();
    renderer.destroySharedPoolSlots(presenter.bufferManager());
    renderer.shutdown();
    presenter.shutdown();
}

void tst_DeviceLostAndCompositorLifecycle::waylandCompositorOutputTeardownClearsWindowSafely() {
    QQmlEngine engine;
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(640, 360)), qPrintable(renderer.lastError()));
    QVERIFY2(renderer.loadQmlUrl(QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/ShellRoot.qml")),
                                 &engine),
             qPrintable(renderer.lastError()));

    QQuickItem *root = renderer.rootItem();
    QVERIFY(root != nullptr);
    auto *comp = root->findChild<SystaliaCompositor *>();
    QVERIFY(comp != nullptr);
    comp->stopXWayland();

    auto *output = root->findChild<QWaylandQuickOutput *>();
    QVERIFY(output != nullptr);
    QCOMPARE(output->window(), renderer.window());

    QVERIFY(renderer.renderFrame());

    // Shutdown limpa hostWindow e primaryWaylandOutput.window sem segfault em QWaylandOutput::initialize
    renderer.shutdown();
    QVERIFY(!renderer.isInitialized());
}

QTEST_MAIN(tst_DeviceLostAndCompositorLifecycle)
#include "tst_DeviceLostAndCompositorLifecycle.moc"
