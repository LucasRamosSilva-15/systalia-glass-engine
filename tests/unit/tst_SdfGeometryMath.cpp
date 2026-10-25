#include <QFile>
#include <QTest>
#include <QVector2D>
#include <QVector4D>
#include <algorithm>
#include <cmath>

#include "SystaliaClientClip.hpp"

class TestSdfGeometryMath final : public QObject {
    Q_OBJECT

private slots:
    void signedDistance_data();
    void signedDistance();
    void shaderCoverageAndDiscardThreshold();
};

void TestSdfGeometryMath::signedDistance_data() {
    QTest::addColumn<QPointF>("point");
    QTest::addColumn<float>("expectedDistance");
    QTest::addColumn<float>("tolerance");

    QTest::newRow("center") << QPointF(100.0, 80.0) << -80.0f << 0.001f;
    QTest::newRow("top-edge") << QPointF(100.0, 0.0) << 0.0f << 0.001f;
    QTest::newRow("bottom-edge") << QPointF(100.0, 160.0) << 0.0f << 0.001f;
    QTest::newRow("left-edge") << QPointF(0.0, 80.0) << 0.0f << 0.001f;
    QTest::newRow("right-edge") << QPointF(200.0, 80.0) << 0.0f << 0.001f;
    QTest::newRow("bottom-right-corner-center") << QPointF(168.0, 128.0) << -32.0f << 0.001f;
    QTest::newRow("bottom-right-superellipse-boundary")
        << QPointF(194.91, 154.91) << 0.0f << 0.02f;
    QTest::newRow("bottom-right-square-vertex-is-outside")
        << QPointF(200.0, 160.0) << 6.055f << 0.01f;
    QTest::newRow("bottom-left-square-vertex-is-outside")
        << QPointF(0.0, 160.0) << 5.298f << 0.01f;
    QTest::newRow("distant-exterior") << QPointF(400.0, 400.0) << 270.5f << 0.5f;
}

void TestSdfGeometryMath::signedDistance() {
    QFETCH(QPointF, point);
    QFETCH(float, expectedDistance);
    QFETCH(float, tolerance);

    const SystaliaClientClipGeometry geometry = SystaliaClientClipGeometry::fromWindow(
        QSizeF(200.0, 160.0), 0.0f, 0.0f, QVector4D(20.0f, 24.0f, 28.0f, 32.0f));
    QVERIFY(std::abs(geometry.signedDistance(point) - expectedDistance) <= tolerance);
}

void TestSdfGeometryMath::shaderCoverageAndDiscardThreshold() {
    QFile shader(QStringLiteral(SGE_SOURCE_DIR "/shaders/valo_optical.frag"));
    QVERIFY2(shader.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(shader.errorString()));
    const QByteArray source = shader.readAll();
    QVERIFY(source.contains("fwidth(dist)"));
    QVERIFY(source.contains("if (alphaMask <= 0.001) discard;"));

    const auto smoothstep = [](float edge0, float edge1, float value) {
        const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    const auto alphaAt = [&smoothstep](float distance, float fwidth) {
        const float fw = std::max(fwidth, 0.75f);
        return 1.0f - smoothstep(-fw * 0.5f, fw * 0.5f, distance);
    };

    QVERIFY(alphaAt(0.0f, 1.0f) > 0.001f);
    QVERIFY(alphaAt(1.0f, 1.0f) <= 0.001f);
}

QTEST_GUILESS_MAIN(TestSdfGeometryMath)
#include "tst_SdfGeometryMath.moc"
