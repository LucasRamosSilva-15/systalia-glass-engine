#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "LiquidTilingService.hpp"
#include "SystaliaGlassItem.hpp"

void qml_register_types_Systalia_Engine();

class TestLiquidTiling final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void calculateSnapTargetResolvesEdgesAndQuadrantsOn1080p();
    void calculateFusedRadiiFlattensContactEdges();
    void asymmetricSplitGeometryConservesAvailableArea();
    void quadrantGeometriesPartitionAvailableArea();
    void splitRatioClampsToValidRange();
    void dragPreviewUpdatesAndClears();
    void pairedWindowsFormActiveSplitAndCleanupOnDestruction();
    void qmlSnapPreviewAndTileSplitterInstantiateCleanly();
};

void TestLiquidTiling::initTestCase() {
    qml_register_types_Systalia_Engine();
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
}

void TestLiquidTiling::calculateSnapTargetResolvesEdgesAndQuadrantsOn1080p() {
    LiquidTilingService service;
    const QRectF screen(0.0, 0.0, 1920.0, 1080.0);

    // 1. Quadrantes nos 4 cantos (distância <= 18px da borda)
    QCOMPARE(service.calculateSnapTarget(QPointF(5.0, 5.0), screen),
             static_cast<int>(LiquidTilingService::SnapTopLeft));
    QCOMPARE(service.calculateSnapTarget(QPointF(1915.0, 8.0), screen),
             static_cast<int>(LiquidTilingService::SnapTopRight));
    QCOMPARE(service.calculateSnapTarget(QPointF(10.0, 1072.0), screen),
             static_cast<int>(LiquidTilingService::SnapBottomLeft));
    QCOMPARE(service.calculateSnapTarget(QPointF(1912.0, 1075.0), screen),
             static_cast<int>(LiquidTilingService::SnapBottomRight));

    // 2. Bordas esquerda, direita e topo (fora da zona de canto)
    QCOMPARE(service.calculateSnapTarget(QPointF(0.0, 540.0), screen),
             static_cast<int>(LiquidTilingService::SnapLeft));
    QCOMPARE(service.calculateSnapTarget(QPointF(18.0, 540.0), screen),
             static_cast<int>(LiquidTilingService::SnapLeft));
    QCOMPARE(service.calculateSnapTarget(QPointF(1902.0, 540.0), screen),
             static_cast<int>(LiquidTilingService::SnapRight));
    QCOMPARE(service.calculateSnapTarget(QPointF(1920.0, 540.0), screen),
             static_cast<int>(LiquidTilingService::SnapRight));
    QCOMPARE(service.calculateSnapTarget(QPointF(960.0, 12.0), screen),
             static_cast<int>(LiquidTilingService::SnapTop));
    QCOMPARE(service.calculateSnapTarget(QPointF(960.0, 18.0), screen),
             static_cast<int>(LiquidTilingService::SnapTop));

    // 3. Limiar exato (> 18px deve retornar SnapNone) e centro livre
    QCOMPARE(service.calculateSnapTarget(QPointF(19.0, 540.0), screen),
             static_cast<int>(LiquidTilingService::SnapNone));
    QCOMPARE(service.calculateSnapTarget(QPointF(1901.0, 540.0), screen),
             static_cast<int>(LiquidTilingService::SnapNone));
    QCOMPARE(service.calculateSnapTarget(QPointF(960.0, 19.0), screen),
             static_cast<int>(LiquidTilingService::SnapNone));
    QCOMPARE(service.calculateSnapTarget(QPointF(960.0, 540.0), screen),
             static_cast<int>(LiquidTilingService::SnapNone));
}

