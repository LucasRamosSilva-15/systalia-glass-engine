#include <QtTest>

#include "compositor/waylib/WaylibCompositorAdapter.hpp"
#include "services/applications/AppRegistryService.hpp"
#include "services/applications/ProcessLauncherService.hpp"
#include "services/system/SystemTelemetryService.hpp"
#include "services/windows/WorkspaceManagerService.hpp"

#include <wcursor.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wseat.h>
#include <wserver.h>
#include <wxdgdecorationmanager.h>
#include <wxdgpopupsurface.h>
#include <wxdgpopupsurfaceitem.h>
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

WXdgPopupSurfaceItem *findPopupItemInLayer(QQuickItem *popupLayer) {
    if (!popupLayer) {
        return nullptr;
    }
    for (QQuickItem *delegateRoot : popupLayer->childItems()) {
        if (auto *direct = qobject_cast<WXdgPopupSurfaceItem *>(delegateRoot)) {
            return direct;
        }
        for (QQuickItem *child : delegateRoot->findChildren<QQuickItem *>()) {
            if (auto *item = qobject_cast<WXdgPopupSurfaceItem *>(child)) {
                return item;
            }
        }
    }
    return nullptr;
}

QQuickItem *findVisualChildByObjectName(QQuickItem *root, const QString &name) {
    if (!root) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *found = findVisualChildByObjectName(child, name)) {
            return found;
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
    engine.addImageProvider(QStringLiteral("desktopIcons"), new DesktopIconProvider());

    WaylibCompositorAdapter adapter;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/systalia/waylib/WaylibNestedShell.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> rootObj(component.createWithInitialProperties(
        {{QStringLiteral("compositorAdapter"), QVariant::fromValue(&adapter)}}));
    QVERIFY2(!rootObj.isNull(), qPrintable(component.errorString()));

    auto *renderWindow = rootObj->findChild<WOutputRenderWindow *>();
    QVERIFY(renderWindow != nullptr);

    // 1. Verifica TopBar V2, Dock V2, separação de camadas e backdrop real sem loop de feedback
    auto *contentLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("desktopContentLayer"));
    auto *backdropSource = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibDesktopBackdropSource"));
    auto *overlayBackdropSource = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibOverlayCompositeBackdropSource"));
    auto *chromeLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("shellChromeLayer"));
    auto *topBar = renderWindow->findChild<QQuickItem *>(QStringLiteral("topBar"));
    auto *dockV2 = renderWindow->findChild<QQuickItem *>(QStringLiteral("valoDockV2"));
    auto *clientLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibClientLayer"));
    QVERIFY2(contentLayer != nullptr, "desktopContentLayer não encontrado na shell nested");
    QVERIFY2(backdropSource != nullptr, "waylibDesktopBackdropSource não encontrado na shell nested");
    QVERIFY2(overlayBackdropSource != nullptr, "waylibOverlayCompositeBackdropSource não encontrado na shell nested");
    QVERIFY2(chromeLayer != nullptr, "shellChromeLayer não encontrado na shell nested");
    QVERIFY2(topBar != nullptr, "SystaliaTopBarV2 não encontrado na shell nested");
    QVERIFY2(dockV2 != nullptr, "SystaliaDockV2 não encontrado na shell nested");
    QVERIFY(clientLayer != nullptr);
    QCOMPARE(dockV2->property("dockItemCount").toInt(), 5);
    QCOMPARE(backdropSource->property("sourceItem").value<QQuickItem *>(), contentLayer);
    QCOMPARE(topBar->property("backdropSource").value<QQuickItem *>(), overlayBackdropSource);
    QCOMPARE(dockV2->property("backdropSource").value<QQuickItem *>(), overlayBackdropSource);
    QVERIFY(topBar->property("blurProfile").toInt() >= 1);
    QVERIFY(dockV2->property("blurProfile").toInt() >= 1);
    QVERIFY2(contentLayer->findChild<QQuickItem *>(QStringLiteral("topBar")) == nullptr,
             "Loop de feedback detectado: topBar dentro de desktopContentLayer");
    QVERIFY2(contentLayer->findChild<QQuickItem *>(QStringLiteral("valoDockV2")) == nullptr,
             "Loop de feedback detectado: valoDockV2 dentro de desktopContentLayer");

    // Verifica geometria integral da Dock dentro do viewport (sem corte inferior)
    const QPointF dockTopLeft = dockV2->mapToScene(QPointF(0.0, 0.0));
    const qreal dockBottom = dockTopLeft.y() + dockV2->height();
    QVERIFY2(dockTopLeft.y() >= 0.0, "Dock não pode sair pela borda superior");
    QVERIFY2(dockBottom <= renderWindow->height() - 8.0,
             qPrintable(QStringLiteral("Dock cortada na borda inferior: bottom=%1 windowHeight=%2")
                            .arg(dockBottom).arg(renderWindow->height())));

    // Verifica vínculo de WSeat / WCursor / WOutputLayout para entrada de mouse e teclado
    QVERIFY(adapter.seat() != nullptr);
    QVERIFY(adapter.cursor() != nullptr);
    QCOMPARE(adapter.seat()->cursor(), adapter.cursor());
    QCOMPARE(adapter.cursor()->layout(), adapter.outputAdapter()->outputLayout());
    QVERIFY(adapter.cursor()->isVisible());

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
    QCOMPARE(adapter.cursor()->eventWindow(), static_cast<QWindow *>(renderWindow));
    QCOMPARE(adapter.seat()->keyboardFocusWindow(), static_cast<QWindow *>(renderWindow));
    QVERIFY(adapter.xdgDecorationManager() != nullptr);
    QCOMPARE(adapter.xdgDecorationManager()->preferredMode(), WXdgDecorationManager::Server);
    // Socket isolado não deve sobrescrever WAYLAND_DISPLAY do ambiente pai
    QCOMPARE(qgetenv("WAYLAND_DISPLAY"), originalWaylandDisplay);

    QTRY_VERIFY(adapter.outputAdapter()->outputCount() >= 1);
    QTRY_COMPARE(adapter.outputAdapter()->primaryOutputSize(), QSize(1280, 760));
    QVERIFY(adapter.outputAdapter()->primaryOutput() != nullptr);
    QVERIFY(adapter.outputAdapter()->primaryOutput()->forceSoftwareCursor());
    QVERIFY(adapter.outputAdapter()->primaryOutput()->cursorList().contains(adapter.cursor()));
    auto *cursorLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibCursorOverlayLayer"));
    auto *cursorArrow = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibSoftwarePointer"));
    QVERIFY(cursorLayer != nullptr && cursorArrow != nullptr);
    adapter.cursor()->setPosition(QPointF(240.0, 180.0));
    QCOMPARE(adapter.cursor()->position(), QPointF(240.0, 180.0));
    QCOMPARE(cursorArrow->property("cursorState").toString(), QStringLiteral("default"));
    QCOMPARE(cursorArrow->property("hotspotX").toReal(), 1.0);
    QCOMPARE(cursorArrow->property("hotspotY").toReal(), 1.0);
    QCOMPARE(cursorArrow->position(), QPointF(239.0, 179.0));

    // Simula redimensionamento dinâmico do output nested (ex.: maximização WL-1 -> 1920x1040)
    QVERIFY(adapter.outputAdapter()->requestOutputResize(1920, 1040));
    QTRY_COMPARE(adapter.outputAdapter()->primaryOutputSize(), QSize(1920, 1040));
    QTRY_COMPARE(qRound(renderWindow->width()), 1920);
    QTRY_COMPARE(qRound(renderWindow->height()), 1040);
    QCOMPARE(qRound(contentLayer->width()), 1920);
    QCOMPARE(qRound(contentLayer->height()), 1040);
    QCOMPARE(backdropSource->property("textureSize").toSize(), QSize(1920, 1040));
    QCOMPARE(qRound(topBar->width()), 1920 - 16);
    QVERIFY(dockV2->mapToScene(QPointF(0.0, 0.0)).y() + dockV2->height() <= 1040.0 - 8.0);
    if (auto *viewport = renderWindow->findChild<WOutputViewport *>(QStringLiteral("waylibOutputViewport"))) {
        QCOMPARE(qRound(viewport->width()), 1920);
        QCOMPARE(qRound(viewport->height()), 1040);
    }

    // Restaura tamanho nominal 1280x760 para os cenários multi-janela
    QVERIFY(adapter.outputAdapter()->requestOutputResize(1280, 760));
    QTRY_COMPARE(adapter.outputAdapter()->primaryOutputSize(), QSize(1280, 760));
    QTRY_COMPARE(qRound(renderWindow->width()), 1280);
    QTRY_COMPARE(qRound(renderWindow->height()), 760);

    auto *surfaceAdapter = adapter.surfaceAdapter();
    QAbstractListModel *model = surfaceAdapter->windowModel();
    QVERIFY(model != nullptr);

    const QString clientBin = QCoreApplication::applicationDirPath() + QStringLiteral("/tst_waylib_test_client");
    QVERIFY2(QFile::exists(clientBin), qPrintable(clientBin));

    // 1b. Separação Control Center Window (Dock System Core) vs Quick Menu (TopBar),
    //     Z-order abaixo da Dock, remoção de waylibWindowStrip, wallpaper real, FPS ativo e ProcessLauncher
    {
        auto *controlCenter = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibControlCenterV2"));
        auto *materialWindow = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibMaterialWindow"));
        auto *filesWindow = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibFilesWindowV2"));
        auto *internalLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibInternalWindowsLayer"));
        auto *chromeLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("shellChromeLayer"));
        auto *wallpaperImg = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibWallpaperImage"));
        auto *wallpaperBackdrop = renderWindow->findChild<QObject *>(QStringLiteral("waylibWallpaperBackdropSource"));
        QVERIFY(controlCenter != nullptr);
        QVERIFY(materialWindow != nullptr);
        QVERIFY(filesWindow != nullptr);
        QVERIFY(internalLayer != nullptr);
        QVERIFY(chromeLayer != nullptr);
        QVERIFY(wallpaperImg != nullptr);
        QVERIFY(wallpaperBackdrop != nullptr);

        // waylibWindowStrip foi removido
        QVERIFY(renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibWindowStrip")) == nullptr);

        // SDE-KWIN | VALO branding permanece visível na TopBar
        auto *brandingLabel = topBar->findChild<QQuickItem *>(QStringLiteral("desktopBrandingLabel"));
        QVERIFY(brandingLabel != nullptr);
        QCOMPARE(brandingLabel->property("text").toString(), QStringLiteral("SDE-KWIN | VALO"));

        // Separação: Dock settingsRequested abre janela completa Systalia Control Center (SystaliaMaterialWindow)
        QVERIFY(!materialWindow->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dockV2, "settingsRequested"));
        QVERIFY(materialWindow->property("visible").toBool());
        QVERIFY(dockV2->property("settingsOpen").toBool());
        QCOMPARE(materialWindow->property("backdropSource").value<QObject *>(), backdropSource);
        QVERIFY(internalLayer->z() < chromeLayer->z());
        QVERIFY(materialWindow->z() < dockV2->z());
        QVERIFY(materialWindow->x() >= 0.0 && materialWindow->y() >= 60.0);
        QVERIFY(materialWindow->x() + materialWindow->width() <= 1280.0);
        QVERIFY(materialWindow->y() + materialWindow->height() <= 760.0);
        QVERIFY(!controlCenter->property("opened").toBool());

        // TopBar controlCenterRequested abre o menu rápido compacto sem prender sobreposição
        QVERIFY(QMetaObject::invokeMethod(topBar, "controlCenterRequested"));
        QVERIFY(controlCenter->property("opened").toBool());
        QVERIFY(controlCenter->property("visible").toBool());
        QCOMPARE(controlCenter->property("backdropSource").value<QObject *>(), overlayBackdropSource);
        QVERIFY(QMetaObject::invokeMethod(topBar, "controlCenterRequested"));
        QVERIFY(!controlCenter->property("opened").toBool());

        // FilesWindow V2 + MaterialWindow sobrepostas -> ValoInternalWindowBackdropV2 ativo!
        auto *internalBackdrop = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibInternalWindowBackdropV2"));
        QVERIFY(internalBackdrop != nullptr);
        QVERIFY(!filesWindow->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dockV2, "filesRequested"));
        QVERIFY(filesWindow->property("visible").toBool());
        QVERIFY(dockV2->property("filesVisible").toBool());
        QVERIFY(filesWindow->z() < dockV2->z());
        filesWindow->setX(120.0);
        materialWindow->setX(220.0);
        QTRY_VERIFY(internalBackdrop->property("compositeAvailable").toBool());
        QVERIFY(filesWindow->z() > materialWindow->z());
        QVERIFY(filesWindow->property("backdropSource").value<QObject *>() != backdropSource);
        QCOMPARE(materialWindow->property("backdropSource").value<QObject *>(), backdropSource);

        QVERIFY(QMetaObject::invokeMethod(dockV2, "settingsRequested"));
        QVERIFY(!materialWindow->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dockV2, "filesRequested"));
        QVERIFY(!filesWindow->property("visible").toBool());

        // Menu esquerdo da TopBar (SystaliaFlowMenu), AppRegistry, desktopIcons, NotificationHost e BrightnessDimmer
        auto *flowMenu = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibFlowMenu"));
        auto *notificationHost = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibNotificationHost"));
        auto *brightnessDimmer = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibSoftwareBrightnessDimmer"));
        QVERIFY(flowMenu != nullptr);
        QVERIFY(notificationHost != nullptr);
        QVERIFY(brightnessDimmer != nullptr);
        QVERIFY(engine.imageProvider(QStringLiteral("desktopIcons")) != nullptr);
        auto *appRegistry = engine.singletonInstance<AppRegistryService *>(
            QStringLiteral("Systalia.System"), QStringLiteral("AppRegistry"));
        QVERIFY(appRegistry != nullptr);
        QVERIFY(!flowMenu->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(topBar, "launcherRequested"));
        QVERIFY(flowMenu->property("opened").toBool());
        QVERIFY(flowMenu->property("visible").toBool());
        QCOMPARE(flowMenu->property("backdropSource").value<QObject *>(), overlayBackdropSource);
        QVERIFY(QMetaObject::invokeMethod(flowMenu, "close"));
        QVERIFY(!flowMenu->property("opened").toBool());

        // Protocolos Wayland obrigatórios publicados no registro do socket ativo
        {
            QProcess wlInfo;
            QProcessEnvironment infoEnv = QProcessEnvironment::systemEnvironment();
            infoEnv.insert(QStringLiteral("WAYLAND_DISPLAY"), adapter.socketName());
            wlInfo.setProcessEnvironment(infoEnv);
            wlInfo.start(QStringLiteral("wayland-info"));
            QVERIFY2(wlInfo.waitForStarted(2000), qPrintable(wlInfo.errorString()));
            QVERIFY(waitUntil([&]() { return wlInfo.state() == QProcess::NotRunning; }, 4000));
            QCOMPARE(wlInfo.exitStatus(), QProcess::NormalExit);
            QCOMPARE(wlInfo.exitCode(), 0);
            const QByteArray infoOut = wlInfo.readAllStandardOutput();
            QVERIFY(infoOut.contains("wl_data_device_manager"));
            QVERIFY(infoOut.contains("zwp_primary_selection_device_manager_v1"));
            QVERIFY(infoOut.contains("xdg_activation_v1"));
            QVERIFY(infoOut.contains("wp_fractional_scale_manager_v1"));
            QVERIFY(infoOut.contains("wp_cursor_shape_manager_v1"));
            QVERIFY(infoOut.contains("zwp_text_input_manager_v3"));
        }

        // Telemetria FPS > 0 após ciclos reais de renderEnd no WOutputRenderWindow
        auto *telemetry = engine.singletonInstance<SystemTelemetryService *>(
            QStringLiteral("Systalia.System"), QStringLiteral("SystemTelemetry"));
        QVERIFY(telemetry != nullptr);
        for (int i = 0; i < 6; ++i) {
            renderWindow->update();
            QCoreApplication::processEvents();
        }
        QTRY_VERIFY_WITH_TIMEOUT(telemetry->fps() > 0, 2500);

        auto *launcher = engine.singletonInstance<ProcessLauncherService *>(
            QStringLiteral("Systalia.System"), QStringLiteral("ProcessLauncher"));
        QVERIFY(launcher != nullptr);
        QTRY_COMPARE(launcher->waylandDisplay(), adapter.socketName());
        QTRY_VERIFY(launcher->isolatedNestedSession());
        const QProcessEnvironment nestedEnv = ProcessLauncherService::waylandProcessEnvironment(
            adapter.socketName(), true);
        QVERIFY(!nestedEnv.contains(QStringLiteral("DISPLAY")));
        QCOMPARE(nestedEnv.value(QStringLiteral("ELECTRON_OZONE_PLATFORM_HINT")), QStringLiteral("wayland"));
        const QStringList codeArgs = ProcessLauncherService::effectiveArgumentsForLaunch(
            QStringLiteral("code"), {}, adapter.socketName(), true);
        QVERIFY(std::any_of(codeArgs.cbegin(), codeArgs.cend(), [](const QString &arg) {
            return arg.startsWith(QStringLiteral("--user-data-dir="));
        }));
        QVERIFY(codeArgs.contains(QStringLiteral("--new-window")));
        QVERIFY(codeArgs.contains(QStringLiteral("--ozone-platform=wayland")));
        const int beforeAdded = surfaceAddedSpy.count();
        const int beforeRemoved = surfaceRemovedSpy.count();
        QVERIFY(launcher->launch(clientBin));
        QVERIFY(waitUntil([&]() { return surfaceAddedSpy.count() > beforeAdded; }, 5000));
        QCOMPARE(surfaceAdapter->surfaceCount(), 1);
        QVERIFY(surfaceAdapter->activeSurface() != nullptr);
        QVERIFY(waitUntil([&]() { return surfaceRemovedSpy.count() > beforeRemoved; }, 5000));
        QCOMPARE(surfaceAdapter->surfaceCount(), 0);
        surfaceAddedSpy.clear();
        surfaceRemovedSpy.clear();
    }

    // 2. Ciclo de cliente único + popup XDG: conecta, mapeia toplevel + XdgPopupSurfaceItem e desconecta
    {
        QSignalSpy popupAddedSpy(surfaceAdapter, &WaylibSurfaceAdapter::popupAdded);
        QSignalSpy popupRemovedSpy(surfaceAdapter, &WaylibSurfaceAdapter::popupRemoved);

        QProcess soloClient;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
        env.insert(QStringLiteral("WAYLAND_DISPLAY"), adapter.socketName());
        env.insert(QStringLiteral("SGE_TEST_CLIENT_MS"), QStringLiteral("1500"));
        env.insert(QStringLiteral("SGE_TEST_CLIENT_POPUP"), QStringLiteral("1"));
        soloClient.setProcessEnvironment(env);
        soloClient.start(clientBin);
        QVERIFY2(soloClient.waitForStarted(3000), qPrintable(soloClient.errorString()));

        QVERIFY(waitUntil([&]() { return surfaceAddedSpy.count() >= 1; }, 5000));
        auto *surf = qobject_cast<WXdgToplevelSurface *>(surfaceAdapter->surfaces().first());
        QVERIFY(surf != nullptr);
        QVERIFY(waitUntil([&]() { return findToplevelItemInLayer(clientLayer, surf) != nullptr; }, 3000));
        QCOMPARE(surfaceAdapter->surfaceCount(), 1);

        QVERIFY(waitUntil([&]() { return popupAddedSpy.count() >= 1; }, 5000));
        QVERIFY(surfaceAdapter->popupCount() >= 1);
        QVERIFY(surfaceAdapter->popupModel() != nullptr);
        auto *popupLayer = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibPopupLayer"));
        QVERIFY(popupLayer != nullptr);
        QVERIFY(waitUntil([&]() {
            return findPopupItemInLayer(popupLayer) != nullptr;
        }, 3000));

        QSignalSpy moveReqSpy(surfaceAdapter, &WaylibSurfaceAdapter::moveRequested);
        QSignalSpy resizeReqSpy(surfaceAdapter, &WaylibSurfaceAdapter::resizeRequested);
        Q_EMIT surf->requestMove(adapter.seat(), 1);
        Q_EMIT surf->requestResize(adapter.seat(), Qt::RightEdge | Qt::BottomEdge, 2);
        QCOMPARE(moveReqSpy.count(), 1);
        QCOMPARE(resizeReqSpy.count(), 1);

        QVERIFY(waitUntil([&]() { return surfaceRemovedSpy.count() >= 1; }, 5000));
        QVERIFY(waitUntil([&]() { return popupRemovedSpy.count() >= 1; }, 3000));
        QCOMPARE(surfaceAdapter->popupCount(), 0);
        QVERIFY(soloClient.state() == QProcess::NotRunning || soloClient.waitForFinished(3000));
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

    // Transparência inter-janelas para clientes Wayland: frame frontal B recebe estágio composto != wallpaperOnly
    auto *frameBInit = findToplevelItemInLayer(clientLayer, surfB)->parentItem()->parentItem();
    QVERIFY(frameBInit != nullptr);
    QTRY_VERIFY(frameBInit->property("backdropSource").value<QObject *>() != backdropSource);

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

    // Verifica moldura VALO Glass V2, subsurfaces habilitadas e botão fechar na SSD (geometria ativa)
    auto *toplevelItemA = findToplevelItemInLayer(clientLayer, surfA);
    QVERIFY(toplevelItemA != nullptr);
    QVERIFY(toplevelItemA->subsurfacesVisible());
    auto *frameA = toplevelItemA->parentItem()->parentItem();
    QVERIFY(frameA != nullptr);
    QVERIFY(frameA->property("radius").toReal() > 0.0);
    QVERIFY(frameA->findChild<QQuickItem *>(QStringLiteral("clientDecorationV2")) != nullptr);
    QVERIFY(frameA->findChild<QQuickItem *>(QStringLiteral("sgeTitleBar")) != nullptr);

    // 3b. Movimento livre sob Dock e TopBar (sem clip em waylibClientLayer e sem clamping de workarea)
    QVERIFY(!contentLayer->clip());
    frameA->setProperty("normalY", -48.0); // invade faixa da TopBar (sceneY = 12 < 53)
    QCOMPARE(frameA->property("normalY").toReal(), -48.0);
    QVERIFY(frameA->mapToScene(QPointF(0.0, 0.0)).y() < 53.0);
    frameA->setProperty("normalY", 500.0); // invade faixa da Dock (sceneY + height > height - 137)
    QCOMPARE(frameA->property("normalY").toReal(), 500.0);
    QVERIFY(frameA->mapToScene(QPointF(0.0, frameA->height())).y() > renderWindow->height() - 137.0);
    frameA->setProperty("normalY", 24.0);

    // 3c. Sistema de redimensionamento 8-direções em WaylibWindowFrame e janelas internas
    const QStringList eightEdges = {
        QStringLiteral("top"), QStringLiteral("bottom"), QStringLiteral("left"), QStringLiteral("right"),
        QStringLiteral("topLeft"), QStringLiteral("bottomRight"), QStringLiteral("topRight"), QStringLiteral("bottomLeft")
    };
    for (const QString &edgeName : eightEdges) {
        auto *handle = findVisualChildByObjectName(frameA, QStringLiteral("resize_") + edgeName);
        QVERIFY2(handle != nullptr, qPrintable(edgeName));
        QVERIFY(handle->acceptHoverEvents());
    }
    const qreal origAX = frameA->property("normalX").toReal();
    const qreal origAW = frameA->property("normalWidth").toReal();
    QVERIFY(QMetaObject::invokeMethod(frameA, "beginResize",
                                      Q_ARG(QVariant, static_cast<int>(Qt::LeftEdge)),
                                      Q_ARG(QVariant, QPointF(origAX, 100.0))));
    QCOMPARE(adapter.cursorShapeKey(), QStringLiteral("resize-ew"));
    QVERIFY(QMetaObject::invokeMethod(frameA, "updateResize",
                                      Q_ARG(QVariant, QPointF(origAX - 40.0, 100.0))));
    QCOMPARE(frameA->property("normalWidth").toReal(), origAW + 40.0);
    QCOMPARE(frameA->property("normalX").toReal(), origAX - 40.0);
    QVERIFY(QMetaObject::invokeMethod(frameA, "finishResize"));
    QCOMPARE(adapter.cursorShapeKey(), QStringLiteral("default"));

    auto *filesWindowItem = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibFilesWindowV2"));
    auto *materialWindowItem = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibMaterialWindow"));
    auto *filesHandles = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibFilesResizeHandles"));
    auto *materialHandles = renderWindow->findChild<QQuickItem *>(QStringLiteral("waylibMaterialResizeHandles"));
    QVERIFY(filesWindowItem != nullptr && materialWindowItem != nullptr);
    QVERIFY(filesHandles != nullptr && materialHandles != nullptr);
    for (const QString &edgeName : eightEdges) {
        QVERIFY(findVisualChildByObjectName(filesHandles, QStringLiteral("internalResize_") + edgeName) != nullptr);
        QVERIFY(findVisualChildByObjectName(materialHandles, QStringLiteral("internalResize_") + edgeName) != nullptr);
    }
    const qreal origFilesW = filesWindowItem->width();
    QVERIFY(QMetaObject::invokeMethod(filesHandles, "begin",
                                      Q_ARG(QVariant, static_cast<int>(Qt::RightEdge)),
                                      Q_ARG(QVariant, QPointF(origFilesW, 200.0))));
    QVERIFY(QMetaObject::invokeMethod(filesHandles, "move",
                                      Q_ARG(QVariant, static_cast<int>(Qt::RightEdge)),
                                      Q_ARG(QVariant, QPointF(origFilesW + 50.0, 200.0))));
    QCOMPARE(filesWindowItem->width(), origFilesW + 50.0);
    QVERIFY(QMetaObject::invokeMethod(filesHandles, "finish"));

    // 3d. Alternador de quadros no centro da SystaliaTopBarV2 (clique real na pílula + transição)
    auto *pill1 = findVisualChildByObjectName(topBar, QStringLiteral("workspacePillMouseArea_1"));
    auto *pill0 = findVisualChildByObjectName(topBar, QStringLiteral("workspacePillMouseArea_0"));
    QVERIFY(pill1 != nullptr && pill0 != nullptr);
    auto *wsMgr = WorkspaceManagerService::instance();
    QVERIFY(wsMgr != nullptr);
    QCOMPARE(wsMgr->currentWorkspace(), 0);
    QVERIFY(wsMgr->hasWindowsInWorkspace(0));
    QVERIFY(pill1->isVisible() && pill1->isEnabled());
    QCOMPARE(pill1->height(), 26.0);
    QCOMPARE(pill1->property("cursorShape").toInt(), static_cast<int>(Qt::PointingHandCursor));
    wsMgr->switchToWorkspace(1);
    QVERIFY(QMetaObject::invokeMethod(topBar, "workspaceSelected", Q_ARG(int, 1)));
    QTRY_COMPARE(wsMgr->currentWorkspace(), 1);
    QTRY_VERIFY(!wsMgr->isTransitioning());
    QVERIFY(!frameA->isVisible());
    wsMgr->switchToWorkspace(0);
    QVERIFY(QMetaObject::invokeMethod(topBar, "workspaceSelected", Q_ARG(int, 0)));
    QTRY_COMPARE(wsMgr->currentWorkspace(), 0);
    QTRY_VERIFY(!wsMgr->isTransitioning());
    QVERIFY(frameA->isVisible());

    // 3e. Motor de cursores dinâmicos VALO (11 estados, hotspots e precedência C++/QML)
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::ArrowCursor), QStringLiteral("default"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::PointingHandCursor), QStringLiteral("pointer"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::OpenHandCursor), QStringLiteral("grab"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::ClosedHandCursor), QStringLiteral("grabbing"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::SizeHorCursor), QStringLiteral("resize-ew"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::SizeVerCursor), QStringLiteral("resize-ns"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::SizeFDiagCursor), QStringLiteral("resize-nwse"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::SizeBDiagCursor), QStringLiteral("resize-nesw"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::IBeamCursor), QStringLiteral("text"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::BusyCursor), QStringLiteral("busy"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::ForbiddenCursor), QStringLiteral("not-allowed"));

    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::Pointer), QStringLiteral("pointer"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::EWResize), QStringLiteral("resize-ew"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::NSResize), QStringLiteral("resize-ns"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::NWSEResize), QStringLiteral("resize-nwse"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::NESWResize), QStringLiteral("resize-nesw"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::Text), QStringLiteral("text"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::Wait), QStringLiteral("busy"));
    QCOMPARE(WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape::NotAllowed), QStringLiteral("not-allowed"));

    adapter.cursor()->setPosition(QPointF(300.0, 220.0));
    adapter.setCompositorCursorOverride(QStringLiteral("resize-ew"));
    QCOMPARE(cursorArrow->property("cursorState").toString(), QStringLiteral("resize-ew"));
    QCOMPARE(cursorArrow->property("hotspotX").toReal(), 13.0);
    QCOMPARE(cursorArrow->property("hotspotY").toReal(), 12.0);
    QCOMPARE(cursorArrow->position(), QPointF(287.0, 208.0));
    adapter.setCompositorCursorOverride(QStringLiteral("pointer"));
    QCOMPARE(cursorArrow->property("hotspotX").toReal(), 7.0);
    QCOMPARE(cursorArrow->property("hotspotY").toReal(), 2.0);
    adapter.setCompositorCursorOverride(QStringLiteral("busy"));
    QCOMPARE(cursorArrow->property("cursorState").toString(), QStringLiteral("busy"));
    adapter.setCompositorCursorOverride(QString());
    QCOMPARE(cursorArrow->property("cursorState").toString(), QStringLiteral("default"));

    // Precedência hoveredCursorKey -> compositorCursorOverride -> default
    adapter.setHoveredCursorKey(QStringLiteral("resize-nesw"));
    QCOMPARE(adapter.cursorShapeKey(), QStringLiteral("resize-nesw"));
    QCOMPARE(cursorArrow->property("cursorState").toString(), QStringLiteral("resize-nesw"));
    adapter.setCompositorCursorOverride(QStringLiteral("grabbing"));
    QCOMPARE(adapter.cursorShapeKey(), QStringLiteral("grabbing"));
    adapter.setCompositorCursorOverride(QString());
    QCOMPARE(adapter.cursorShapeKey(), QStringLiteral("resize-nesw"));
    adapter.setHoveredCursorKey(QString());
    QCOMPARE(adapter.cursorShapeKey(), QStringLiteral("default"));

    auto *closeBtn = frameA->findChild<QQuickItem *>(QStringLiteral("closeToplevelButton"));
    QVERIFY(closeBtn != nullptr);
    QTRY_VERIFY(frameA->width() < 1000.0
                && closeBtn->mapToItem(frameA, QPointF(0.0, 0.0)).x() > 100.0
                && closeBtn->mapToItem(frameA, QPointF(0.0, 0.0)).x() < frameA->width());
    QVERIFY(closeBtn->isVisible());
    const QPointF closeCenter = closeBtn->mapToScene(closeBtn->boundingRect().center());
    QVERIFY(closeCenter.x() > 0.0 && closeCenter.y() > 0.0);
    adapter.cursor()->setPosition(closeCenter);
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
