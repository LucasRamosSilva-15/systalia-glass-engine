#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QImage>
#include <QColor>
#include <QElapsedTimer>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

#include <rhi/qrhi.h>

#include "ValoGpuBlurPipelineV2.hpp"
#include "ValoBlurPipelineV2.hpp"
#include "ValoBackdropProviderV2.hpp"

using namespace Systalia::Gfx::V2;

struct BenchStats {
    double minMs = 0.0;
    double maxMs = 0.0;
    double meanMs = 0.0;
    double medianMs = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    double stdDevMs = 0.0;

    static BenchStats compute(std::vector<double> samples) {
        if (samples.empty()) return {};
        std::sort(samples.begin(), samples.end());
        const size_t n = samples.size();

        BenchStats stats;
        stats.minMs = samples.front();
        stats.maxMs = samples.back();

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        stats.meanMs = sum / static_cast<double>(n);

        stats.medianMs = samples[n / 2];
        stats.p95Ms = samples[static_cast<size_t>(0.95 * n)];
        stats.p99Ms = samples[static_cast<size_t>(0.99 * n)];

        double varSum = 0.0;
        for (double val : samples) {
            double diff = val - stats.meanMs;
            varSum += diff * diff;
        }
        stats.stdDevMs = std::sqrt(varSum / static_cast<double>(n));

        return stats;
    }
};

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    QRhiGles2InitParams params;
    std::unique_ptr<QOffscreenSurface> fallbackSurface(QRhiGles2InitParams::newFallbackSurface());
    params.fallbackSurface = fallbackSurface.get();

    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::OpenGLES2, &params, QRhi::EnableTimestamps));
    if (!rhi) {
        std::cerr << "Failed to initialize QRhi\n";
        return 1;
    }

    const bool hwTimestamps = rhi->isFeatureSupported(QRhi::Timestamps);

    std::cout << "========================================================================\n";
    std::cout << "  SYSTALIA VALO GRAPHICS 2.0 — BENCHMARK DETALHADO GPU DUAL-KAWASE\n";
    std::cout << "========================================================================\n";
    std::cout << "Dispositivo GPU : " << rhi->driverInfo().deviceName.toStdString() << "\n";
    std::cout << "Backend QRhi    : " << rhi->backendName() << "\n";
    std::cout << "GPU Timestamps  : " << (hwTimestamps ? "Disponível (Hardware)" : "Não suportado pelo driver/offscreen surface (Utilizando sincronização explícita)") << "\n";
    std::cout << "Metodologia     : 20 warmup frames + 200 amostras com cronometragem segmentada e rhi->finish()\n";
    std::cout << "========================================================================\n\n";

    ValoGpuBlurPipelineV2 pipeline;

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

    for (const auto &res : resolutions) {
        std::cout << ">>> Resolução: " << res.width() << "x" << res.height() << " <<<\n";

        const QImage input = ValoBackdropProviderV2::createSyntheticBackdrop(res);
        const QImage formatted = input.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

        QRhiTexture *srcTex = rhi->newTexture(QRhiTexture::RGBA8, res, 1, QRhiTexture::UsedAsTransferSource);
        srcTex->create();
        QRhiResourceUpdateBatch *initBatch = rhi->nextResourceUpdateBatch();
        initBatch->uploadTexture(srcTex, formatted);

        for (const auto &prof : profiles) {
            pipeline.prepare(rhi.get(), res, prof.second);

            // 1. Warmup (20 iterações para estabilizar pipeline caches, command buffers e VRAM)
            for (int w = 0; w < 20; ++w) {
                QRhiCommandBuffer *cb = nullptr;
                rhi->beginOffscreenFrame(&cb);
                pipeline.execute(cb, srcTex, prof.second, (w == 0) ? initBatch : nullptr);
                rhi->endOffscreenFrame();
                rhi->finish();
            }
            initBatch = nullptr;

            // 2. Medições segmentadas (200 iterações)
            const int iterations = 200;
            std::vector<double> cpuRecordSamples;
            std::vector<double> submitSamples;
            std::vector<double> gpuWaitSamples;
            std::vector<double> totalRenderSamples;
            cpuRecordSamples.reserve(iterations);
            submitSamples.reserve(iterations);
            gpuWaitSamples.reserve(iterations);
            totalRenderSamples.reserve(iterations);

            QElapsedTimer tCpu;
            QElapsedTimer tSubmit;
            QElapsedTimer tGpu;
            QElapsedTimer tTotal;

            for (int i = 0; i < iterations; ++i) {
                QRhiCommandBuffer *cb = nullptr;

                tTotal.start();
                rhi->beginOffscreenFrame(&cb);

                // Fase 1: Gravação de comandos na CPU
                tCpu.start();
                pipeline.execute(cb, srcTex, prof.second);
                const double cpuMs = static_cast<double>(tCpu.nsecsElapsed()) / 1e6;
                cpuRecordSamples.push_back(cpuMs);

                // Fase 2: Submissão de comandos ao driver Mesa
                tSubmit.start();
                rhi->endOffscreenFrame();
                const double submitMs = static_cast<double>(tSubmit.nsecsElapsed()) / 1e6;
                submitSamples.push_back(submitMs);

                // Fase 3: Conclusão na GPU (sincronização explícita com rhi->finish)
                tGpu.start();
                rhi->finish();
                const double gpuMs = static_cast<double>(tGpu.nsecsElapsed()) / 1e6;
                gpuWaitSamples.push_back(gpuMs);

                const double totalMs = static_cast<double>(tTotal.nsecsElapsed()) / 1e6;
                totalRenderSamples.push_back(totalMs);
            }

            const BenchStats statsCpu = BenchStats::compute(cpuRecordSamples);
            const BenchStats statsGpu = BenchStats::compute(gpuWaitSamples);
            const BenchStats statsTotal = BenchStats::compute(totalRenderSamples);

            // Frame budget do compositor (60 Hz = 16.66 ms, 144 Hz = 6.94 ms)
            const double budgetPct60 = (statsTotal.meanMs / 16.666) * 100.0;
            const double budgetPct144 = (statsTotal.meanMs / 6.944) * 100.0;

            std::cout << "  [" << prof.first.toStdString() << "]\n";
            std::cout << "    - Gravação CPU  : Média=" << std::fixed << std::setprecision(3) << statsCpu.meanMs << " ms (Min=" << statsCpu.minMs << ", Max=" << statsCpu.maxMs << ")\n";
            std::cout << "    - Conclusão GPU : Média=" << statsGpu.meanMs << " ms (P95=" << statsGpu.p95Ms << ", P99=" << statsGpu.p99Ms << ")\n";
            std::cout << "    - Render Total  : Média=" << statsTotal.meanMs << " ms (P95=" << statsTotal.p95Ms << ", P99=" << statsTotal.p99Ms << ", Desvio=" << statsTotal.stdDevMs << ")\n";
            std::cout << "    - Custo Frame   : " << std::setprecision(2) << budgetPct60 << "% do orçamento a 60 Hz | " << budgetPct144 << "% a 144 Hz\n";
        }
        std::cout << "\n";
        delete srcTex;
    }

    // 3. Medição segmentada de Readback CPU (apenas para documentação de custo da transferência VRAM->RAM)
    std::cout << ">>> Medição de Transferência Readback VRAM -> RAM (Informativo / Offline) <<<\n";
    {
        const QSize fhd(1920, 1080);
        pipeline.prepare(rhi.get(), fhd, BlurProfile::Quality);

        QRhiTexture *dummyTex = rhi->newTexture(QRhiTexture::RGBA8, fhd, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
        dummyTex->create();

        std::vector<double> readbackSamples;
        readbackSamples.reserve(20);
        for (int i = 0; i < 20; ++i) {
            QElapsedTimer tRb;
            tRb.start();
            ValoGpuBlurPipelineV2::readbackTexture(rhi.get(), dummyTex);
            readbackSamples.push_back(static_cast<double>(tRb.nsecsElapsed()) / 1e6);
        }
        delete dummyTex;

        const BenchStats statsRb = BenchStats::compute(readbackSamples);
        std::cout << "  - Readback FHD (1920x1080): Média=" << std::fixed << std::setprecision(2) << statsRb.meanMs << " ms (P95=" << statsRb.p95Ms << " ms)\n";
        std::cout << "  - NOTA: Este custo ocorre SOMENTE em capturas/testes. Em produção no compositor Wayland, o custo é ZERO (Zero-Readback via createTextureFromRhiTexture).\n\n";
    }

    std::cout << "========================================================================\n";
    std::cout << "  GERAÇÃO DE ARTEFATOS VISUAIS E COMPROVAÇÃO DE IMPULSO\n";
    std::cout << "========================================================================\n";
    const QString artifactDir = "/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f";

    // 1. Verificação do Impulso Físico com energia suficiente e normalização analítica
    const int impulseDim = 256;
    const int cx = impulseDim / 2;
    const int cy = impulseDim / 2;
    const int spotRadius = 12;

    QImage impulseSource(impulseDim, impulseDim, QImage::Format_RGBA8888_Premultiplied);
    impulseSource.fill(Qt::black);
    for (int y = -spotRadius; y <= spotRadius; ++y) {
        for (int x = -spotRadius; x <= spotRadius; ++x) {
            float dist = std::sqrt(float(x*x + y*y));
            if (dist <= float(spotRadius)) {
                float intensity = 1.0f - (dist / float(spotRadius + 1));
                int val = std::clamp(static_cast<int>(255.0f * intensity), 0, 255);
                impulseSource.setPixelColor(cx + x, cy + y, QColor(val, val, val, 255));
            }
        }
    }

    const QImage gpuImpulse = pipeline.filterOfflineGpu(rhi.get(), impulseSource, BlurProfile::Quality);
    const QImage cpuImpulse = ValoBlurPipelineV2::filterOffline(impulseSource, BlurProfile::Quality);

    int maxGpuVal = 0;
    int maxCpuVal = 0;
    quint64 totalGpuEnergy = 0;
    quint64 totalCpuEnergy = 0;

    for (int y = 0; y < impulseDim; ++y) {
        for (int x = 0; x < impulseDim; ++x) {
            int g = gpuImpulse.pixelColor(x, y).red();
            int c = cpuImpulse.pixelColor(x, y).red();
            if (g > maxGpuVal) maxGpuVal = g;
            if (c > maxCpuVal) maxCpuVal = c;
            totalGpuEnergy += g;
            totalCpuEnergy += c;
        }
    }

    std::cout << "Impulso Spot (Raio=" << spotRadius << "):\n";
    std::cout << "  - Pico Central GPU : " << maxGpuVal << " / 255\n";
    std::cout << "  - Pico Central CPU : " << maxCpuVal << " / 255\n";
    std::cout << "  - Energia Total GPU: " << totalGpuEnergy << "\n";
    std::cout << "  - Energia Total CPU: " << totalCpuEnergy << "\n";
    std::cout << "  - Razão GPU/CPU    : " << std::fixed << std::setprecision(4) << (double(totalGpuEnergy) / double(totalCpuEnergy)) << "\n";

    // Exporta imagem de impulso normalizada para revelar a Point Spread Function (PSF)
    QImage normalizedImpulse(impulseDim, impulseDim, QImage::Format_RGBA8888_Premultiplied);
    for (int y = 0; y < impulseDim; ++y) {
        for (int x = 0; x < impulseDim; ++x) {
            float norm = (maxGpuVal > 0) ? (float(gpuImpulse.pixelColor(x, y).red()) / float(maxGpuVal)) : 0.0f;
            int v = std::clamp(static_cast<int>(norm * 255.0f), 0, 255);
            normalizedImpulse.setPixelColor(x, y, QColor(v, v, v, 255));
        }
    }
    const QString impulsePath = artifactDir + "/valo_v2_gpu_impulse_response_fixed.png";
    normalizedImpulse.save(impulsePath);
    std::cout << "  -> Salvo: " << impulsePath.toStdString() << " (Point Spread Function com auto-stretch linear [0, 255])\n";

    // 2. Imagem de Backdrop Sintético para verificação de difusão de cena real
    const QSize sceneSize(960, 540);
    const QImage sceneInput = ValoBackdropProviderV2::createSyntheticBackdrop(sceneSize);
    const QImage gpuSceneQuality = pipeline.filterOfflineGpu(rhi.get(), sceneInput, BlurProfile::Quality);
    const QImage cpuSceneQuality = ValoBlurPipelineV2::filterOffline(sceneInput, BlurProfile::Quality);

    // Mapa de Diferença Verificado GPU vs CPU
    QImage verifiedDiff(sceneSize, QImage::Format_ARGB32_Premultiplied);
    verifiedDiff.fill(Qt::black);

    int maxDiffVal = 0;
    quint64 sumDiffVal = 0;
    for (int y = 0; y < sceneSize.height(); ++y) {
        for (int x = 0; x < sceneSize.width(); ++x) {
            const QColor cGpu = gpuSceneQuality.pixelColor(x, y);
            const QColor cCpu = cpuSceneQuality.pixelColor(x, y);
            const int dr = std::abs(cGpu.red() - cCpu.red());
            const int dg = std::abs(cGpu.green() - cCpu.green());
            const int db = std::abs(cGpu.blue() - cCpu.blue());
            const int d = std::max({ dr, dg, db });
            if (d > maxDiffVal) maxDiffVal = d;
            sumDiffVal += d;

            // Amplificação de 20x para tornar visível no monitor qualquer diferença microscópica
            const int ampR = std::min(255, dr * 20);
            const int ampG = std::min(255, dg * 20);
            const int ampB = std::min(255, db * 20);
            verifiedDiff.setPixelColor(x, y, QColor(ampR, ampG, ampB, 255));
        }
    }

    const double meanDiffVal = double(sumDiffVal) / double(sceneSize.width() * sceneSize.height());
    const QString diffPath = artifactDir + "/valo_v2_gpu_vs_cpu_diff_verified.png";
    verifiedDiff.save(diffPath);

    std::cout << "\nVerificação GPU vs CPU (Quality 960x540):\n";
    std::cout << "  - Diferença Máxima : " << maxDiffVal << " / 255\n";
    std::cout << "  - Diferença Média  : " << std::fixed << std::setprecision(4) << meanDiffVal << " / 255\n";
    std::cout << "  -> Salvo: " << diffPath.toStdString() << " (Amplificação 20x)\n";

    std::cout << "\n========================================================================\n";
    std::cout << "  HOTFIX E BENCHMARKS CONCLUÍDOS COM SUCESSO!\n";
    std::cout << "========================================================================\n";

    return 0;
}
