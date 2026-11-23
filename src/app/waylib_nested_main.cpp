#include "compositor/waylib/WaylibCompositorAdapter.hpp"

#include <woutputrenderwindow.h>
#include <wserver.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QTimer>

#include <iostream>

WAYLIB_SERVER_USE_NAMESPACE

int main(int argc, char *argv[]) {
    WServer::initializeQPA();
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("systalia_waylib_nested"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption optSocket(QStringLiteral("socket"),
                                 QStringLiteral("Nome do socket Wayland isolado."),
                                 QStringLiteral("name"));
    QCommandLineOption optHeadless(QStringLiteral("headless"),
                                   QStringLiteral("Força backend wlroots headless (sem janela nested)."));
    QCommandLineOption optAutoQuit(QStringLiteral("auto-quit-ms"),
                                   QStringLiteral("Encerra automaticamente após N milissegundos."),
                                   QStringLiteral("ms"));
    parser.addOption(optSocket);
    parser.addOption(optHeadless);
    parser.addOption(optAutoQuit);
    parser.process(app);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(SGE_WAYLIB_QML_IMPORT_PATH));

    Systalia::Compositor::WaylibBackend::WaylibCompositorAdapter adapter;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/systalia/waylib/WaylibNestedShell.qml")));
    if (component.isError()) {
        std::cerr << "[FAIL] QML error: " << component.errorString().toStdString() << "\n";
        return 1;
    }

    QObject *rootObj = component.createWithInitialProperties(
        {{QStringLiteral("compositorAdapter"), QVariant::fromValue(&adapter)}});
    if (!rootObj) {
        std::cerr << "[FAIL] Falha ao instanciar WaylibNestedShell.qml: "
                  << component.errorString().toStdString() << "\n";
        return 1;
    }

    auto *renderWindow = rootObj->findChild<WOutputRenderWindow *>();
    if (!renderWindow) {
        std::cerr << "[FAIL] OutputRenderWindow não encontrado em WaylibNestedShell.qml.\n";
        delete rootObj;
        return 1;
    }

    const auto mode = parser.isSet(optHeadless)
                          ? Systalia::Compositor::WaylibBackend::WaylibCompositorAdapter::BackendMode::Headless
                          : Systalia::Compositor::WaylibBackend::WaylibCompositorAdapter::BackendMode::NestedWayland;
    if (!adapter.initialize(renderWindow, &engine, mode, parser.value(optSocket))) {
        std::cerr << "[FAIL] WaylibCompositorAdapter::initialize: "
                  << adapter.lastError().toStdString() << "\n";
        delete rootObj;
        return 1;
    }

    if (parser.isSet(optAutoQuit)) {
        const int ms = qMax(50, parser.value(optAutoQuit).toInt());
        QTimer::singleShot(ms, &app, &QCoreApplication::quit);
    }

    const int rc = app.exec();
    delete rootObj;
    adapter.shutdown();
    return rc;
}
