#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <cstddef>
#include <memory>

#include "OutputManagerService.hpp"
#include "SystaliaCompositor.hpp"
#include "SystaliaGlassItem.hpp"
#include "SystaliaSurfaceProps.hpp"

class TestOutputManager final : public QObject {
    Q_OBJECT

private slots:
    void sideBySideVirtualMonitorGridResolution();
    void verticallyStackedVirtualMonitorGridResolution();
    void windowCenterToScreenIndexAndBoundaryMigration();
    void fractionalScalingAndStd140SurfacePropsAlignment();
    void normalizedBackdropUvProjectionPerDisplay();
    void xdgOutputManagerIntegratesWithCompositor();
};

void TestOutputManager::sideBySideVirtualMonitorGridResolution() {
    OutputManagerService manager;
    manager.clearScreens();

    const int mon0 = manager.addVirtualScreen(
        QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0, 60000);
    const int mon1 = manager.addVirtualScreen(
        QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5, 144000);

    QCOMPARE(mon0, 0);
    QCOMPARE(mon1, 1);
    QCOMPARE(manager.screenCount(), 2);
    QCOMPARE(manager.screenGeometryAt(0), QRect(0, 0, 1920, 1080));
    QCOMPARE(manager.screenGeometryAt(1), QRect(1920, 0, 2560, 1440));
    QCOMPARE(manager.virtualCanvasBounds(), QRect(0, 0, 4480, 1440));

    const QVariantList geometries = manager.screenGeometries();
    QCOMPARE(geometries.size(), 2);
    QCOMPARE(geometries.at(0).toRect(), QRect(0, 0, 1920, 1080));
    QCOMPARE(geometries.at(1).toRect(), QRect(1920, 0, 2560, 1440));
}

void TestOutputManager::verticallyStackedVirtualMonitorGridResolution() {
    OutputManagerService manager;
    manager.clearScreens();

    manager.addVirtualScreen(QStringLiteral("eDP-1"), QRect(0, 0, 1920, 1080), 1.25);
    manager.addVirtualScreen(QStringLiteral("HDMI-1"), QRect(0, 1080, 3840, 2160), 2.0);

    QCOMPARE(manager.screenCount(), 2);
    QCOMPARE(manager.virtualCanvasBounds(), QRect(0, 0, 3840, 3240));
    QCOMPARE(manager.pointToScreenIndex(QPointF(960.0, 540.0)), 0);
    QCOMPARE(manager.pointToScreenIndex(QPointF(1920.0, 1600.0)), 1);
}

void TestOutputManager::windowCenterToScreenIndexAndBoundaryMigration() {
    OutputManagerService manager;
    manager.clearScreens();
    manager.addVirtualScreen(QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0);
    manager.addVirtualScreen(QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5);

    // Janela inteiramente no Monitor 0 (centro em x = 500)
    QCOMPARE(manager.windowCenterToScreenIndex(QRectF(100.0, 100.0, 800.0, 500.0)), 0);

    // Janela na fronteira, mas com centro (x = 1820) ainda no Monitor 0
    QCOMPARE(manager.windowCenterToScreenIndex(QRectF(1420.0, 100.0, 800.0, 500.0)), 0);

    // Janela atravessando a fronteira com centro (x = 2020) no Monitor 1
    QCOMPARE(manager.windowCenterToScreenIndex(QRectF(1620.0, 100.0, 800.0, 500.0)), 1);

    // Validação do despacho de transição de ecrã
    QQuickItem dummyWindow;
    QSignalSpy transitionSpy(&manager, &OutputManagerService::windowScreenTransitioned);

    const int initialScreen = manager.updateWindowOutputRouting(
        &dummyWindow, nullptr, QRectF(120.0, 100.0, 800.0, 500.0));
    QCOMPARE(initialScreen, 0);
    QCOMPARE(transitionSpy.count(), 1);

    const int migratedScreen = manager.updateWindowOutputRouting(
        &dummyWindow, nullptr, QRectF(2100.0, 120.0, 800.0, 500.0));
    QCOMPARE(migratedScreen, 1);
    QCOMPARE(manager.windowScreenIndex(&dummyWindow), 1);
    QCOMPARE(transitionSpy.count(), 2);
    QCOMPARE(transitionSpy.last().at(1).toInt(), 1);
    QCOMPARE(transitionSpy.last().at(2).toInt(), 0);
    QCOMPARE(transitionSpy.last().at(3).toReal(), 1.5);
}

