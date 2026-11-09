#include <QGuiApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRasterWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QtWaylandCompositor/QWaylandOutput>
#include <memory>

#include "SystaliaCompositor.hpp"
#include "SystaliaGlassItem.hpp"
#include "ValoGlassItemV2.hpp"
#include "InputDeviceManager.hpp"
#include <QtWaylandCompositor/QWaylandPointer>
#include <QtWaylandCompositor/QWaylandQuickShellSurfaceItem>

// An independent SHM Wayland client: Qt receives configure, acknowledges it,
// updates xdg window geometry and commits the newly sized buffer.
void qml_register_types_Systalia_Engine();

class ResizeClient final : public QRasterWindow {
    void closeEvent(QCloseEvent *event) override {
        if (QCoreApplication::arguments().contains("--ignore-close"))
            event->ignore();
        else
            QRasterWindow::closeEvent(event);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(0, 0), size()), QColor("#125578"));
    }
};

class TestXdgResize : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void nullAndInvokable();
    void closeRequestUsesCompositorSlot();
    void cancelledCloseKeepsClientUsable();
    void nativePointerMapsIntoMovedClient();
    void popupIsPresentedOutsideTheCapturedClient();
    void minimizeButtonHidesAndActivationRestoresToplevel();
    void maximizeRequestUsesCompositorSlot();
    void activationSetsSeatKeyboardFocus();
    void clientBufferIsBoundAndPainted();
    void unmappedClientDoesNotLeaveAnEmptyFrame();
    void configureHandshake_data();
    void configureHandshake();
    void dragEdges_data();
    void dragEdges();
    void resizeHonorsMinimumFrameGeometry();
    void lostMouseGrabCancelsResize();
    void finishResizeIsIdempotent();
    void clientMinimumConstraintClampsConfigure();
    void clientMaximumConstraintClampsConfigure();
private:
    std::unique_ptr<QTemporaryDir> m_runtime;
    std::unique_ptr<SystaliaCompositor> m_compositor;
    std::unique_ptr<QWaylandOutput> m_output;
    QProcess m_client;
    QTimer m_frames;
    QPointer<QWaylandXdgToplevel> m_toplevel;
    void startClient(bool bounded = false, bool unmapAfterMap = false, bool ignoreClose = false, bool withPopup = false);
    std::unique_ptr<QQuickItem> createContainer(QQmlEngine &engine, QQuickWindow &window,
                                                QObject *toplevelObject = nullptr);
};

void TestXdgResize::initTestCase() {
    m_runtime = std::make_unique<QTemporaryDir>();
    QVERIFY(m_runtime->isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime->path().toUtf8());
    qml_register_types_Systalia_Engine();
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
}

void TestXdgResize::init() {
    m_compositor = std::make_unique<SystaliaCompositor>();
    m_compositor->setSocketName("wayland-resize-test");
    m_compositor->create();
    QVERIFY(m_compositor->isCreated());
    // Protocol-only fixtures have no render loop to complete client frame callbacks.
    connect(&m_frames, &QTimer::timeout, m_compositor.get(), [this] {
        m_compositor->processWaylandEvents();
        for (auto *surface : m_compositor->surfaces()) {
            surface->frameStarted();
            surface->sendFrameCallbacks();
        }
    });
    m_frames.start(16);
    m_output = std::make_unique<QWaylandOutput>(m_compositor.get(), nullptr);
    m_output->addMode(QWaylandOutputMode(QSize(1400, 1000), 60000), true);
    m_output->setCurrentMode(QWaylandOutputMode(QSize(1400, 1000), 60000));
    connect(m_compositor.get(), &SystaliaCompositor::toplevelCreated,
            this, [this](QWaylandXdgToplevel *toplevel) { m_toplevel = toplevel; });
}

