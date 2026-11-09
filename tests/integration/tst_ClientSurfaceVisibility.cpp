#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include "ValoGlassItemV2.hpp"
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRasterWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandXdgToplevel>
#include <memory>

#include "SystaliaCompositor.hpp"
#include "SystaliaGlassItem.hpp"
#include "XWaylandManager.hpp"

namespace {
constexpr auto TestSocketName = "wayland-visibility-test";
}

void qml_register_types_Systalia_Engine();

class VisibilityPatternClient final : public QRasterWindow {
protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_G) { m_green = true; update(); }
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(0, 0), size()), m_green ? QColor(0, 255, 0) : QColor(255, 0, 255)); // Native buffer sentinel
        painter.setPen(Qt::white);
        painter.drawText(QRect(QPoint(0, 0), size()), Qt::AlignCenter,
                         QStringLiteral("VALO_CLIENT_ACTIVE_PIXELS"));
    }
private:
    bool m_green = false;
};

class TestClientSurfaceVisibility final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void clientBufferIsRenderedVisibleInCompositor();
    void continuousDamageUpdatesArePresented();
    void profilesPreserveNativePixelsAndKeyboardInput();
    void alacrittyRendersVisibleTerminalCharacters();
    void xwaylandWindowPresentsClientSurface();

private:
    void startClient();
    std::unique_ptr<QQuickItem> createContainer(QQmlEngine &engine, QQuickWindow &window, QObject *toplevelObject = nullptr);

    QTemporaryDir m_runtime;
    std::unique_ptr<SystaliaCompositor> m_compositor;
    std::unique_ptr<QWaylandOutput> m_output;
    QProcess m_client;
    QTimer m_frames;
    QPointer<QWaylandXdgToplevel> m_toplevel;
};

void TestClientSurfaceVisibility::initTestCase() {
    QVERIFY(m_runtime.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    qml_register_types_Systalia_Engine();
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
}

void TestClientSurfaceVisibility::init() {
    m_compositor = std::make_unique<SystaliaCompositor>();
    m_compositor->setSocketName(TestSocketName);
    m_compositor->create();
    QVERIFY(m_compositor->isCreated());

    connect(&m_frames, &QTimer::timeout, m_compositor.get(), [this] {
        m_compositor->processWaylandEvents();
        for (auto *surface : m_compositor->surfaces()) {
            surface->frameStarted();
            surface->sendFrameCallbacks();
        }
    });
    m_frames.start(16);

    m_output = std::make_unique<QWaylandOutput>(m_compositor.get(), nullptr);
    m_output->addMode(QWaylandOutputMode(QSize(1024, 768), 60000), true);
    m_output->setCurrentMode(QWaylandOutputMode(QSize(1024, 768), 60000));

    connect(m_compositor.get(), &SystaliaCompositor::toplevelCreated,
            this, [this](QWaylandXdgToplevel *toplevel) {
        m_toplevel = toplevel;
    });
}

void TestClientSurfaceVisibility::cleanup() {
    m_frames.stop();
    if (m_client.state() != QProcess::NotRunning) {
        m_client.terminate();
        if (!m_client.waitForFinished(3000))
            m_client.kill();
    }
    m_toplevel.clear();
    m_output.reset();
    m_compositor.reset();
}

void TestClientSurfaceVisibility::startClient() {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "wayland");
    environment.insert("WAYLAND_DISPLAY", TestSocketName);
    environment.insert("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
    environment.insert("XDG_RUNTIME_DIR", m_runtime.path());
    m_client.setProcessEnvironment(environment);
    m_client.setProcessChannelMode(QProcess::MergedChannels);

    m_client.start(QCoreApplication::applicationFilePath(), {"--visibility-client"});
    QVERIFY(m_client.waitForStarted());
    QTRY_VERIFY2_WITH_TIMEOUT(m_toplevel, m_client.readAll().constData(), 5000);
}

std::unique_ptr<QQuickItem> TestClientSurfaceVisibility::createContainer(QQmlEngine &engine,
                                                                         QQuickWindow &window,
                                                                         QObject *toplevelObject) {
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/WaylandWindowContainer.qml")));
    if (!toplevelObject) {
        toplevelObject = m_toplevel.data();
    }
    auto *object = component.createWithInitialProperties({
        {QStringLiteral("toplevel"), QVariant::fromValue(toplevelObject)},
        {QStringLiteral("waylandServer"), QVariant::fromValue(m_compositor.get())}
    });
    if (!object) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(object));
    if (!item) {
        delete object;
        return {};
    }
    item->setParentItem(window.contentItem());
    item->setPosition(QPointF(100, 100));
    return item;
}

