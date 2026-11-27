#include <QtTest>

#include "compositor/waylib/WaylibCompositorAdapter.hpp"

#include <woutputrenderwindow.h>
#include <wseat.h>
#include <wserver.h>
#include <wxdgtoplevelsurface.h>
#include <wxdgtoplevelsurfaceitem.h>

#include <QAbstractListModel>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QSignalSpy>

using namespace Systalia::Compositor::WaylibBackend;
WAYLIB_SERVER_USE_NAMESPACE

namespace {

WXdgToplevelSurfaceItem *findToplevelItemInLayer(QQuickItem *clientLayer, WXdgToplevelSurface *target) {
    if (!clientLayer) {
        return nullptr;
    }
    for (QQuickItem *delegateRoot : clientLayer->childItems()) {
        for (QQuickItem *child : delegateRoot->findChildren<QQuickItem *>()) {
            if (auto *item = qobject_cast<WXdgToplevelSurfaceItem *>(child)) {
                if (item->shellSurface() == target) {
                    return item;
                }
            }
        }
    }
    return nullptr;
}

template <typename Pred>
bool waitUntil(Pred &&pred, int timeoutMs) {
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
        if (pred()) {
            loop.quit();
        }
    });
    timeout.start(timeoutMs);
    pollTimer.start();
    loop.exec();
    return pred();
}

} // namespace

class tst_WaylibBackend final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void initializationFailsGracefullyOnInvalidState();
    void headlessAndMultiWindowValoShellLifecycle();
};

void tst_WaylibBackend::initTestCase() {
    WaylibCompositorAdapter::registerQmlTypes();
}

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

