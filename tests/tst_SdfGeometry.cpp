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

class TestSdfGeometry : public QObject {
    Q_OBJECT

private slots:
    void testOriginIsInside();
    void testStraightEdges();
    void testExternalCorner();
    void testCurvatureContinuity();
};

void TestSdfGeometry::testOriginIsInside() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;

    float dist = evaluateSurfaceSDF(QVector2D(0.0f, 0.0f), halfSize, radii, slant);

    // O centro deve estar profundamente dentro do objeto
    QVERIFY2(dist < -100.0f, "Centro do chassi deve ter distância estritamente negativa");
    QCOMPARE(dist, -290.0f);
}

void TestSdfGeometry::testStraightEdges() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;

    // Ponto na aresta direita reta
    float distX = evaluateSurfaceSDF(QVector2D(halfSize.x(), 0.0f), halfSize, radii, slant);
    QVERIFY2(std::abs(distX) < 1.0f, "Aresta direita reta deve ter distância próxima de 0");

    // Ponto na aresta superior reta
    float distY = evaluateSurfaceSDF(QVector2D(0.0f, halfSize.y()), halfSize, radii, slant);
    QVERIFY2(std::abs(distY) < 1.0f, "Aresta superior reta deve ter distância próxima de 0");
}

void TestSdfGeometry::testExternalCorner() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(24.0f, 24.0f, 24.0f, 24.0f);
    const float slant = 0.0f;

    // Ponto externo à quina
    float dist = evaluateSurfaceSDF(QVector2D(halfSize.x() + 10.0f, halfSize.y() + 10.0f),
                                    halfSize, radii, slant);
    QVERIFY2(dist > 0.0f, "Ponto exterior às quinas deve ter distância estritamente positiva");
}

void TestSdfGeometry::testCurvatureContinuity() {
    const QVector2D halfSize(430.0f, 290.0f);
    const QVector4D radii(48.0f, 48.0f, 48.0f, 48.0f);
    const float slant = 0.0f;

    // Amostragem radial ao longo do canto superior direito (de 0 a 90 graus)
    const QVector2D cornerCenter(halfSize.x() - 48.0f, halfSize.y() - 48.0f);
    const float testRadius = 48.0f;

    float prevDist = 0.0f;
    for (int step = 0; step <= 90; ++step) {
        float angle = static_cast<float>(step) * (3.14159265f / 180.0f);
        QVector2D samplePoint(cornerCenter.x() + std::cos(angle) * testRadius,
                              cornerCenter.y() + std::sin(angle) * testRadius);

        float currentDist = evaluateSurfaceSDF(samplePoint, halfSize, radii, slant);

        if (step > 0) {
            float delta = std::abs(currentDist - prevDist);
            // Continuidade: não pode haver saltos bruscos entre passos adjacentes de 1 grau
            QVERIFY2(delta < 2.0f, "Descontinuidade detectada na curvatura do canto G2");
        }
        prevDist = currentDist;
    }
}

QTEST_MAIN(TestSdfGeometry)
#include "tst_SdfGeometry.moc"
