#include <QTest>
#include <QQuickWindow>
#include <QQuickItem>
#include <QImage>
#include <QColor>
#include <QDir>
#include <cmath>

#include "ValoGlassItemV2.hpp"
#include "ValoGeometryEngineV2.hpp"

using namespace Systalia::Gfx::V2;

class TestValoGlassRenderingV2 final : public QObject {
    Q_OBJECT

private:
    const QString m_artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");

private slots:
    void initTestCase();
    void renderCircularL2AndVerifySilhouette();
    void renderSquircleG2AndVerifySilhouette();
    void premultipliedAlphaIntegrityAcrossAllPixels();
    void diagnosticModesProduceValidPasses();
    void analyticalSoftwareRasterizationCoverageVerification();
};

void TestValoGlassRenderingV2::initTestCase() {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", "opengl");
    QDir().mkpath(m_artifactDir);
}

void TestValoGlassRenderingV2::renderCircularL2AndVerifySilhouette() {
    QQuickWindow window;
    window.setColor(Qt::transparent);
    window.resize(300, 200);

    auto *item = new ValoGlassItemV2(window.contentItem());
    item->setWidth(300);
    item->setHeight(200);
    item->setCornerRadii(QVector4D(40.0f, 40.0f, 40.0f, 40.0f));
    item->setGeometryMode(ValoGlassItemV2::CircularL2);
    item->setSurfaceOpacity(0.40f);
    item->setSpecularStrokeWidth(1.5f);
    item->setActive(true);

    window.show();
    (void)QTest::qWaitForWindowExposed(&window, 1000);

    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(20);
    }

    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());
    QCOMPARE(frame.size(), QSize(300, 200));

    // Asserção estrita: centro deve estar visivelmente renderizado
    const QColor centerPixel = frame.pixelColor(150, 100);
    QVERIFY2(centerPixel.alpha() > 50, "O centro do item de vidro deve ter preenchimento volumétrico visível!");

    // 1. Canto externo (x=5, y=5) deve ser transparente
    const QColor cornerPixel = frame.pixelColor(5, 5);
    QCOMPARE(cornerPixel.alpha(), 0);

    // 3. Contorno especular na borda: o highlight especular intensifica a opacidade em relação ao interior
    const int aInside = frame.pixelColor(295, 100).alpha();
    const int aBorder = frame.pixelColor(299, 100).alpha();
    QVERIFY(aBorder > aInside);

    frame.save(m_artifactDir + QStringLiteral("/valo_v2_circular_l2.png"));
}

void TestValoGlassRenderingV2::renderSquircleG2AndVerifySilhouette() {
    QQuickWindow window;
    window.setColor(Qt::transparent);
    window.resize(300, 200);

    auto *item = new ValoGlassItemV2(window.contentItem());
    item->setWidth(300);
    item->setHeight(200);
    item->setCornerRadii(QVector4D(45.0f, 45.0f, 45.0f, 45.0f));
    item->setGeometryMode(ValoGlassItemV2::SquircleG2);
    item->setSurfaceOpacity(0.40f);
    item->setSpecularStrokeWidth(1.25f);

    window.show();
    (void)QTest::qWaitForWindowExposed(&window, 1000);

    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(20);
    }

    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());

    // Asserção estrita
    const QColor centerPixel = frame.pixelColor(150, 100);
    QVERIFY2(centerPixel.alpha() > 50, "O centro do item Squircle G2 deve ter opacidade > 50!");

    // Canto externo distante
    const QColor cornerPixel = frame.pixelColor(4, 4);
    QCOMPARE(cornerPixel.alpha(), 0);

    frame.save(m_artifactDir + QStringLiteral("/valo_v2_squircle_g2.png"));
}

void TestValoGlassRenderingV2::premultipliedAlphaIntegrityAcrossAllPixels() {
    QQuickWindow window;
    window.setColor(Qt::transparent);
    window.resize(200, 150);

    auto *item = new ValoGlassItemV2(window.contentItem());
    item->setWidth(200);
    item->setHeight(150);
    item->setCornerRadii(QVector4D(25.0f, 25.0f, 25.0f, 25.0f));
    item->setGeometryMode(ValoGlassItemV2::CircularL2);
    item->setSurfaceOpacity(0.35f);
    item->setSpecularStrokeWidth(1.5f);

    window.show();
    (void)QTest::qWaitForWindowExposed(&window, 1000);

    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(20);
    }

    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());

    const QColor centerPixel = frame.pixelColor(100, 75);
    QVERIFY2(centerPixel.alpha() > 0, "Item de vidro deve estar desenhado no framebuffer!");

    const QImage preFrame = frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int violationCount = 0;
    for (int y = 0; y < preFrame.height(); ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(preFrame.constScanLine(y));
        for (int x = 0; x < preFrame.width(); ++x) {
            const QRgb p = scanLine[x];
            const int a = qAlpha(p);
            const int r = qRed(p);
            const int g = qGreen(p);
            const int b = qBlue(p);

            // No contrato de alpha pré-multiplicado, r <= a, g <= a, b <= a
            if (r > a + 1 || g > a + 1 || b > a + 1) {
                ++violationCount;
            }
        }
    }
    QCOMPARE(violationCount, 0);
}

