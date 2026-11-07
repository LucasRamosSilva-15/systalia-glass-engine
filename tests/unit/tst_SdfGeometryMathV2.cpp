#include <QTest>
#include <QVector2D>
#include <QVector4D>
#include <cmath>
#include <algorithm>

#include "v2/ValoGeometryEngineV2.hpp"

using namespace Systalia::Gfx::V2;

class TestSdfGeometryMathV2 final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void unroundedRectangleExactDistances();
    void symmetricalCircularCornersL2Exact();
    void fourIndependentAsymmetricRadii();
    void excessiveRadiiNormalizationCssLevel3();
    void pointClassificationInsideBorderOutside();
    void surfaceNormalsAreUnitLengthAndOutward();
    void degenerateGeometriesHandledSafely();
    void squircleG2CurvatureContinuity();
    void analyticalAccuracyStrictlyLessThanPointZeroOnePx();
    void explicitModeSelectionConsistency();
};

void TestSdfGeometryMathV2::initTestCase() {
}

void TestSdfGeometryMathV2::unroundedRectangleExactDistances() {
    const QVector2D size(400.0f, 200.0f); // halfSize = (200, 100)
    const QVector4D zeroRadii(0.0f, 0.0f, 0.0f, 0.0f);

    // Centro da forma: ponto mais interior (-min(halfSize.x, halfSize.y))
    const float distCenter = ValoGeometryEngineV2::evaluateSignedDistanceL2(
        QVector2D(0.0f, 0.0f), size, zeroRadii);
    QCOMPARE(distCenter, -100.0f);

    // Ponto sobre a borda direita (x = 200, y = 0)
    const float distEdgeRight = ValoGeometryEngineV2::evaluateSignedDistanceL2(
        QVector2D(200.0f, 0.0f), size, zeroRadii);
    QCOMPARE(distEdgeRight, 0.0f);

    // Ponto sobre a borda superior (x = 0, y = -100)
    const float distEdgeTop = ValoGeometryEngineV2::evaluateSignedDistanceL2(
        QVector2D(0.0f, -100.0f), size, zeroRadii);
    QCOMPARE(distEdgeTop, 0.0f);

    // Ponto sobre o vértice superior direito (x = 200, y = -100)
    const float distCorner = ValoGeometryEngineV2::evaluateSignedDistanceL2(
        QVector2D(200.0f, -100.0f), size, zeroRadii);
    QCOMPARE(distCorner, 0.0f);

    // Ponto exterior à direita (x = 215, y = 0) -> dist = +15
    const float distOutsideRight = ValoGeometryEngineV2::evaluateSignedDistanceL2(
        QVector2D(215.0f, 0.0f), size, zeroRadii);
    QCOMPARE(distOutsideRight, 15.0f);

    // Ponto exterior diagonal ao vértice (x = 203, y = -104) -> dist = sqrt(3^2 + 4^2) = 5
    const float distOutsideDiag = ValoGeometryEngineV2::evaluateSignedDistanceL2(
        QVector2D(203.0f, -104.0f), size, zeroRadii);
    QCOMPARE(distOutsideDiag, 5.0f);
}

void TestSdfGeometryMathV2::symmetricalCircularCornersL2Exact() {
    // Geometria da Dock VALO: 555 x 125, raio 47 px
    const QVector2D size(555.0f, 125.0f);
    const float r = 47.0f;
    const QVector4D radii(r, r, r, r);
    const float bx = 555.0f * 0.5f;
    const float by = 125.0f * 0.5f;

    // Centro do arco do canto superior direito
    const QVector2D arcCenter(bx - r, -(by - r));

    // Testar pontos em um círculo concêntrico ao arco
    // Para qualquer ponto a um ângulo theta com raio d = r + offset:
    for (int deg = 0; deg <= 90; deg += 15) {
        const float rad = deg * (3.14159265358979323846f / 180.0f);
        const float cosT = std::cos(rad);
        const float sinT = std::sin(rad);

        // Ponto sobre a borda circular (offset = 0)
        const QVector2D pBorder = arcCenter + QVector2D(r * cosT, -r * sinT);
        const float distBorder = ValoGeometryEngineV2::evaluateSignedDistanceL2(pBorder, size, radii);
        QVERIFY2(std::abs(distBorder) < 0.0001f, qPrintable(QString::number(distBorder)));

        // Ponto externo (offset = +12.5 px)
        const QVector2D pOutside = arcCenter + QVector2D((r + 12.5f) * cosT, -(r + 12.5f) * sinT);
        const float distOutside = ValoGeometryEngineV2::evaluateSignedDistanceL2(pOutside, size, radii);
        QVERIFY2(std::abs(distOutside - 12.5f) < 0.0001f, qPrintable(QString::number(distOutside)));

        // Ponto interno ao canto circular (offset = -8.0 px)
        const QVector2D pInside = arcCenter + QVector2D((r - 8.0f) * cosT, -(r - 8.0f) * sinT);
        const float distInside = ValoGeometryEngineV2::evaluateSignedDistanceL2(pInside, size, radii);
        QVERIFY2(std::abs(distInside - (-8.0f)) < 0.0001f, qPrintable(QString::number(distInside)));
    }
}

