#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSeat>
#include <fcntl.h>

#include "compositor/OutputManagerService.hpp"
#include "compositor/SystaliaCompositor.hpp"
#include "system/InputDeviceManager.hpp"
#include "system/WorkspaceManagerService.hpp"

class tst_InputHardwareBackend final : public QObject {
    Q_OBJECT

private slots:
    void platformEnvironmentDetectionForHeadlessNestedAndBareMetal();
    void kmsNativeModeSelectionAndEglfsJsonGeneration();
    void libinputPipelineToWaylandSeatAndGestureRouting();
    void pointerBoundaryClampingAgainstKmsGeometry();
};

void tst_InputHardwareBackend::platformEnvironmentDetectionForHeadlessNestedAndBareMetal() {
    // 1. Sessão Headless (offscreen / minimal para CTest)
    const auto headlessCfg = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArrayLiteral("wayland-0"),
        QByteArrayLiteral(":0"),
        QByteArrayLiteral("offscreen"));
    QCOMPARE(headlessCfg.mode, InputDeviceManager::SessionHeadless);
    QCOMPARE(headlessCfg.qtQpaPlatform, QByteArrayLiteral("offscreen"));
    QCOMPARE(headlessCfg.qsgRhiBackend, QByteArrayLiteral("vulkan"));
    QVERIFY(!headlessCfg.atomicDrmEnabled);

    // 2. Sessão Aninhada Wayland (WAYLAND_DISPLAY definido)
    const auto nestedWaylandCfg = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArrayLiteral("wayland-0"),
        QByteArray(),
        QByteArray());
    QCOMPARE(nestedWaylandCfg.mode, InputDeviceManager::SessionNested);
    QCOMPARE(nestedWaylandCfg.qtQpaPlatform, QByteArrayLiteral("wayland;xcb"));
    QCOMPARE(nestedWaylandCfg.qsgRhiBackend, QByteArrayLiteral("vulkan"));
    QVERIFY(!nestedWaylandCfg.atomicDrmEnabled);

    // 3. Sessão Aninhada X11 (DISPLAY definido)
    const auto nestedX11Cfg = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArray(),
        QByteArrayLiteral(":1"),
        QByteArray());
    QCOMPARE(nestedX11Cfg.mode, InputDeviceManager::SessionNested);
    QCOMPARE(nestedX11Cfg.qtQpaPlatform, QByteArrayLiteral("wayland;xcb"));
    QCOMPARE(nestedX11Cfg.qsgRhiBackend, QByteArrayLiteral("vulkan"));

    // 4. Sessão Bare-Metal em TTY puro (sem WAYLAND_DISPLAY e sem DISPLAY)
    const auto bareMetalCfg = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArray(),
        QByteArray(),
        QByteArray(),
        QStringLiteral("config/eglfs_kms.json"));
    QCOMPARE(bareMetalCfg.mode, InputDeviceManager::SessionBareMetalKms);
    QCOMPARE(bareMetalCfg.qtQpaPlatform, QByteArrayLiteral("eglfs"));
    QCOMPARE(bareMetalCfg.qtQpaEglfsIntegration, QByteArrayLiteral("eglfs_kms"));
    QCOMPARE(bareMetalCfg.qsgRhiBackend, QByteArrayLiteral("vulkan"));
    QCOMPARE(bareMetalCfg.eglfsKmsConfigPath, QByteArrayLiteral("config/eglfs_kms.json"));
    QVERIFY(bareMetalCfg.atomicDrmEnabled);
}

void tst_InputHardwareBackend::kmsNativeModeSelectionAndEglfsJsonGeneration() {
    QList<InputDeviceManager::KmsConnectorMode> modes = {
        {QStringLiteral("HDMI-A-1"), QSize(1920, 1080), 60, true, false},
        {QStringLiteral("DP-1"), QSize(1920, 1080), 144, true, true},
        {QStringLiteral("DP-2"), QSize(3840, 2160), 60, true, true},
        {QStringLiteral("DP-3"), QSize(7680, 4320), 60, false, false} // Desconectado
    };

    // Deve selecionar o conector conectado de maior resolução e taxa de atualização (DP-2 4K@60Hz)
    const auto best = InputDeviceManager::selectBestKmsConnectorMode(modes);
    QCOMPARE(best.connectorName, QStringLiteral("DP-2"));
    QCOMPARE(best.resolution, QSize(3840, 2160));
    QCOMPARE(best.refreshRateHz, 60);

    // Em empate de resolução (1080p), deve selecionar a maior taxa de atualização (144Hz vs 60Hz)
    QList<InputDeviceManager::KmsConnectorMode> fhdModes = {
        {QStringLiteral("HDMI-A-1"), QSize(1920, 1080), 60, true, true},
        {QStringLiteral("DP-1"), QSize(1920, 1080), 144, true, false}
    };
    const auto bestFhd = InputDeviceManager::selectBestKmsConnectorMode(fhdModes);
    QCOMPARE(bestFhd.connectorName, QStringLiteral("DP-1"));
    QCOMPARE(bestFhd.refreshRateHz, 144);

    InputDeviceManager inputMgr;
    const QByteArray jsonBytes = inputMgr.generateEglfsKmsConfigJson(
        QStringLiteral("/dev/dri/card0"), bestFhd);
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    QVERIFY(doc.isObject());
    const QJsonObject root = doc.object();
    QCOMPARE(root.value(QStringLiteral("device")).toString(), QStringLiteral("/dev/dri/card0"));
    QCOMPARE(root.value(QStringLiteral("hwcursor")).toBool(), true);
}

