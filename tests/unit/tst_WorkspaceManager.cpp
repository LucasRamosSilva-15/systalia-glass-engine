#include <QKeyEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "SystaliaCompositor.hpp"
#include "WorkspaceManagerService.hpp"

class TestWorkspaceManager final : public QObject {
    Q_OBJECT

private slots:
    void initialStateIsDefault();
    void sequentialNavigationRespectsBounds();
    void directSwitchRejectsOutOfBoundsIndices();
    void transitionStateActivatesAndSettles();
    void windowWorkspaceAssociationAndPersistence();
    void moveActiveWindowToWorkspaceUpdatesAssociation();
    void destroyedWindowsAreCleanedUpAutomatically();
    void compositorInterceptsWorkspaceKeyboardShortcuts();
    void workspaceViewportTranslatesWithParallax();
};

void TestWorkspaceManager::initialStateIsDefault() {
    WorkspaceManagerService manager;
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(manager.totalWorkspaces(), 4);
    QCOMPARE(manager.isTransitioning(), false);
    QCOMPARE(manager.activeWindow(), nullptr);
}

void TestWorkspaceManager::sequentialNavigationRespectsBounds() {
    WorkspaceManagerService manager;
    QSignalSpy workspaceSpy(&manager, &WorkspaceManagerService::currentWorkspaceChanged);

    // Retenção no limite mínimo (0)
    manager.previousWorkspace();
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(workspaceSpy.count(), 0);

    // Avanço sequencial 0 -> 1 -> 2 -> 3
    manager.nextWorkspace();
    QCOMPARE(manager.currentWorkspace(), 1);
    manager.nextWorkspace();
    QCOMPARE(manager.currentWorkspace(), 2);
    manager.nextWorkspace();
    QCOMPARE(manager.currentWorkspace(), 3);
    QCOMPARE(workspaceSpy.count(), 3);

    // Retenção no limite máximo (3)
    manager.nextWorkspace();
    QCOMPARE(manager.currentWorkspace(), 3);
    QCOMPARE(workspaceSpy.count(), 3);

    // Recuo sequencial 3 -> 2 -> 1 -> 0
    manager.previousWorkspace();
    QCOMPARE(manager.currentWorkspace(), 2);
    manager.previousWorkspace();
    QCOMPARE(manager.currentWorkspace(), 1);
    manager.previousWorkspace();
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(workspaceSpy.count(), 6);

    // Retenção novamente em 0
    manager.previousWorkspace();
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(workspaceSpy.count(), 6);
}

void TestWorkspaceManager::directSwitchRejectsOutOfBoundsIndices() {
    WorkspaceManagerService manager;
    QSignalSpy workspaceSpy(&manager, &WorkspaceManagerService::currentWorkspaceChanged);

    manager.switchToWorkspace(-1);
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(workspaceSpy.count(), 0);

    manager.switchToWorkspace(-100);
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(workspaceSpy.count(), 0);

    manager.switchToWorkspace(4);
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(workspaceSpy.count(), 0);

    manager.switchToWorkspace(99);
    QCOMPARE(manager.currentWorkspace(), 0);
    QCOMPARE(workspaceSpy.count(), 0);

    manager.switchToWorkspace(2);
    QCOMPARE(manager.currentWorkspace(), 2);
    QCOMPARE(workspaceSpy.count(), 1);

    // Comutar para o mesmo índice não deve emitir sinal redundante
    manager.switchToWorkspace(2);
    QCOMPARE(manager.currentWorkspace(), 2);
    QCOMPARE(workspaceSpy.count(), 1);

    manager.switchToWorkspace(3);
    QCOMPARE(manager.currentWorkspace(), 3);
    QCOMPARE(workspaceSpy.count(), 2);
}

void TestWorkspaceManager::transitionStateActivatesAndSettles() {
    WorkspaceManagerService manager;
    QSignalSpy transitionSpy(&manager, &WorkspaceManagerService::isTransitioningChanged);

    QCOMPARE(manager.isTransitioning(), false);
    manager.switchToWorkspace(1);
    QCOMPARE(manager.isTransitioning(), true);
    QVERIFY(transitionSpy.count() >= 1);

    QTRY_COMPARE_WITH_TIMEOUT(manager.isTransitioning(), false, 1000);
}

void TestWorkspaceManager::windowWorkspaceAssociationAndPersistence() {
    WorkspaceManagerService manager;
    QQuickItem windowA;
    QQuickItem windowB;

    QCOMPARE(manager.windowWorkspace(nullptr), -1);
    QCOMPARE(manager.windowWorkspace(&windowA), 0);

    manager.setWindowWorkspace(&windowA, 2);
    manager.setWindowWorkspace(&windowB, 3);

    QCOMPARE(manager.windowWorkspace(&windowA), 2);
    QCOMPARE(manager.windowWorkspace(&windowB), 3);
    QVERIFY(!manager.hasWindowsInWorkspace(0));
    QVERIFY(!manager.hasWindowsInWorkspace(1));
    QVERIFY(manager.hasWindowsInWorkspace(2));
    QVERIFY(manager.hasWindowsInWorkspace(3));
    QCOMPARE(manager.windowCountInWorkspace(2), 1);
    QCOMPARE(manager.windowCountInWorkspace(3), 1);

    // Índices inválidos são ignorados e preservam a associação anterior
    manager.setWindowWorkspace(&windowA, -1);
    manager.setWindowWorkspace(&windowA, 5);
    QCOMPARE(manager.windowWorkspace(&windowA), 2);
}

