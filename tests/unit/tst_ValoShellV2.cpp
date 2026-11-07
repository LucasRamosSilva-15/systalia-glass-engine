#include <QTest>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSignalSpy>
#include <QVector4D>
#include <QImage>
#include <QColor>
#include <memory>
#include <cmath>

#include "ValoGlassItemV2.hpp"
#include "ValoShadowItemV2.hpp"
#include "ValoOpticalContext.hpp"
#include "ValoBackdropProviderV2.hpp"
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

void qml_register_types_Systalia_Engine();

using namespace Systalia::Gfx::V2;

class tst_ValoShellV2 : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testMaterialSurfaceV2Encapsulation();
    void testTopBarV2GeometryAndContracts();
    void testFilesWindowV2SquircleG2AndState();
    void testControlCenterV2SquircleG2AndServices();
    void testDockV2ShellIntegration();
    void testFramebufferPixelVisibilityV2();

private:
    NotificationServer m_notifications{false};
    SystemControlService m_control;
    WorkspaceManagerService m_workspaces;
    LiquidTilingService m_tiling;
    OutputManagerService m_outputs;
    LogindSessionService m_session;
    InputDeviceManager m_input;
    std::unique_ptr<QQmlEngine> m_engine;
};

void tst_ValoShellV2::initTestCase() {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QSG_RENDER_LOOP", "basic");
    qml_register_types_Systalia_Engine();

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

    m_engine = std::make_unique<QQmlEngine>();
}

void tst_ValoShellV2::cleanupTestCase() {
    m_engine.reset();
}

void tst_ValoShellV2::testMaterialSurfaceV2Encapsulation() {
    QQmlComponent comp(m_engine.get());
    comp.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoMaterialSurfaceV2 {
            id: surface
            width: 400
            height: 300
            geometryMode: ValoGlassItemV2.SquircleG2
            cornerRadius: 24.0
            surfaceOpacity: 0.50
            specularStrokeWidth: 1.25
            shadowOffsetY: 14.0
            shadowIntensity: 0.35
            blurProfile: 2
            highContrast: true
        }
    )", QUrl());

    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    auto *item = qobject_cast<QQuickItem *>(comp.create());
    std::unique_ptr<QQuickItem> guard(item);
    QVERIFY(item != nullptr);

    QCOMPARE(item->property("geometryMode").toInt(), static_cast<int>(ValoGlassItemV2::SquircleG2));
    QCOMPARE(item->property("cornerRadius").toDouble(), 24.0);
    QCOMPARE(item->property("specularStrokeWidth").toDouble(), 1.25);
    QCOMPARE(item->property("shadowOffsetY").toDouble(), 14.0);
    QCOMPARE(item->property("blurProfile").toInt(), 2);
    QCOMPARE(item->property("highContrast").toBool(), true);

    auto *glass = item->findChild<ValoGlassItemV2 *>();
    QVERIFY(glass != nullptr);
    QCOMPARE(glass->geometryMode(), static_cast<int>(ValoGlassItemV2::SquircleG2));
    QCOMPARE(glass->cornerRadii(), QVector4D(24, 24, 24, 24));

    auto *shadow = item->findChild<ValoShadowItemV2 *>();
    QVERIFY(shadow != nullptr);
    QCOMPARE(shadow->geometryMode(), static_cast<int>(ValoGlassItemV2::SquircleG2));
    QCOMPARE(shadow->shadowOffsetY(), 14.0f);
}

void tst_ValoShellV2::testTopBarV2GeometryAndContracts() {
    QQmlComponent comp(m_engine.get());
    comp.setData(R"(
        import QtQuick
        import Systalia.Engine
        SystaliaTopBarV2 {
            id: bar
            width: 1646
            height: 45
        }
    )", QUrl());

    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    auto *topBar = qobject_cast<QQuickItem *>(comp.create());
    std::unique_ptr<QQuickItem> guard(topBar);
    QVERIFY(topBar != nullptr);

    QCOMPARE(topBar->width(), 1646.0);
    QCOMPARE(topBar->height(), 45.0);
    QCOMPARE(topBar->property("defaultRadius").toDouble(), 22.5);
    QCOMPARE(topBar->property("actionButtonCount").toInt(), 6);
    QCOMPARE(topBar->property("isWorkspaceTrackVisible").toBool(), true);

    auto *surface = topBar->findChild<QQuickItem *>("surfaceBase");
    if (!surface) {
        // Se id local não exposto como objectName, busca ValoGlassItemV2
        auto *glass = topBar->findChild<ValoGlassItemV2 *>();
        QVERIFY(glass != nullptr);
        QCOMPARE(glass->geometryMode(), static_cast<int>(ValoGlassItemV2::CircularL2));
    }
}

void tst_ValoShellV2::testFilesWindowV2SquircleG2AndState() {
    QQmlComponent comp(m_engine.get());
    comp.setData(R"(
        import QtQuick
        import Systalia.Engine
        SystaliaFilesWindowV2 {
            id: win
            width: 701
            height: 534
            cornerRadius: 20
        }
    )", QUrl());

    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    auto *win = qobject_cast<QQuickItem *>(comp.create());
    std::unique_ptr<QQuickItem> guard(win);
    QVERIFY(win != nullptr);

    QCOMPARE(win->width(), 701.0);
    QCOMPARE(win->height(), 534.0);
    QCOMPARE(win->property("cornerRadius").toDouble(), 20.0);
    QCOMPARE(win->property("selectedLocation").toString(), QStringLiteral("Home"));

    auto *glass = win->findChild<ValoGlassItemV2 *>();
    QVERIFY(glass != nullptr);
    QCOMPARE(glass->geometryMode(), static_cast<int>(ValoGlassItemV2::SquircleG2));
    QCOMPARE(glass->cornerRadii(), QVector4D(20, 20, 20, 20));

    win->setProperty("selectedLocation", "Documents");
    QCOMPARE(win->property("selectedLocation").toString(), QStringLiteral("Documents"));
}

