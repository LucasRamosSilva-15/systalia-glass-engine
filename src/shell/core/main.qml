import QtQuick
import QtQuick.Controls
import QtWayland.Compositor
import Systalia.Engine
import Systalia.Compositor 1.0
import Systalia.System 1.0

ApplicationWindow {
    id: rootWindow
    width: 1662
    height: 975
    visible: true
    title: qsTr("Systalia Glass Engine — Systalia Desktop Environment")
    color: ValoTheme.primitiveNavy950

    // Aliases e adaptadores preservados para compatibilidade integral com testes e modo nested/headless
    property alias waylandServer: shellRoot.waylandServer
    property alias primaryWaylandOutput: shellRoot.primaryWaylandOutput
    property alias glassChassis: shellRoot.glassChassis
    property alias materialWindow: shellRoot.materialWindow
    property alias flowMenu: shellRoot.flowMenu
    property alias controlCenter: shellRoot.controlCenter
    property alias systemDock: shellRoot.systemDock
    property alias topBar: shellRoot.topBar
    property alias internalWindowBackdrop: shellRoot.internalWindowBackdrop
    property alias overlayBackdrop: shellRoot.overlayBackdrop
    property alias backdropGrab: shellRoot.backdropGrab
    property alias trackedClientContainers: shellRoot.trackedClientContainers
    property alias filesWindowMaximized: shellRoot.filesWindowMaximized
    property alias materialWindowMaximized: shellRoot.materialWindowMaximized
    property alias notificationHost: shellRoot.notificationHost
    property alias clientWindowsLayer: shellRoot.clientWindowsLayer
    readonly property var filesOpticalItem: shellRoot.filesOpticalItem
    readonly property var dockOpticalItem: shellRoot.dockOpticalItem
    readonly property var topBarOpticalItem: shellRoot.topBarOpticalItem

    function bringWindowToFront(win) { shellRoot.bringWindowToFront(win) }
    function raiseFileWindow() { shellRoot.raiseFileWindow() }
    function minimizeFileWindow() { shellRoot.minimizeFileWindow() }
    function closeFileWindow() { shellRoot.closeFileWindow() }
    function toggleFileWindowMaximized() { shellRoot.toggleFileWindowMaximized() }
    function raiseMaterialWindow() { shellRoot.raiseMaterialWindow() }
    function minimizeMaterialWindow() { shellRoot.minimizeMaterialWindow() }
    function closeMaterialWindow() { shellRoot.closeMaterialWindow() }
    function toggleMaterialWindowMaximized() { shellRoot.toggleMaterialWindowMaximized() }
    function unsnapShellWindow(win) { shellRoot.unsnapShellWindow(win) }
    function finishShellWindowDrag(win, snapMode, targetWorkspace) {
        shellRoot.finishShellWindowDrag(win, snapMode, targetWorkspace)
    }
    function externalBackdropBlocked(targetWindow, internalWindows, isOverlay) {
        return shellRoot.externalBackdropBlocked(targetWindow, internalWindows, isOverlay)
    }
    function nearestBackdropWindow(targetWindow, isOverlay) {
        return shellRoot.nearestBackdropWindow(targetWindow, isOverlay)
    }

    ShellRoot {
        id: shellRoot
        anchors.fill: parent
        hostWindow: rootWindow
    }
}