void TestSdfGeometryMathV2::fourIndependentAsymmetricRadii() {
    const QVector2D size(400.0f, 300.0f); // bx = 200, by = 150
    // TL = 10, TR = 20, BL = 30, BR = 40
    const QVector4D radii(10.0f, 20.0f, 30.0f, 40.0f);

    // Canto Top-Left (x < 0, y < 0) -> raio 10
    const QVector2D cTL(-200.0f + 10.0f, -150.0f + 10.0f);
    const QVector2D pTL = cTL + QVector2D(-10.0f, 0.0f); // vértice mais à esquerda
    QCOMPARE(ValoGeometryEngineV2::selectRadius(QVector2D(-100.0f, -100.0f), radii), 10.0f);
    QVERIFY(std::abs(ValoGeometryEngineV2::evaluateSignedDistanceL2(pTL, size, radii)) < 0.0001f);

    // Canto Top-Right (x > 0, y < 0) -> raio 20
    const QVector2D cTR(200.0f - 20.0f, -150.0f + 20.0f);
    const QVector2D pTR = cTR + QVector2D(20.0f, 0.0f);
    QCOMPARE(ValoGeometryEngineV2::selectRadius(QVector2D(100.0f, -100.0f), radii), 20.0f);
    QVERIFY(std::abs(ValoGeometryEngineV2::evaluateSignedDistanceL2(pTR, size, radii)) < 0.0001f);

    // Canto Bottom-Left (x < 0, y > 0) -> raio 30
    const QVector2D cBL(-200.0f + 30.0f, 150.0f - 30.0f);
    const QVector2D pBL = cBL + QVector2D(-30.0f, 0.0f);
    QCOMPARE(ValoGeometryEngineV2::selectRadius(QVector2D(-100.0f, 100.0f), radii), 30.0f);
    QVERIFY(std::abs(ValoGeometryEngineV2::evaluateSignedDistanceL2(pBL, size, radii)) < 0.0001f);

    // Canto Bottom-Right (x > 0, y > 0) -> raio 40
    const QVector2D cBR(200.0f - 40.0f, 150.0f - 40.0f);
    const QVector2D pBR = cBR + QVector2D(40.0f, 0.0f);
    QCOMPARE(ValoGeometryEngineV2::selectRadius(QVector2D(100.0f, 100.0f), radii), 40.0f);
    QVERIFY(std::abs(ValoGeometryEngineV2::evaluateSignedDistanceL2(pBR, size, radii)) < 0.0001f);
}

void TestSdfGeometryMathV2::excessiveRadiiNormalizationCssLevel3() {
    const QVector2D size(100.0f, 100.0f);
    // Raios nominais excessivos (cada canto com 80 px num retângulo de 100x100)
    // Soma no topo: 80 + 80 = 160 > 100 -> fator w/sum = 100/160 = 0.625
    // Raio escalado: 80 * 0.625 = 50.0 px (exatamente metade do lado)
    const QVector4D oversized(80.0f, 80.0f, 80.0f, 80.0f);

    const QVector4D normalized = ValoGeometryEngineV2::normalizeRadii(size, oversized);
    QCOMPARE(normalized.x(), 50.0f);
    QCOMPARE(normalized.y(), 50.0f);
    QCOMPARE(normalized.z(), 50.0f);
    QCOMPARE(normalized.w(), 50.0f);

    // Se tamanho for degenerado (<= 0), raios devem ser zerados
    const QVector4D zeroNorm = ValoGeometryEngineV2::normalizeRadii(QVector2D(0.0f, 100.0f), oversized);
    QCOMPARE(zeroNorm, QVector4D(0.0f, 0.0f, 0.0f, 0.0f));
}

