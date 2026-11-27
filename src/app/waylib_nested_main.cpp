#include "compositor/waylib/WaylibCompositorAdapter.hpp"

#include <woutputrenderwindow.h>
#include <wserver.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QSocketNotifier>
#include <QTimer>

#include <csignal>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
int g_waylibSignalFd[2] = {-1, -1};

void handleWaylibPosixSignal(int) {
    const char byte = 1;
    if (g_waylibSignalFd[0] >= 0) {
        const ssize_t ignored = ::write(g_waylibSignalFd[0], &byte, sizeof(byte));
        Q_UNUSED(ignored);
    }
}
} // namespace

int main(int argc, char *argv[]) {
    WServer::initializeQPA();
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("systalia_waylib_nested"));

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_waylibSignalFd) == 0) {
        auto *signalNotifier = new QSocketNotifier(g_waylibSignalFd[1], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, &app, [signalNotifier]() {
            signalNotifier->setEnabled(false);
            char tmp = 0;
            const ssize_t ignored = ::read(g_waylibSignalFd[1], &tmp, sizeof(tmp));
            Q_UNUSED(ignored);
            QCoreApplication::quit();
        });
        struct sigaction sa {};
        sa.sa_handler = handleWaylibPosixSignal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        ::sigaction(SIGTERM, &sa, nullptr);
        ::sigaction(SIGINT, &sa, nullptr);
    }

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
    Systalia::Compositor::WaylibBackend::WaylibCompositorAdapter::registerQmlTypes();

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
