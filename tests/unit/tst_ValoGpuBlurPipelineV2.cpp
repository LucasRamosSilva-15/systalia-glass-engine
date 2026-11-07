#include <QTest>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QImage>
#include <QColor>
#include <cmath>
#include <memory>

#include <rhi/qrhi.h>

#include "ValoGpuBlurPipelineV2.hpp"
#include "ValoBlurPipelineV2.hpp"

using namespace Systalia::Gfx::V2;

class tst_ValoGpuBlurPipelineV2 : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testResourceAllocationAndPooling();
    void testPerformanceBypassZeroPasses();
    void testMultipassBalancedExecution();
    void testMultipassQualityExecution();
    void testImpulseSpreadAndEnergyConservation();
    void testDynamicResolutionResize();

private:
    std::unique_ptr<QOffscreenSurface> m_fallbackSurface;
    std::unique_ptr<QRhi> m_rhi;
};

void tst_ValoGpuBlurPipelineV2::initTestCase() {
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QRhiGles2InitParams params;
    m_fallbackSurface.reset(QRhiGles2InitParams::newFallbackSurface());
    params.fallbackSurface = m_fallbackSurface.get();

    m_rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
    QVERIFY2(m_rhi != nullptr, "QRhi::OpenGLES2 must initialize with physical GPU");
}

void tst_ValoGpuBlurPipelineV2::cleanupTestCase() {
    m_rhi.reset();
    m_fallbackSurface.reset();
}

void tst_ValoGpuBlurPipelineV2::testResourceAllocationAndPooling() {
    ValoGpuBlurPipelineV2 pipeline;
    const QSize size(640, 360);

    // 1. Performance: sem texturas alocadas
    QVERIFY(pipeline.prepare(m_rhi.get(), size, BlurProfile::Performance));
    QVERIFY(pipeline.isPrepared());
    QCOMPARE(pipeline.blurredSize(), size);

    // 2. Balanced: aloca 2 down + 2 up
    QVERIFY(pipeline.prepare(m_rhi.get(), size, BlurProfile::Balanced));
    QVERIFY(pipeline.isPrepared());

    // 3. Re-preparar com os mesmos parâmetros deve reutilizar a pool sem recriação
    QVERIFY(pipeline.prepare(m_rhi.get(), size, BlurProfile::Balanced));
    QVERIFY(pipeline.isPrepared());

    // 4. Quality: aloca 4 down + 4 up
    QVERIFY(pipeline.prepare(m_rhi.get(), size, BlurProfile::Quality));
    QVERIFY(pipeline.isPrepared());

    pipeline.releaseResources();
    QVERIFY(!pipeline.isPrepared());
}

void tst_ValoGpuBlurPipelineV2::testPerformanceBypassZeroPasses() {
    ValoGpuBlurPipelineV2 pipeline;
    const QSize size(128, 128);

    QImage input(size, QImage::Format_RGBA8888_Premultiplied);
    input.fill(QColor(40, 80, 160, 255));

    const QImage output = pipeline.filterOfflineGpu(m_rhi.get(), input, BlurProfile::Performance);
    QCOMPARE(output.size(), size);
    QVERIFY(!output.isNull());

    // No modo Performance, a imagem deve ser idêntica (bit a bit / pixel a pixel)
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            QCOMPARE(output.pixelColor(x, y), input.pixelColor(x, y));
        }
    }
}

void tst_ValoGpuBlurPipelineV2::testMultipassBalancedExecution() {
    ValoGpuBlurPipelineV2 pipeline;
    const QSize size(128, 128);

    // Cria imagem com transição brusca de cores (degrau de luminância)
    QImage input(size, QImage::Format_RGBA8888_Premultiplied);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            const int val = (x < size.width() / 2) ? 20 : 220;
            input.setPixelColor(x, y, QColor(val, val, val, 255));
        }
    }

    const QImage output = pipeline.filterOfflineGpu(m_rhi.get(), input, BlurProfile::Balanced);
    QCOMPARE(output.size(), size);
    QVERIFY(!output.isNull());

    // O pixel na borda do degrau (x = 64) deve ser atenuado suavemente
    const QColor edgeBefore1 = input.pixelColor(63, 64);
    const QColor edgeBefore2 = input.pixelColor(64, 64);
    const QColor edgeAfter = output.pixelColor(64, 64);

    QCOMPARE(edgeBefore1.red(), 20);
    QCOMPARE(edgeBefore2.red(), 220);
    // Transição suave
    QVERIFY(edgeAfter.red() > 40 && edgeAfter.red() < 200);
}