void TestSdfGeometryMathV2::pointClassificationInsideBorderOutside() {
    const QVector2D size(200.0f, 100.0f);
    const QVector4D radii(10.0f, 10.0f, 10.0f, 10.0f);

    // Ponto no centro -> Inside
    QCOMPARE(ValoGeometryEngineV2::classifyPoint(QVector2D(0.0f, 0.0f), size, radii, GeometryMode::CircularL2),
             PointClassification::Inside);

    // Ponto exatamente na borda -> Border
    QCOMPARE(ValoGeometryEngineV2::classifyPoint(QVector2D(100.0f, 0.0f), size, radii, GeometryMode::CircularL2),
             PointClassification::Border);

    // Ponto subpixel a 0.2px da borda (com tolerância padrão de 0.5px) -> Border
    QCOMPARE(ValoGeometryEngineV2::classifyPoint(QVector2D(100.2f, 0.0f), size, radii, GeometryMode::CircularL2),
             PointClassification::Border);

    // Ponto exterior a 5px -> Outside
    QCOMPARE(ValoGeometryEngineV2::classifyPoint(QVector2D(105.0f, 0.0f), size, radii, GeometryMode::CircularL2),
             PointClassification::Outside);
}

void TestSdfGeometryMathV2::surfaceNormalsAreUnitLengthAndOutward() {
    const QVector2D size(300.0f, 200.0f);
    const QVector4D radii(25.0f, 25.0f, 25.0f, 25.0f);

    // Normal na borda direita: deve apontar para (+1, 0)
    const QVector2D normRight = ValoGeometryEngineV2::evaluateNormal(
        QVector2D(150.0f, 0.0f), size, radii, GeometryMode::CircularL2);
    QVERIFY(std::abs(normRight.x() - 1.0f) < 0.01f);
    QVERIFY(std::abs(normRight.y()) < 0.01f);
    QVERIFY(std::abs(normRight.length() - 1.0f) < 0.001f);

    // Normal na borda superior: deve apontar para (0, -1)
    const QVector2D normTop = ValoGeometryEngineV2::evaluateNormal(
        QVector2D(0.0f, -100.0f), size, radii, GeometryMode::CircularL2);
    QVERIFY(std::abs(normTop.x()) < 0.01f);
    QVERIFY(std::abs(normTop.y() - (-1.0f)) < 0.01f);
    QVERIFY(std::abs(normTop.length() - 1.0f) < 0.001f);

    // Normal no canto em 45 graus: deve ter norma unitária
    const QVector2D pCorner(150.0f, -100.0f);
    const QVector2D normCorner = ValoGeometryEngineV2::evaluateNormal(
        pCorner, size, radii, GeometryMode::CircularL2);
    QVERIFY(std::abs(normCorner.length() - 1.0f) < 0.01f);
    QVERIFY(normCorner.x() > 0.5f);
    QVERIFY(normCorner.y() < -0.5f);
}

void TestSdfGeometryMathV2::degenerateGeometriesHandledSafely() {
    const QVector4D radii(10.0f, 10.0f, 10.0f, 10.0f);

    // Largura ou altura zero não devem travar nem gerar NaN
    const float distZeroW = ValoGeometryEngineV2::evaluateSignedDistanceL2(
        QVector2D(10.0f, 10.0f), QVector2D(0.0f, 100.0f), radii);
    QVERIFY(std::isinf(distZeroW));

    const float distZeroH = ValoGeometryEngineV2::evaluateSignedDistanceG2(
        QVector2D(10.0f, 10.0f), QVector2D(100.0f, -5.0f), radii);
    QVERIFY(std::isinf(distZeroH));
}