void tst_WaylibBackend::headlessAndMultiWindowValoShellLifecycle() {
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

    // 1. Verifica TopBar V2 e Dock V2 reais carregados diretamente na árvore QML
    auto *topBar = renderWindow->findChild<QQuickItem *>(QStringLiteral("topBar"));
    auto *dockV2 = renderWindow->findChild<QQuickItem *>(QStringLiteral("valoDockV2"));
    auto *clientLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibClientLayer"));
    QVERIFY2(topBar != nullptr, "SystaliaTopBarV2 não encontrado na shell nested");
    QVERIFY2(dockV2 != nullptr, "SystaliaDockV2 não encontrado na shell nested");
    QVERIFY(clientLayer != nullptr);
    QCOMPARE(dockV2->property("dockItemCount").toInt(), 5);

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

    auto *surfaceAdapter = adapter.surfaceAdapter();
    QAbstractListModel *model = surfaceAdapter->windowModel();
    QVERIFY(model != nullptr);

    const QString clientBin = QCoreApplication::applicationDirPath() + QStringLiteral("/tst_waylib_test_client");
    QVERIFY2(QFile::exists(clientBin), qPrintable(clientBin));

    // 2. Ciclo de cliente único: conecta, mapeia XdgToplevelSurfaceItem e desconecta
    {
        QProcess soloClient;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
        env.insert(QStringLiteral("WAYLAND_DISPLAY"), adapter.socketName());
        env.insert(QStringLiteral("SGE_TEST_CLIENT_MS"), QStringLiteral("800"));
        soloClient.setProcessEnvironment(env);
        soloClient.start(clientBin);
        QVERIFY2(soloClient.waitForStarted(3000), qPrintable(soloClient.errorString()));

        QVERIFY(waitUntil([&]() { return surfaceAddedSpy.count() >= 1; }, 5000));
        auto *surf = qobject_cast<WXdgToplevelSurface *>(surfaceAdapter->surfaces().first());
        QVERIFY(surf != nullptr);
        QVERIFY(waitUntil([&]() { return findToplevelItemInLayer(clientLayer, surf) != nullptr; }, 3000));
        QCOMPARE(surfaceAdapter->surfaceCount(), 1);

        QVERIFY(waitUntil([&]() { return surfaceRemovedSpy.count() >= 1; }, 5000));
        QVERIFY(soloClient.waitForFinished(3000));
        QCOMPARE(surfaceAdapter->surfaceCount(), 0);
        QVERIFY(surfaceAdapter->activeSurface() == nullptr);
    }

    // 3. Múltiplas janelas simultâneas (Cliente A + Cliente B): foco, z-order, TopBar, Dock, fechar
    QProcess clientA;
    QProcess clientB;

    auto startClient = [&](QProcess &proc, const QString &title, const QString &appId) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
        env.insert(QStringLiteral("WAYLAND_DISPLAY"), adapter.socketName());
        env.insert(QStringLiteral("SGE_TEST_CLIENT_TITLE"), title);
        env.insert(QStringLiteral("SGE_TEST_CLIENT_APP_ID"), appId);
        env.insert(QStringLiteral("SGE_TEST_CLIENT_MS"), QStringLiteral("0"));
        proc.setProcessEnvironment(env);
        proc.start(clientBin);
    };

    startClient(clientA, QStringLiteral("Window Alpha"), QStringLiteral("org.systalia.alpha"));
    QVERIFY2(clientA.waitForStarted(3000), qPrintable(clientA.errorString()));
    QVERIFY(waitUntil([&]() {
        return surfaceAdapter->surfaceCount() == 1
               && surfaceAdapter->activeTitle() == QStringLiteral("Window Alpha");
    }, 5000));

    auto *surfA = surfaceAdapter->activeSurface();
    QVERIFY(surfA != nullptr);

    startClient(clientB, QStringLiteral("Window Beta"), QStringLiteral("org.systalia.beta"));
    QVERIFY2(clientB.waitForStarted(3000), qPrintable(clientB.errorString()));
    QVERIFY(waitUntil([&]() {
        return surfaceAdapter->surfaceCount() == 2
               && surfaceAdapter->activeTitle() == QStringLiteral("Window Beta")
               && surfaceAdapter->activeAppId() == QStringLiteral("org.systalia.beta");
    }, 5000));

    auto *surfB = surfaceAdapter->activeSurface();
    QVERIFY(surfB != nullptr && surfB != surfA);

    QCOMPARE(model->rowCount(), 2);
    QVERIFY(waitUntil([&]() {
        return findToplevelItemInLayer(clientLayer, surfA) != nullptr
               && findToplevelItemInLayer(clientLayer, surfB) != nullptr;
    }, 3000));

    // Cliente B deve estar focado e acima de A no z-order; TopBar reflete título ativo
    QVERIFY(surfB->isActivated());
    QVERIFY(!surfA->isActivated());
    QVERIFY(surfaceAdapter->zOrderForSurface(surfB) > surfaceAdapter->zOrderForSurface(surfA));
    QCOMPARE(topBar->property("activeWindowTitle").toString(), QStringLiteral("Window Beta"));
    if (adapter.seat()) {
        QCOMPARE(adapter.seat()->keyboardFocusSurface(), surfB->surface());
    }

    // Alterna foco para A e verifica elevação, seat focus e TopBar
    QSignalSpy activeSpy(surfaceAdapter, &WaylibSurfaceAdapter::activeSurfaceChanged);
    surfaceAdapter->activateSurface(surfA);
    QCOMPARE(surfaceAdapter->activeSurface(), surfA);
    QVERIFY(waitUntil([&]() { return surfA->isActivated() && !surfB->isActivated(); }, 3000));
    QVERIFY(surfaceAdapter->zOrderForSurface(surfA) > surfaceAdapter->zOrderForSurface(surfB));
    QCOMPARE(topBar->property("activeWindowTitle").toString(), QStringLiteral("Window Alpha"));
    QVERIFY(activeSpy.count() >= 1);
    if (adapter.seat()) {
        QCOMPARE(adapter.seat()->keyboardFocusSurface(), surfA->surface());
    }

    // Minimizar A transfere foco para B; focusOrRestore(A) restaura e refoca A
    surfaceAdapter->toggleMinimize(surfA);
    QVERIFY(surfaceAdapter->isMinimized(surfA));
    QCOMPARE(surfaceAdapter->activeSurface(), surfB);
    QVERIFY(waitUntil([&]() { return surfB->isActivated(); }, 3000));
    surfaceAdapter->focusOrRestore(surfA);
    QVERIFY(!surfaceAdapter->isMinimized(surfA));
    QCOMPARE(surfaceAdapter->activeSurface(), surfA);
    QVERIFY(waitUntil([&]() { return surfA->isActivated(); }, 3000));

    // Maximizar e restaurar A
    surfaceAdapter->toggleMaximize(surfA);
    QVERIFY(surfaceAdapter->isMaximized(surfA));
    surfaceAdapter->toggleMaximize(surfA);
    QVERIFY(!surfaceAdapter->isMaximized(surfA));

    // Solicita fechamento de A: apenas A encerra, foco volta para B
    surfaceAdapter->requestClose(surfA);
    QVERIFY(waitUntil([&]() {
        return surfaceAdapter->surfaceCount() == 1
               && clientA.state() == QProcess::NotRunning;
    }, 5000));
    QCOMPARE(clientA.exitStatus(), QProcess::NormalExit);
    QCOMPARE(clientA.exitCode(), 0);
    QCOMPARE(surfaceAdapter->activeSurface(), surfB);
    QVERIFY(waitUntil([&]() { return surfB->isActivated(); }, 3000));
    QCOMPARE(topBar->property("activeWindowTitle").toString(), QStringLiteral("Window Beta"));

    // 4. Encerra compositor ainda com cliente B conectado (sem crash nem use-after-free) + shutdown idempotente
    rootObj.reset();
    adapter.shutdown();
    QVERIFY(!adapter.isRunning());
    QCOMPARE(adapter.outputAdapter()->outputCount(), 0);
    adapter.shutdown();
    if (clientB.state() != QProcess::NotRunning) {
        if (!clientB.waitForFinished(3000)) {
            clientB.terminate();
            QVERIFY(clientB.waitForFinished(2000));
        }
    }
}

int main(int argc, char *argv[]) {
    WServer::initializeQPA();
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication app(argc, argv);
    tst_WaylibBackend tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_WaylibBackend.moc"
