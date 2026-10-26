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

int main(int argc, char *argv[]) {
    // Configuração obrigatória do backend Vulkan QRhi antes de QGuiApplication
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    qputenv("QSG_RHI_BACKEND", "vulkan");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SystaliaGlassEngine"));
    app.setOrganizationName(QStringLiteral("Systalia"));

    NotificationServer notificationServer;
    SystemControlService systemControlService;

    // Registro dos tipos QML
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

    return app.exec();
}