void tst_ValoGpuBlurPipelineV2::testMultipassQualityExecution() {
    ValoGpuBlurPipelineV2 pipeline;
    const QSize size(128, 128);

    // Impulso unitário central 2x2
    QImage input(size, QImage::Format_RGBA8888_Premultiplied);
    input.fill(Qt::black);
    const int cx = size.width() / 2;
    const int cy = size.height() / 2;
    input.setPixelColor(cx, cy, Qt::white);
    input.setPixelColor(cx + 1, cy, Qt::white);
    input.setPixelColor(cx, cy + 1, Qt::white);
    input.setPixelColor(cx + 1, cy + 1, Qt::white);

    const QImage gpuBlurred = pipeline.filterOfflineGpu(m_rhi.get(), input, BlurProfile::Quality);
    QCOMPARE(gpuBlurred.size(), size);
    QVERIFY(!gpuBlurred.isNull());

    // Referência matemática CPU
    const QImage cpuBlurred = ValoBlurPipelineV2::filterOffline(input, BlurProfile::Quality);
    QCOMPARE(cpuBlurred.size(), size);

    // Calcula MSE e PSNR entre a execução GPU multipass e a referência CPU
    double mse = 0.0;
    const int totalPixels = size.width() * size.height();
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            const QColor cGpu = gpuBlurred.pixelColor(x, y);
            const QColor cCpu = cpuBlurred.pixelColor(x, y);
            const double diff = static_cast<double>(cGpu.red() - cCpu.red());
            mse += diff * diff;
        }
    }
    mse /= static_cast<double>(totalPixels);
    const double psnr = (mse > 1e-6) ? (10.0 * std::log10((255.0 * 255.0) / mse)) : 99.0;

    // PSNR entre GPU e CPU reference deve ser alto (> 30 dB) demonstrando equivalência matemática
    QVERIFY2(psnr > 30.0, qPrintable(QString("GPU vs CPU PSNR is %1 dB (expected > 30 dB)").arg(psnr)));
}

void tst_ValoGpuBlurPipelineV2::testImpulseSpreadAndEnergyConservation() {
    ValoGpuBlurPipelineV2 pipeline;
    const QSize size(128, 128);

    // 1. Conservação de energia fotométrica em bloco central uniforme
    QImage block(size, QImage::Format_RGBA8888_Premultiplied);
    block.fill(QColor(60, 140, 220, 255));

    quint64 sumOriginalLuminance = 0;
    for (int y = 32; y < 96; ++y) {
        for (int x = 32; x < 96; ++x) {
            const QColor c = block.pixelColor(x, y);
            sumOriginalLuminance += c.red() + c.green() + c.blue();
        }
    }

    const QImage blurredBlock = pipeline.filterOfflineGpu(m_rhi.get(), block, BlurProfile::Balanced);

    quint64 sumBlurredLuminance = 0;
    for (int y = 32; y < 96; ++y) {
        for (int x = 32; x < 96; ++x) {
            const QColor c = blurredBlock.pixelColor(x, y);
            sumBlurredLuminance += c.red() + c.green() + c.blue();
        }
    }

    const double energyRatio = static_cast<double>(sumBlurredLuminance) / static_cast<double>(sumOriginalLuminance);
    QVERIFY2(std::abs(energyRatio - 1.0) < 0.05,
             qPrintable(QString("Energy ratio in uniform region: %1 (expected ~1.0)").arg(energyRatio)));

    // 2. Verificação de espalhamento de impulso estritamente positivo
    QImage impulse(size, QImage::Format_RGBA8888_Premultiplied);
    impulse.fill(Qt::black);
    const int cx = size.width() / 2;
    const int cy = size.height() / 2;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            impulse.setPixelColor(cx + dx, cy + dy, Qt::white);
        }
    }

    const QImage blurredImpulse = pipeline.filterOfflineGpu(m_rhi.get(), impulse, BlurProfile::Quality);
    const float spread = ValoBlurPipelineV2::measureImpulseSpread(blurredImpulse);
    QVERIFY2(spread > 10.0f, qPrintable(QString("Impulse spread was %1 px (expected > 10 px)").arg(spread)));
}

void tst_ValoGpuBlurPipelineV2::testDynamicResolutionResize() {
    ValoGpuBlurPipelineV2 pipeline;

    // Começa em 320x180
    const QSize sz1(320, 180);
    QImage img1(sz1, QImage::Format_RGBA8888_Premultiplied);
    img1.fill(Qt::blue);
    QImage out1 = pipeline.filterOfflineGpu(m_rhi.get(), img1, BlurProfile::Balanced);
    QCOMPARE(out1.size(), sz1);

    // Redimensiona para 640x360
    const QSize sz2(640, 360);
    QImage img2(sz2, QImage::Format_RGBA8888_Premultiplied);
    img2.fill(Qt::green);
    QImage out2 = pipeline.filterOfflineGpu(m_rhi.get(), img2, BlurProfile::Balanced);
    QCOMPARE(out2.size(), sz2);

    // Redimensiona para 1280x720
    const QSize sz3(1280, 720);
    QImage img3(sz3, QImage::Format_RGBA8888_Premultiplied);
    img3.fill(Qt::red);
    QImage out3 = pipeline.filterOfflineGpu(m_rhi.get(), img3, BlurProfile::Balanced);
    QCOMPARE(out3.size(), sz3);
}

QTEST_MAIN(tst_ValoGpuBlurPipelineV2)
#include "tst_ValoGpuBlurPipelineV2.moc"