void TestXdgResize::startClient(bool bounded, bool unmapAfterMap, bool ignoreClose, bool withPopup) {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "wayland");
    environment.insert("WAYLAND_DISPLAY", "wayland-resize-test");
    environment.insert("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
    m_client.setProcessEnvironment(environment);
    m_client.setProcessChannelMode(QProcess::MergedChannels);
    QStringList arguments{"--resize-client"};
    if (withPopup) arguments.append("--popup-client");
    if (ignoreClose) arguments.append("--ignore-close");
    if (bounded) arguments.append("--bounded-client");
    if (unmapAfterMap) arguments.append("--unmap-after-map");
    m_client.start(QCoreApplication::applicationFilePath(), arguments);
    QVERIFY(m_client.waitForStarted());
    QTRY_VERIFY2_WITH_TIMEOUT(m_toplevel, m_client.readAll().constData(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(800, 500), 5000);
}

void TestXdgResize::cleanup() {
    if (m_client.state() != QProcess::NotRunning) {
        m_client.terminate();
        QVERIFY(m_client.waitForFinished(3000));
    }
    QCoreApplication::processEvents();
    m_frames.stop();
    m_output.reset();
    m_compositor.reset();
    m_toplevel.clear();
}

void TestXdgResize::nullAndInvokable() {
    QSignalSpy created(m_compositor.get(), &SystaliaCompositor::toplevelCreated);
    QVERIFY(created.isValid());
    QVERIFY(QMetaObject::invokeMethod(m_compositor.get(), "requestToplevelResize",
            Q_ARG(QWaylandXdgToplevel *, nullptr), Q_ARG(QSize, QSize(-1, -1)), Q_ARG(bool, true)));
    m_compositor->requestToplevelResize(nullptr, QSize(0, 0), false);
    QCOMPARE(created.count(), 0);
    QCOMPARE(m_compositor->clientCount(), 0);
}

void TestXdgResize::closeRequestUsesCompositorSlot() {
    QVERIFY(!m_compositor->closeToplevel(nullptr));
    QSignalSpy qmlToplevelCreated(m_compositor.get(),
                                  &SystaliaCompositor::toplevelCreatedForQml);
    QVERIFY(qmlToplevelCreated.isValid());
    startClient();
    QVERIFY(m_toplevel);
    QCOMPARE(qmlToplevelCreated.count(), 1);
    QCOMPARE(qmlToplevelCreated.at(0).at(0).value<QObject *>(), m_toplevel.data());

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    QObject *toplevelObject = qmlToplevelCreated.at(0).at(0).value<QObject *>();
    auto item = createContainer(engine, window, toplevelObject);
    QVERIFY(item);
    auto *closeButton = item->findChild<QQuickItem *>("closeToplevelButton");
    QVERIFY(closeButton);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const QPoint closePosition = closeButton->mapToScene(
        QPointF(closeButton->width() / 2.0, closeButton->height() / 2.0)).toPoint();
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, closePosition);
    QTRY_VERIFY_WITH_TIMEOUT(m_client.state() == QProcess::NotRunning, 3000);
}

void TestXdgResize::cancelledCloseKeepsClientUsable() {
    startClient(false, false, true);
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    auto ownedItem = createContainer(engine, window, m_toplevel);
    QVERIFY(ownedItem);
    // QML is allowed to destroy a dynamic container, so track it without owning a dangling pointer.
    QPointer<QQuickItem> item = ownedItem.release();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *closeButton = item->findChild<QQuickItem *>("closeToplevelButton");
    QVERIFY(closeButton);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                     closeButton->mapToScene(QPointF(14, 14)).toPoint());
    QTest::qWait(400);
    QVERIFY(item);
    QVERIFY(item->isVisible());
    QVERIFY(m_toplevel && m_toplevel->xdgSurface()->surface()->hasContent());
    QCOMPARE(m_client.state(), QProcess::Running);
    delete item.data();
}

