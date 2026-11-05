#include <QClipboard>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "ProcessLauncherService.hpp"
#include "SystaliaCompositor.hpp"
#include "WindowStackModel.hpp"
#include "XWaylandManager.hpp"

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

class TestXWaylandIntegration final : public QObject {
    Q_OBJECT

private slots:
    void testCommandLineArgumentsFormatting();
    void testAsyncDisplayFdHandshakeAndEnvironmentExport();
    void testProcessLifecycleStartStopAndDescriptorCleanup();
    void testLegacyWindowMetadataAndCompositorMapping();
    void testBidirectionalClipboardBridge();
    void testNativeSurfaceIsNotAssignedToPendingX11Window();
};

void TestXWaylandIntegration::testCommandLineArgumentsFormatting() {
    // 1. Validação da montagem exata da linha de comandos Xwayland:
    //    Xwayland :1 -rootless -terminate -core -listenfd <fd_x11> -wm <fd_wm> -displayfd <fd_display>
    const QStringList args = XWaylandManager::buildCommandLineArguments(
        QStringLiteral(":1"), 12, 15, 18);

    const QStringList expected{
        QStringLiteral(":1"),
        QStringLiteral("-rootless"),
        QStringLiteral("-terminate"),
        QStringLiteral("-core"),
        QStringLiteral("-listenfd"),
        QStringLiteral("12"),
        QStringLiteral("-wm"),
        QStringLiteral("15"),
        QStringLiteral("-displayfd"),
        QStringLiteral("18")
    };
    QCOMPARE(args, expected);

    // Normalização automática quando o número é fornecido sem ':'
    const QStringList normalizedArgs = XWaylandManager::buildCommandLineArguments(
        QStringLiteral("2"), 5, 6, 7);
    QCOMPARE(normalizedArgs.first(), QStringLiteral(":2"));

    // Validação do parser estático para payloads com e sem ':'
    bool ok = false;
    QCOMPARE(XWaylandManager::parseDisplayFdPayload(QByteArrayLiteral(":1\n"), &ok),
             QStringLiteral(":1"));
    QVERIFY(ok);

    QCOMPARE(XWaylandManager::parseDisplayFdPayload(QByteArrayLiteral("1\n"), &ok),
             QStringLiteral(":1"));
    QVERIFY(ok);

    QCOMPARE(XWaylandManager::parseDisplayFdPayload(QByteArrayLiteral("invalid\n"), &ok),
             QString());
    QVERIFY(!ok);
}

void TestXWaylandIntegration::testAsyncDisplayFdHandshakeAndEnvironmentExport() {
#if defined(Q_OS_UNIX)
    const QByteArray previousDisplay = qgetenv("DISPLAY");

    int fds[2] = {-1, -1};
    QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    XWaylandManager manager;
    QSignalSpy displayReadySpy(&manager, &XWaylandManager::displayReady);
    QVERIFY(displayReadySpy.isValid());

    QVERIFY(manager.attachDisplayFdReadDescriptor(fds[0]));
    QVERIFY(!manager.isReady());

    // Injeção simulada do descritor reportando ":1\n"
    const QByteArray payload = QByteArrayLiteral(":1\n");
    const ssize_t written = ::write(fds[1], payload.constData(), static_cast<size_t>(payload.size()));
    QCOMPARE(written, static_cast<ssize_t>(payload.size()));

    QTRY_COMPARE_WITH_TIMEOUT(displayReadySpy.count(), 1, 2000);
    QCOMPARE(displayReadySpy.at(0).at(0).toString(), QStringLiteral(":1"));
    QVERIFY(manager.isReady());
    QCOMPARE(manager.displayName(), QStringLiteral(":1"));
    QCOMPARE(manager.displayNumber(), 1);

    // Confirma que DISPLAY=:1 foi exposto globalmente e propagado ao ProcessLauncherService
    QCOMPARE(qEnvironmentVariable("DISPLAY"), QStringLiteral(":1"));
    const QProcessEnvironment launcherEnv = ProcessLauncherService::waylandProcessEnvironment();
    QCOMPARE(launcherEnv.value(QStringLiteral("DISPLAY")), QStringLiteral(":1"));

    ::close(fds[1]);
    manager.stop();

    if (!previousDisplay.isEmpty()) {
        qputenv("DISPLAY", previousDisplay);
    } else {
        qunsetenv("DISPLAY");
    }
#endif
}