void TestLiquidTiling::calculateFusedRadiiFlattensContactEdges() {
    LiquidTilingService service;
    constexpr qreal r = 24.0;

    // SnapNone: todos os 4 cantos preservam a curvatura G2
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapNone, r),
             QVector4D(24.0f, 24.0f, 24.0f, 24.0f));

    // SnapLeft: borda direita achatada para 0.0 (tl: 24, tr: 0, bl: 24, br: 0)
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapLeft, r),
             QVector4D(24.0f, 0.0f, 24.0f, 0.0f));

    // SnapRight: borda esquerda achatada para 0.0 (tl: 0, tr: 24, bl: 0, br: 24)
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapRight, r),
             QVector4D(0.0f, 24.0f, 0.0f, 24.0f));

    // SnapTop: maximização limpa com cantos internos achatados
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapTop, r),
             QVector4D(0.0f, 0.0f, 0.0f, 0.0f));

    // Quadrantes: apenas o canto exterior correspondente retém curvatura
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapTopLeft, r),
             QVector4D(24.0f, 0.0f, 0.0f, 0.0f));
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapTopRight, r),
             QVector4D(0.0f, 24.0f, 0.0f, 0.0f));
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapBottomLeft, r),
             QVector4D(0.0f, 0.0f, 24.0f, 0.0f));
    QCOMPARE(service.calculateFusedRadii(LiquidTilingService::SnapBottomRight, r),
             QVector4D(0.0f, 0.0f, 0.0f, 24.0f));
}

void TestLiquidTiling::asymmetricSplitGeometryConservesAvailableArea() {
    LiquidTilingService service;
    const QRectF availableArea(0.0, 60.0, 1920.0, 926.0);
    const qreal gap = service.splitterGap();

    // Divisão 0.33 (33% esquerda / 67% direita)
    const QRectF left33 = service.getSnapGeometry(LiquidTilingService::SnapLeft, availableArea, 0.33);
    const QRectF right33 = service.getSnapGeometry(LiquidTilingService::SnapRight, availableArea, 0.33);

    QVERIFY(left33.isValid());
    QVERIFY(right33.isValid());
    QCOMPARE(left33.x(), availableArea.x());
    QCOMPARE(left33.y(), availableArea.y());
    QCOMPARE(left33.height(), availableArea.height());
    QCOMPARE(right33.height(), availableArea.height());
    QVERIFY(qFuzzyCompare(left33.width() + right33.width() + gap, availableArea.width()));
    QVERIFY(qFuzzyCompare(left33.right() + gap, right33.left()));
    QVERIFY(qFuzzyCompare(right33.right(), availableArea.right()));

    // Divisão 0.66 (66% esquerda / 34% direita)
    const QRectF left66 = service.getSnapGeometry(LiquidTilingService::SnapLeft, availableArea, 0.66);
    const QRectF right66 = service.getSnapGeometry(LiquidTilingService::SnapRight, availableArea, 0.66);

    QVERIFY(left66.isValid());
    QVERIFY(right66.isValid());
    QVERIFY(left66.width() > right66.width());
    QVERIFY(qFuzzyCompare(left66.width() + right66.width() + gap, availableArea.width()));
    QVERIFY(qFuzzyCompare(left66.right() + gap, right66.left()));
    QVERIFY(qFuzzyCompare(right66.right(), availableArea.right()));
}

void TestLiquidTiling::quadrantGeometriesPartitionAvailableArea() {
    LiquidTilingService service;
    const QRectF availableArea(10.0, 50.0, 1600.0, 900.0);
    const qreal gap = service.splitterGap();

    const QRectF tl = service.getSnapGeometry(LiquidTilingService::SnapTopLeft, availableArea, 0.5);
    const QRectF tr = service.getSnapGeometry(LiquidTilingService::SnapTopRight, availableArea, 0.5);
    const QRectF bl = service.getSnapGeometry(LiquidTilingService::SnapBottomLeft, availableArea, 0.5);
    const QRectF br = service.getSnapGeometry(LiquidTilingService::SnapBottomRight, availableArea, 0.5);

    QVERIFY(qFuzzyCompare(tl.width() + tr.width() + gap, availableArea.width()));
    QVERIFY(qFuzzyCompare(bl.width() + br.width() + gap, availableArea.width()));
    QVERIFY(qFuzzyCompare(tl.height() + bl.height() + gap, availableArea.height()));
    QVERIFY(qFuzzyCompare(tr.height() + br.height() + gap, availableArea.height()));
}