void TestXdgResize::nativePointerMapsIntoMovedClient() {
    startClient();
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    m_output = std::make_unique<QWaylandOutput>(m_compositor.get(), &window);
    auto item = createContainer(engine, window, m_toplevel);
    QVERIFY(item);
    item->setPosition(QPointF(300, 150));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *client = item->findChild<QQuickItem *>("clientSurfaceItem");
    QVERIFY(client);
    const QPointF local(80, 60);
    const QPointF global = client->mapToScene(local) + QPointF(window.position());
    InputDeviceManager input;
    input.attachCompositor(m_compositor.get());
    input.setKmsDisplayBounds(QRect(0, 0, 2000, 1500));
    input.setPointerPosition(global - QPointF(10, 10));
    input.processLibinputPointerMotion(10, 10);
    auto *pointer = m_compositor->defaultSeat()->pointer();
    QTRY_VERIFY(pointer->mouseFocus());
    QTRY_COMPARE(pointer->currentLocalPosition(), local);
    m_output.reset();
}

void TestXdgResize::popupIsPresentedOutsideTheCapturedClient() {
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    m_output = std::make_unique<QWaylandOutput>(m_compositor.get(), &window);
    startClient(false, false, false, true);
    auto item = createContainer(engine, window, m_toplevel);
    QVERIFY(item);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QWaylandQuickShellSurfaceItem *popupItem = nullptr;
    const auto findPopup = [&] {
        for (auto *child : item->childItems()) {
            if (auto *shellItem = qobject_cast<QWaylandQuickShellSurfaceItem *>(child))
                popupItem = shellItem;
        }
        return popupItem != nullptr;
    };
    auto *client = item->findChild<QQuickItem *>("clientSurfaceItem");
    QVERIFY(client);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                     client->mapToScene(QPointF(80, 60)).toPoint());
    QTRY_VERIFY2_WITH_TIMEOUT(findPopup(), m_client.readAll().constData(), 5000);
    QVERIFY(popupItem->isVisible());
    QVERIFY(!popupItem->clip());
    QVERIFY(popupItem->parentItem() == item.get());
    QTRY_VERIFY(popupItem->surface() && popupItem->surface()->hasContent());
    m_output.reset();
}

void TestXdgResize::minimizeButtonHidesAndActivationRestoresToplevel() {
    QVERIFY(!m_compositor->minimizeToplevel(nullptr));
    QVERIFY(!m_compositor->isToplevelMinimized(nullptr));
    QSignalSpy qmlToplevelCreated(m_compositor.get(),
                                  &SystaliaCompositor::toplevelCreatedForQml);
    QVERIFY(qmlToplevelCreated.isValid());
    startClient();
    QVERIFY(m_toplevel);

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    QObject *toplevelObject = qmlToplevelCreated.at(0).at(0).value<QObject *>();
    auto item = createContainer(engine, window, toplevelObject);
    QVERIFY(item);
    auto *minimizeButton = item->findChild<QQuickItem *>("minimizeToplevelButton");
    QVERIFY(minimizeButton);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY(item->isVisible());
    const QPoint minimizePosition = minimizeButton->mapToScene(
        QPointF(minimizeButton->width() / 2.0, minimizeButton->height() / 2.0)).toPoint();
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, minimizePosition);

    QTRY_VERIFY(m_compositor->isToplevelMinimized(m_toplevel.data()));
    QTRY_VERIFY(!item->isVisible());
    QVERIFY(m_compositor->activateToplevel(m_toplevel.data()));
    QTRY_VERIFY(!m_compositor->isToplevelMinimized(m_toplevel.data()));
    QTRY_VERIFY(item->isVisible());
    QCOMPARE(m_compositor->defaultSeat()->keyboardFocus(),
             m_toplevel->xdgSurface()->surface());
}

