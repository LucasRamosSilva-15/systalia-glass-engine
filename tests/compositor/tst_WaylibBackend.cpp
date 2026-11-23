#include <QtTest>

#include "compositor/waylib/WaylibCompositorAdapter.hpp"

#include <woutputrenderwindow.h>
#include <wserver.h>
#include <wxdgtoplevelsurfaceitem.h>

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QSignalSpy>

using namespace Systalia::Compositor::WaylibBackend;
WAYLIB_SERVER_USE_NAMESPACE

class tst_WaylibBackend final : public QObject {
    Q_OBJECT

private slots:
    void initializationFailsGracefullyOnInvalidState();
    void headlessLifecycleSocketIsolationAndClientSurface();
};

void tst_WaylibBackend::initializationFailsGracefullyOnInvalidState() {
    WaylibCompositorAdapter adapter;
    QVERIFY(!adapter.isRunning());
    QVERIFY(!adapter.initialize(nullptr, nullptr, WaylibCompositorAdapter::BackendMode::Headless));
    QVERIFY(!adapter.lastError().isEmpty());

    const QByteArray prevDisplay = qgetenv("WAYLAND_DISPLAY");
    qunsetenv("WAYLAND_DISPLAY");
    QQmlEngine dummyEngine;
    WOutputRenderWindow dummyWindow;
    QVERIFY(!adapter.initialize(&dummyWindow, &dummyEngine, WaylibCompositorAdapter::BackendMode::NestedWayland));
    QVERIFY(!adapter.lastError().isEmpty());
    if (!prevDisplay.isEmpty()) {
        qputenv("WAYLAND_DISPLAY", prevDisplay);
    }
}

void tst_WaylibBackend::headlessLifecycleSocketIsolationAndClientSurface() {
    const QByteArray originalWaylandDisplay = qgetenv("WAYLAND_DISPLAY");

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(SGE_WAYLIB_QML_IMPORT_PATH));

    WaylibCompositorAdapter adapter;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/systalia/waylib/WaylibNestedShell.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> rootObj(component.createWithInitialProperties(
        {{QStringLiteral("compositorAdapter"), QVariant::fromValue(&adapter)}}));
    QVERIFY2(!rootObj.isNull(), qPrintable(component.errorString()));

    auto *renderWindow = rootObj->findChild<WOutputRenderWindow *>();
    QVERIFY(renderWindow != nullptr);

    const QString customSocket = QStringLiteral("systalia-waylib-test-%1")
                                     .arg(QCoreApplication::applicationPid());
    QSignalSpy startedSpy(&adapter, &WaylibCompositorAdapter::initialized);
    QSignalSpy surfaceAddedSpy(adapter.surfaceAdapter(), &WaylibSurfaceAdapter::surfaceAdded);
    QSignalSpy surfaceRemovedSpy(adapter.surfaceAdapter(), &WaylibSurfaceAdapter::surfaceRemoved);

    QVERIFY2(adapter.initialize(renderWindow, &engine,
                                WaylibCompositorAdapter::BackendMode::Headless, customSocket),
             qPrintable(adapter.lastError()));
    QVERIFY(adapter.isRunning());
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(adapter.socketName(), customSocket);
    // Socket isolado não deve sobrescrever WAYLAND_DISPLAY do ambiente pai
    QCOMPARE(qgetenv("WAYLAND_DISPLAY"), originalWaylandDisplay);

    QTRY_VERIFY(adapter.outputAdapter()->outputCount() >= 1);

    // Conecta um cliente Wayland real no socket isolado
    const QString clientBin = QCoreApplication::applicationDirPath() + QStringLiteral("/tst_waylib_test_client");
    QVERIFY2(QFile::exists(clientBin), qPrintable(clientBin));

    QProcess client;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), adapter.socketName());
    env.insert(QStringLiteral("SGE_TEST_CLIENT_MS"), QStringLiteral("1800"));
    client.setProcessEnvironment(env);
    client.start(clientBin);
    QVERIFY2(client.waitForStarted(3000), qPrintable(client.errorString()));

    bool sawToplevelItem = false;
    auto checkItem = [&]() {
        auto *clientLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibClientLayer"));
        if (!clientLayer) {
            return;
        }
        for (QQuickItem *delegateRoot : clientLayer->childItems()) {
            for (QQuickItem *child : delegateRoot->childItems()) {
                if (auto *item = qobject_cast<WXdgToplevelSurfaceItem *>(child)) {
                    if (item->surface() != nullptr && item->shellSurface() != nullptr) {
                        sawToplevelItem = true;
                        return;
                    }
                }
            }
        }
    };

    auto waitUntil = [&](auto &&pred, int timeoutMs) -> bool {
        if (pred()) {
            return true;
        }
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QTimer pollTimer;
        pollTimer.setInterval(10);
        QObject::connect(&pollTimer, &QTimer::timeout, &loop, [&]() {
            checkItem();
            if (pred()) {
                loop.quit();
            }
        });
        timeout.start(timeoutMs);
        pollTimer.start();
        loop.exec();
        return pred();
    };

    QVERIFY(waitUntil([&]() { return surfaceAddedSpy.count() >= 1; }, 5000));
    QVERIFY(waitUntil([&]() { checkItem(); return sawToplevelItem; }, 3000));
    QCOMPARE(adapter.surfaceAdapter()->surfaceCount(), 1);

    QVERIFY(waitUntil([&]() { return surfaceRemovedSpy.count() >= 1; }, 5000));
    QVERIFY(client.waitForFinished(3000));
    QCOMPARE(adapter.surfaceAdapter()->surfaceCount(), 0);

    rootObj.reset();
    adapter.shutdown();
    QVERIFY(!adapter.isRunning());
    QCOMPARE(adapter.outputAdapter()->outputCount(), 0);
    adapter.shutdown(); // Idempotente
}

int main(int argc, char *argv[]) {
    WServer::initializeQPA();
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication app(argc, argv);
    tst_WaylibBackend tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_WaylibBackend.moc"