void tst_InputHardwareBackend::libinputPipelineToWaylandSeatAndGestureRouting() {
    QTemporaryDir runtimeDir;
    QVERIFY(runtimeDir.isValid());
    qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8());

    WorkspaceManagerService wsManager;
    wsManager.setTotalWorkspaces(4);
    wsManager.switchToWorkspace(0);

    OutputManagerService outManager;
    outManager.clearScreens();
    outManager.addVirtualScreen(QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0, 144000);

    InputDeviceManager inputMgr;
    inputMgr.setKmsDisplayBounds(QRect(0, 0, 1920, 1080));
    inputMgr.setPointerPosition(QPointF(500.0, 400.0));
    inputMgr.setPointerAcceleration(1.5);
    inputMgr.resetTelemetryCounters();

    SystaliaCompositor compositor;
    compositor.setSocketName(QByteArrayLiteral("wayland-sge-input-test"));
    compositor.create();
    QVERIFY(compositor.isCreated());
    QVERIFY(compositor.defaultSeat() != nullptr);
    QCOMPARE(inputMgr.targetSeat(), compositor.defaultSeat());

    // Verifica callbacks da interface restrita libinput_interface
    const libinput_interface &iface = InputDeviceManager::restrictedInterface();
    QVERIFY(iface.open_restricted != nullptr);
    QVERIFY(iface.close_restricted != nullptr);
    const int devNullFd = iface.open_restricted("/dev/null", O_RDONLY, nullptr);
    QVERIFY(devNullFd >= 0);
    iface.close_restricted(devNullFd, nullptr);

    QSignalSpy keySpy(&inputMgr, &InputDeviceManager::keyEventDispatched);
    QSignalSpy motionSpy(&inputMgr, &InputDeviceManager::pointerMotionDispatched);
    QSignalSpy buttonSpy(&inputMgr, &InputDeviceManager::pointerButtonDispatched);
    QSignalSpy swipeSpy(&inputMgr, &InputDeviceManager::workspaceSwipeTriggered);

    // 1. Evento de teclado LIBINPUT_EVENT_KEYBOARD_KEY (KEY_A = 30 -> XKB = 38, Qt::Key_A)
    InputDeviceManager::SimulatedLibinputEvent keyPress{};
    keyPress.type = InputDeviceManager::LibinputEventKeyboardKey;
    keyPress.linuxScanCode = 30u;
    keyPress.pressed = true;
    QVERIFY(inputMgr.dispatchSimulatedEvent(keyPress));

    InputDeviceManager::SimulatedLibinputEvent keyRelease = keyPress;
    keyRelease.pressed = false;
    QVERIFY(inputMgr.dispatchSimulatedEvent(keyRelease));

    QCOMPARE(keySpy.count(), 2);
    QCOMPARE(keySpy.first().at(0).toUInt(), 30u);
    QCOMPARE(keySpy.first().at(1).toUInt(), 38u);
    QCOMPARE(keySpy.first().at(2).toInt(), int(Qt::Key_A));
    QCOMPARE(keySpy.first().at(3).toBool(), true);
    QCOMPARE(inputMgr.dispatchedKeyEvents(), 2);

    // 2. Evento de movimento relativo LIBINPUT_EVENT_POINTER_MOTION com aceleração 1.5x
    InputDeviceManager::SimulatedLibinputEvent ptrMotion{};
    ptrMotion.type = InputDeviceManager::LibinputEventPointerMotion;
    ptrMotion.dx = 40.0;
    ptrMotion.dy = -20.0;
    QVERIFY(inputMgr.dispatchSimulatedEvent(ptrMotion));

    QCOMPARE(motionSpy.count(), 1);
    // Posição esperada: (500 + 40*1.5, 400 - 20*1.5) = (560, 370)
    QCOMPARE(inputMgr.pointerPosition(), QPointF(560.0, 370.0));

    // 3. Evento de botão LIBINPUT_EVENT_POINTER_BUTTON (BTN_LEFT = 0x110)
    InputDeviceManager::SimulatedLibinputEvent btnPress{};
    btnPress.type = InputDeviceManager::LibinputEventPointerButton;
    btnPress.linuxButtonCode = 0x110u;
    btnPress.pressed = true;
    QVERIFY(inputMgr.dispatchSimulatedEvent(btnPress));

    InputDeviceManager::SimulatedLibinputEvent btnRelease = btnPress;
    btnRelease.pressed = false;
    QVERIFY(inputMgr.dispatchSimulatedEvent(btnRelease));

    QCOMPARE(buttonSpy.count(), 2);
    QCOMPARE(qvariant_cast<Qt::MouseButton>(buttonSpy.first().at(0)), Qt::LeftButton);
    QCOMPARE(buttonSpy.first().at(1).toBool(), true);

    // 4. Evento de gesto LIBINPUT_EVENT_GESTURE_SWIPE (3 dedos para a esquerda -> avança para Workspace 1)
    InputDeviceManager::SimulatedLibinputEvent swipeBegin{};
    swipeBegin.type = InputDeviceManager::LibinputEventGestureSwipeBegin;
    swipeBegin.fingerCount = 3;
    QVERIFY(inputMgr.dispatchSimulatedEvent(swipeBegin));

    InputDeviceManager::SimulatedLibinputEvent swipeUpdate{};
    swipeUpdate.type = InputDeviceManager::LibinputEventGestureSwipeUpdate;
    swipeUpdate.fingerCount = 3;
    swipeUpdate.dx = -140.0;
    swipeUpdate.dy = 4.0;
    QVERIFY(inputMgr.dispatchSimulatedEvent(swipeUpdate));

    InputDeviceManager::SimulatedLibinputEvent swipeEnd{};
    swipeEnd.type = InputDeviceManager::LibinputEventGestureSwipeEnd;
    swipeEnd.fingerCount = 3;
    QVERIFY(inputMgr.dispatchSimulatedEvent(swipeEnd));

    QCOMPARE(swipeSpy.count(), 1);
    QCOMPARE(swipeSpy.first().at(0).toInt(), 1);
    QCOMPARE(wsManager.currentWorkspace(), 1);
    QCOMPARE(inputMgr.dispatchedSwipeGestures(), 1);
}

