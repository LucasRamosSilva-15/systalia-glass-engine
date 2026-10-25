#include <QGuiApplication>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRasterWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandXdgToplevel>

#include "SystaliaCompositor.hpp"
#include "SystaliaGlassItem.hpp"

class DecorationClient final : public QRasterWindow {
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(0, 0), size()), QColor(QStringLiteral("#143b52")));
    }
};

class TestXdgShellDecoration final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void serverDecorationPolicyAndMinimumFrameSize();

private:
    void startClient();
    QTemporaryDir m_runtime;
    std::unique_ptr<SystaliaCompositor> m_compositor;
    std::unique_ptr<QWaylandOutput> m_output;
    QProcess m_client;
    QTimer m_frames;
    QPointer<QWaylandXdgToplevel> m_toplevel;
};

void TestXdgShellDecoration::initTestCase() {
    QVERIFY(m_runtime.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
}

void TestXdgShellDecoration::init() {
    m_compositor = std::make_unique<SystaliaCompositor>();
    m_compositor->setSocketName(QByteArrayLiteral("wayland-xdg-decoration-integration"));
    m_compositor->create();
    QVERIFY(m_compositor->isCreated());
    m_output = std::make_unique<QWaylandOutput>(m_compositor.get(), nullptr);
    m_output->addMode(QWaylandOutputMode(QSize(1280, 800), 60000), true);
    m_output->setCurrentMode(QWaylandOutputMode(QSize(1280, 800), 60000));
    connect(&m_frames, &QTimer::timeout, m_compositor.get(), [this] {
        m_compositor->processWaylandEvents();
        for (QWaylandSurface *surface : m_compositor->surfaces()) {
            surface->frameStarted();
            surface->sendFrameCallbacks();
        }
    });
    m_frames.start(16);
    connect(m_compositor.get(), &SystaliaCompositor::toplevelCreated,
            this, [this](QWaylandXdgToplevel *toplevel) { m_toplevel = toplevel; });
}

void TestXdgShellDecoration::cleanup() {
    if (m_client.state() != QProcess::NotRunning) {
        m_client.terminate();
        if (!m_client.waitForFinished(3000)) {
            m_client.kill();
            m_client.waitForFinished(3000);
        }
    }
    QCoreApplication::processEvents();
    m_frames.stop();
    m_output.reset();
    m_compositor.reset();
    m_toplevel.clear();
}

void TestXdgShellDecoration::startClient() {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), m_runtime.path());
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"), QStringLiteral("1"));
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), m_compositor->socketName());
    m_client.setProcessEnvironment(environment);
    m_client.setProcessChannelMode(QProcess::MergedChannels);
    m_client.start(QCoreApplication::applicationFilePath(),
                   {QStringLiteral("--xdg-decoration-client")});
    QVERIFY(m_client.waitForStarted());
    QTRY_VERIFY2_WITH_TIMEOUT(m_toplevel, m_client.readAll().constData(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(m_toplevel->xdgSurface()->windowGeometry().size(),
                              QSize(800, 500), 5000);
}

void TestXdgShellDecoration::serverDecorationPolicyAndMinimumFrameSize() {
    QVERIFY(m_compositor->decorationManager()->isInitialized());
    QCOMPARE(m_compositor->decorationManager()->preferredMode(),
             QWaylandXdgToplevel::ServerSideDecoration);

    startClient();
    QVERIFY(m_toplevel);
    QTRY_COMPARE(m_toplevel->decorationMode(), QWaylandXdgToplevel::ServerSideDecoration);

    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/WaylandWindowContainer.qml")));
    QQuickItem workspace;
    std::unique_ptr<QQuickItem> container(qobject_cast<QQuickItem *>(
        component.createWithInitialProperties({
            {QStringLiteral("toplevel"), QVariant::fromValue(static_cast<QObject *>(m_toplevel.data()))},
            {QStringLiteral("waylandServer"), QVariant::fromValue(static_cast<QObject *>(m_compositor.get()))},
            {QStringLiteral("workspace"), QVariant::fromValue(static_cast<QObject *>(&workspace))}
        })));
    QVERIFY2(container, qPrintable(component.errorString()));
    container->setParentItem(&workspace);

    m_compositor->requestToplevelResize(m_toplevel, QSize(100, 100), true);
    QTRY_COMPARE_WITH_TIMEOUT(m_toplevel->xdgSurface()->windowGeometry().size(),
                              QSize(240, 160), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(container->size(), QSizeF(256.0, 212.0), 3000);

    const QVariant startPosition = QPointF(0.0, 0.0);
    const QVariant minimumPosition = QPointF(1000.0, 1000.0);
    QVERIFY(QMetaObject::invokeMethod(container.get(), "beginResize",
        Q_ARG(QVariant, QVariant(int(Qt::TopEdge | Qt::LeftEdge))),
        Q_ARG(QVariant, startPosition)));
    QVERIFY(QMetaObject::invokeMethod(container.get(), "updateResize",
        Q_ARG(QVariant, minimumPosition)));
    QVERIFY(QMetaObject::invokeMethod(container.get(), "finishResize"));
    QTRY_COMPARE_WITH_TIMEOUT(m_toplevel->xdgSurface()->windowGeometry().size(),
                              QSize(304, 168), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(container->size(), QSizeF(320.0, 220.0), 3000);
    QCOMPARE(container->parentItem(), &workspace);
    QCOMPARE(workspace.childItems().size(), 1);
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--xdg-decoration-client"))) {
        DecorationClient window;
        window.resize(640, 400);
        window.show();
        return app.exec();
    }
    TestXdgShellDecoration test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_XdgShellDecoration.moc"