void TestWorkspaceManager::moveActiveWindowToWorkspaceUpdatesAssociation() {
    WorkspaceManagerService manager;
    QQuickItem activeWin;

    manager.setWindowWorkspace(&activeWin, 0);
    manager.setActiveWindow(&activeWin);
    QCOMPARE(manager.activeWindow(), &activeWin);
    QCOMPARE(manager.windowWorkspace(&activeWin), 0);

    manager.moveActiveWindowToWorkspace(3);
    QCOMPARE(manager.windowWorkspace(&activeWin), 3);
    QCOMPARE(manager.windowCountInWorkspace(0), 0);
    QCOMPARE(manager.windowCountInWorkspace(3), 1);

    // Índice inválido não altera o workspace da janela ativa
    manager.moveActiveWindowToWorkspace(-2);
    manager.moveActiveWindowToWorkspace(10);
    QCOMPARE(manager.windowWorkspace(&activeWin), 3);
}

void TestWorkspaceManager::destroyedWindowsAreCleanedUpAutomatically() {
    WorkspaceManagerService manager;
    {
        auto transientWindow = std::make_unique<QQuickItem>();
        manager.setWindowWorkspace(transientWindow.get(), 1);
        QVERIFY(manager.hasWindowsInWorkspace(1));
        QCOMPARE(manager.windowCountInWorkspace(1), 1);
    }
    QVERIFY(!manager.hasWindowsInWorkspace(1));
    QCOMPARE(manager.windowCountInWorkspace(1), 0);
}

void TestWorkspaceManager::compositorInterceptsWorkspaceKeyboardShortcuts() {
    WorkspaceManagerService manager;
    SystaliaCompositor compositor;
    QQuickWindow hostWindow;
    compositor.attachTaskSwitcherWindow(&hostWindow);

    QQuickItem activeWin;
    manager.setWindowWorkspace(&activeWin, 0);
    manager.setActiveWindow(&activeWin);

    // Super + Ctrl + Right -> Workspace 1
    QKeyEvent nextPress(QEvent::KeyPress, Qt::Key_Right,
                        Qt::MetaModifier | Qt::ControlModifier);
    QCoreApplication::sendEvent(&hostWindow, &nextPress);
    QCOMPARE(manager.currentWorkspace(), 1);

    // Super + Ctrl + Left -> Workspace 0
    QKeyEvent prevPress(QEvent::KeyPress, Qt::Key_Left,
                        Qt::MetaModifier | Qt::ControlModifier);
    QCoreApplication::sendEvent(&hostWindow, &prevPress);
    QCOMPARE(manager.currentWorkspace(), 0);

    // Super + 4 -> Workspace 3
    QKeyEvent switch4Press(QEvent::KeyPress, Qt::Key_4, Qt::MetaModifier);
    QCoreApplication::sendEvent(&hostWindow, &switch4Press);
    QCOMPARE(manager.currentWorkspace(), 3);

    // Super + Shift + 3 -> Move active window para Workspace 2
    QKeyEvent move3Press(QEvent::KeyPress, Qt::Key_3,
                         Qt::MetaModifier | Qt::ShiftModifier);
    QCoreApplication::sendEvent(&hostWindow, &move3Press);
    QCOMPARE(manager.windowWorkspace(&activeWin), 2);
}

void TestWorkspaceManager::workspaceViewportTranslatesWithParallax() {
    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/WorkspaceViewport.qml")));
    std::unique_ptr<QQuickItem> viewport(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(viewport, qPrintable(component.errorString()));

    viewport->setWidth(1600.0);
    viewport->setHeight(900.0);
    viewport->setProperty("totalWorkspaces", 4);
    viewport->setProperty("currentWorkspace", 0);

    QCOMPARE(viewport->property("viewportOffset").toReal(), 0.0);
    QCOMPARE(viewport->property("parallaxOffset").toReal(), 0.0);

    viewport->setProperty("currentWorkspace", 2);
    QTRY_COMPARE_WITH_TIMEOUT(viewport->property("viewportOffset").toReal(), -3200.0, 1000);
    const qreal parallax = viewport->property("parallaxOffset").toReal();
    QVERIFY(std::abs(parallax - (-3200.0 * 0.35)) < 1.0);
}

QTEST_MAIN(TestWorkspaceManager)
#include "tst_WorkspaceManager.moc"