void TestXWaylandIntegration::testProcessLifecycleStartStopAndDescriptorCleanup() {
#if defined(Q_OS_UNIX)
    XWaylandManager manager;

    // 1. Verifica a alocação de pares de descritores Unix e limpeza em stop()
    QVERIFY(manager.createSocketPairs(97));
    QVERIFY(manager.waylandServerFd() >= 0);
    QVERIFY(manager.waylandClientFd() >= 0);
    QVERIFY(manager.wmMasterFd() >= 0);
    QVERIFY(manager.wmClientFd() >= 0);
    QVERIFY(manager.displayReadFd() >= 0);
    QVERIFY(manager.displayWriteFd() >= 0);
    QVERIFY(manager.x11ListenFd() >= 0);

    const int savedWmMasterFd = manager.wmMasterFd();
    const int savedListenFd = manager.x11ListenFd();

    manager.stop();

    QCOMPARE(manager.waylandServerFd(), -1);
    QCOMPARE(manager.waylandClientFd(), -1);
    QCOMPARE(manager.wmMasterFd(), -1);
    QCOMPARE(manager.wmClientFd(), -1);
    QCOMPARE(manager.displayReadFd(), -1);
    QCOMPARE(manager.displayWriteFd(), -1);
    QCOMPARE(manager.x11ListenFd(), -1);

    // Confirma que os descritores foram efetivamente fechados no kernel (fcntl retorna -1 / EBADF)
    QCOMPARE(::fcntl(savedWmMasterFd, F_GETFD), -1);
    QCOMPARE(::fcntl(savedListenFd, F_GETFD), -1);

    // 2. Valida o ciclo de vida completo com um QProcess real em execução (binário sleep ou Xwayland)
    const QString sleepBin = QStandardPaths::findExecutable(QStringLiteral("sleep"));
    if (!sleepBin.isEmpty()) {
        // Script ou binário simulando processo Xwayland de longa duração para testar terminate()/kill()
        const QString shBin = QStandardPaths::findExecutable(QStringLiteral("sh"));
        if (!shBin.isEmpty()) {
            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            const QString mockXwayland = tempDir.filePath(QStringLiteral("mock_Xwayland.sh"));
            {
                QFile script(mockXwayland);
                QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Text));
                script.write("#!/bin/sh\nsleep 30\n");
                script.close();
                QVERIFY(QFile::setPermissions(
                    mockXwayland,
                    QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
            }

            manager.setXwaylandExecutable(mockXwayland);
            QCOMPARE(manager.xwaylandExecutable(), mockXwayland);

            QVERIFY(manager.start(96));
            QVERIFY(manager.isRunning());
            QCOMPARE(manager.currentCommandLineArguments().first(), QStringLiteral(":96"));
            QVERIFY(manager.currentCommandLineArguments().contains(QStringLiteral("-rootless")));
            QVERIFY(manager.currentCommandLineArguments().contains(QStringLiteral("-terminate")));
            QVERIFY(manager.currentCommandLineArguments().contains(QStringLiteral("-core")));
            QVERIFY(manager.currentCommandLineArguments().contains(QStringLiteral("-listenfd")));
            QVERIFY(manager.currentCommandLineArguments().contains(QStringLiteral("-wm")));
            QVERIFY(manager.currentCommandLineArguments().contains(QStringLiteral("-displayfd")));

            // Invocar stop() deve finalizar o QProcess e limpar todos os descritores temporários
            manager.stop();
            QVERIFY(!manager.isRunning());
            QVERIFY(!manager.isReady());
            QCOMPARE(manager.wmMasterFd(), -1);
            QCOMPARE(manager.displayReadFd(), -1);
            QCOMPARE(manager.x11ListenFd(), -1);
        }
    }
#endif
}

