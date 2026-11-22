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
#include "ValoGlassItemV2.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"
#include "WorkspaceManagerService.hpp"

#include <QImage>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QWaylandQuickOutput>

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
        snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 1280, 720});
        snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {0x34325258}});
        KmsConnectorInfo dp;
        dp.connectorId = 20;
        dp.name = QStringLiteral("DP-1");
        dp.connected = true;
        dp.possibleCrtcs = 0x1;
        dp.modes.append(DrmOutputMode{QStringLiteral("DP-1"), 1280, 720, 60, true});
        dp.preferredMode = dp.modes.first();
        snap.connectors.append(dp);
        return snap;
    }
};

class MockGbm final : public IGbmBackend {
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

class MockAtomic final : public IAtomicKmsBackend {
public:
    int commitCalls = 0;
    uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override { return static_cast<uint32_t>(b.id); }
    void removeFramebuffer(int, uint32_t) override {}
    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &) override {
        ++commitCalls;
        return AtomicCommitResult{true, -1};
    }
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

class tst_ShellRootIntegration final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void traditionalMainQmlLoadsSingleWindowAndCompositor();
    void experimentalShellRootRendersThroughControlledVulkanLoop();
    void handlesQmlLoadErrorsCleanly();

private:
    QTemporaryDir m_runtimeDir;
    NotificationServer m_notif;
    SystemControlService m_control;
    WorkspaceManagerService m_workspaces;
    LiquidTilingService m_tiling;
    OutputManagerService m_outputs;
    LogindSessionService m_session;
    InputDeviceManager m_input;
    std::unique_ptr<QQmlApplicationEngine> m_sharedEngine;
};

void tst_ShellRootIntegration::initTestCase() {
    QVERIFY(m_runtimeDir.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtimeDir.path().toUtf8());
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    registerShellTypes(&m_notif, &m_control, &m_workspaces, &m_tiling, &m_outputs, &m_session, &m_input);
    m_sharedEngine = std::make_unique<QQmlApplicationEngine>();
    m_sharedEngine->addImageProvider(QStringLiteral("desktopIcons"), new DesktopIconProvider());
}

void tst_ShellRootIntegration::traditionalMainQmlLoadsSingleWindowAndCompositor() {
    if (qEnvironmentVariableIsSet("SGE_RUN_TRADITIONAL_MAIN_CHECK")) {
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("desktopIcons"), new DesktopIconProvider());
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());

        QObject *root = engine.rootObjects().first();
        auto *win = qobject_cast<QQuickWindow *>(root);
        QVERIFY(win != nullptr);
        QCOMPARE(root->findChildren<QQuickWindow *>().size(), 0);
        QCOMPARE(root->findChildren<SystaliaCompositor *>().size(), 1);
        QCOMPARE(root->findChildren<QWaylandQuickOutput *>().size(), 1);

        auto *comp = root->findChild<SystaliaCompositor *>();
        QVERIFY(comp != nullptr);
        comp->stopXWayland();

        auto *out = root->findChild<QWaylandQuickOutput *>();
        QVERIFY(out != nullptr);
        QCOMPARE(out->window(), win);
        QVERIFY(!root->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>().isEmpty());
        return;
    }

    QTemporaryDir childRuntime;
    QVERIFY(childRuntime.isValid());
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_RUNTIME_DIR"), childRuntime.path());
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("SGE_RUN_TRADITIONAL_MAIN_CHECK"), QStringLiteral("1"));
    proc.setProcessEnvironment(env);
    proc.start(QCoreApplication::applicationFilePath(),
               {QStringLiteral("traditionalMainQmlLoadsSingleWindowAndCompositor")});
    QVERIFY(proc.waitForFinished(15000));
    QCOMPARE(proc.exitStatus(), QProcess::NormalExit);
    QCOMPARE(proc.exitCode(), 0);
}

void tst_ShellRootIntegration::experimentalShellRootRendersThroughControlledVulkanLoop() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(1280, 720)), qPrintable(renderer.lastError()));
    QVERIFY2(renderer.loadQmlUrl(QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/ShellRoot.qml")),
                                 m_sharedEngine.get()),
             qPrintable(renderer.lastError()));

    QQuickItem *rootItem = renderer.rootItem();
    QVERIFY(rootItem != nullptr);
    // Garante ausência de segunda QQuickWindow, segundo compositor ou segundo output
    QCOMPARE(rootItem->findChildren<QQuickWindow *>().size(), 0);
    QCOMPARE(rootItem->findChildren<SystaliaCompositor *>().size(), 1);
    QCOMPARE(rootItem->findChildren<QWaylandQuickOutput *>().size(), 1);
    QVERIFY(!rootItem->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>().isEmpty());

    auto *comp = rootItem->findChild<SystaliaCompositor *>();
    QVERIFY(comp != nullptr);
    comp->stopXWayland();

    auto *output = rootItem->findChild<QWaylandQuickOutput *>();
    QVERIFY(output != nullptr);
    QCOMPARE(output->window(), renderer.window());

    MockKms kms;
    MockGbm gbm;
    MockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 1280, 720, 60, true}));

    QWaylandOutputFrameSink sink(output);
    QVERIFY(!output->automaticFrameCallback());
    WaylandFrameCoordinator waylandCoord(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(renderer, presenter, waylandCoord, dispatcher);

    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(atomic.commitCalls, 1);

    // Readback real da cena ShellRoot.qml renderizada em GPU Vulkan (RADV)
    const QImage frame = renderer.readbackImage();
    QVERIFY(!frame.isNull());
    QCOMPARE(frame.size(), QSize(1280, 720));

    // Verifica que o framebuffer contém pixels não nulos da shell VALO V2
    bool hasNonBlack = false;
    for (int y = 0; y < frame.height() && !hasNonBlack; y += 24) {
        for (int x = 0; x < frame.width(); x += 24) {
            const QColor c = frame.pixelColor(x, y);
            if (c.red() > 0 || c.green() > 0 || c.blue() > 0) {
                hasNonBlack = true;
                break;
            }
        }
    }
    QVERIFY(hasNonBlack);

    dispatcher.dispatchSimulatedPageFlip(1, 0);
    QCOMPARE(loop.presentedCount(), 1ULL);

    // Resize + suspensão + retomada + destruição ordenada
    loop.suspend();
    renderer.setSuspended(true);
    QVERIFY(renderer.resize(QSize(1662, 975)));
    renderer.setSuspended(false);
    loop.resume(10);
    QTRY_VERIFY(loop.isFlipInFlight());

    loop.shutdown();
    presenter.shutdown();
    renderer.shutdown();
    QCOMPARE(gbm.activeBuffers, 0);
}

void tst_ShellRootIntegration::handlesQmlLoadErrorsCleanly() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));
    QVERIFY(!renderer.loadQmlUrl(QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/DoesNotExist.qml"))));
    QVERIFY(!renderer.lastError().isEmpty());
    QVERIFY(!renderer.loadQmlSource(QByteArrayLiteral("import QtQuick\nQtObject { }\n")));
    renderer.shutdown();
}

QTEST_MAIN(tst_ShellRootIntegration)
#include "tst_ShellRootIntegration.moc"
