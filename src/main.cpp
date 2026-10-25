#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDebug>
#include "SystaliaGlassItem.hpp"
#include "SystaliaCompositor.hpp"
#include "SystemTelemetryService.hpp"

int main(int argc, char *argv[]) {
    // Configuração obrigatória do backend Vulkan QRhi antes de QGuiApplication
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    qputenv("QSG_RHI_BACKEND", "vulkan");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SystaliaGlassEngine"));
    app.setOrganizationName(QStringLiteral("Systalia"));

    // Registro dos tipos QML
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
    qmlRegisterType<SystaliaCompositor>("Systalia.Compositor", 1, 0, "SystaliaCompositor");

    SystemTelemetryService telemetry;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("systemTelemetry"), &telemetry);
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
