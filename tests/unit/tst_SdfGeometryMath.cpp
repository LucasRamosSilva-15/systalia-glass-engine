#include <QFile>
#include <QTest>
#include <QVector4D>
#include <algorithm>
#include <cmath>

#include "SystaliaClientClip.hpp"

namespace {

SystaliaClientClipGeometry clipGeometry() {
    return SystaliaClientClipGeometry::fromWindow(
        QSizeF(200.0, 160.0), 0.0f, 0.0f, QVector4D(20.0f, 24.0f, 28.0f, 32.0f));
}

float referenceAlpha(float distance, float derivativeWidth) {
    const float width = std::max(derivativeWidth, 0.75f);
    const float t = std::clamp((distance + width * 0.5f) / width, 0.0f, 1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return 1.0f - smooth;
}

} // namespace

class TestSdfGeometryMath final : public QObject {
    Q_OBJECT

private slots:
    void interiorCenterHasNegativeDistance();
    void flatEdges_data();
    void flatEdges();
    void roundedCornerCenterHasNegativeDistance();
    void superellipseBoundaryHasZeroDistance();
    void roundedBoxVertices_data();
    void roundedBoxVertices();
    void distantExteriorHasPositiveDistance();
    void shaderDeclaresDerivativeAndDiscardContract();
    void alphaCoverage_data();
    void alphaCoverage();
};

void TestSdfGeometryMath::interiorCenterHasNegativeDistance() {
    QCOMPARE(clipGeometry().signedDistance(QPointF(100.0, 80.0)), -80.0f);
}

void TestSdfGeometryMath::flatEdges_data() {
    QTest::addColumn<QPointF>("point");
    QTest::addColumn<QString>("edge");
    QTest::newRow("top") << QPointF(100.0, 0.0) << QStringLiteral("top");
    QTest::newRow("bottom") << QPointF(100.0, 160.0) << QStringLiteral("bottom");
    QTest::newRow("left") << QPointF(0.0, 80.0) << QStringLiteral("left");
    QTest::newRow("right") << QPointF(200.0, 80.0) << QStringLiteral("right");
}

void TestSdfGeometryMath::flatEdges() {
    QFETCH(QPointF, point);
    QFETCH(QString, edge);
    const float distance = clipGeometry().signedDistance(point);
    QVERIFY2(std::abs(distance) <= 0.001f,
             qPrintable(QStringLiteral("The %1 straight edge must have zero distance").arg(edge)));
}

void TestSdfGeometryMath::roundedCornerCenterHasNegativeDistance() {
    QCOMPARE(clipGeometry().signedDistance(QPointF(168.0, 128.0)), -32.0f);
}

void TestSdfGeometryMath::superellipseBoundaryHasZeroDistance() {
    const float distance = clipGeometry().signedDistance(QPointF(194.91, 154.91));
    QVERIFY(std::abs(distance) <= 0.02f);
}

void TestSdfGeometryMath::roundedBoxVertices_data() {
    QTest::addColumn<QPointF>("point");
    QTest::addColumn<float>("expectedDistance");
    QTest::newRow("bottom-right") << QPointF(200.0, 160.0) << 6.055f;
    QTest::newRow("bottom-left") << QPointF(0.0, 160.0) << 5.298f;
}

void TestSdfGeometryMath::roundedBoxVertices() {
    QFETCH(QPointF, point);
    QFETCH(float, expectedDistance);
    QVERIFY(std::abs(clipGeometry().signedDistance(point) - expectedDistance) <= 0.01f);
}

void TestSdfGeometryMath::distantExteriorHasPositiveDistance() {
    QVERIFY(std::abs(clipGeometry().signedDistance(QPointF(400.0, 400.0)) - 270.5f) <= 0.5f);
}

void TestSdfGeometryMath::shaderDeclaresDerivativeAndDiscardContract() {
    QFile shader(QStringLiteral(SGE_SOURCE_DIR "/shaders/valo_optical.frag"));
    QVERIFY2(shader.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(shader.errorString()));
    const QByteArray source = shader.readAll();
    QVERIFY(source.contains("fwidth(dist)"));
    QVERIFY(source.contains("if (alphaMask <= 0.001) discard;"));
}

void TestSdfGeometryMath::alphaCoverage_data() {
    QTest::addColumn<float>("distance");
    QTest::addColumn<float>("expectedAlpha");
    QTest::addColumn<bool>("discarded");
    QTest::newRow("deep-interior") << -0.375f << 1.0f << false;
    QTest::newRow("boundary") << 0.0f << 0.5f << false;
    QTest::newRow("visible-antialias-fringe") << 0.35f << 0.0032592f << false;
    QTest::newRow("below-alpha-threshold") << 0.374f << 0.0000053f << true;
    QTest::newRow("at-pixel-edge") << 0.375f << 0.0f << true;
    QTest::newRow("outside") << 1.0f << 0.0f << true;
}

void TestSdfGeometryMath::alphaCoverage() {
    QFETCH(float, distance);
    QFETCH(float, expectedAlpha);
    QFETCH(bool, discarded);
    const float alpha = referenceAlpha(distance, 0.75f);
    QVERIFY(std::abs(alpha - expectedAlpha) <= 0.00002f);
    QCOMPARE(alpha <= 0.001f, discarded);
}

QTEST_GUILESS_MAIN(TestSdfGeometryMath)
#include "tst_SdfGeometryMath.moc"