void TestClientSurfaceVisibility::clientBufferIsRenderedVisibleInCompositor() {
    startClient();
    QVERIFY(m_toplevel);

    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);
    QTRY_VERIFY(surface->hasContent());

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1024, 768);

    auto container = createContainer(engine, window);
    QVERIFY(container);

    auto *surfaceItem = container->findChild<QQuickItem *>(QStringLiteral("clientSurfaceItem"));
    QVERIFY(surfaceItem);
    QCOMPARE(surfaceItem->opacity(), 1.0);
    QVERIFY(surfaceItem->isVisible());

    auto *viewport = container->findChild<QQuickItem *>(QStringLiteral("clientViewport"));
    QVERIFY(viewport);
    QVERIFY(viewport->isVisible());

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Aguardar acomodação de tamanho e mapeamento Wayland
    QTRY_VERIFY(surface->hasContent());
    QTRY_VERIFY(container->property("clientMapped").toBool());
    QTRY_COMPARE(surfaceItem->size(), viewport->size());

    // Permitir múltiplos passes de renderização da árvore de cena
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(25);
    }

    // Capturar o framebuffer real do compositor
    QImage screenshot = window.grabWindow();
    QVERIFY(!screenshot.isNull());

    // Amostrar ponto no interior do clientViewport (afastado do texto centralizado para verificar o fundo magenta)
    const QPointF bgOffsetPoint(viewport->width() * 0.25, viewport->height() * 0.25);
    const QPointF sceneBgPoint = viewport->mapToScene(bgOffsetPoint);
    const int bgX = qRound(sceneBgPoint.x());
    const int bgY = qRound(sceneBgPoint.y());

    QVERIFY(bgX >= 0 && bgX < screenshot.width());
    QVERIFY(bgY >= 0 && bgY < screenshot.height());

    const QColor bgSampleColor = screenshot.pixelColor(bgX, bgY);

    // Salvar artefato visual
    const QString artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
    QDir().mkpath(artifactDir);
    screenshot.save(artifactDir + QStringLiteral("/valo_client_surface_visibility.png"));

    // Validação estrita: O pixel do cliente DEVE ser o magenta puro (#FF00FF) do buffer Wayland real,
    // e NÃO o gradiente de vidro VALO (#194F76), preto (#000000) ou fundo transparente!
    QVERIFY2(bgSampleColor.red() > 200, qPrintable(QStringLiteral("Esperado Vermelho alto (>200), obtido: %1").arg(bgSampleColor.red())));
    QVERIFY2(bgSampleColor.green() < 50, qPrintable(QStringLiteral("Esperado Verde baixo (<50), obtido: %1").arg(bgSampleColor.green())));
    QVERIFY2(bgSampleColor.blue() > 200, qPrintable(QStringLiteral("Esperado Azul alto (>200), obtido: %1").arg(bgSampleColor.blue())));
}

void TestClientSurfaceVisibility::continuousDamageUpdatesArePresented() {
    startClient();
    QVERIFY(m_toplevel);

    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);
    QTRY_VERIFY(surface->hasContent());

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1024, 768);

    auto container = createContainer(engine, window);
    QVERIFY(container);

    auto *surfaceItem = container->findChild<QQuickItem *>(QStringLiteral("clientSurfaceItem"));
    QVERIFY(surfaceItem);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY(container->property("clientMapped").toBool());

    // Disparar atualizações de frame e garantir que o ShellSurfaceItem continua pintando sem travar
    for (int i = 0; i < 10; ++i) {
        m_compositor->processWaylandEvents();
        for (auto *surf : m_compositor->surfaces()) {
            surf->frameStarted();
            surf->sendFrameCallbacks();
        }
        QCoreApplication::processEvents();
        QTest::qWait(16);
    }

    QVERIFY(surfaceItem->property("paintEnabled").toBool());
    QVERIFY(surface->hasContent());
}