void TestSdfGeometryMathV2::squircleG2CurvatureContinuity() {
    const QVector2D size(400.0f, 200.0f);
    const float r = 20.0f;
    const QVector4D radii(r, r, r, r);

    // Amostrar a transição do topo reto para o canto G2:
    // Com width=400 (bx=200), o topo reto termina em x = bx - r = 180.
    // Avaliar a derivada da distância ao longo de x cruzando x = 180 a y = -100 (-by):
    float prevDist = ValoGeometryEngineV2::evaluateSignedDistanceG2(QVector2D(170.0f, -100.0f), size, radii);
    for (float x = 171.0f; x <= 190.0f; x += 1.0f) {
        const float curDist = ValoGeometryEngineV2::evaluateSignedDistanceG2(QVector2D(x, -100.0f), size, radii);
        // A função deve ser estritamente contínua sem saltos
        const float delta = std::abs(curDist - prevDist);
        QVERIFY2(delta < 0.5f, qPrintable(QString("Salto em x=%1: %2").arg(x).arg(delta)));
        prevDist = curDist;
    }
}

void TestSdfGeometryMathV2::analyticalAccuracyStrictlyLessThanPointZeroOnePx() {
    // Referência geométrica euclidiana independente:
    // Para um rounded box com cantos circulares de raio R, a distância analítica exata
    // de um ponto (x, y) no primeiro quadrante é dada pelo modelo vetorial:
    const QVector2D size(300.0f, 180.0f);
    const float r = 35.0f;
    const QVector4D radii(r, r, r, r);
    const float bx = 150.0f;
    const float by = 90.0f;
    const QVector2D arcCenter(bx - r, by - r);

    auto groundTruthL2 = [=](float x, float y) -> float {
        const float px = std::abs(x);
        const float py = std::abs(y);
        const float qx = px - (bx - r);
        const float qy = py - (by - r);

        if (qx > 0.0f && qy > 0.0f) {
            // Região de transição do canto circular: distância exata ao arco
            return std::sqrt(qx * qx + qy * qy) - r;
        } else if (qx > 0.0f) {
            // Região vertical reta: distância ao bordo vertical
            return qx - r; // = px - bx
        } else if (qy > 0.0f) {
            // Região horizontal reta: distância ao bordo horizontal
            return qy - r; // = py - by
        } else {
            // Região interna: distância ao bordo mais próximo (valor negativo)
            return std::max(qx, qy) - r; // = std::max(px - bx, py - by)
        }
    };

    // Amostrar uma grade densa de 20x20 = 400 pontos cobrindo interior, borda e exterior
    float maxError = 0.0f;
    for (float x = 0.0f; x <= 220.0f; x += 11.0f) {
        for (float y = 0.0f; y <= 150.0f; y += 8.0f) {
            const float engineVal = ValoGeometryEngineV2::evaluateSignedDistanceL2(QVector2D(x, y), size, radii);
            const float groundTruth = groundTruthL2(x, y);
            const float err = std::abs(engineVal - groundTruth);
            maxError = std::max(maxError, err);

            // Exigência arquitetural do handover: erro analítico < 0.01 px
            QVERIFY2(err < 0.01f, qPrintable(QString("Erro em (%1, %2): %3 px").arg(x).arg(y).arg(err)));
        }
    }

    // Comprovação estatística registrada
    QVERIFY2(maxError < 0.001f, qPrintable(QString("Erro analítico máximo medido: %1 px").arg(maxError)));
}

void TestSdfGeometryMathV2::explicitModeSelectionConsistency() {
    const QVector2D size(200.0f, 100.0f);
    const QVector4D radii(15.0f, 15.0f, 15.0f, 15.0f);
    const QVector2D p(95.0f, 45.0f);

    const float l2Explicit = ValoGeometryEngineV2::evaluateSignedDistance(p, size, radii, GeometryMode::CircularL2);
    const float l2Direct = ValoGeometryEngineV2::evaluateSignedDistanceL2(p, size, radii);
    QCOMPARE(l2Explicit, l2Direct);

    const float g2Explicit = ValoGeometryEngineV2::evaluateSignedDistance(p, size, radii, GeometryMode::SquircleG2);
    const float g2Direct = ValoGeometryEngineV2::evaluateSignedDistanceG2(p, size, radii);
    QCOMPARE(g2Explicit, g2Direct);
}

QTEST_MAIN(TestSdfGeometryMathV2)
#include "tst_SdfGeometryMathV2.moc"

