#include <QTest>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include "SystaliaCompositor.hpp"

class TestWaylandSocket : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testSocketCreationAndDestruction();
    void testClientCountInitialZero();
private:
    QTemporaryDir m_runtime;
};

void TestWaylandSocket::initTestCase() {
    QVERIFY(m_runtime.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
}

void TestWaylandSocket::testSocketCreationAndDestruction() {
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        QSKIP("Variável de ambiente XDG_RUNTIME_DIR não configurada.");
    }

    const QString socketPath = runtimeDir + QStringLiteral("/wayland-systalia-0");

    // Limpar qualquer resquício prévio antes do teste
    if (QFile::exists(socketPath)) {
        QFile::remove(socketPath);
    }

    {
        SystaliaCompositor compositor;
        compositor.setSocketName("wayland-systalia-0");
        compositor.create();

        QVERIFY2(compositor.isCreated(), "O compositor Wayland deve reportar isCreated() == true");
        QCOMPARE(compositor.socketName(), QByteArray("wayland-systalia-0"));
        QVERIFY2(QFile::exists(socketPath), "O arquivo de socket Unix wayland-systalia-0 deve existir em XDG_RUNTIME_DIR");
    }

    // Após o término do escopo e destruição do SystaliaCompositor, o socket deve ser removido
    QVERIFY2(!QFile::exists(socketPath), "O socket Unix deve ser removido com segurança ao destruir o compositor");
}

void TestWaylandSocket::testClientCountInitialZero() {
    SystaliaCompositor compositor;
    QCOMPARE(compositor.clientCount(), 0);
}

QTEST_MAIN(TestWaylandSocket)
#include "tst_WaylandSocket.moc"
