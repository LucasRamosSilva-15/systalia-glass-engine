#include <QTest>
#include <QVector2D>
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

static QVector2D computeRefractionOffset(QVector2D p, QVector2D halfSize, QVector4D radii, float slant, float ior) {
    float dist = evaluateSurfaceSDF(p, halfSize, radii, slant);
    QVector2D norm = calculateNormal(p, halfSize, radii, slant);
    float distFromEdge = std::max(-dist, 0.0f);

    const float MENISCUS_MAX = 7.0f;
    float meniscusFactor = std::clamp(1.0f - (distFromEdge / MENISCUS_MAX), 0.0f, 1.0f);
    float meniscusProfile = meniscusFactor * meniscusFactor * (3.0f - 2.0f * meniscusFactor); // smoothstep(0, 1, factor)

    QVector2D p_norm(p.x() / std::max(halfSize.x(), 1.0f),
                     p.y() / std::max(halfSize.y(), 1.0f));
    QVector2D bodyRefraction = p_norm * (ior - 1.0f) * 0.14f;
    QVector2D meniscusRefraction = norm * (meniscusProfile * (ior - 1.0f) * 0.10f);

    return bodyRefraction + meniscusRefraction;
}

class TestOpticalPhysics : public QObject {
    Q_OBJECT

private slots:
    void testZeroRefractionAtIor1();
    void testMeniscusRefractionAtIorGreaterThan1();
    void testCauchyDispersionSeparation();
    void testNormalVectorsUnitLength();
};

void TestOpticalPhysics::testZeroRefractionAtIor1() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;
    const float ior = 1.0f;

    // Teste em múltiplos pontos: centro, menisco e quina
    QVector2D offsetCenter = computeRefractionOffset(QVector2D(0.0f, 0.0f), halfSize, radii, slant, ior);
    QCOMPARE(offsetCenter.x(), 0.0f);
    QCOMPARE(offsetCenter.y(), 0.0f);

    QVector2D offsetEdge = computeRefractionOffset(QVector2D(halfSize.x() - 3.0f, 0.0f), halfSize, radii, slant, ior);
    QCOMPARE(offsetEdge.x(), 0.0f);
    QCOMPARE(offsetEdge.y(), 0.0f);
}

void TestOpticalPhysics::testMeniscusRefractionAtIorGreaterThan1() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;
    const float ior = 1.25f;

    // Ponto situado a 2.0px da aresta direita (dentro do menisco de 7px)
    QVector2D pointNearRightEdge(halfSize.x() - 2.0f, 0.0f);
    QVector2D offset = computeRefractionOffset(pointNearRightEdge, halfSize, radii, slant, ior);

    // O vetor de refração deve ser estritamente não nulo
    QVERIFY2(offset.length() > 0.005f, "Refração do menisco com IOR 1.25 deve produzir deflexão mensurável");

    // Na aresta direita, a componente X da refração deve ser positiva (orientada pela normal externa)
    QVERIFY2(offset.x() > 0.0f, "A deflexão na aresta direita deve seguir a direção normal");
}

void TestOpticalPhysics::testCauchyDispersionSeparation() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;
    const float ior = 1.25f;
    const float dispersion = 0.035f;

    QVector2D pointNearEdge(halfSize.x() - 2.0f, 0.0f);
    QVector2D offset = computeRefractionOffset(pointNearEdge, halfSize, radii, slant, ior);

    QVector2D screenUV(0.5f, 0.5f);
    QVector2D refrUV_R = screenUV + offset * (1.0f - dispersion * 2.0f);
    QVector2D refrUV_G = screenUV + offset;
    QVector2D refrUV_B = screenUV + offset * (1.0f + dispersion * 2.0f);

    float chromaticDistance = (refrUV_B - refrUV_R).length();

    // A separação espectral entre R e B deve ser estritamente positiva e proporcional à dispersão
    QVERIFY2(chromaticDistance > 0.0f, "Canais R e B devem apresentar defasagem espectral distinta");
    QVERIFY2((refrUV_G - screenUV).length() > 0.0f, "Canal G deve manter o offset base");
}

void TestOpticalPhysics::testNormalVectorsUnitLength() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;

    // Normal na aresta direita deve apontar para (+1, 0) com magnitude unitária
    QVector2D normRight = calculateNormal(QVector2D(halfSize.x(), 0.0f), halfSize, radii, slant);
    QCOMPARE(normRight.x(), 1.0f);
    QCOMPARE(normRight.y(), 0.0f);
    QVERIFY(std::abs(normRight.length() - 1.0f) < 1e-4f);

    // Normal na aresta superior deve apontar para (0, +1) com magnitude unitária
    QVector2D normTop = calculateNormal(QVector2D(0.0f, halfSize.y()), halfSize, radii, slant);
    QCOMPARE(normTop.x(), 0.0f);
    QCOMPARE(normTop.y(), 1.0f);
    QVERIFY(std::abs(normTop.length() - 1.0f) < 1e-4f);
}

QTEST_MAIN(TestOpticalPhysics)
#include "tst_OpticalPhysics.moc"