void TestXWaylandIntegration::testLegacyWindowMetadataAndCompositorMapping() {
    // 1. Parser de WM_CLASS ("instance\0class\0")
    const QByteArray rawWmClass("Navigator\0Firefox\0", 18);
    QCOMPARE(XWaylandSurfaceWrapper::parseWmClassPayload(rawWmClass),
             QStringLiteral("firefox"));

    // 2. Criação e atualização de metadados de janela X11
    SystaliaCompositor compositor;
    XWaylandManager *xwl = compositor.xwaylandManager();
    QVERIFY(xwl != nullptr);

    XWaylandSurfaceWrapper *win = xwl->registerOrUpdateX11Window(
        0x200001u,
        QStringLiteral("xclock"),
        QStringLiteral("XClock"),
        QRect(0, 0, 320, 320));
    QVERIFY(win != nullptr);
    QVERIFY(win->isXWayland());
    QCOMPARE(win->x11WindowId(), 0x200001u);
    QCOMPARE(win->title(), QStringLiteral("xclock"));
    QCOMPARE(win->appId(), QStringLiteral("XClock"));
    QCOMPARE(win->windowGeometry(), QRect(0, 0, 320, 320));
    QCOMPARE(xwl->managedWindowCount(), 1);

    // Estados EWMH (_NET_WM_STATE)
    QSignalSpy maxSpy(win, &XWaylandSurfaceWrapper::maximizedChanged);
    QSignalSpy minSpy(win, &XWaylandSurfaceWrapper::minimizedChanged);
    win->setMaximized(true);
    win->setMinimized(true);
    QCOMPARE(maxSpy.count(), 1);
    QCOMPARE(minSpy.count(), 1);
    QVERIFY(win->maximized());
    QVERIFY(win->minimized());

    // Remoção limpa
    xwl->unregisterX11Window(0x200001u);
    QCOMPARE(xwl->managedWindowCount(), 0);
}

void TestXWaylandIntegration::testNativeSurfaceIsNotAssignedToPendingX11Window() {
    SystaliaCompositor compositor;
    auto *manager = compositor.xwaylandManager();
    auto *window = manager->registerOrUpdateX11Window(0x200002, "pending", "legacy", QRect(0, 0, 320, 240));
    QWaylandSurface nativeSurface;
    manager->notifySurfaceCreated(&nativeSurface);
    QVERIFY(!window->surface());
    QCOMPARE(compositor.windowCount(), 0);
}

void TestXWaylandIntegration::testBidirectionalClipboardBridge() {
    XWaylandManager manager;
    QSignalSpy clipboardSpy(&manager, &XWaylandManager::clipboardSynchronized);
    QVERIFY(clipboardSpy.isValid());

    // 1. Wayland -> X11 (CLIPBOARD e PRIMARY)
    manager.syncWaylandToX11Clipboard(QStringLiteral("Texto Wayland Nativo UTF-8"), false);
    QCOMPARE(manager.clipboardText(), QStringLiteral("Texto Wayland Nativo UTF-8"));
    QCOMPARE(clipboardSpy.count(), 1);
    QCOMPARE(clipboardSpy.at(0).at(0).toString(), QStringLiteral("Texto Wayland Nativo UTF-8"));
    QCOMPARE(clipboardSpy.at(0).at(1).value<XWaylandManager::ClipboardDirection>(),
             XWaylandManager::ClipboardDirection::WaylandToX11);

    manager.syncWaylandToX11Clipboard(QStringLiteral("Seleção Primária Wayland"), true);
    QCOMPARE(manager.primarySelectionText(), QStringLiteral("Seleção Primária Wayland"));
    QCOMPARE(clipboardSpy.count(), 2);

    // 2. X11 -> Wayland (CLIPBOARD)
    manager.syncX11ToWaylandClipboard(QStringLiteral("Texto Cliente X11 Legado"), false);
    QCOMPARE(manager.clipboardText(), QStringLiteral("Texto Cliente X11 Legado"));
    QCOMPARE(clipboardSpy.count(), 3);
    QCOMPARE(clipboardSpy.at(2).at(0).toString(), QStringLiteral("Texto Cliente X11 Legado"));
    QCOMPARE(clipboardSpy.at(2).at(1).value<XWaylandManager::ClipboardDirection>(),
             XWaylandManager::ClipboardDirection::X11ToWayland);

    if (QClipboard *cb = QGuiApplication::clipboard()) {
        QCOMPARE(cb->text(QClipboard::Clipboard), QStringLiteral("Texto Cliente X11 Legado"));
    }
}

QTEST_MAIN(TestXWaylandIntegration)
#include "tst_XWaylandIntegration.moc"