void TestLiquidTiling::splitRatioClampsToValidRange() {
    LiquidTilingService service;
    QSignalSpy ratioSpy(&service, &LiquidTilingService::splitRatioChanged);

    service.setSplitRatio(0.05);
    QCOMPARE(service.splitRatio(), LiquidTilingService::kMinSplitRatio);
    QCOMPARE(ratioSpy.count(), 1);

    service.setSplitRatio(0.95);
    QCOMPARE(service.splitRatio(), LiquidTilingService::kMaxSplitRatio);
    QCOMPARE(ratioSpy.count(), 2);

    service.setSplitRatio(0.45);
    QCOMPARE(service.splitRatio(), 0.45);
    QCOMPARE(ratioSpy.count(), 3);
}

void TestLiquidTiling::dragPreviewUpdatesAndClears() {
    LiquidTilingService service;
    const QRectF screen(0.0, 0.0, 1920.0, 1080.0);
    const QRectF area(0.0, 60.0, 1920.0, 926.0);

    QVERIFY(!service.previewVisible());
    service.updateSnapPreview(QPointF(8.0, 500.0), screen, area, 24.0);
    QVERIFY(service.previewVisible());
    QCOMPARE(service.previewSnapTarget(), static_cast<int>(LiquidTilingService::SnapLeft));
    QCOMPARE(service.previewRadii(), QVector4D(24.0f, 0.0f, 24.0f, 0.0f));

    service.clearSnapPreview();
    QVERIFY(!service.previewVisible());
    QCOMPARE(service.previewSnapTarget(), static_cast<int>(LiquidTilingService::SnapNone));
}

void TestLiquidTiling::pairedWindowsFormActiveSplitAndCleanupOnDestruction() {
    LiquidTilingService service;
    auto winLeft = std::make_unique<QQuickItem>();
    auto winRight = std::make_unique<QQuickItem>();

    QVERIFY(!service.hasActiveSplitInWorkspace(0));

    service.registerWindowSnap(winLeft.get(), LiquidTilingService::SnapLeft, 0);
    QCOMPARE(service.windowSnapTarget(winLeft.get()),
             static_cast<int>(LiquidTilingService::SnapLeft));
    QVERIFY(!service.hasActiveSplitInWorkspace(0));

    service.registerWindowSnap(winRight.get(), LiquidTilingService::SnapRight, 0);
    QVERIFY(service.hasActiveSplitInWorkspace(0));
    QVERIFY(!service.hasActiveSplitInWorkspace(1));

    // Destruir a janela da direita desfaz automaticamente o par ativo
    winRight.reset();
    QVERIFY(!service.hasActiveSplitInWorkspace(0));
}

void TestLiquidTiling::qmlSnapPreviewAndTileSplitterInstantiateCleanly() {
    QQmlEngine engine;
    LiquidTilingService service;

    QQmlComponent previewComp(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/shell/SnapPreviewOverlay.qml")));
    QVERIFY2(previewComp.isReady(), qPrintable(previewComp.errorString()));
    std::unique_ptr<QQuickItem> previewItem(qobject_cast<QQuickItem *>(previewComp.create()));
    QVERIFY(previewItem != nullptr);

    QQmlComponent splitterComp(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/shell/TileSplitter.qml")));
    QVERIFY2(splitterComp.isReady(), qPrintable(splitterComp.errorString()));
    std::unique_ptr<QQuickItem> splitterItem(qobject_cast<QQuickItem *>(splitterComp.create()));
    QVERIFY(splitterItem != nullptr);

    splitterItem->setProperty("tilingService", QVariant::fromValue(static_cast<QObject *>(&service)));
    splitterItem->setProperty("availableArea", QRectF(0.0, 0.0, 1920.0, 1080.0));
    splitterItem->setProperty("activeSplit", true);
    QCOMPARE(splitterItem->property("activeSplit").toBool(), true);
    QCOMPARE(splitterItem->height(), 1080.0);
    QVERIFY(splitterItem->x() > 900.0 && splitterItem->x() < 1020.0);
}

QTEST_MAIN(TestLiquidTiling)
#include "tst_LiquidTiling.moc"
