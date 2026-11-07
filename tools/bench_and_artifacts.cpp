#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QImage>
#include <QColor>
#include <QElapsedTimer>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

#include <rhi/qrhi.h>

#include "ValoGpuBlurPipelineV2.hpp"
#include "ValoBlurPipelineV2.hpp"
#include "ValoBackdropProviderV2.hpp"

using namespace Systalia::Gfx::V2;

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    QRhiGles2InitParams params;
    std::unique_ptr<QOffscreenSurface> fallbackSurface(QRhiGles2InitParams::newFallbackSurface());
    params.fallbackSurface = fallbackSurface.get();

    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::OpenGLES2, &params));
    if (!rhi) {
        std::cerr << "Failed to initialize QRhi\n";
        return 1;
    }

    std::cout << "=== SYSTALIA VALO GRAPHICS 2.0 | GPU DUAL-KAWASE BENCHMARK ===" << std::endl;
    std::cout << "GPU: " << rhi->driverInfo().deviceName.toStdString() << std::endl;
    std::cout << "Backend: " << rhi->backendName() << std::endl;

    const std::vector<QSize> resolutions = {
        QSize(640, 360),
        QSize(1280, 720),
        QSize(1920, 1080)
    };

    const std::vector<std::pair<QString, BlurProfile>> profiles = {
        { "Performance (0 passes)", BlurProfile::Performance },
        { "Balanced (4 passes)",    BlurProfile::Balanced },
        { "Quality (8 passes)",     BlurProfile::Quality }
    };

    ValoGpuBlurPipelineV2 pipeline;

    for (const auto &res : resolutions) {
        std::cout << "\n--- Resolution: " << res.width() << "x" << res.height() << " ---" << std::endl;

        const QImage input = ValoBackdropProviderV2::createSyntheticBackdrop(res);
        const QImage formatted = input.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

        QRhiTexture *srcTex = rhi->newTexture(QRhiTexture::RGBA8, res, 1, QRhiTexture::UsedAsTransferSource);
        srcTex->create();
        QRhiResourceUpdateBatch *initBatch = rhi->nextResourceUpdateBatch();
        initBatch->uploadTexture(srcTex, formatted);

        for (const auto &prof : profiles) {
            pipeline.prepare(rhi.get(), res, prof.second);

            // Warmup
            QRhiCommandBuffer *cb = nullptr;
            rhi->beginOffscreenFrame(&cb);
            pipeline.execute(cb, srcTex, prof.second, initBatch);
            initBatch = nullptr;
            rhi->endOffscreenFrame();
            rhi->finish();

            // Benchmark: 50 iterações na GPU
            const int iterations = 50;
            QElapsedTimer timer;
            timer.start();

            for (int i = 0; i < iterations; ++i) {
                rhi->beginOffscreenFrame(&cb);
                pipeline.execute(cb, srcTex, prof.second);
                rhi->endOffscreenFrame();
                rhi->finish();
            }

            const qint64 totalNsecs = timer.nsecsElapsed();
            const double avgMs = (double(totalNsecs) / 1e6) / iterations;
            const double fps = 1000.0 / avgMs;

            std::cout << "  Profile: " << std::left << std::setw(25) << prof.first.toStdString()
                      << " Avg: " << std::fixed << std::setprecision(3) << avgMs << " ms"
                      << " (" << std::setprecision(1) << fps << " FPS)" << std::endl;
        }

        delete srcTex;
    }

    std::cout << "\n=== GENERATING VISUAL GPU ARTIFACTS ===" << std::endl;
    const QString artifactDir = "/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f";

    // 1. Imagem real de backdrop 960x540
    const QSize artifactSize(960, 540);
    const QImage synthBackdrop = ValoBackdropProviderV2::createSyntheticBackdrop(artifactSize);

    // 2. GPU Quality Blur
    const QImage gpuQuality = pipeline.filterOfflineGpu(rhi.get(), synthBackdrop, BlurProfile::Quality);
    const QString qualityPath = artifactDir + "/valo_v2_gpu_blur_quality.png";
    gpuQuality.save(qualityPath);
    std::cout << "Saved: " << qualityPath.toStdString() << " (" << gpuQuality.width() << "x" << gpuQuality.height() << ")" << std::endl;

    // 3. GPU Balanced Blur
    const QImage gpuBalanced = pipeline.filterOfflineGpu(rhi.get(), synthBackdrop, BlurProfile::Balanced);
    const QString balancedPath = artifactDir + "/valo_v2_gpu_blur_balanced.png";
    gpuBalanced.save(balancedPath);
    std::cout << "Saved: " << balancedPath.toStdString() << " (" << gpuBalanced.width() << "x" << gpuBalanced.height() << ")" << std::endl;

    // 4. Diferença GPU vs CPU (Quality)
    const QImage cpuQuality = ValoBlurPipelineV2::filterOffline(synthBackdrop, BlurProfile::Quality);
    QImage diffImage(artifactSize, QImage::Format_ARGB32_Premultiplied);
    diffImage.fill(Qt::black);

    double maxDiff = 0.0;
    double sumDiff = 0.0;
    for (int y = 0; y < artifactSize.height(); ++y) {
        for (int x = 0; x < artifactSize.width(); ++x) {
            const QColor cGpu = gpuQuality.pixelColor(x, y);
            const QColor cCpu = cpuQuality.pixelColor(x, y);
            const int dr = std::abs(cGpu.red() - cCpu.red());
            const int dg = std::abs(cGpu.green() - cCpu.green());
            const int db = std::abs(cGpu.blue() - cCpu.blue());
            const int dMax = std::max({ dr, dg, db });
            if (dMax > maxDiff) maxDiff = dMax;
            sumDiff += dMax;

            // Amplificação de 10x para visualização clara de diferenças sutis de arredondamento
            const int ampR = std::min(255, dr * 10);
            const int ampG = std::min(255, dg * 10);
            const int ampB = std::min(255, db * 10);
            diffImage.setPixelColor(x, y, QColor(ampR, ampG, ampB, 255));
        }
    }
    const double meanDiff = sumDiff / (artifactSize.width() * artifactSize.height());
    const QString diffPath = artifactDir + "/valo_v2_gpu_vs_cpu_diff.png";
    diffImage.save(diffPath);
    std::cout << "Saved: " << diffPath.toStdString() << " [Max Diff: " << maxDiff << ", Mean Diff: " << meanDiff << "]" << std::endl;

    // 5. GPU Impulse Response
    const QSize impulseSize(256, 256);
    QImage impulse(impulseSize, QImage::Format_RGBA8888_Premultiplied);
    impulse.fill(Qt::black);
    const int cx = impulseSize.width() / 2;
    const int cy = impulseSize.height() / 2;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            impulse.setPixelColor(cx + dx, cy + dy, Qt::white);
        }
    }

    const QImage gpuImpulse = pipeline.filterOfflineGpu(rhi.get(), impulse, BlurProfile::Quality);
    const QString impulsePath = artifactDir + "/valo_v2_gpu_impulse_response.png";
    gpuImpulse.save(impulsePath);
    std::cout << "Saved: " << impulsePath.toStdString() << std::endl;

    std::cout << "\nAll GPU benchmarks and visual artifacts completed successfully!" << std::endl;
    return 0;
}
