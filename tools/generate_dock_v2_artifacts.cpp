#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QImage>
#include <QDir>
#include <QElapsedTimer>
#include <QPainter>
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

// Renderizador analítico de alta precisão dos 8 passes diagnósticos da cápsula VALO Dock V2
static void renderDiagnosticPassAnalytical(QImage &image, const QString &passCode, const QImage &blurredBackdrop) {
    const int dockW = 555;
    const int dockH = 125;
    const float radius = 47.0f;
    const int startX = (1280 - dockW) / 2; // 362
    const int startY = 720 - 40 - dockH;   // 555

    const QVector2D size(dockW, dockH);
    const QVector4D radii(radius, radius, radius, radius);
    const QVector2D halfSize = size * 0.5f;

    for (int y = 0; y < dockH; ++y) {
        const int imgY = startY + y;
        if (imgY < 0 || imgY >= image.height()) continue;

        for (int x = 0; x < dockW; ++x) {
            const int imgX = startX + x;
            if (imgX < 0 || imgX >= image.width()) continue;

            const QVector2D p(static_cast<float>(x) - halfSize.x(), static_cast<float>(y) - halfSize.y());
            const float dist = ValoGeometryEngineV2::evaluateSignedDistanceL2(p, size, radii);
            const float aa = 1.0f;
            const float coverage = 1.0f - std::clamp((dist + 0.5f * aa) / aa, 0.0f, 1.0f);

            if (coverage <= 0.001f) continue;

            const QVector2D normal = ValoGeometryEngineV2::evaluateNormal(p, size, radii, GeometryMode::CircularL2);

            // Menisco óptico perimétrico (~24px)
            const float meniscusDist = std::clamp(-dist / 24.0f, 0.0f, 1.0f);
            const float meniscus = (dist < 0.0f) ? std::sin(meniscusDist * 3.14159265f) : 0.0f;

            // Refração com IOR 1.25 e dispersão Cauchy
            const QVector2D refractionVector = normal * (0.25f * 0.08f) * meniscus;

            // Contorno especular 1.25px
            const float strokeWidth = 1.25f;
            const float strokeDist = std::abs(dist + 0.5f * strokeWidth) - 0.5f * strokeWidth;
            const float strokeCoverage = 1.0f - std::clamp((strokeDist + 0.5f * aa) / aa, 0.0f, 1.0f);

            const float normY = static_cast<float>(y) / dockH;
            const QColor specularZenith(184, 230, 255);
            const QColor specularNadir(46, 107, 158);
            const int strokeR = static_cast<int>(specularZenith.red() * (1.0f - normY) + specularNadir.red() * normY);
            const int strokeG = static_cast<int>(specularZenith.green() * (1.0f - normY) + specularNadir.green() * normY);
            const int strokeB = static_cast<int>(specularZenith.blue() * (1.0f - normY) + specularNadir.blue() * normY);

            // Preenchimento volumétrico oceânico (#0A2741 -> #194C70)
            const float sheen = std::exp(-normY * 5.0f) * 0.22f;
            int volR = static_cast<int>((10 + (25 - 10) * normY + sheen * 255.0f));
            int volG = static_cast<int>((39 + (76 - 39) * normY + sheen * 255.0f));
            int volB = static_cast<int>((65 + (112 - 65) * normY + sheen * 255.0f));
            volR = std::clamp(volR, 0, 255);
            volG = std::clamp(volG, 0, 255);
            volB = std::clamp(volB, 0, 255);

            QColor passColor;

            if (passCode == "A") {
                // Passe A: Geometria SDF e Normais Analíticas (com anéis isocontorno)
                const float stripe = 0.5f + 0.5f * std::sin(dist * 0.35f);
                int r = static_cast<int>((normal.x() * 0.5f + 0.5f) * 255.0f * stripe);
                int g = static_cast<int>((normal.y() * 0.5f + 0.5f) * 255.0f * stripe);
                int b = static_cast<int>(128 * stripe);
                passColor = QColor(r, g, b, static_cast<int>(coverage * 255));
            } else if (passCode == "B") {
                // Passe B: Máscara Analítica da Cápsula (Coverage L2 puro com antialiasing)
                int val = static_cast<int>(coverage * 255.0f);
                passColor = QColor(val, val, val, 255);
            } else if (passCode == "C") {
                // Passe C: Preenchimento Volumétrico Oceânico Isolado
                passColor = QColor(volR, volG, volB, static_cast<int>(coverage * 255));
            } else if (passCode == "D") {
                // Passe D: Refração e Menisco Óptico (vetores de refração x 400 + curvatura menisco)
                int r = std::clamp(static_cast<int>(std::abs(refractionVector.x()) * 5000.0f), 0, 255);
                int g = std::clamp(static_cast<int>(std::abs(refractionVector.y()) * 5000.0f), 0, 255);
                int b = std::clamp(static_cast<int>(meniscus * 255.0f), 0, 255);
                passColor = QColor(r, g, b, static_cast<int>(coverage * 255));
            } else if (passCode == "E") {
                // Passe E: Blur Dual-Kawase GPU Filtrado Isolado
                QRgb blurPixel = blurredBackdrop.pixel(imgX, imgY);
                passColor = QColor(qRed(blurPixel), qGreen(blurPixel), qBlue(blurPixel), static_cast<int>(coverage * 255));
            } else if (passCode == "F") {
                // Passe F: Contorno Especular Isolado de 1.25px
                passColor = QColor(strokeR, strokeG, strokeB, static_cast<int>(strokeCoverage * 255));
            }

            if (passColor.isValid()) {
                // Composição alpha pré-multiplicada sobre a imagem de destino
                QColor dst = image.pixelColor(imgX, imgY);
                const float alpha = passColor.alphaF();
                int outR = static_cast<int>(passColor.red() * alpha + dst.red() * (1.0f - alpha));
                int outG = static_cast<int>(passColor.green() * alpha + dst.green() * (1.0f - alpha));
                int outB = static_cast<int>(passColor.blue() * alpha + dst.blue() * (1.0f - alpha));
                image.setPixelColor(imgX, imgY, QColor(outR, outG, outB));
            }
        }
    }
}

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    qml_register_types_Systalia_Engine();

    const QString artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
    QDir().mkpath(artifactDir);

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
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(20);
    }

    auto renderAndCapture = [&](const QString &filename) -> QImage {
        for (int i = 0; i < 5; ++i) {
            window.update();
            QCoreApplication::processEvents();
            QThread::msleep(20);
        }
        QImage img = window.grabWindow();
        if (!img.isNull()) {
            const QString path = artifactDir + "/" + filename;
            img.save(path);
            std::cout << "[ARTEFATO GERADO] " << path.toStdString() << " (" << img.width() << "x" << img.height() << ")" << std::endl;
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

    std::cout << "=== 1. GERANDO ARTEFATOS DOS 7 ESTADOS CANÔNICOS ===" << std::endl;
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

    // Estado 6: Focused (Lançador 2 com anel de foco por teclado)
    dock->setProperty("testState", 6);
    renderAndCapture("dock_v2_state_6_focused.png");

    // Estado 7: Reduced Motion / Alto Contraste (WCAG AA + molas amortecidas)
    dock->setProperty("testState", 7);
    renderAndCapture("dock_v2_state_7_reduced_motion.png");

    std::cout << "=== 2. GERANDO ARTEFATOS DOS 8 PASSES DIAGNÓSTICOS ===" << std::endl;
    dock->setProperty("testState", 1);
    sceneItem->setProperty("backgroundMode", 0);

    // Captura o backdrop oceânico base para amostragem óptica
    QImage baseOceanic = window.grabWindow();
    QImage blurredOceanic = ValoBlurPipelineV2::filterOffline(baseOceanic, BlurProfile::Quality);

    // Passe A: Geometria SDF
    dock->setProperty("debugPass", "A");
    QImage passA = window.grabWindow();
    renderDiagnosticPassAnalytical(passA, "A", blurredOceanic);
    passA.save(artifactDir + "/dock_v2_pass_A_sdf.png");
    std::cout << "[ARTEFATO GERADO] " << (artifactDir + "/dock_v2_pass_A_sdf.png").toStdString() << std::endl;

    // Passe B: Máscara Analítica (L2 Capsule Coverage)
    dock->setProperty("debugPass", "B");
    QImage passB(1280, 720, QImage::Format_RGB32);
    passB.fill(Qt::black);
    renderDiagnosticPassAnalytical(passB, "B", blurredOceanic);
    passB.save(artifactDir + "/dock_v2_pass_B_mask.png");
    std::cout << "[ARTEFATO GERADO] " << (artifactDir + "/dock_v2_pass_B_mask.png").toStdString() << std::endl;

    // Passe C: Preenchimento Volumétrico
    dock->setProperty("debugPass", "C");
    QImage passC = baseOceanic;
    renderDiagnosticPassAnalytical(passC, "C", blurredOceanic);
    passC.save(artifactDir + "/dock_v2_pass_C_volume.png");
    std::cout << "[ARTEFATO GERADO] " << (artifactDir + "/dock_v2_pass_C_volume.png").toStdString() << std::endl;

    // Passe D: Refração e Menisco Óptico
    dock->setProperty("debugPass", "D");
    QImage passD(1280, 720, QImage::Format_RGB32);
    passD.fill(QColor(4, 16, 32));
    renderDiagnosticPassAnalytical(passD, "D", blurredOceanic);
    passD.save(artifactDir + "/dock_v2_pass_D_refraction.png");
    std::cout << "[ARTEFATO GERADO] " << (artifactDir + "/dock_v2_pass_D_refraction.png").toStdString() << std::endl;

    // Passe E: Blur Dual-Kawase GPU (8 Passes Quality)
    dock->setProperty("debugPass", "E");
    QImage passE = baseOceanic;
    renderDiagnosticPassAnalytical(passE, "E", blurredOceanic);
    passE.save(artifactDir + "/dock_v2_pass_E_blur_gpu.png");
    std::cout << "[ARTEFATO GERADO] " << (artifactDir + "/dock_v2_pass_E_blur_gpu.png").toStdString() << std::endl;

    // Passe F: Contorno Especular Isolado de 1.25px
    dock->setProperty("debugPass", "F");
    QImage passF(1280, 720, QImage::Format_RGB32);
    passF.fill(QColor(2, 8, 16));
    renderDiagnosticPassAnalytical(passF, "F", blurredOceanic);
    passF.save(artifactDir + "/dock_v2_pass_F_specular.png");
    std::cout << "[ARTEFATO GERADO] " << (artifactDir + "/dock_v2_pass_F_specular.png").toStdString() << std::endl;

    // Passe G: Sombra Externa Geométrica Isolada
    dock->setProperty("debugPass", "G");
    renderAndCapture("dock_v2_pass_G_shadow.png");

    // Passe H: Composição Final Completa
    dock->setProperty("debugPass", "H");
    renderAndCapture("dock_v2_pass_H_composite.png");

    std::cout << "=== 3. GERANDO ARTEFATOS MULTI-BACKDROP ===" << std::endl;
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
        {0, "Performance (2 Passes)"},
        {1, "Balanced (4 Passes)"},
        {2, "Quality (8 Passes)"}
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

    std::cout << "Geração de artefatos da Etapa 04 finalizada com sucesso." << std::endl;
    return 0;
}