void TestXdgResize::maximizeRequestUsesCompositorSlot() {
    QVERIFY(!m_compositor->toggleToplevelMaximized(nullptr, QSize(800, 500)));
    QSignalSpy qmlToplevelCreated(m_compositor.get(),
                                  &SystaliaCompositor::toplevelCreatedForQml);
    QVERIFY(qmlToplevelCreated.isValid());
    startClient();
    QVERIFY(m_toplevel);

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    QObject *toplevelObject = qmlToplevelCreated.at(0).at(0).value<QObject *>();
    auto item = createContainer(engine, window, toplevelObject);
    QVERIFY(item);
    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);
    QVERIFY(surface->primaryView());
    surface->primaryView()->setOutput(m_output.get());
    auto *maximizeButton = item->findChild<QQuickItem *>("maximizeToplevelButton");
    QVERIFY(maximizeButton);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const QPoint maximizePosition = maximizeButton->mapToScene(
        QPointF(maximizeButton->width() / 2.0, maximizeButton->height() / 2.0)).toPoint();
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, maximizePosition);
    QTRY_VERIFY(m_toplevel->maximized());
    QTRY_VERIFY(item->property("maximizedBySge").toBool());
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(1384, 934));
    QTRY_COMPARE(item->size(), QSizeF(1400, 1000));
    QCOMPARE(item->position(), QPointF(0, 0));
    QCOMPARE(item->property("restorePosition").toPointF(), QPointF(250, 180));

    const QPoint restorePosition = maximizeButton->mapToScene(
        QPointF(maximizeButton->width() / 2.0, maximizeButton->height() / 2.0)).toPoint();
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, restorePosition);
    QCOMPARE(item->position(), QPointF(250, 180));
    QTRY_VERIFY(!m_toplevel->maximized());
    QTRY_VERIFY(!item->property("maximizedBySge").toBool());
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(800, 500));
    QTRY_COMPARE(item->size(), QSizeF(816, 566));
    QCOMPARE(item->property("restorePosition").toPointF(), QPointF(250, 180));
    QTRY_COMPARE(item->position(), QPointF(250, 180));
}

void TestXdgResize::activationSetsSeatKeyboardFocus() {
    QVERIFY(!m_compositor->activateToplevel(nullptr));
    bool activatedBeforeInitialCommit = false;
    connect(m_compositor.get(), &SystaliaCompositor::toplevelCreated,
            this, [this, &activatedBeforeInitialCommit](QWaylandXdgToplevel *toplevel) {
        activatedBeforeInitialCommit = m_compositor->activateToplevel(toplevel);
    });
    startClient();
    QVERIFY(m_toplevel);
    QVERIFY(activatedBeforeInitialCommit);

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    auto item = createContainer(engine, window, m_toplevel.data());
    QVERIFY(item);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);
    QTRY_VERIFY(surface->primaryView());
    QVERIFY(surface->primaryView());
    surface->primaryView()->setOutput(m_output.get());

    QVERIFY(m_compositor->activateToplevel(m_toplevel.data()));
    QCOMPARE(m_compositor->defaultSeat()->keyboardFocus(), surface);
}

void TestXdgResize::clientBufferIsBoundAndPainted() {
    startClient();
    QVERIFY(m_toplevel);

    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);
    QTRY_VERIFY(surface->hasContent());

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    auto item = createContainer(engine, window);
    QVERIFY(item);

    auto *surfaceItem = item->findChild<QQuickItem *>("clientSurfaceItem");
    QVERIFY(surfaceItem);
    QCOMPARE(surfaceItem->opacity(), 1.0);
    QCOMPARE(surfaceItem->property("surface").value<QWaylandSurface *>(), surface);
    QVERIFY(!item->findChild<QQuickItem *>("clientTextureCapture"));

    auto *viewport = item->findChild<QQuickItem *>("clientViewport");
    QVERIFY(viewport);
    QVERIFY(viewport->clip());
    QCOMPARE(viewport->position(),QPointF(8,58));
    QCOMPARE(viewport->z(), 1.0);

    auto *glass = item->findChild<Systalia::Gfx::V2::ValoGlassItemV2 *>();
    QVERIFY(glass);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY(surface->hasContent());
    QTRY_VERIFY(item->property("clientMapped").toBool());
    QTRY_COMPARE(surfaceItem->size(), viewport->size());
    QCOMPARE(surfaceItem->size(),QSizeF(800,500));
    QCOMPARE(item->size(),QSizeF(816,566));
}