void TestOutputManager::fractionalScalingAndStd140SurfacePropsAlignment() {
    // Validação dos invariantes de ABI std140 (128 bytes e offset 88 para screenResolution)
    static_assert(sizeof(SystaliaSurfaceProps) == 128);
    static_assert(alignof(SystaliaSurfaceProps) == 16);
    static_assert(offsetof(SystaliaSurfaceProps, windowSize) == 80);
    static_assert(offsetof(SystaliaSurfaceProps, screenResolution) == 88);

    QCOMPARE(sizeof(SystaliaSurfaceProps), size_t(128));
    QCOMPARE(offsetof(SystaliaSurfaceProps, screenResolution), size_t(88));

    OutputManagerService manager;
    manager.clearScreens();
    manager.addVirtualScreen(QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.25);
    manager.addVirtualScreen(QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5);
    manager.addVirtualScreen(QStringLiteral("DP-3"), QRect(4480, 0, 3840, 2160), 2.0);

    QCOMPARE(manager.primaryScale(), 1.25);
    QCOMPARE(manager.screenScale120thsAt(0), 150);
    QCOMPARE(manager.screenScale120thsAt(1), 180);
    QCOMPARE(manager.screenScale120thsAt(2), 240);

    // Resolução física real = logicalSize * scale
    QCOMPARE(manager.physicalResolutionAt(0), QVector2D(2400.0f, 1350.0f));
    QCOMPARE(manager.physicalResolutionAt(1), QVector2D(3840.0f, 2160.0f));
    QCOMPARE(manager.physicalResolutionAt(2), QVector2D(7680.0f, 4320.0f));

    QQuickWindow hostWindow;
    hostWindow.resize(1920, 1080);
    SystaliaGlassItem glassItem(hostWindow.contentItem());
    glassItem.setWidth(800.0);
    glassItem.setHeight(500.0);
    glassItem.setDevicePixelRatio(1.5);

    QCOMPARE(glassItem.devicePixelRatio(), 1.5);
    QCOMPARE(glassItem.effectivePhysicalResolution(), QVector2D(2880.0f, 1620.0f));
}

void TestOutputManager::normalizedBackdropUvProjectionPerDisplay() {
    OutputManagerService manager;
    manager.clearScreens();
    manager.addVirtualScreen(QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0);
    manager.addVirtualScreen(QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5);

    const QVector2D uvCenterMon0 = manager.projectBackdropUV(QPointF(960.0, 540.0), 0);
    QVERIFY(std::abs(uvCenterMon0.x() - 0.5f) < 0.001f);
    QVERIFY(std::abs(uvCenterMon0.y() - 0.5f) < 0.001f);

    const QVector2D uvCenterMon1 = manager.projectBackdropUV(QPointF(1920.0 + 1280.0, 720.0), 1);
    QVERIFY(std::abs(uvCenterMon1.x() - 0.5f) < 0.001f);
    QVERIFY(std::abs(uvCenterMon1.y() - 0.5f) < 0.001f);
}

void TestOutputManager::xdgOutputManagerIntegratesWithCompositor() {
    QTemporaryDir runtimeDir;
    QVERIFY(runtimeDir.isValid());
    qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8());

    OutputManagerService manager;
    manager.clearScreens();
    manager.addVirtualScreen(QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.25);
    manager.addVirtualScreen(QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5);

    SystaliaCompositor compositor;
    compositor.setSocketName(QByteArrayLiteral("wayland-sge-output-test"));
    compositor.create();

    QVERIFY(compositor.isCreated());
    QCOMPARE(compositor.outputManager(), &manager);
    QVERIFY(manager.xdgOutputManager() != nullptr);
    QVERIFY(manager.xdgOutputManager()->isInitialized());
}

QTEST_MAIN(TestOutputManager)
#include "tst_OutputManager.moc"