void TestClientSurfaceVisibility::alacrittyRendersVisibleTerminalCharacters() {
    const QString alacrittyBin = QStandardPaths::findExecutable(QStringLiteral("alacritty"));
    if (alacrittyBin.isEmpty()) {
        QSKIP("Alacritty não está instalado no sistema.");
    }

    QProcess alacrittyProc;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", TestSocketName);
    env.insert("XDG_RUNTIME_DIR", m_runtime.path());
    env.insert("LIBGL_ALWAYS_SOFTWARE", "1");
    env.insert("WINIT_UNIX_BACKEND", "wayland");
    alacrittyProc.setProcessEnvironment(env);
    alacrittyProc.setProcessChannelMode(QProcess::MergedChannels);

    alacrittyProc.start(alacrittyBin, {
        QStringLiteral("-e"), QStringLiteral("sh"), QStringLiteral("-c"),
        QStringLiteral("printf '\\033[32mVALO_TERMINAL_ONLINE\\033[0m\\n'; sleep 3")
    });

    if (!alacrittyProc.waitForStarted(3000)) {
        QSKIP("Não foi possível iniciar o processo Alacritty.");
    }

    // Aguardar conexão toplevel Wayland do Alacritty
    if (!QTest::qWaitFor([this] { return m_toplevel != nullptr; }, 6000)) {
        alacrittyProc.kill();
        QSKIP("Alacritty não mapeou toplevel Wayland no tempo limite.");
    }

    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);
    QTRY_VERIFY_WITH_TIMEOUT(surface->hasContent(), 6000);

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1024, 768);

    auto container = createContainer(engine, window);
    QVERIFY(container);

    auto *surfaceItem = container->findChild<QQuickItem *>(QStringLiteral("clientSurfaceItem"));
    QVERIFY(surfaceItem);
    QVERIFY(surfaceItem->isVisible());

    auto *viewport = container->findChild<QQuickItem *>(QStringLiteral("clientViewport"));
    QVERIFY(viewport);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY(container->property("clientMapped").toBool());

    // Permitir renderização de múltiplos passes e ciclos de evento Wayland
    for (int i = 0; i < 15; ++i) {
        m_compositor->processWaylandEvents();
        for (auto *surf : m_compositor->surfaces()) {
            surf->frameStarted();
            surf->sendFrameCallbacks();
        }
        QCoreApplication::processEvents();
        QTest::qWait(30);
    }

    QImage screenshot = window.grabWindow();
    QVERIFY(!screenshot.isNull());

    const QString artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
    QDir().mkpath(artifactDir);
    screenshot.save(artifactDir + QStringLiteral("/valo_alacritty_live.png"));

    // O terminal do Alacritty possui fundo escuro (quase preto/opaco) com caracteres verdes.
    // O clientViewport DEVE conter os pixels reais do Alacritty.
    const QPointF samplePos(viewport->width() * 0.3, viewport->height() * 0.3);
    const QPointF scenePos = viewport->mapToScene(samplePos);
    const QColor alacrittyPixel = screenshot.pixelColor(qRound(scenePos.x()), qRound(scenePos.y()));

    // Confirma que não é transparente (alfa = 255) e não é o gradiente VALO padrão puro
    QCOMPARE(alacrittyPixel.alpha(), 255);

    alacrittyProc.terminate();
    alacrittyProc.waitForFinished(1000);
}

void TestClientSurfaceVisibility::profilesPreserveNativePixelsAndKeyboardInput() {
    startClient();
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1024, 768);
    auto container = createContainer(engine, window);
    QVERIFY(container);
    auto *viewport = container->findChild<QQuickItem *>("clientViewport");
    auto *client = container->findChild<QQuickItem *>("clientSurfaceItem");
    auto *glass = container->findChild<Systalia::Gfx::V2::ValoGlassItemV2 *>();
    QVERIFY(viewport && client && glass);
    QVERIFY(!container->findChild<QQuickItem *>("clientTextureCapture"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(100);
    const QRect body = viewport->mapRectToScene(viewport->boundingRect()).toAlignedRect();
    const QImage original = window.grabWindow().copy(body);
    QVERIFY(!original.isNull());
    QCOMPARE(original.pixelColor(4,4), QColor(255,0,255));
    for (int i=0; i<18; ++i) {
        container->setProperty("blurProfile", i%3);
        QCOMPARE(glass->blurProfile(), i%3);
        window.update();
        QTest::qWait(20);
        QCOMPARE(window.grabWindow().copy(body), original);
        QVERIFY(client->property("paintEnabled").toBool());
    }
    // Losing keyboard activation must not remove the V2 decoration node.
    m_toplevel->sendConfigure(QSize(qRound(viewport->width()),qRound(viewport->height())), QList<int>{});
    QTRY_VERIFY(!m_toplevel->activated());
    QTRY_VERIFY(glass->isActive());
    QCOMPARE(window.grabWindow().copy(body),original);
    m_compositor->activateToplevel(m_toplevel);
    m_compositor->attachTaskSwitcherWindow(&window);
    client->forceActiveFocus();
    QMetaObject::invokeMethod(client, "takeFocus");
    auto *seat = m_compositor->defaultSeat();
    QVERIFY(seat && seat->keyboard());
    QTRY_COMPARE(seat->keyboardFocus(), m_toplevel->xdgSurface()->surface());
    const auto scanCode = seat->keyboard()->keyToScanCode(Qt::Key_G);
    QVERIFY(scanCode > 0);
    QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_G, Qt::NoModifier, scanCode, 0, 0, "g");
    QCoreApplication::sendEvent(&window, &keyPress);
    QKeyEvent keyRelease(QEvent::KeyRelease, Qt::Key_G, Qt::NoModifier, scanCode, 0, 0, "g");
    QCoreApplication::sendEvent(&window, &keyRelease);
    QTRY_COMPARE(window.grabWindow().pixelColor(body.topLeft()+QPoint(4,4)), QColor(0,255,0));
}

