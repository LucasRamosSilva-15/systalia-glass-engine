#include <QTest>
#include <QFile>
#include <QJSEngine>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>

#include "SystaliaCompositor.hpp"
#include "SystaliaGlassItem.hpp"
#include "SystemTelemetryService.hpp"
#include "ProcessLauncherService.hpp"
#include "AppRegistryService.hpp"
#include "NotificationServer.hpp"
#include "SystemControlService.hpp"
#include "WorkspaceManagerService.hpp"
#include "LiquidTilingService.hpp"
#include "OutputManagerService.hpp"
#include "InputDeviceManager.hpp"
#include "LogindSessionService.hpp"

class TestShellStateRegression final : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_runtime;
    NotificationServer m_notifications{false};
    SystemControlService m_control;
    WorkspaceManagerService m_workspaces;
    LiquidTilingService m_tiling;
    OutputManagerService m_outputs;
    LogindSessionService m_session;
    InputDeviceManager m_input;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QObject *m_root = nullptr;

    QVariant evaluate(const QString &code) {
        QQmlExpression expression(QQmlEngine::contextForObject(m_root), m_root, code);
        QVariant value = expression.evaluate();
        if (expression.hasError()) {
            QTest::qFail(qPrintable(expression.error().toString()), __FILE__, __LINE__);
            return {};
        }
        return value.metaType().id() == qMetaTypeId<QJSValue>()
            ? value.value<QJSValue>().toVariant() : value;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_runtime.isValid());
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
        qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
        qmlRegisterType<SystaliaCompositor>("Systalia.Compositor", 1, 0, "SystaliaCompositor");
        qmlRegisterSingletonType<SystemTelemetryService>("Systalia.System", 1, 0, "SystemTelemetry",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemTelemetryService; });
        qmlRegisterSingletonType<ProcessLauncherService>("Systalia.System", 1, 0, "ProcessLauncher",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new ProcessLauncherService; });
        qmlRegisterSingletonType<AppRegistryService>("Systalia.System", 1, 0, "AppRegistry",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new AppRegistryService; });
        qmlRegisterSingletonInstance("Systalia.System", 1, 0, "Notifications", &m_notifications);
        qmlRegisterSingletonInstance("Systalia.System", 1, 0, "SystemControl", &m_control);
        qmlRegisterSingletonInstance("Systalia.System", 1, 0, "WorkspaceManager", &m_workspaces);
        qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LiquidTiling", &m_tiling);
        qmlRegisterSingletonInstance("Systalia.System", 1, 0, "OutputManager", &m_outputs);
        qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LogindSession", &m_session);
        qmlRegisterSingletonInstance("Systalia.System", 1, 0, "InputDeviceManager", &m_input);
        m_engine = std::make_unique<QQmlApplicationEngine>();
        m_engine->addImageProvider(QStringLiteral("desktopIcons"), new DesktopIconProvider);
        m_engine->load(QUrl("qrc:/qt/qml/Systalia/Engine/src/qml/main.qml"));
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_root = m_engine->rootObjects().first();
        // These tests need the full shell but do not depend on an X server.
        evaluate("waylandServer.stopXWayland()");
    }

    void init() {
        m_workspaces.switchToWorkspace(0);
        m_workspaces.setTransitioning(false);
        evaluate("WorkspaceManager.setWindowWorkspace(glassChassis, 0);"
                 "WorkspaceManager.setWindowWorkspace(materialWindow, 0);"
                 "raiseFileWindow(); raiseMaterialWindow(); flowMenu.close()");
    }

    void minimizedMaterialStaysHiddenAfterWorkspaceRoundtrip() {
        evaluate("materialWindow.windowOpen = false; materialWindow.visible = false");
        m_workspaces.switchToWorkspace(1);
        m_workspaces.setTransitioning(false);
        m_workspaces.switchToWorkspace(0);
        m_workspaces.setTransitioning(false);
        QVERIFY(!evaluate("materialWindow.visible").toBool());
        evaluate("raiseMaterialWindow()");
        QVERIFY(evaluate("materialWindow.visible").toBool());
    }

    void movedShellWindowsAreRecoveredInTheirWorkspace() {
        evaluate("WorkspaceManager.setWindowWorkspace(glassChassis, 2); raiseFileWindow()");
        m_workspaces.setTransitioning(false);
        QCOMPARE(m_workspaces.currentWorkspace(), 2);
        QVERIFY(evaluate("glassChassis.visible && glassChassis.parent.objectName === 'workspacePage_2'").toBool());
        evaluate("WorkspaceManager.setWindowWorkspace(materialWindow, 3); raiseMaterialWindow()");
        m_workspaces.setTransitioning(false);
        QCOMPARE(m_workspaces.currentWorkspace(), 3);
        QVERIFY(evaluate("materialWindow.visible").toBool());
    }

    void movingAToplevelAlsoMovesItsVisualRegistration() {
        QObject toplevel;
        QQuickItem container;
        container.setProperty("toplevel", QVariant::fromValue<QObject *>(&toplevel));
        m_workspaces.setWindowWorkspace(&container, 0);
        m_workspaces.setObjectWorkspace(&toplevel, 2);
        QCOMPARE(m_workspaces.windowWorkspace(&container), 2);
        QCOMPARE(m_workspaces.objectWorkspace(&toplevel), 2);
        m_workspaces.switchToWorkspace(2);
        m_workspaces.setTransitioning(false);
        QVERIFY(container.isVisible());
    }

    void hiddenSnappedWindowsDoNotLeaveAnActiveSplitter() {
        evaluate("finishShellWindowDrag(glassChassis, Qt.point(0, height / 2));"
                 "finishShellWindowDrag(materialWindow, Qt.point(width, height / 2))");
        QVERIFY(m_tiling.hasActiveSplitInWorkspace(0));
        evaluate("minimizeMaterialWindow()");
        QVERIFY(!m_tiling.hasActiveSplitInWorkspace(0));
        evaluate("raiseMaterialWindow()");
        QVERIFY(m_tiling.hasActiveSplitInWorkspace(0));
        evaluate("WorkspaceManager.setWindowWorkspace(materialWindow, 1)");
        QVERIFY(!m_tiling.hasActiveSplitInWorkspace(0));
        evaluate("unsnapShellWindow(glassChassis); unsnapShellWindow(materialWindow)");
    }

    void doNotDisturbSuppressesNotificationPresentation() {
        m_control.setDoNotDisturb(true);
        const uint id = m_notifications.Notify("test", 0, "", "message", "body", {}, {}, 0);
        QCOMPARE(m_notifications.notificationCount(), 1);
        QVERIFY(!evaluate("notificationHost.visible").toBool());
        m_notifications.CloseNotification(id);
        m_control.setDoNotDisturb(false);
    }

    void longWindowTitlesStayClearOfActions() {
        const QString title(200, QChar('W'));
        for (int width : {320, 700}) {
            QQmlComponent component(m_engine.get(),
                QUrl("qrc:/qt/qml/Systalia/Engine/src/qml/shell/SystaliaWindowHeader.qml"));
            std::unique_ptr<QObject> header(component.createWithInitialProperties({
                {"width", width}, {"height", 36}, {"title", title}, {"badgeText", "VALO"}}));
            QVERIFY2(header, qPrintable(component.errorString()));
            auto *item = qobject_cast<QQuickItem *>(header.get());
            QVERIFY(item);
            QQuickItem *label = nullptr;
            QQuickItem *icon = nullptr;
            for (auto *child : item->findChildren<QQuickItem *>()) {
                if (child->property("text").toString() == title)
                    label = child;
                if (child->property("text").toString() == QString::fromUtf8("▰"))
                    icon = child;
            }
            QVERIFY(label && icon);
            auto *minimize = item->findChild<QQuickItem *>("minimizeToplevelButton");
            QVERIFY(minimize);
            QVERIFY(label->width() > 0);
            QVERIFY(label->mapToItem(item, QPointF(label->width(), 0)).x()
                    <= minimize->mapToItem(item, QPointF()).x());
            QVERIFY(icon->y() >= 0 && icon->y() + icon->height() <= item->height());
        }
    }

    void hardwareBrightnessDoesNotEnableSoftwareDimming() {
        QTemporaryDir binaries;
        QFile executable(binaries.filePath("brightnessctl"));
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write("#!/bin/sh\necho 'panel,backlight,50,50%,100'\n");
        executable.close();
        QVERIFY(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const auto previousPath = qgetenv("PATH");
        qputenv("PATH", binaries.path().toUtf8());
        SystemControlService control;
        qputenv("PATH", previousPath);
        QTRY_COMPARE(control.brightness(), 50);
        QVERIFY(!control.softwareBrightness());
    }

    void pulseAudioMuteParserAcceptsLabeledResponses() {
        QVERIFY(SystemControlService::parsePactlMuted("Mute: yes\n"));
        QVERIFY(!SystemControlService::parsePactlMuted("Mute: no\n"));
    }

    void powerActionsUseSystemctlAndReportFailure() {
        QTemporaryDir binaries;
        QVERIFY(binaries.isValid());
        QFile executable(binaries.filePath("systemctl"));
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write("#!/bin/sh\necho \"denied:$1\" >&2\nexit 1\n");
        executable.close();
        QVERIFY(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const auto previousPath = qgetenv("PATH");
        qputenv("PATH", binaries.path().toUtf8());
        SystemControlService control;
        qputenv("PATH", previousPath);
        QSignalSpy result(&control, &SystemControlService::powerActionFinished);
        control.powerOff();
        QTRY_COMPARE(result.count(), 1);
        QCOMPARE(result.first().at(0).toString(), QString("poweroff"));
        QVERIFY(!result.first().at(1).toBool());
        QVERIFY(result.first().at(2).toString().contains("denied:poweroff"));
    }

    void desktopTerminalRequirementReachesTheModel() {
        QTemporaryDir entries;
        QFile file(entries.filePath("editor.desktop"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Desktop Entry]\nType=Application\nName=Editor\nExec=vi\nTerminal=true\n");
        file.close();
        AppRegistryService registry({entries.path()});
        QCOMPARE(registry.rowCount(), 1);
        QVERIFY(registry.data(registry.index(0, 0), AppRegistryService::TerminalRole).toBool());
    }

    void clientBackdropAndAnimationFollowTheCanvas() {
        QProcess client;
        client.setProcessEnvironment(ProcessLauncherService::waylandProcessEnvironment());
        client.start(QCoreApplication::applicationDirPath() + "/tst_xdg_resize", {"--resize-client"});
        QVERIFY(client.waitForStarted());
        QTRY_COMPARE(evaluate("trackedClientContainers.length").toInt(), 1);
        evaluate("trackedClientContainers[0].syncOutputRouting()");
        QVERIFY(evaluate("trackedClientContainers[0].screenResolution.x === width * devicePixelRatio").toBool());
        const double timeBefore = evaluate("trackedClientContainers[0].animTime").toDouble();
        QTRY_VERIFY(evaluate("trackedClientContainers[0].animTime").toDouble() > timeBefore);
        const int previousWidth = m_root->property("width").toInt();
        m_root->setProperty("width", 1100);
        QTRY_VERIFY(evaluate("trackedClientContainers[0].screenResolution.x === width * devicePixelRatio").toBool());
        m_root->setProperty("width", previousWidth);
        client.terminate();
        QVERIFY(client.waitForFinished());
        QTRY_COMPARE(evaluate("trackedClientContainers.length").toInt(), 0);
    }

    void hostScreenSyncPreservesSurvivingOutput() {
        auto *compositor = qobject_cast<SystaliaCompositor *>(evaluate("waylandServer").value<QObject *>());
        QVERIFY(compositor);
        const auto original = compositor->outputs();
        QVERIFY(!original.isEmpty());
        m_outputs.syncWithHostScreens();
        QCOMPARE(compositor->outputs(), original);
    }

    void controlCenterSlidersInteractReliably() {
        evaluate("controlCenter.open()");
        QVERIFY(evaluate("controlCenter.opened").toBool());
        auto *volumeSlider = m_root->findChild<QQuickItem *>("volumeSlider");
        auto *brightnessSlider = m_root->findChild<QQuickItem *>("brightnessSlider");
        QVERIFY(volumeSlider);
        QVERIFY(brightnessSlider);

        m_control.setVolume(50);
        QCOMPARE(m_control.volume(), 50);
        QCOMPARE(volumeSlider->property("value").toInt(), 50);

        m_control.setBrightness(75);
        QCOMPARE(m_control.brightness(), 75);
        QCOMPARE(brightnessSlider->property("value").toInt(), 75);

        evaluate("controlCenter.close()");
        QVERIFY(!evaluate("controlCenter.opened").toBool());
    }

    void topBarVisualElementsAreUniqueAndUnconflicted() {
        QVERIFY(evaluate("topBar.visible").toBool());
        QCOMPARE(evaluate("topBar.actionButtonCount").toInt(), 6);
        QVERIFY(evaluate("topBar.isWorkspaceTrackVisible").toBool());
    }

    void captureValoDesktopShellVisuals() {
        auto *window = qobject_cast<QQuickWindow *>(m_root);
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));

        evaluate("raiseFileWindow(); raiseMaterialWindow(); controlCenter.close()");
        for (int i = 0; i < 6; ++i) {
            QCoreApplication::processEvents();
            QTest::qWait(25);
        }

        const QString artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
        QImage shellShot = window->grabWindow();
        QVERIFY(!shellShot.isNull());
        shellShot.save(artifactDir + QStringLiteral("/valo_shell_desktop_complete.png"));

        evaluate("controlCenter.open()");
        for (int i = 0; i < 6; ++i) {
            QCoreApplication::processEvents();
            QTest::qWait(25);
        }
        QImage controlShot = window->grabWindow();
        QVERIFY(!controlShot.isNull());
        controlShot.save(artifactDir + QStringLiteral("/valo_control_center_live.png"));

        evaluate("controlCenter.close()");
    }
};

QTEST_MAIN(TestShellStateRegression)
#include "tst_ShellStateRegression.moc"
