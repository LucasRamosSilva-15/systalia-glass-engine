#include <QTest>
#include <QSignalSpy>
#include "SystaliaCompositor.hpp"
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QtWaylandCompositor/QWaylandSeat>

class TestXdgShell : public QObject {
    Q_OBJECT

private slots:
    void testXdgShellExtensionRegistration();
    void testDefaultSeatCapabilities();
    void testToplevelSignalsAvailable();
};

void TestXdgShell::testXdgShellExtensionRegistration() {
    SystaliaCompositor compositor;
    compositor.setSocketName("wayland-test-shell-0");
    compositor.create();

    QVERIFY2(compositor.isCreated(), "O compositor Wayland deve estar inicializado");
    QVERIFY2(compositor.xdgShell() != nullptr, "A extensão QWaylandXdgShell não deve ser nula");
    QVERIFY2(compositor.xdgShell()->isInitialized(), "A extensão QWaylandXdgShell deve estar registrada no registro global Wayland");
    QCOMPARE(compositor.xdgShell()->interfaceName(), QByteArray("xdg_wm_base"));
}

void TestXdgShell::testDefaultSeatCapabilities() {
    SystaliaCompositor compositor;
    compositor.setSocketName("wayland-test-shell-1");
    compositor.create();

    QVERIFY2(compositor.defaultSeat() != nullptr, "O compositor deve inicializar um assento padrão (defaultSeat)");
    QVERIFY2(compositor.defaultSeat()->capabilities().testFlag(QWaylandSeat::Keyboard), "O assento padrão deve expor capacidade de teclado (Keyboard)");
    QVERIFY2(compositor.defaultSeat()->capabilities().testFlag(QWaylandSeat::Pointer), "O assento padrão deve expor capacidade de ponteiro (Pointer)");
    QVERIFY2(compositor.defaultSeat()->keyboard() != nullptr, "Objeto QWaylandKeyboard deve estar instanciado no assento");
    QVERIFY2(compositor.defaultSeat()->pointer() != nullptr, "Objeto QWaylandPointer deve estar instanciado no assento");
}

void TestXdgShell::testToplevelSignalsAvailable() {
    SystaliaCompositor compositor;
    QSignalSpy spyToplevel(&compositor, &SystaliaCompositor::toplevelCreated);
    QVERIFY2(spyToplevel.isValid(), "Sinal toplevelCreated(QWaylandXdgToplevel*) deve ser válido");

    QSignalSpy spyPopup(&compositor, &SystaliaCompositor::popupCreated);
    QVERIFY2(spyPopup.isValid(), "Sinal popupCreated(QWaylandXdgPopup*) deve ser válido");
}

QTEST_MAIN(TestXdgShell)
#include "tst_XdgShell.moc"
