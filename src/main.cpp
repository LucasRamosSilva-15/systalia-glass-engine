#include <QGuiApplication>
#include <QJSEngine>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QUrl>
#include <QDebug>
#include "SystaliaGlassItem.hpp"
#include "SystaliaCompositor.hpp"
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

int main(int argc, char *argv[]) {
    // Deteção dinâmica de sessão (Bare-Metal DRM/KMS via TTY vs. Aninhada Wayland/X11 vs. Headless)
    // Configura QT_QPA_PLATFORM=eglfs, QT_QPA_EGLFS_INTEGRATION=eglfs_kms e QSG_RHI_BACKEND=vulkan
    // antes da instanciação de QGuiApplication quando nenhuma sessão hospedeira está ativa.
    const auto platformCfg = InputDeviceManager::configureProcessPlatformEnvironment(
        QStringLiteral("config/eglfs_kms.json"));
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SystaliaGlassEngine"));
    app.setOrganizationName(QStringLiteral("Systalia"));

    NotificationServer notificationServer;
    SystemControlService systemControlService;
    WorkspaceManagerService workspaceManagerService;
    LiquidTilingService liquidTilingService;
    OutputManagerService outputManagerService;
    LogindSessionService logindSessionService;
    InputDeviceManager inputDeviceManager;
    if (platformCfg.mode == InputDeviceManager::SessionBareMetalKms) {
        if (logindSessionService.initializeSession(false)) {
            logindSessionService.acquirePrimaryDrmDevice(inputDeviceManager.activeDrmDevice());
        }
        inputDeviceManager.initializeHardwareContext();
    }

    // Registro dos tipos QML
    qml_register_types_Systalia_Engine();
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
    qmlRegisterType<SystaliaCompositor>("Systalia.Compositor", 1, 0, "SystaliaCompositor");
    qmlRegisterSingletonType<SystemTelemetryService>(
        "Systalia.System", 1, 0, "SystemTelemetry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new SystemTelemetryService();
        });
    qmlRegisterSingletonType<ProcessLauncherService>(
        "Systalia.System", 1, 0, "ProcessLauncher",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new ProcessLauncherService();
        });
    qmlRegisterSingletonType<AppRegistryService>(
        "Systalia.System", 1, 0, "AppRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new AppRegistryService();
        });
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "Notifications",
                                 &notificationServer);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "SystemControl",
                                 &systemControlService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "WorkspaceManager",
                                 &workspaceManagerService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LiquidTiling",
                                 &liquidTilingService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "OutputManager",
                                 &outputManagerService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LogindSession",
                                 &logindSessionService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "InputDeviceManager",
                                 &inputDeviceManager);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("desktopIcons"), new DesktopIconProvider());

    const QUrl url(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qCritical() << "Falha ao instanciar o objeto raiz a partir de" << url;
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Tentando carregar a partir do caminho de arquivo local...";
        engine.load(QUrl::fromLocalFile(QStringLiteral("src/qml/main.qml")));
    }

    if (auto *window = engine.rootObjects().isEmpty() ? nullptr
                     : qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        QObject::connect(&logindSessionService, &LogindSessionService::gpuSubmissionSuspendedChanged,
                         window, [window, wasVisible = false](bool suspended) mutable {
            if (suspended) {
                wasVisible = window->isVisible();
                window->hide();
                window->releaseResources();
            } else if (wasVisible) {
                window->show();
                window->update();
            }
        });
    }

    return app.exec();
}