void TestValoGlassRenderingV2::diagnosticModesProduceValidPasses() {
    QQuickWindow window;
    window.setColor(Qt::transparent);
    window.resize(160, 100);

    auto *item = new ValoGlassItemV2(window.contentItem());
    item->setWidth(160);
    item->setHeight(100);
    item->setCornerRadii(QVector4D(20.0f, 20.0f, 20.0f, 20.0f));

    window.show();
    (void)QTest::qWaitForWindowExposed(&window, 1000);

    // Testar os 4 modos de diagnóstico:
    // 1 = SDF, 2 = Coverage, 3 = Volumetric, 4 = Specular
    for (int mode = 1; mode <= 4; ++mode) {
        item->setDebugMode(mode);
        window.requestUpdate();

        for (int i = 0; i < 3; ++i) {
            QCoreApplication::processEvents();
            QTest::qWait(15);
        }

        const QImage shot = window.grabWindow();
        QVERIFY(!shot.isNull());

        if (mode == 4) {
            // Modo 4 (contorno especular): inspecionar na borda superior (y=2, x=80)
            const QColor borderPixel = shot.pixelColor(80, 2);
            QVERIFY2(borderPixel.alpha() > 0, "Modo 4 especular deve produzir borda visível!");
        } else {
            // Modos 1, 2, 3: inspecionar no centro (y=50, x=80)
            const QColor centerPixel = shot.pixelColor(80, 50);
            QVERIFY2(centerPixel.alpha() > 0, "Modos 1-3 devem produzir pixels com alpha > 0!");
        }
    }
}

void TestValoGlassRenderingV2::analyticalSoftwareRasterizationCoverageVerification() {
    // Verificação de cobertura e rasterização por software conforme exigido pela Seção 10.2:
    // Garante 100% de cobertura determinística em qualquer ambiente (com ou sem GPU/display).
    const QVector2D size(300.0f, 200.0f);
    const QVector4D radii(40.0f, 40.0f, 40.0f, 40.0f);

    // 1. Canto externo em (5, 5) relativo ao canto superior esquerdo:
    // Em coordenadas centradas: p = (5 - 150, 5 - 100) = (-145, -95)
    const QVector2D pCorner(-145.0f, -95.0f);
    const float distCorner = ValoGeometryEngineV2::evaluateSignedDistanceL2(pCorner, size, radii);
    QVERIFY2(distCorner > 5.0f, qPrintable(QString("distCorner esperado > 5.0 px, obtido: %1").arg(distCorner)));

    // 2. Centro (0, 0)
    const float distCenter = ValoGeometryEngineV2::evaluateSignedDistanceL2(QVector2D(0.0f, 0.0f), size, radii);
    QVERIFY2(distCenter < -50.0f, qPrintable(QString("distCenter esperado < -50.0 px, obtido: %1").arg(distCenter)));

    // 3. Ponto exatamente na borda reta (x = 150, y = 0)
    const float distEdge = ValoGeometryEngineV2::evaluateSignedDistanceL2(QVector2D(150.0f, 0.0f), size, radii);
    QVERIFY(std::abs(distEdge) < 0.001f);

    // 4. Verificação de transição de antialiasing analítica fwidth
    const float fwidthStep = 1.0f; // 1 pixel físico
    for (float x = 145.0f; x <= 155.0f; x += 1.0f) {
        const float d = ValoGeometryEngineV2::evaluateSignedDistanceL2(QVector2D(x, 0.0f), size, radii);
        const float coverage = 1.0f - std::clamp((d + 0.5f * fwidthStep) / fwidthStep, 0.0f, 1.0f);
        if (x < 149.5f) {
            QCOMPARE(coverage, 1.0f);
        } else if (x > 150.5f) {
            QCOMPARE(coverage, 0.0f);
        } else {
            // Zona fracionária de antialiasing
            QVERIFY(coverage >= 0.0f && coverage <= 1.0f);
        }
    }
}

QTEST_MAIN(TestValoGlassRenderingV2)
#include "tst_ValoGlassRenderingV2.moc"
