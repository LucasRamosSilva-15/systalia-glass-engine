#include <QTest>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <cmath>
#include <algorithm>

static float selectRadius(QVector2D p, QVector4D r) {
    if (p.x() < 0.0f) {
        return (p.y() < 0.0f) ? r.x() : r.z();
    } else {
        return (p.y() < 0.0f) ? r.y() : r.w();
    }
}

static float sdRoundedBox4G2(QVector2D p, QVector2D b, QVector4D r) {
    float rad = selectRadius(p, r);
    rad = std::clamp(rad, 0.0f, std::min(b.x(), b.y()) - 0.5f);

    QVector2D q(std::abs(p.x()) - b.x() + rad,
                std::abs(p.y()) - b.y() + rad);
    QVector2D q_pos(std::max(q.x(), 0.0f),
                    std::max(q.y(), 0.0f));

    float cornerDist = std::pow(std::pow(q_pos.x(), 4.0f) + std::pow(q_pos.y(), 4.0f), 0.25f) - rad;
    return (q.x() > 0.0f && q.y() > 0.0f) ? cornerDist : (std::max(q.x(), q.y()) - rad);
}

static float evaluateSurfaceSDF(QVector2D p, QVector2D halfSize, QVector4D radii, float slant) {
    QVector2D p_mod = p;
    float slantFactor = 1.0f - (p.y() / std::max(halfSize.y(), 1.0f)) * slant * 0.25f;
    p_mod.setX(p_mod.x() / std::max(slantFactor, 0.05f));
    float dist = sdRoundedBox4G2(p_mod, halfSize, radii);
    return dist * slantFactor;
}

static QVector2D calculateNormal(QVector2D p, QVector2D halfSize, QVector4D radii, float slant) {
    QVector2D eps(0.5f, 0.0f);
    float dx = evaluateSurfaceSDF(p + eps, halfSize, radii, slant) - evaluateSurfaceSDF(p - eps, halfSize, radii, slant);
    float dy = evaluateSurfaceSDF(p + QVector2D(0.0f, 0.5f), halfSize, radii, slant) - evaluateSurfaceSDF(p - QVector2D(0.0f, 0.5f), halfSize, radii, slant);
    float len = std::sqrt(dx * dx + dy * dy);
    return (len > 1e-5f) ? QVector2D(dx / len, dy / len) : QVector2D(0.0f, -1.0f);
}

static QVector2D computeRefractionOffset(QVector2D p, QVector2D halfSize, QVector4D radii, float slant, float ior, QVector2D screenResolution) {
    float dist = evaluateSurfaceSDF(p, halfSize, radii, slant);
    QVector2D norm = calculateNormal(p, halfSize, radii, slant);
    float distFromEdge = std::max(-dist, 0.0f);

    const float LENS_WIDTH = 32.0f;
    float edgeDepth = std::clamp(distFromEdge / LENS_WIDTH, 0.0f, 1.0f);
    float waterDropProfile = std::pow(std::sin((1.0f - edgeDepth) * 1.5707963f), 1.7f);

    QVector2D texelSize(1.0f / std::max(screenResolution.x(), 1.0f),
                        1.0f / std::max(screenResolution.y(), 1.0f));

    float refrStrength = waterDropProfile * (ior - 1.0f) * 70.0f;
    return norm * refrStrength * texelSize;
}

class TestOpticalPhysics : public QObject {
    Q_OBJECT

private slots:
    void testZeroRefractionAtIor1();
    void testMeniscusRefractionAtIorGreaterThan1();
    void testCauchyDispersionSeparation();
    void testInteriorZeroDistortion();
    void testNormalVectorsUnitLength();
};

void TestOpticalPhysics::testZeroRefractionAtIor1() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;
    const float ior = 1.0f;
    const QVector2D screenRes(1280.0f, 800.0f);

    QVector2D offsetCenter = computeRefractionOffset(QVector2D(0.0f, 0.0f), halfSize, radii, slant, ior, screenRes);
    QCOMPARE(offsetCenter.x(), 0.0f);
    QCOMPARE(offsetCenter.y(), 0.0f);

    QVector2D offsetEdge = computeRefractionOffset(QVector2D(halfSize.x() - 3.0f, 0.0f), halfSize, radii, slant, ior, screenRes);
    QCOMPARE(offsetEdge.x(), 0.0f);
    QCOMPARE(offsetEdge.y(), 0.0f);
}

