#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>
#include <QImage>
#include <QPainter>
#include <QDir>
#include <QThread>
#include <iostream>
#include <string>

#include "ValoGlassItemV2.hpp"
#include "SystemTelemetryService.hpp"
#include "ProcessLauncherService.hpp"
#include "AppRegistryService.hpp"
#include "NotificationServer.hpp"
#include "SystemControlService.hpp"
#include "WorkspaceManagerService.hpp"
#include "LiquidTilingService.hpp"
#include "OutputManagerService.hpp"
#include "InputDeviceManager.hpp"
#include "LogindSessionService.hpp"

void qml_register_types_Systalia_Engine();

using namespace Systalia::Gfx::V2;

int main(int argc, char **argv) {
    std::string backend = "vulkan";
    std::string platform = "wayland";
    std::string artifactDir = "/home/lucasramos/Documentos/systalia-glass-engine/artifacts/graphics-v2-stage-05-1";
    std::string brainDir = "/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f";

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
        }
    }

    qputenv("QT_QPA_PLATFORM", QByteArray::fromStdString(platform));
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", QByteArray::fromStdString(backend));
    qputenv("QSG_INFO", "0");

    QGuiApplication app(argc, argv);
    qml_register_types_Systalia_Engine();

    NotificationServer notifications{false};
    SystemControlService control;
    WorkspaceManagerService workspaces;
    LiquidTilingService tiling;
    OutputManagerService outputs;
    LogindSessionService session;
    InputDeviceManager input;

    qmlRegisterSingletonType<SystemTelemetryService>("Systalia.System", 1, 0, "SystemTelemetry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemTelemetryService; });
    qmlRegisterSingletonType<ProcessLauncherService>("Systalia.System", 1, 0, "ProcessLauncher",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new ProcessLauncherService; });
    qmlRegisterSingletonType<AppRegistryService>("Systalia.System", 1, 0, "AppRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new AppRegistryService; });
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "Notifications", &notifications);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "SystemControl", &control);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "WorkspaceManager", &workspaces);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LiquidTiling", &tiling);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "OutputManager", &outputs);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LogindSession", &session);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "InputDeviceManager", &input);

    const QString qArtifactDir = QString::fromStdString(artifactDir);
    const QString qBrainDir = QString::fromStdString(brainDir);
    QDir().mkpath(qArtifactDir);
    QDir().mkpath(qBrainDir);

    QQuickWindow window;
    window.resize(1920, 1080);
    window.setColor(Qt::black);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        import Systalia.System
        import "qrc:/qt/qml/Systalia/Engine/src/qml/shell" as Shell
        import "qrc:/qt/qml/Systalia/Engine/src/qml/graphics/v2" as GfxV2

        Item {
            id: root
            width: 1920
            height: 1080

            property alias filesWindow: filesWindowItem

            // Systalia Oceanic Wallpaper
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.00; color: "#0A1128" }
                    GradientStop { position: 0.35; color: "#001F54" }
                    GradientStop { position: 0.65; color: "#034078" }
                    GradientStop { position: 0.85; color: "#1282A2" }
                    GradientStop { position: 1.00; color: "#0A1128" }
                }

                Rectangle {
                    x: 200; y: 150; width: 600; height: 600
                    radius: 300
                    color: "#00E5FF"
                    opacity: 0.15
                }
                Rectangle {
                    x: 1100; y: 400; width: 700; height: 700
                    radius: 350
                    color: "#7928CA"
                    opacity: 0.18
                }
            }

            // Window layer
            Item {
                anchors.fill: parent

                Shell.SystaliaFilesWindowV2 {
                    id: filesWindowItem
                    x: 320
                    y: 160
                    width: 701
                    height: 534
                    visible: true
                }
            }
        }
    )", QUrl("qrc:/qt/qml/Systalia/Engine/src/qml/"));

    if (!component.isReady()) {
        std::cerr << "Erro compilando QML: " << component.errorString().toStdString() << std::endl;
        return 1;
    }

    auto *sceneItem = qobject_cast<QQuickItem*>(component.create());
    if (!sceneItem) return 1;
    sceneItem->setParentItem(window.contentItem());
    sceneItem->setWidth(1920);
    sceneItem->setHeight(1080);

    window.show();
    for (int i = 0; i < 20; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QThread::msleep(15);
    }

    QObject *filesWindow = sceneItem->property("filesWindow").value<QObject*>();

    auto captureWindowArea = [&](const QString &filename) {
        for (int i = 0; i < 15; ++i) {
            window.update();
            QCoreApplication::processEvents();
            QThread::msleep(15);
        }
        QImage full = window.grabWindow();
        if (!full.isNull()) {
            QRect cropRect(300, 140, 741, 574);
            QImage cropped = full.copy(cropRect);
            cropped.save(qArtifactDir + "/" + filename);
            cropped.save(qBrainDir + "/" + filename);
            std::cout << "[ARTEFATO GERADO] " << filename.toStdString() << " (" << cropped.width() << "x" << cropped.height() << ")" << std::endl;
        }
    };

    // 1. Pass H (Composição Completa Corrigida)
    if (filesWindow) filesWindow->setProperty("debugPass", "H");
    captureWindowArea("stage_05_1_files_window_corrected_" + QString::fromStdString(backend) + ".png");

    // 2. Pass I (Cáusticas Isoladas)
    if (filesWindow) filesWindow->setProperty("debugPass", "I");
    captureWindowArea("stage_05_1_pass_I_caustics_" + QString::fromStdString(backend) + ".png");

    // 3. Pass J (Reflexão Prismática Isolada)
    if (filesWindow) filesWindow->setProperty("debugPass", "J");
    captureWindowArea("stage_05_1_pass_J_reflection_" + QString::fromStdString(backend) + ".png");

    // 4. Pass K (Material Básico sem Cáusticas/Reflexão)
    if (filesWindow) filesWindow->setProperty("debugPass", "K");
    captureWindowArea("stage_05_1_pass_K_basic_" + QString::fromStdString(backend) + ".png");

    // 5. Pass D (Refração Vetores)
    if (filesWindow) filesWindow->setProperty("debugPass", "D");
    captureWindowArea("stage_05_1_pass_D_refraction_" + QString::fromStdString(backend) + ".png");

    // 6. Pass E (Blur GPU Amostrado)
    if (filesWindow) filesWindow->setProperty("debugPass", "E");
    captureWindowArea("stage_05_1_pass_E_blur_" + QString::fromStdString(backend) + ".png");

    // Restaura H
    if (filesWindow) filesWindow->setProperty("debugPass", "H");

    return 0;
}
