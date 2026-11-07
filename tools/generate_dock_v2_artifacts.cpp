#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>
#include <QImage>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <iomanip>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <string>

#include "ValoGeometryEngineV2.hpp"
#include "ValoBlurPipelineV2.hpp"
#include "ValoBackdropProviderV2.hpp"
#include "ValoOrganicTransform.hpp"

void qml_register_types_Systalia_Engine();

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
    std::string backend = "opengl";
    std::string platform = "wayland";
    std::string artifactDir = "/home/lucasramos/Documentos/systalia-glass-engine/artifacts/graphics-v2-stage-04-2";
    std::string brainDir = "/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f";
    std::string targetPass = "all";
    int filterProfile = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc) {
            backend = argv[++i];
        } else if (arg == "--platform" && i + 1 < argc) {
            platform = argv[++i];
        } else if (arg == "--artifact-dir" && i + 1 < argc) {
            artifactDir = argv[++i];
        } else if (arg == "--brain-dir" && i + 1 < argc) {
            brainDir = argv[++i];
        } else if (arg == "--profile" && i + 1 < argc) {
            filterProfile = std::stoi(argv[++i]);
        } else if (arg == "--pass" && i + 1 < argc) {
            targetPass = argv[++i];
        }
    }

    // Configuração estrita de ambiente para forçar QRhi real sem fallback de software
    qputenv("QT_QPA_PLATFORM", QByteArray::fromStdString(platform));
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", QByteArray::fromStdString(backend));
    qputenv("QSG_INFO", "1");

    QGuiApplication app(argc, argv);
    qml_register_types_Systalia_Engine();

    const QString qArtifactDir = QString::fromStdString(artifactDir);
    const QString qBrainDir = QString::fromStdString(brainDir);
    QDir().mkpath(qArtifactDir);
    QDir().mkpath(qBrainDir);

    QQuickWindow window;
    window.resize(1280, 720);
    window.setColor(Qt::black);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockV2Scene {
            id: scene
            anchors.fill: parent
            showHud: false
        }
    )", QUrl());

    if (!component.isReady()) {
        std::cerr << "Erro compilando ValoDockV2Scene: " << component.errorString().toStdString() << std::endl;
        return 1;
    }

    auto *sceneItem = qobject_cast<QQuickItem*>(component.create());
    if (!sceneItem) {
        std::cerr << "Erro instanciando ValoDockV2Scene." << std::endl;
        return 1;
    }
    sceneItem->setParentItem(window.contentItem());
    sceneItem->setWidth(1280);
    sceneItem->setHeight(720);

    window.show();
    for (int i = 0; i < 15; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QThread::msleep(15);
    }

    // Validação estrita de inicialização do RHI em hardware real
    QSGRendererInterface *rif = window.rendererInterface();
    if (!rif) {
        std::cerr << "[ERRO CRÍTICO] QSGRendererInterface não disponível!" << std::endl;
        return 1;
    }

    const auto api = rif->graphicsApi();
    if (api == QSGRendererInterface::Software) {
        std::cerr << "[ERRO CRÍTICO] Software Fallback detectado! Abortando execução de artefatos GPU." << std::endl;
        return 1;
    }

    QRhi *rhi = window.rhi();
    if (!rhi) {
        std::cerr << "[ERRO CRÍTICO] QRhi não instanciado na janela gráfica!" << std::endl;
        return 1;
    }

    std::cout << "================================================================================" << std::endl;
    std::cout << "   VALO GRAPHICS 2.0 — GERADOR DE ARTEFATOS HOMOLOGADOS DA DOCK V2" << std::endl;
    std::cout << "   Backend: " << backend << " | Platform: " << platform << " | API: " << api << std::endl;
    std::cout << "   Device:  " << rhi->backendName() << " (" << rhi->driverInfo().deviceName.toStdString() << ")" << std::endl;
    std::cout << "================================================================================" << std::endl;

    auto renderAndCapture = [&](const QString &filename) -> QImage {
        for (int i = 0; i < 8; ++i) {
            window.update();
            QCoreApplication::processEvents();
            QThread::msleep(15);
        }
        QImage img = window.grabWindow();
        if (!img.isNull()) {
            const QString path1 = qArtifactDir + "/" + filename;
            const QString path2 = qBrainDir + "/" + filename;
            img.save(path1);
            img.save(path2);
            std::cout << "[ARTEFATO GERADO] " << path1.toStdString() << " (" << img.width() << "x" << img.height() << ")" << std::endl;
        } else {
            std::cerr << "[FALHA GRAB] " << filename.toStdString() << std::endl;
        }
        return img;
    };

    QObject *dock = sceneItem->property("dock").value<QObject*>();
    if (!dock) {
        std::cerr << "Dock não encontrada na cena!" << std::endl;
        return 1;
    }

    if (filterProfile >= 0) {
        dock->setProperty("blurProfile", filterProfile);
    }

    std::cout << "\n=== 1. GERANDO ARTEFATOS DOS 7 ESTADOS CANÔNICOS (GPU REAL) ===" << std::endl;
    sceneItem->setProperty("backgroundMode", 0); // Fundo Oceânico padrão
    dock->setProperty("debugPass", "H"); // Composição completa

    // Estado 1: Idle (Repouso)
    dock->setProperty("testState", 1);
    renderAndCapture("dock_v2_state_1_idle.png");

    // Estado 2: Hover (Lançador 2 com escala física 1.08)
    dock->setProperty("testState", 2);
    renderAndCapture("dock_v2_state_2_hover.png");

    // Estado 3: Press (Lançador 2 comprimido a 0.94)
    dock->setProperty("testState", 3);
    renderAndCapture("dock_v2_state_3_press.png");

    // Estado 4: Active (Lançador 0 com indicador ativo #2EE8F0)
    dock->setProperty("testState", 4);
    renderAndCapture("dock_v2_state_4_active.png");

    // Estado 5: Background Running (Lançadores 1 e 3 com indicador em segundo plano)
    dock->setProperty("testState", 5);
    renderAndCapture("dock_v2_state_5_background.png");

    // Estado 6: Focused (Lançador 2 com anel de foco por teclado #37DDFA)
    dock->setProperty("testState", 6);
    renderAndCapture("dock_v2_state_6_focused.png");

    // Estado 7: Reduced Motion / Alto Contraste (WCAG AA + molas amortecidas)
    dock->setProperty("testState", 7);
    renderAndCapture("dock_v2_state_7_reduced_motion.png");

    std::cout << "\n=== 2. GERANDO ARTEFATOS DOS 8 PASSES DIAGNÓSTICOS (GPU REAL) ===" << std::endl;
    dock->setProperty("testState", 1);
    sceneItem->setProperty("backgroundMode", 0);

    const struct { const char *pass; const char *filename; } passes[] = {
        {"A", "dock_v2_pass_A_sdf.png"},
        {"B", "dock_v2_pass_B_mask.png"},
        {"C", "dock_v2_pass_C_volume.png"},
        {"D", "dock_v2_pass_D_refraction.png"},
        {"E", "dock_v2_pass_E_blur_gpu.png"},
        {"F", "dock_v2_pass_F_specular.png"},
        {"G", "dock_v2_pass_G_shadow.png"},
        {"H", "dock_v2_pass_H_composite.png"}
    };

    for (const auto &p : passes) {
        if (targetPass != "all" && targetPass != p.pass) {
            continue;
        }
        dock->setProperty("debugPass", p.pass);
        renderAndCapture(p.filename);
    }

    std::cout << "\n=== 3. GERANDO ARTEFATOS MULTI-BACKDROP (GPU REAL) ===" << std::endl;
    dock->setProperty("debugPass", "H");

    const struct { int mode; const char *filename; } bgs[] = {
        {0, "dock_v2_bg_oceanic.png"},
        {1, "dock_v2_bg_grid.png"},
        {2, "dock_v2_bg_light.png"},
        {3, "dock_v2_bg_dark.png"},
        {4, "dock_v2_bg_prism.png"}
    };

    for (const auto &bg : bgs) {
        sceneItem->setProperty("backgroundMode", bg.mode);
        renderAndCapture(bg.filename);
    }

    std::cout << "\n=== 4. BENCHMARK DE RENDERIZAÇÃO DA DOCK V2 ===" << std::endl;
    const struct { int profile; const char *name; } profiles[] = {
        {0, "Performance (0 passes / Direct)"},
        {1, "Balanced (4 passes GPU)"},
        {2, "Quality (8 passes GPU)"}
    };

    for (const auto &prof : profiles) {
        dock->setProperty("blurProfile", prof.profile);
        dock->setProperty("testState", 1);
        sceneItem->setProperty("backgroundMode", 0);

        for (int w = 0; w < 10; ++w) {
            window.update();
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }

        std::vector<double> frameTimes;
        frameTimes.reserve(100);
        QElapsedTimer timer;

        for (int frame = 0; frame < 100; ++frame) {
            timer.start();
            window.update();
            QCoreApplication::processEvents();
            double elapsed = timer.nsecsElapsed() / 1e6;
            frameTimes.push_back(elapsed);
        }

        auto stats = BenchStats::compute(frameTimes);
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[BENCHMARK " << prof.name << "]"
                  << " Mean: " << stats.meanMs << " ms |"
                  << " Median: " << stats.medianMs << " ms |"
                  << " P95: " << stats.p95Ms << " ms |"
                  << " P99: " << stats.p99Ms << " ms |"
                  << " Min: " << stats.minMs << " ms |"
                  << " Max: " << stats.maxMs << " ms |"
                  << " StdDev: " << stats.stdDevMs << " ms"
                  << std::endl;
    }

    std::cout << "\nGeração de artefatos da Etapa 04.2 finalizada com sucesso." << std::endl;
    return 0;
}
