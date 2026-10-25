#include <QTest>
#include <cmath>
#include "SystaliaClientClip.hpp"
#include "SystaliaSurfaceProps.hpp"

class TestUnifiedComposition final : public QObject {
    Q_OBJECT

private slots:
    void clientStd140BlockHasExpectedSize();
    void clientTextureFlagDefaultsToDisabled();
    void clientTitleBarHeightMatchesLayout();
    void clientViewportUsesTheConfiguredInsets();
    void clientTextureCoordinatesSpanTheViewport();
    void clientSdfInteriorAndStraightEdgeDistances();
    void clientSdfMatchesTheRoundedCornerBoundary();
    void innerAndOuterCornerCentersAreConcentric();
    void slantKeepsTheClientMaskFinite();
};

void TestUnifiedComposition::clientStd140BlockHasExpectedSize() {
    const SystaliaSurfaceProps props;
    QCOMPARE(sizeof(props), size_t(128));
    QCOMPARE(offsetof(SystaliaSurfaceProps, hasClientTexture), size_t(120));
    QCOMPARE(offsetof(SystaliaSurfaceProps, titleBarHeight), size_t(124));
}

void TestUnifiedComposition::clientTextureFlagDefaultsToDisabled() {
    const SystaliaSurfaceProps props;
    QCOMPARE(props.hasClientTexture, 0.0f);
}

void TestUnifiedComposition::clientTitleBarHeightMatchesLayout() {
    const SystaliaSurfaceProps props;
    QCOMPARE(props.titleBarHeight, SystaliaDefaultTitleBarHeight);
}

void TestUnifiedComposition::clientViewportUsesTheConfiguredInsets() {
    const auto geometry = SystaliaClientClipGeometry::fromWindow(
        QSizeF(680.0, 480.0), SystaliaDefaultTitleBarHeight,
        SystaliaClientClipInset, QVector4D(24.0f, 24.0f, 32.0f, 32.0f));

    QCOMPARE(geometry.bounds, QRectF(8.0, 44.0, 664.0, 428.0));
    QCOMPARE(geometry.radii, QVector4D(0.0f, 0.0f, 24.0f, 24.0f));
}

void TestUnifiedComposition::clientTextureCoordinatesSpanTheViewport() {
    const auto geometry = SystaliaClientClipGeometry::fromWindow(
        QSizeF(680.0, 480.0), SystaliaDefaultTitleBarHeight,
        SystaliaClientClipInset, QVector4D(24.0f, 24.0f, 32.0f, 32.0f));
    QCOMPARE(geometry.textureCoordinates(QPointF(8.0, 44.0)), QPointF(0.0, 0.0));
    QCOMPARE(geometry.textureCoordinates(QPointF(672.0, 472.0)), QPointF(1.0, 1.0));
}

void TestUnifiedComposition::clientSdfInteriorAndStraightEdgeDistances() {
    const auto geometry = SystaliaClientClipGeometry::fromWindow(
        QSizeF(680.0, 480.0), 36.0f, 8.0f,
        QVector4D(24.0f, 24.0f, 32.0f, 32.0f));
    QCOMPARE(geometry.signedDistance(QPointF(340.0, 258.0)), -214.0f);
    QCOMPARE(geometry.signedDistance(QPointF(8.0, 100.0)), 0.0f);
    QVERIFY(geometry.signedDistance(QPointF(9.0, 100.0)) < 0.0f);
}

void TestUnifiedComposition::clientSdfMatchesTheRoundedCornerBoundary() {
    const auto geometry = SystaliaClientClipGeometry::fromWindow(
        QSizeF(680.0, 480.0), 36.0f, 8.0f,
        QVector4D(24.0f, 24.0f, 32.0f, 32.0f));
    const QPointF innerCornerCenter(geometry.bounds.right() - geometry.radii.w(),
                                    geometry.bounds.bottom() - geometry.radii.w());
    const float cornerOffset = geometry.radii.w() / std::sqrt(std::sqrt(2.0f));
    const QPointF squircleBoundary(innerCornerCenter.x() + cornerOffset,
                                   innerCornerCenter.y() + cornerOffset);
    QVERIFY(std::abs(geometry.signedDistance(squircleBoundary)) < 0.0001f);
}

void TestUnifiedComposition::innerAndOuterCornerCentersAreConcentric() {
    const auto geometry = SystaliaClientClipGeometry::fromWindow(
        QSizeF(680.0, 480.0), 36.0f, 8.0f,
        QVector4D(24.0f, 24.0f, 32.0f, 32.0f));
    const QPointF innerCornerCenter(geometry.bounds.right() - geometry.radii.w(),
                                    geometry.bounds.bottom() - geometry.radii.w());
    const QPointF outerCornerCenter(680.0 - 32.0, 480.0 - 32.0);
    QCOMPARE(innerCornerCenter, outerCornerCenter);
}

void TestUnifiedComposition::slantKeepsTheClientMaskFinite() {
    const auto geometry = SystaliaClientClipGeometry::fromWindow(
        QSizeF(680.0, 480.0), 36.0f, 8.0f,
        QVector4D(24.0f, 24.0f, 32.0f, 32.0f));

    QVERIFY(std::isfinite(geometry.signedDistance(QPointF(340.0, 258.0), 0.35f)));
    QVERIFY(geometry.signedDistance(QPointF(340.0, 258.0), 0.35f) < 0.0f);
}

QTEST_APPLESS_MAIN(TestUnifiedComposition)
#include "tst_UnifiedComposition.moc"
