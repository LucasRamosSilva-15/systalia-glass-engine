#include <QTest>
#include <QSignalSpy>
#include "SystaliaCompositor.hpp"
#include <QtWaylandCompositor/QWaylandXdgDecorationManagerV1>
#include <QtWaylandCompositor/QWaylandXdgToplevel>
#include <wayland-server-core.h>

class TestXdgDecorations : public QObject {
    Q_OBJECT

private slots:
    void testDecorationManagerInstantiation();
    void testPreferredModeServerSide();
    void testInterfaceName();
    void testPreferredModeSignal();
};

void TestXdgDecorations::testDecorationManagerInstantiation() {
    SystaliaCompositor compositor;
    compositor.setSocketName("wayland-test-decor-0");
    compositor.create();

    QVERIFY2(compositor.isCreated(), "O compositor Wayland deve estar criado e ativo");
    QVERIFY2(compositor.decorationManager() != nullptr, "QWaylandXdgDecorationManagerV1 deve estar instanciado");
    QVERIFY2(compositor.decorationManager()->isInitialized(), "QWaylandXdgDecorationManagerV1 deve estar inicializado no registro Wayland");
}

void TestXdgDecorations::testPreferredModeServerSide() {
    SystaliaCompositor compositor;
    compositor.setSocketName("wayland-test-decor-1");
    compositor.create();

    QVERIFY2(compositor.decorationManager() != nullptr, "O decoration manager deve estar disponível");
    QCOMPARE(compositor.decorationManager()->preferredMode(), QWaylandXdgToplevel::ServerSideDecoration);
}

void TestXdgDecorations::testInterfaceName() {
    SystaliaCompositor compositor;
    compositor.setSocketName("wayland-test-decor-2");
    compositor.create();

    QCOMPARE(QByteArray(QWaylandXdgDecorationManagerV1::interface()->name), QByteArray("zxdg_decoration_manager_v1"));
}

void TestXdgDecorations::testPreferredModeSignal() {
    SystaliaCompositor compositor;
    QSignalSpy spy(compositor.decorationManager(), &QWaylandXdgDecorationManagerV1::preferredModeChanged);

    compositor.decorationManager()->setPreferredMode(QWaylandXdgToplevel::ClientSideDecoration);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(compositor.decorationManager()->preferredMode(), QWaylandXdgToplevel::ClientSideDecoration);

    // Reafirmar ServerSideDecoration
    compositor.decorationManager()->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(compositor.decorationManager()->preferredMode(), QWaylandXdgToplevel::ServerSideDecoration);
}

QTEST_MAIN(TestXdgDecorations)
#include "tst_XdgDecorations.moc"