void TestXdgResize::unmappedClientDoesNotLeaveAnEmptyFrame() {
    startClient(false, true);
    QVERIFY(m_toplevel);
    QWaylandSurface *surface = m_toplevel->xdgSurface()->surface();
    QVERIFY(surface);

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    auto item = createContainer(engine, window);
    QVERIFY(item);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTRY_VERIFY_WITH_TIMEOUT(surface->hasContent(), 3000);
    QTRY_VERIFY(item->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!surface->hasContent(), 4000);
    QTRY_VERIFY(!item->isVisible());
}

void TestXdgResize::configureHandshake_data() {
    QTest::addColumn<QSize>("requested");
    QTest::addColumn<QSize>("expected");
    QTest::newRow("negative") << QSize(-100, -50) << QSize(240, 160);
    QTest::newRow("zero") << QSize(0, 0) << QSize(240, 160);
    QTest::newRow("below-minimum") << QSize(239, 159) << QSize(240, 160);
    QTest::newRow("width-only") << QSize(120, 500) << QSize(240, 500);
    QTest::newRow("height-only") << QSize(800, 10) << QSize(800, 160);
    QTest::newRow("unchanged-valid") << QSize(820, 530) << QSize(820, 530);
}

void TestXdgResize::configureHandshake() {
    QFETCH(QSize, requested);
    QFETCH(QSize, expected);
    startClient();
    QVERIFY(m_toplevel);
    QSignalSpy resizing(m_toplevel, &QWaylandXdgToplevel::resizingChanged);
    QSignalSpy activated(m_toplevel, &QWaylandXdgToplevel::activatedChanged);
    QSignalSpy geometry(m_toplevel->xdgSurface(), &QWaylandXdgSurface::windowGeometryChanged);
    QVERIFY(resizing.isValid());
    QVERIFY(activated.isValid());
    QVERIFY(geometry.isValid());
    m_compositor->requestToplevelResize(m_toplevel, requested, true);
    QTRY_VERIFY(m_toplevel->resizing());
    QTRY_VERIFY(m_toplevel->activated());
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), expected);
    QCOMPARE(resizing.count(), 1);
    QVERIFY(geometry.count() > 0);
    m_compositor->requestToplevelResize(m_toplevel, requested, false);
    QTRY_VERIFY(!m_toplevel->resizing());
    QVERIFY(m_toplevel->activated());
    QCOMPARE(resizing.count(), 2);
    QCOMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), expected);
}

std::unique_ptr<QQuickItem> TestXdgResize::createContainer(QQmlEngine &engine,
                                                           QQuickWindow &window,
                                                           QObject *toplevelObject) {
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Systalia/Engine/src/qml/WaylandWindowContainer.qml"));
    if (!toplevelObject) {
        toplevelObject = m_toplevel.data();
    }
    auto *object = component.createWithInitialProperties({
        {"toplevel", QVariant::fromValue(toplevelObject)},
        {"waylandServer", QVariant::fromValue(m_compositor.get())}
    });
    if (!object) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(object));
    if (!item) { delete object; return {}; }
    item->setParentItem(window.contentItem());
    item->setPosition(QPointF(250, 180));
    return item;
}

void TestXdgResize::dragEdges_data() {
    QTest::addColumn<QString>("handle");
    QTest::addColumn<int>("edges");
    QTest::addColumn<int>("cursor");
    QTest::newRow("top") << QString("top") << int(Qt::TopEdge) << int(Qt::SizeVerCursor);
    QTest::newRow("bottom") << QString("bottom") << int(Qt::BottomEdge) << int(Qt::SizeVerCursor);
    QTest::newRow("left") << QString("left") << int(Qt::LeftEdge) << int(Qt::SizeHorCursor);
    QTest::newRow("right") << QString("right") << int(Qt::RightEdge) << int(Qt::SizeHorCursor);
    QTest::newRow("top-left") << QString("topLeft") << int(Qt::TopEdge | Qt::LeftEdge) << int(Qt::SizeFDiagCursor);
    QTest::newRow("bottom-right") << QString("bottomRight") << int(Qt::BottomEdge | Qt::RightEdge) << int(Qt::SizeFDiagCursor);
    QTest::newRow("top-right") << QString("topRight") << int(Qt::TopEdge | Qt::RightEdge) << int(Qt::SizeBDiagCursor);
    QTest::newRow("bottom-left") << QString("bottomLeft") << int(Qt::BottomEdge | Qt::LeftEdge) << int(Qt::SizeBDiagCursor);
}

