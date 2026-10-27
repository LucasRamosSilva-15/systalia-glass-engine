#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>
#include "SystaliaCompositor.hpp"
#include <QtWaylandCompositor/QWaylandXdgDecorationManagerV1>
#include <QtWaylandCompositor/QWaylandXdgToplevel>
#include <wayland-server-core.h>

class TestXdgDecorations : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testDecorationManagerInstantiation();
    void testPreferredModeServerSide();
    void testInterfaceName();
    void preferredModeChangeEmitsSignal_data();
    void preferredModeChangeEmitsSignal();
    void settingCurrentPreferredModeDoesNotEmit();
private:
    std::unique_ptr<QTemporaryDir> m_runtime;
};

void TestXdgDecorations::initTestCase() {
    m_runtime = std::make_unique<QTemporaryDir>();
    QVERIFY(m_runtime->isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime->path().toUtf8());
}

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

void TestXdgDecorations::preferredModeChangeEmitsSignal_data() {
    QTest::addColumn<int>("initialMode");
    QTest::addColumn<int>("requestedMode");
    QTest::newRow("client-side")
        << int(QWaylandXdgToplevel::ServerSideDecoration)
        << int(QWaylandXdgToplevel::ClientSideDecoration);
    QTest::newRow("server-side")
        << int(QWaylandXdgToplevel::ClientSideDecoration)
        << int(QWaylandXdgToplevel::ServerSideDecoration);
}

void TestXdgDecorations::preferredModeChangeEmitsSignal() {
    QFETCH(int, initialMode);
    QFETCH(int, requestedMode);
    SystaliaCompositor compositor;
    compositor.setSocketName("wayland-test-decor-mode");
    compositor.create();
    QVERIFY(compositor.decorationManager());
    compositor.decorationManager()->setPreferredMode(
        static_cast<QWaylandXdgToplevel::DecorationMode>(initialMode));
    QSignalSpy spy(compositor.decorationManager(),
                   &QWaylandXdgDecorationManagerV1::preferredModeChanged);
    QVERIFY(spy.isValid());
    compositor.decorationManager()->setPreferredMode(
        static_cast<QWaylandXdgToplevel::DecorationMode>(requestedMode));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(int(compositor.decorationManager()->preferredMode()), requestedMode);
}

void TestXdgDecorations::settingCurrentPreferredModeDoesNotEmit() {
    SystaliaCompositor compositor;
    compositor.setSocketName("wayland-test-decor-noop");
    compositor.create();
    QVERIFY(compositor.decorationManager());
    QSignalSpy spy(compositor.decorationManager(),
                   &QWaylandXdgDecorationManagerV1::preferredModeChanged);
    QVERIFY(spy.isValid());
    compositor.decorationManager()->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestXdgDecorations)
#include "tst_XdgDecorations.moc"