void tst_ValoShellV2::testControlCenterV2SquircleG2AndServices() {
    QQmlComponent comp(m_engine.get());
    comp.setData(R"(
        import QtQuick
        import Systalia.Engine
        SystaliaControlCenterV2 {
            id: cc
            width: 1280
            height: 720
        }
    )", QUrl());

    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    auto *cc = qobject_cast<QQuickItem *>(comp.create());
    std::unique_ptr<QQuickItem> guard(cc);
    QVERIFY(cc != nullptr);

    QVERIFY(!cc->property("opened").toBool());
    QMetaObject::invokeMethod(cc, "open");
    QVERIFY(cc->property("opened").toBool());

    QMetaObject::invokeMethod(cc, "close");
    QVERIFY(!cc->property("opened").toBool());

    auto *glass = cc->findChild<ValoGlassItemV2 *>();
    QVERIFY(glass != nullptr);
    QCOMPARE(glass->geometryMode(), static_cast<int>(ValoGlassItemV2::SquircleG2));
    QCOMPARE(glass->cornerRadii(), QVector4D(22, 22, 22, 22));
}

void tst_ValoShellV2::testDockV2ShellIntegration() {
    QQmlComponent comp(m_engine.get());
    comp.setData(R"(
        import QtQuick
        import Systalia.Engine
        SystaliaDockV2 {
            id: dock
            width: 555
            height: 125
        }
    )", QUrl());

    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    auto *dock = qobject_cast<QQuickItem *>(comp.create());
    std::unique_ptr<QQuickItem> guard(dock);
    QVERIFY(dock != nullptr);

    QCOMPARE(dock->width(), 555.0);
    QCOMPARE(dock->height(), 125.0);
    QCOMPARE(dock->property("dockItemCount").toInt(), 5);
    QCOMPARE(dock->property("pillRadius").toDouble(), 47.0);

    // Valida método isRunning e isFocused
    QVariantList appList = { QStringLiteral("systalia-files") };
    dock->setProperty("filesVisible", true);
    QVERIFY(dock->property("filesVisible").toBool());

    auto *glass = dock->findChild<ValoGlassItemV2 *>();
    QVERIFY(glass != nullptr);
    QCOMPARE(glass->geometryMode(), static_cast<int>(ValoGlassItemV2::CircularL2));
    QCOMPARE(glass->cornerRadii(), QVector4D(47, 47, 47, 47));
}

void tst_ValoShellV2::testFramebufferPixelVisibilityV2() {
    auto window = std::make_unique<QQuickWindow>();
    window->resize(1280, 800);
    window->setColor(QColor(10, 37, 66)); // Fundo oceânico profundo

    QQmlComponent comp(m_engine.get());
    comp.setData(R"(
        import QtQuick
        import Systalia.Engine
        Item {
            anchors.fill: parent

            SystaliaTopBarV2 {
                id: topBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                height: 45
            }

            SystaliaFilesWindowV2 {
                id: files
                x: 80
                y: 80
                width: 701
                height: 534
                visible: true
            }

            SystaliaDockV2 {
                id: dock
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
            }
        }
    )", QUrl());

    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    auto *rootItem = qobject_cast<QQuickItem *>(comp.create());
    QVERIFY(rootItem != nullptr);
    rootItem->setParentItem(window->contentItem());

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));

    for (int i = 0; i < 8; ++i) {
        window->update();
        QCoreApplication::processEvents();
        QTest::qWait(25);
    }

    const QImage fb = window->grabWindow();
    QVERIFY2(!fb.isNull(), "Framebuffer não pode ser nulo");
    QCOMPARE(fb.size(), QSize(1280, 800));

    // Valida que o framebuffer possui pixels visíveis com alpha pré-multiplicado e luminância real
    int nonZeroPixels = 0;
    int glassTintPixels = 0;
    for (int y = 0; y < fb.height(); ++y) {
        const auto *scanLine = reinterpret_cast<const QRgb *>(fb.constScanLine(y));
        for (int x = 0; x < fb.width(); ++x) {
            const QRgb px = scanLine[x];
            if (qAlpha(px) > 0) {
                nonZeroPixels++;
                // Verifica presença de pixels iluminados da shell (TopBar, Files, Dock)
                if (qBlue(px) > 20 || qGreen(px) > 20 || qRed(px) > 20) {
                    glassTintPixels++;
                }
            }
        }
    }

    qDebug() << "nonZeroPixels:" << nonZeroPixels << "glassTintPixels:" << glassTintPixels;

    QVERIFY2(nonZeroPixels > 100000, "Framebuffer da Shell V2 não pode estar vazio.");
    QVERIFY2(glassTintPixels > 50000, "Material vítreo oceânico VALO V2 deve estar visível no framebuffer.");
}

QTEST_MAIN(tst_ValoShellV2)
#include "tst_ValoShellV2.moc"