void TestXdgResize::dragEdges() {
    QFETCH(QString, handle);
    QFETCH(int, edges);
    QFETCH(int, cursor);
    startClient();
    QVERIFY(m_toplevel);
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    auto item = createContainer(engine, window);
    QVERIFY(item);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_COMPARE(item->width(), 816.0);
    QTRY_COMPARE(item->height(), 566.0);
    QQuickItem *zone = nullptr;
    for (auto *child : item->childItems()) {
        if (child->objectName() == "resize_" + handle) zone = child;
    }
    QVERIFY(zone);
    QCOMPARE(zone->property("cursorShape").toInt(), cursor);
    const QPoint press = zone->mapToScene(QPointF(zone->width() / 2, zone->height() / 2)).toPoint();
    const QPoint delta((edges & Qt::LeftEdge) ? -70 : ((edges & Qt::RightEdge) ? 70 : 0),
                       (edges & Qt::TopEdge) ? -45 : ((edges & Qt::BottomEdge) ? 45 : 0));
    const QSize expected(800 + qAbs(delta.x()), 500 + qAbs(delta.y()));
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, press);
    QTRY_VERIFY(item->property("resizing").toBool());
    QTest::mouseMove(&window, press + delta);
    // No event processing: requests alone must not resize the glass.
    QCOMPARE(item->size(), QSizeF(816, 566));
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), expected);
    QTRY_COMPARE(item->size(), QSizeF(expected.width() + 16, expected.height() + 66));
    QCOMPARE(item->x() + item->width(), (edges & Qt::LeftEdge) ? 1066.0 : 250.0 + item->width());
    QCOMPARE(item->y() + item->height(), (edges & Qt::TopEdge) ? 746.0 : 180.0 + item->height());
    auto *glass = item->findChild<Systalia::Gfx::V2::ValoGlassItemV2 *>();
    QVERIFY(glass);
    QCOMPARE(glass->size(), item->size());
    auto *surfaceItem = item->findChild<QQuickItem *>("clientSurfaceItem");
    QVERIFY(surfaceItem);
    QTRY_COMPARE(surfaceItem->size(), QSizeF(expected));
    // Another motion after the left/top origin changed must use parent coordinates.
    QTest::mouseMove(&window, press + delta * 2);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, press + delta * 2);
    QTRY_VERIFY(!m_toplevel->resizing());
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(800 + 2 * qAbs(delta.x()), 500 + 2 * qAbs(delta.y())));
    QVERIFY(!item->property("resizing").toBool());
}

void TestXdgResize::resizeHonorsMinimumFrameGeometry() {
    startClient();
    QVERIFY(m_toplevel);
    QQmlEngine engine;
    QQuickWindow window;
    auto item = createContainer(engine, window);
    QVERIFY(item);
    QVERIFY(QMetaObject::invokeMethod(item.get(), "beginResize", Q_ARG(QVariant, int(Qt::TopEdge | Qt::LeftEdge)),
                                     Q_ARG(QVariant, QPointF(250, 180))));
    QVERIFY(QMetaObject::invokeMethod(item.get(), "updateResize", Q_ARG(QVariant, QPointF(2000, 2000))));
    QVERIFY(QMetaObject::invokeMethod(item.get(), "finishResize"));
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(304, 168));
    QTRY_COMPARE(item->size(), QSizeF(320, 234));
    QCOMPARE(item->position(), QPointF(746, 512));
    QTRY_VERIFY(!m_toplevel->resizing());
    QVERIFY(m_toplevel->activated());
}

