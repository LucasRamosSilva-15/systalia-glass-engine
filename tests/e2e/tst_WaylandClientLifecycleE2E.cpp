#include <QGuiApplication>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRasterWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtWaylandCompositor/QWaylandXdgToplevel>
#include <memory>

#include "SystaliaCompositor.hpp"
#include "SystaliaGlassItem.hpp"

class LifecycleClientWindow final : public QRasterWindow {
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(0, 0), size()), QColor(QStringLiteral("#00b8d9")));
        painter.setPen(Qt::white);
        painter.drawText(QRect(QPoint(0, 0), size()), Qt::AlignCenter,
                         QStringLiteral("SGE lifecycle buffer"));
    }
};

class TestWaylandClientLifecycleE2E final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void connectionCreatesOneCompositorClient();
    void xdgClientCommitsARealBuffer();
    void clientSurfaceIsAttachedToWorkspaceContainer();
    void disconnectReleasesClientSurfaceAndContainer();

private:
    void startClient();
    std::unique_ptr<QObject> createWorkspaceContainer(QQmlEngine &engine,
                                                       QQuickItem &workspace,
                                                       QQuickItem *&container);
    QTemporaryDir m_runtime;
    std::unique_ptr<SystaliaCompositor> m_compositor;
    QProcess m_client;
    QPointer<QWaylandXdgToplevel> m_toplevel;
    QPointer<QWaylandSurface> m_surface;
};

void TestWaylandClientLifecycleE2E::initTestCase() {
    QVERIFY(m_runtime.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
}

void TestWaylandClientLifecycleE2E::init() {
    m_compositor = std::make_unique<SystaliaCompositor>();
    m_compositor->setSocketName(QByteArrayLiteral("wayland-systalia-test-e2e"));
    m_compositor->create();
    QVERIFY(m_compositor->isCreated());
    connect(m_compositor.get(), &SystaliaCompositor::toplevelCreated,
            this, [this](QWaylandXdgToplevel *toplevel) {
        m_toplevel = toplevel;
        if (toplevel && toplevel->xdgSurface())
            m_surface = toplevel->xdgSurface()->surface();
    });
}

void TestWaylandClientLifecycleE2E::cleanup() {
    if (m_client.state() != QProcess::NotRunning) {
        m_client.terminate();
        if (!m_client.waitForFinished(3000)) {
            m_client.kill();
            m_client.waitForFinished(3000);
        }
    }
    QCoreApplication::processEvents();
    m_compositor.reset();
    m_toplevel.clear();
    m_surface.clear();
}

void TestWaylandClientLifecycleE2E::startClient() {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), m_runtime.path());
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), m_compositor->socketName());
    environment.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"), QStringLiteral("1"));
    m_client.setProcessEnvironment(environment);
    m_client.setProcessChannelMode(QProcess::MergedChannels);
    m_client.start(QCoreApplication::applicationFilePath(),
                   {QStringLiteral("--wayland-lifecycle-client")});
    QVERIFY2(m_client.waitForStarted(), qPrintable(m_client.errorString()));
}

void TestWaylandClientLifecycleE2E::connectionCreatesOneCompositorClient() {
    QCOMPARE(m_compositor->clientCount(), 0);
    startClient();
    QTRY_COMPARE_WITH_TIMEOUT(m_compositor->clientCount(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_toplevel, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_surface, 5000);
}

void TestWaylandClientLifecycleE2E::xdgClientCommitsARealBuffer() {
    startClient();
    QTRY_VERIFY_WITH_TIMEOUT(m_surface, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_surface->hasContent(), 5000);
}

std::unique_ptr<QObject> TestWaylandClientLifecycleE2E::createWorkspaceContainer(
    QQmlEngine &engine, QQuickItem &workspace, QQuickItem *&container) {
    QQmlComponent factory(&engine);
    factory.setData(R"(
        import QtQuick
        import Systalia.Engine
        Item {
            id: factoryRoot
            required property var targetToplevel
            required property var targetServer
            required property var workspace
            property var windowContainer: null
            Component {
                id: clientWindowComponent
                WaylandWindowContainer {
                    toplevel: factoryRoot.targetToplevel
                    waylandServer: factoryRoot.targetServer
                    workspace: factoryRoot.workspace
                }
            }
            Component.onCompleted: {
                windowContainer = clientWindowComponent.createObject(workspace)
            }
        })", QUrl());
    std::unique_ptr<QObject> factoryObject(factory.createWithInitialProperties({
        {QStringLiteral("targetToplevel"), QVariant::fromValue(m_toplevel.data())},
        {QStringLiteral("targetServer"), QVariant::fromValue(m_compositor.get())},
        {QStringLiteral("workspace"), QVariant::fromValue(&workspace)}
    }));
    if (!factoryObject)
        return {};
    container = qobject_cast<QQuickItem *>(
        factoryObject->property("windowContainer").value<QObject *>());
    return factoryObject;
}

void TestWaylandClientLifecycleE2E::clientSurfaceIsAttachedToWorkspaceContainer() {
    startClient();
    QTRY_VERIFY_WITH_TIMEOUT(m_surface, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_surface->hasContent(), 5000);
    QQmlEngine engine;
    QQuickItem workspace;
    workspace.setSize(QSizeF(1280.0, 800.0));
    QQuickItem *container = nullptr;
    std::unique_ptr<QObject> factoryObject = createWorkspaceContainer(engine, workspace, container);
    QVERIFY(factoryObject);
    QVERIFY(container);
    QCOMPARE(container->parentItem(), &workspace);
    QCOMPARE(workspace.childItems().size(), 1);
    QQuickItem *clientItem = container->findChild<QQuickItem *>(
        QStringLiteral("clientSurfaceItem"));
    QVERIFY(clientItem);
    QCOMPARE(clientItem->property("surface").value<QWaylandSurface *>(), m_surface.data());

}

void TestWaylandClientLifecycleE2E::disconnectReleasesClientSurfaceAndContainer() {
    startClient();
    QTRY_VERIFY_WITH_TIMEOUT(m_toplevel, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_surface, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_surface->hasContent(), 5000);

    QQmlEngine engine;
    QQuickItem workspace;
    QQuickItem *containerObject = nullptr;
    std::unique_ptr<QObject> factoryObject = createWorkspaceContainer(
        engine, workspace, containerObject);
    QVERIFY(factoryObject);
    QPointer<QQuickItem> container = containerObject;
    QVERIFY(container);

    const QPointer<QWaylandXdgToplevel> toplevel = m_toplevel;
    const QPointer<QWaylandSurface> surface = m_surface;
    m_client.terminate();
    QVERIFY(m_client.waitForFinished(3000));
    QTRY_COMPARE_WITH_TIMEOUT(m_compositor->clientCount(), 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(toplevel.isNull(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(surface.isNull(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(container.isNull(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(workspace.childItems().size(), 0, 5000);
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--wayland-lifecycle-client"))) {
        LifecycleClientWindow window;
        window.resize(480, 300);
        window.show();
        return app.exec();
    }
    TestWaylandClientLifecycleE2E test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_WaylandClientLifecycleE2E.moc"