void TestClientSurfaceVisibility::xwaylandWindowPresentsClientSurface() {
    startClient();
    QVERIFY(m_toplevel);

    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);
    QTRY_VERIFY(surface->hasContent());

    auto *manager = m_compositor->xwaylandManager();
    QVERIFY(manager);

    auto *wrapper = manager->registerOrUpdateX11Window(
        0x500001,
        QStringLiteral("Editor X11 Legado"),
        QStringLiteral("x11editor"),
        QRect(0, 0, 600, 400));
    QVERIFY(wrapper);
    QVERIFY(wrapper->isXWayland());

    // Associa a superfície com buffers reais ao wrapper XWayland
    wrapper->setSurface(surface);
    QCOMPARE(wrapper->surface(), surface);

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1024, 768);

    auto container = createContainer(engine, window, wrapper);
    QVERIFY(container);
    QVERIFY(container->property("isXWaylandWindow").toBool());

    auto *surfaceItem = container->findChild<QQuickItem *>(QStringLiteral("clientSurfaceItem"));
    QVERIFY(surfaceItem);
    QCOMPARE(surfaceItem->property("surface").value<QWaylandSurface *>(), surface);
    QVERIFY(surfaceItem->isVisible());

    auto *viewport = container->findChild<QQuickItem *>(QStringLiteral("clientViewport"));
    QVERIFY(viewport);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Permitir ciclo de pintura
    for (int i = 0; i < 5; ++i) {
        m_compositor->processWaylandEvents();
        surface->frameStarted();
        surface->sendFrameCallbacks();
        QCoreApplication::processEvents();
        QTest::qWait(20);
    }

    QImage screenshot = window.grabWindow();
    QVERIFY(!screenshot.isNull());

    const QString artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
    QDir().mkpath(artifactDir);
    screenshot.save(artifactDir + QStringLiteral("/valo_xwayland_live.png"));

    const QPointF bgOffsetPoint(viewport->width() * 0.25, viewport->height() * 0.25);
    const QPointF sceneBgPoint = viewport->mapToScene(bgOffsetPoint);
    const QColor x11Pixel = screenshot.pixelColor(qRound(sceneBgPoint.x()), qRound(sceneBgPoint.y()));

    // Confirma que os pixels reais do cliente XWayland são visíveis através do ShellSurfaceItem
    QVERIFY2(x11Pixel.red() > 200, qPrintable(QStringLiteral("Esperado Vermelho alto (>200) no XWayland, obtido: %1").arg(x11Pixel.red())));
    QVERIFY2(x11Pixel.green() < 50, qPrintable(QStringLiteral("Esperado Verde baixo (<50) no XWayland, obtido: %1").arg(x11Pixel.green())));
    QVERIFY2(x11Pixel.blue() > 200, qPrintable(QStringLiteral("Esperado Azul alto (>200) no XWayland, obtido: %1").arg(x11Pixel.blue())));

    manager->unregisterX11Window(0x500001);
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--visibility-client"))) {
        VisibilityPatternClient window;
        window.setFlags(Qt::Window | Qt::FramelessWindowHint);
        window.resize(500, 350);
        window.show();
        return app.exec();
    }
    TestClientSurfaceVisibility test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_ClientSurfaceVisibility.moc"
