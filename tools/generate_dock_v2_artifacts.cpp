#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
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
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    qml_register_types_Systalia_Engine();

    const QString artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
    const QString repoArtifactDir = QStringLiteral("/home/lucasramos/Documentos/systalia-glass-engine/artifacts");
    QDir().mkpath(artifactDir);
    QDir().mkpath(repoArtifactDir);

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
        QCoreApplication::processEvents();
        QThread::msleep(20);
    }

    auto renderAndCapture = [&](const QString &filename) -> QImage {
        for (int i = 0; i < 6; ++i) {
            window.update();
            QCoreApplication::processEvents();
            QThread::msleep(20);
        }
        QImage img = window.grabWindow();
        if (!img.isNull()) {
            const QString path1 = artifactDir + "/" + filename;
            const QString path2 = repoArtifactDir + "/" + filename;
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

    std::cout << "=== 1. GERANDO ARTEFATOS DOS 7 ESTADOS CANÔNICOS (GPU REAL) ===" << std::endl;
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

    std::cout << "=== 2. GERANDO ARTEFATOS DOS 8 PASSES DIAGNÓSTICOS (GPU REAL) ===" << std::endl;
    dock->setProperty("testState", 1);
    sceneItem->setProperty("backgroundMode", 0);

    // Passe A: Geometria SDF e Normais Analíticas (valo_diag_sdf.frag)
    dock->setProperty("debugPass", "A");
    renderAndCapture("dock_v2_pass_A_sdf.png");

    // Passe B: Máscara Analítica (L2 Capsule Coverage) (valo_diag_mask.frag)
    dock->setProperty("debugPass", "B");
    renderAndCapture("dock_v2_pass_B_mask.png");

    // Passe C: Preenchimento Volumétrico Oceânico (valo_diag_volume.frag)
    dock->setProperty("debugPass", "C");
    renderAndCapture("dock_v2_pass_C_volume.png");

    // Passe D: Refração e Menisco Óptico (valo_diag_refraction.frag)
    dock->setProperty("debugPass", "D");
    renderAndCapture("dock_v2_pass_D_refraction.png");

    // Passe E: Blur Dual-Kawase GPU (8 Passes Quality) (valo_diag_blur.frag)
    dock->setProperty("debugPass", "E");
    renderAndCapture("dock_v2_pass_E_blur_gpu.png");

    // Passe F: Contorno Especular Isolado de 1.25px (valo_diag_specular.frag)
    dock->setProperty("debugPass", "F");
    renderAndCapture("dock_v2_pass_F_specular.png");

    // Passe G: Sombra Externa Analítica Contínua (valo_shadow_v2.frag)
    dock->setProperty("debugPass", "G");
    renderAndCapture("dock_v2_pass_G_shadow.png");

    // Passe H: Composição Final Completa (valo_optical_v2.frag)
    dock->setProperty("debugPass", "H");
    renderAndCapture("dock_v2_pass_H_composite.png");

    std::cout << "=== 3. GERANDO ARTEFATOS MULTI-BACKDROP (GPU REAL) ===" << std::endl;
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

    std::cout << "=== 4. BENCHMARK DE RENDERIZAÇÃO DA DOCK V2 ===" << std::endl;
    const struct { int profile; const char *name; } profiles[] = {
        {0, "Performance (0 passes / Direct)"},
        {1, "Balanced (4 passes)"},
        {2, "Quality (8 passes)"}
    };

    for (const auto &prof : profiles) {
        dock->setProperty("blurProfile", prof.profile);
        dock->setProperty("testState", 1);
        sceneItem->setProperty("backgroundMode", 0);

        for (int w = 0; w < 10; ++w) {
            window.update();
            QCoreApplication::processEvents();
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

    std::cout << "Geração de artefatos da Etapa 04.1 finalizada com sucesso." << std::endl;
    return 0;
}
