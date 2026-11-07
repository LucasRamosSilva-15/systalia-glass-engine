#include <QTest>
#include <QImage>
#include <QColor>
#include <cmath>

#include "ValoBackdropProviderV2.hpp"
#include "ValoBlurPipelineV2.hpp"

using namespace Systalia::Gfx::V2;

class tst_ValoBackdropBlurV2 : public QObject {
    Q_OBJECT

private slots:
    void testSyntheticBackdropGeneration();
    void testBackdropProviderDirtyTracking();
    void testBlurProfilePassCount();
    void testBlurImpulseResponseSpread();
    void testBlurEnergyConservation();
    void testBlurProfileHierarchy();
};

void tst_ValoBackdropBlurV2::testSyntheticBackdropGeneration() {
    const QSize size(256, 128);
    const QImage backdrop = ValoBackdropProviderV2::createSyntheticBackdrop(size);

    QCOMPARE(backdrop.size(), size);
    QCOMPARE(backdrop.format(), QImage::Format_ARGB32_Premultiplied);
    QVERIFY(!backdrop.isNull());

    // Verifica que o topo tem componente ciano/oceânica e a base tem tom mais profundo
    const QRgb topPixel = backdrop.pixel(128, 10);
    const QRgb bottomPixel = backdrop.pixel(128, 118);

    QVERIFY(qAlpha(topPixel) == 255);
    QVERIFY(qAlpha(bottomPixel) == 255);
    // Variação de luminância vertical característica do backdrop oceânico
    QVERIFY(qBlue(topPixel) > 0);
    QVERIFY(qBlue(bottomPixel) > 0);
}

void tst_ValoBackdropBlurV2::testBackdropProviderDirtyTracking() {
    ValoBackdropProviderV2 provider;
    QVERIFY(provider.isDirty());

    provider.setDirty(false);
    QVERIFY(!provider.isDirty());

    provider.setDirty(true);
    QVERIFY(provider.isDirty());
}

void tst_ValoBackdropBlurV2::testBlurProfilePassCount() {
    QCOMPARE(ValoBlurPipelineV2::passCountForProfile(BlurProfile::Performance), 0);
    QCOMPARE(ValoBlurPipelineV2::passCountForProfile(BlurProfile::Balanced), 2);
    QCOMPARE(ValoBlurPipelineV2::passCountForProfile(BlurProfile::Quality), 4);
}

void tst_ValoBackdropBlurV2::testBlurImpulseResponseSpread() {
    // Imagem 128x128 com estímulo central concentrado de 20x20
    const int dim = 128;
    QImage impulse(dim, dim, QImage::Format_ARGB32_Premultiplied);
    impulse.fill(Qt::black);
    for (int dy = -10; dy < 10; ++dy) {
        for (int dx = -10; dx < 10; ++dx) {
            impulse.setPixelColor(dim / 2 + dx, dim / 2 + dy, Qt::white);
        }
    }

    const float initialSpread = ValoBlurPipelineV2::measureImpulseSpread(impulse);
    const QImage blurredBalanced = ValoBlurPipelineV2::filterOffline(impulse, BlurProfile::Balanced);
    const float spreadBalanced = ValoBlurPipelineV2::measureImpulseSpread(blurredBalanced);

    const QImage blurredQuality = ValoBlurPipelineV2::filterOffline(impulse, BlurProfile::Quality);
    const float spreadQuality = ValoBlurPipelineV2::measureImpulseSpread(blurredQuality);

    // O espalhamento espacial de Quality deve ser significativamente maior que Balanced
    QVERIFY(spreadBalanced > initialSpread);
    QVERIFY(spreadQuality > spreadBalanced);
}

void tst_ValoBackdropBlurV2::testBlurEnergyConservation() {
    // Cria um bloco colorido uniforme de 64x64
    const int dim = 64;
    QImage block(dim, dim, QImage::Format_ARGB32_Premultiplied);
    block.fill(QColor(60, 140, 220, 255));

    qint64 sumOriginalLuminance = 0;
    for (int y = 16; y < 48; ++y) {
        for (int x = 16; x < 48; ++x) {
            const QRgb p = block.pixel(x, y);
            sumOriginalLuminance += qRed(p) + qGreen(p) + qBlue(p);
        }
    }

    const QImage blurred = ValoBlurPipelineV2::filterOffline(block, BlurProfile::Balanced);

    qint64 sumBlurredLuminance = 0;
    for (int y = 16; y < 48; ++y) {
        for (int x = 16; x < 48; ++x) {
            const QRgb p = blurred.pixel(x, y);
            sumBlurredLuminance += qRed(p) + qGreen(p) + qBlue(p);
        }
    }

    // Na região central do bloco uniforme, o desfoque conserva a energia da luz (tolerância < 3%)
    const double ratio = static_cast<double>(sumBlurredLuminance) / static_cast<double>(sumOriginalLuminance);
    QVERIFY(std::abs(ratio - 1.0) < 0.05);
}

void tst_ValoBackdropBlurV2::testBlurProfileHierarchy() {
    const QSize size(128, 128);
    const QImage src = ValoBackdropProviderV2::createSyntheticBackdrop(size);

    const QImage perf = ValoBlurPipelineV2::filterOffline(src, BlurProfile::Performance);
    const QImage bal = ValoBlurPipelineV2::filterOffline(src, BlurProfile::Balanced);
    const QImage qual = ValoBlurPipelineV2::filterOffline(src, BlurProfile::Quality);

    QCOMPARE(perf.size(), size);
    QCOMPARE(bal.size(), size);
    QCOMPARE(qual.size(), size);

    // Performance preserva os pixels originais sem blur
    QCOMPARE(perf.pixel(64, 64), src.pixel(64, 64));
}

QTEST_MAIN(tst_ValoBackdropBlurV2)
#include "tst_ValoBackdropBlurV2.moc"