void tst_InputHardwareBackend::pointerBoundaryClampingAgainstKmsGeometry() {
    InputDeviceManager inputMgr;
    const QRect kmsGeometry(0, 0, 2560, 1440);
    inputMgr.setKmsDisplayBounds(kmsGeometry);
    inputMgr.setPointerAcceleration(1.0);
    inputMgr.setPointerPosition(QPointF(1280.0, 720.0));

    // Movimento extremo para fora da borda superior-esquerda (-5000, -5000)
    const QPointF topLeftClamped = inputMgr.processLibinputPointerMotion(-5000.0, -5000.0);
    QCOMPARE(topLeftClamped, QPointF(0.0, 0.0));

    // Movimento extremo para fora da borda inferior-direita (+10000, +10000)
    const QPointF bottomRightClamped = inputMgr.processLibinputPointerMotion(10000.0, 10000.0);
    QCOMPARE(bottomRightClamped, QPointF(2559.0, 1439.0));

    // Movimento absoluto normalizado com valores além de [0.0, 1.0]
    const QPointF absMin = inputMgr.processLibinputPointerMotionAbsolute(-0.5, -2.0);
    QCOMPARE(absMin, QPointF(0.0, 0.0));

    const QPointF absMax = inputMgr.processLibinputPointerMotionAbsolute(1.8, 3.2);
    QCOMPARE(absMax, QPointF(2559.0, 1439.0));

    const QPointF absMid = inputMgr.processLibinputPointerMotionAbsolute(0.5, 0.5);
    QVERIFY(absMid.x() >= 1279.0 && absMid.x() <= 1280.0);
    QVERIFY(absMid.y() >= 719.0 && absMid.y() <= 720.0);
}

QTEST_MAIN(tst_InputHardwareBackend)
#include "tst_InputHardwareBackend.moc"