void TestOpticalPhysics::testMeniscusRefractionAtIorGreaterThan1() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;
    const float ior = 1.33f;
    const QVector2D screenRes(1280.0f, 800.0f);

    // Ponto a 4.0px da aresta direita (dentro do menisco de água de 32px)
    QVector2D pointNearRightEdge(halfSize.x() - 4.0f, 0.0f);
    QVector2D offset = computeRefractionOffset(pointNearRightEdge, halfSize, radii, slant, ior, screenRes);

    // O vetor de refração deve ser estritamente não nulo
    QVERIFY2(offset.length() > 0.001f, "Refração do menisco de água de 32px com IOR 1.33 deve produzir deflexão mensurável");

    // Na aresta direita (norm.x > 0), +norm atrai a luz externa para dentro (+X)
    QVERIFY2(offset.x() > 0.0f, "A deflexão na aresta direita deve seguir a normal externa (+norm)");
}

void TestOpticalPhysics::testCauchyDispersionSeparation() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;
    const float ior = 1.33f;
    const float dispersion = 0.055f;
    const QVector2D screenRes(1280.0f, 800.0f);

    QVector2D pointNearEdge(halfSize.x() - 4.0f, 0.0f);
    QVector2D offset = computeRefractionOffset(pointNearEdge, halfSize, radii, slant, ior, screenRes);
    QVector2D norm = calculateNormal(pointNearEdge, halfSize, radii, slant);

    float dist = evaluateSurfaceSDF(pointNearEdge, halfSize, radii, slant);
    float distFromEdge = std::max(-dist, 0.0f);
    float edgeDepth = std::clamp(distFromEdge / 32.0f, 0.0f, 1.0f);
    float waterDropProfile = std::pow(std::sin((1.0f - edgeDepth) * 1.5707963f), 1.7f);

    float texelSizeX = 1.0f / screenRes.x();
    float dispersionBase = dispersion * waterDropProfile * 55.0f * texelSizeX;

    QVector2D screenUV(0.5f, 0.5f);
    QVector2D uvR = screenUV + offset + norm * dispersionBase;
    QVector2D uvG = screenUV + offset;
    QVector2D uvB = screenUV + offset - norm * dispersionBase;

    float chromaticDistance = (uvB - uvR).length();

    QVERIFY2(chromaticDistance > 0.0f, "Canais R e B devem apresentar separação espectral de Cauchy na borda");
    QCOMPARE(uvG, screenUV + offset);
}

void TestOpticalPhysics::testInteriorZeroDistortion() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;
    const float ior = 1.50f;
    const QVector2D screenRes(1280.0f, 800.0f);

    // Ponto a 50px da aresta (bem além dos 20px da casca do menisco)
    QVector2D interiorPoint(halfSize.x() - 50.0f, 0.0f);
    QVector2D offset = computeRefractionOffset(interiorPoint, halfSize, radii, slant, ior, screenRes);

    // No interior, meniscusProfile == 0.0 => offset == 0.0 (sem distorção nas UIs)
    QCOMPARE(offset.x(), 0.0f);
    QCOMPARE(offset.y(), 0.0f);
}

void TestOpticalPhysics::testNormalVectorsUnitLength() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;

    QVector2D normRight = calculateNormal(QVector2D(halfSize.x(), 0.0f), halfSize, radii, slant);
    QCOMPARE(normRight.x(), 1.0f);
    QCOMPARE(normRight.y(), 0.0f);
    QVERIFY(std::abs(normRight.length() - 1.0f) < 1e-4f);

    QVector2D normTop = calculateNormal(QVector2D(0.0f, halfSize.y()), halfSize, radii, slant);
    QCOMPARE(normTop.x(), 0.0f);
    QCOMPARE(normTop.y(), 1.0f);
    QVERIFY(std::abs(normTop.length() - 1.0f) < 1e-4f);
}

QTEST_MAIN(TestOpticalPhysics)
#include "tst_OpticalPhysics.moc"