void TestXdgResize::lostMouseGrabCancelsResize() {
    startClient();
    QVERIFY(m_toplevel);
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1400, 1000);
    auto item = createContainer(engine, window);
    QVERIFY(item);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_COMPARE(item->size(), QSizeF(816, 566));

    // Exercise MouseArea.onCanceled through a real lost mouse grab.
    QQuickItem *zone = nullptr;
    for (auto *child : item->childItems()) {
        if (child->objectName() == "resize_bottomRight") zone = child;
    }
    QVERIFY(zone);
    const QPoint press = zone->mapToScene(QPointF(4, 4)).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, press);
    QTRY_VERIFY(m_toplevel->resizing());
    QTest::mouseMove(&window, press + QPoint(90, 60));
    zone->ungrabMouse();
    QVERIFY(!item->property("resizing").toBool());
    QTRY_VERIFY(!m_toplevel->resizing());
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(890, 560));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, press + QPoint(90, 60));
}

void TestXdgResize::finishResizeIsIdempotent() {
    startClient();
    QVERIFY(m_toplevel);
    QQmlEngine engine;
    QQuickWindow window;
    auto item = createContainer(engine, window);
    QVERIFY(item);
    QVERIFY(QMetaObject::invokeMethod(item.get(), "beginResize", Q_ARG(QVariant, int(Qt::RightEdge)),
                                     Q_ARG(QVariant, QPointF(250, 180))));
    QVERIFY(QMetaObject::invokeMethod(item.get(), "updateResize", Q_ARG(QVariant, QPointF(340, 180))));
    QVERIFY(QMetaObject::invokeMethod(item.get(), "finishResize"));
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(890, 500));
    QVERIFY(QMetaObject::invokeMethod(item.get(), "finishResize"));
    QCOMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(890, 500));
    QVERIFY(!item->property("resizing").toBool());
}

void TestXdgResize::clientMinimumConstraintClampsConfigure() {
    startClient(true);
    QVERIFY(m_toplevel);
    QTRY_COMPARE(m_toplevel->minSize(), QSize(400, 250));
    m_compositor->requestToplevelResize(m_toplevel, QSize(-10, 0), true);
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(400, 250));
    QTRY_VERIFY(m_toplevel->resizing());
    m_compositor->requestToplevelResize(m_toplevel, QSize(640, 400), false);
    QTRY_VERIFY(!m_toplevel->resizing());
}

void TestXdgResize::clientMaximumConstraintClampsConfigure() {
    startClient(true);
    QVERIFY(m_toplevel);
    QTRY_COMPARE(m_toplevel->maxSize(), QSize(900, 650));
    m_compositor->requestToplevelResize(m_toplevel, QSize(2000, 2000), true);
    QTRY_COMPARE(m_toplevel->xdgSurface()->windowGeometry().size(), QSize(900, 650));
    QTRY_VERIFY(m_toplevel->resizing());
    m_compositor->requestToplevelResize(m_toplevel, QSize(640, 400), false);
    QTRY_VERIFY(!m_toplevel->resizing());
}

int main(int argc, char **argv) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QGuiApplication app(argc, argv);
    if (app.arguments().contains("--resize-client")) {
        ResizeClient window;
        window.setFlags(Qt::Window | Qt::FramelessWindowHint);
        if (app.arguments().contains("--bounded-client")) {
            window.setMinimumSize(QSize(400, 250));
            window.setMaximumSize(QSize(900, 650));
        }
        window.resize(640, 400);
        window.show();
        std::unique_ptr<ResizeClient> popup;
        if (app.arguments().contains("--popup-client")) {
            QTimer::singleShot(800, &window, [&] {
                popup = std::make_unique<ResizeClient>();
                popup->setTransientParent(&window);
                popup->setFlags(Qt::Popup | Qt::FramelessWindowHint);
                popup->resize(160, 120);
                popup->setPosition(window.position() + QPoint(window.width() - 20, window.height() - 20));
                popup->show();
            });
        }
        if (app.arguments().contains("--unmap-after-map")) {
            QTimer::singleShot(1200, &window, &QWindow::hide);
        }
        return app.exec();
    }
    TestXdgResize test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_XdgResize.moc"
