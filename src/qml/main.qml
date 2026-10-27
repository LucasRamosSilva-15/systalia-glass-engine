import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor
import Systalia.Engine
import Systalia.Compositor 1.0
import Systalia.System 1.0
import "shell" as Shell

ApplicationWindow {
    id: rootWindow
    width: 1662
    height: 975
    visible: true
    title: qsTr("Systalia Glass Engine — Systalia Desktop Environment")
    color: SystaliaTheme.ocean

    readonly property real designScale: Math.max(0.1, Math.min(width / 1662, height / 975))
    property bool filesWindowMaximized: false
    property rect filesWindowRestoreGeometry: Qt.rect(113, 193, 701, 534)
    property bool materialWindowMaximized: false
    property rect materialWindowRestoreGeometry: Qt.rect(864, 193, 700, 534)

    function raiseFileWindow() {
        glassChassis.visible = true
        windowCanvas.z = 30
        glassChassis.z = 20
        clientWindowsLayer.z = 20
    }

    function minimizeFileWindow() {
        glassChassis.visible = false
    }

    function closeFileWindow() {
        if (filesWindowMaximized)
            toggleFileWindowMaximized()
        glassChassis.visible = false
    }

    function toggleFileWindowMaximized() {
        if (filesWindowMaximized) {
            glassChassis.x = filesWindowRestoreGeometry.x
            glassChassis.y = filesWindowRestoreGeometry.y
            glassChassis.width = filesWindowRestoreGeometry.width
            glassChassis.height = filesWindowRestoreGeometry.height
            filesWindowMaximized = false
            return
        }

        filesWindowRestoreGeometry = Qt.rect(glassChassis.x, glassChassis.y,
                                             glassChassis.width, glassChassis.height)
        glassChassis.x = 18
        glassChassis.y = 66
        glassChassis.width = 1626
        glassChassis.height = 720
        filesWindowMaximized = true
        raiseFileWindow()
    }

    function raiseMaterialWindow() {
        materialWindow.visible = true
        windowCanvas.z = 30
        materialWindow.z = 21
        clientWindowsLayer.z = 20
    }

    function closeMaterialWindow() {
        if (materialWindowMaximized)
            toggleMaterialWindowMaximized()
        materialWindow.visible = false
    }

    function toggleMaterialWindowMaximized() {
        if (materialWindowMaximized) {
            materialWindow.x = materialWindowRestoreGeometry.x
            materialWindow.y = materialWindowRestoreGeometry.y
            materialWindow.width = materialWindowRestoreGeometry.width
            materialWindow.height = materialWindowRestoreGeometry.height
            materialWindowMaximized = false
            return
        }

        materialWindowRestoreGeometry = Qt.rect(materialWindow.x, materialWindow.y,
                                                 materialWindow.width, materialWindow.height)
        materialWindow.x = 18
        materialWindow.y = 66
        materialWindow.width = 1626
        materialWindow.height = 720
        materialWindowMaximized = true
        raiseMaterialWindow()
    }

    function raiseClientWindow(windowContainer) {
        if (!windowContainer)
            return
        windowCanvas.z = 10
        clientWindowsLayer.z = 40
        clientWindowsLayer.topZIndex += 1
        windowContainer.z = clientWindowsLayer.topZIndex
    }

    Component.onCompleted: {
        SystemTelemetry.attachWindow(rootWindow)
        waylandServer.attachTaskSwitcherWindow(rootWindow)
    }

    SystaliaCompositor {
        id: waylandServer
        property string activeClientTitle: ""
        property string activeClientAppId: ""

        onToplevelCreatedForQml: (toplevelObject) => {
            if (toplevelObject) {
                activeClientTitle = toplevelObject.title
                activeClientAppId = toplevelObject.appId
                toplevelObject.titleChanged.connect(() => { activeClientTitle = toplevelObject.title })
                toplevelObject.appIdChanged.connect(() => { activeClientAppId = toplevelObject.appId })

                const comp = Qt.createComponent("WaylandWindowContainer.qml")
                if (comp.status === Component.Ready) {
                    const offset = (clientWindowsLayer.children.length * 36) % 180
                    const clientWindow = comp.createObject(clientWindowsLayer, {
                        toplevel: toplevelObject,
                        waylandServer: waylandServer,
                        workspace: rootWindow,
                        backdropSource: backdropGrab,
                        screenResolution: Qt.point(rootWindow.width * rootWindow.devicePixelRatio,
                                                   rootWindow.height * rootWindow.devicePixelRatio),
                        animTime: animTimer.tick,
                        x: Math.round(160 + offset),
                        y: Math.round(80 + offset)
                    })
                    if (clientWindow) {
                        rootWindow.raiseClientWindow(clientWindow)
                        Qt.callLater(() => waylandServer.activateToplevel(toplevelObject))
                    }
                } else if (comp.status === Component.Error) {
                    console.error("Erro ao instanciar WaylandWindowContainer:", comp.errorString())
                }
            }
        }

        Component.onCompleted: create()
    }

    WaylandOutput {
        compositor: waylandServer
        window: rootWindow
    }

    Image {
        id: wallpaper
        anchors.fill: parent
        z: -10
        source: Qt.resolvedUrl("assets/figma/valo-ocean-atmosphere.png")
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: true
        smooth: true
        mipmap: true
        visible: status === Image.Ready
    }

    ShaderEffectSource {
        id: backdropGrab
        anchors.fill: wallpaper
        sourceItem: wallpaper
        textureSize: Qt.size(Math.max(1, rootWindow.width * rootWindow.devicePixelRatio),
                             Math.max(1, rootWindow.height * rootWindow.devicePixelRatio))
        live: true
        hideSource: false
        visible: false
        recursive: false
        format: ShaderEffectSource.RGBA8
    }

    Item {
        id: workspace
        anchors.fill: parent
        anchors.topMargin: 60
        anchors.bottomMargin: 94
    }

    Timer {
        id: animTimer
        interval: 16
        running: true
        repeat: true
        property real tick: 0.0
        onTriggered: tick += 0.02
    }

    Item {
        id: windowCanvas
        width: 1662
        height: 975
        scale: rootWindow.designScale
        x: (rootWindow.width - width * scale) / 2
        y: (rootWindow.height - height * scale) / 2
        transformOrigin: Item.TopLeft
        z: 10

        Shell.SystaliaFilesWindow {
            id: glassChassis
            x: 113
            y: 193
            z: 10
            backdropSource: backdropGrab
            screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                          rootWindow.height * rootWindow.devicePixelRatio)
            animationTime: animTimer.tick
            controller: rootWindow
            waylandServer: waylandServer
            refractiveIndex: materialWindow.refractiveIndex
            dispersion: materialWindow.dispersion
            caustics: materialWindow.caustics
            surfaceOpacity: materialWindow.surfaceOpacity
            surfaceSlant: materialWindow.surfaceSlant
            cornerRadius: materialWindow.cornerRadius
            materialActive: materialWindow.materialActive
        }

        Shell.SystaliaMaterialWindow {
            id: materialWindow
            x: 864
            y: 193
            z: 10
            backdropSource: backdropGrab
            screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                          rootWindow.height * rootWindow.devicePixelRatio)
            animationTime: animTimer.tick
            controller: rootWindow
        }
    }

    Item {
        id: clientWindowsLayer
        anchors.fill: workspace
        z: 20
        property int topZIndex: 0
    }

    Item {
        id: topBarCanvas
        width: 1662
        height: 975
        scale: rootWindow.designScale
        x: (rootWindow.width - width * scale) / 2
        y: (rootWindow.height - height * scale) / 2
        transformOrigin: Item.TopLeft
        z: 100

        Shell.SystaliaTopBar {
            id: topBar
            x: 8
            y: 4
            telemetry: SystemTelemetry
            backdropSource: backdropGrab
            screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                          rootWindow.height * rootWindow.devicePixelRatio)
            animationTime: animTimer.tick
        }
    }

    Item {
        id: dockCanvas
        width: 1662
        height: 975
        scale: rootWindow.designScale
        x: (rootWindow.width - width * scale) / 2
        y: (rootWindow.height - height * scale) / 2
        transformOrigin: Item.TopLeft
        z: 95

        Shell.SystaliaDock {
            id: systemDock
            x: 555
            y: 817
            visible: !waylandServer.hasMaximizedToplevel
            backdropSource: backdropGrab
            screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                          rootWindow.height * rootWindow.devicePixelRatio)
            activeAppIds: waylandServer.activeAppIds
            animationTime: animTimer.tick
            filesVisible: glassChassis.visible
            settingsOpen: materialWindow.visible
            refractiveIndex: materialWindow.refractiveIndex
            dispersion: materialWindow.dispersion
            caustics: materialWindow.caustics
            surfaceOpacity: materialWindow.surfaceOpacity
            materialActive: materialWindow.materialActive

            onFilesRequested: {
                if (glassChassis.visible)
                    rootWindow.minimizeFileWindow()
                else
                    rootWindow.raiseFileWindow()
            }
            onAudioRequested: {
                if (!ProcessLauncher.launch("pavucontrol"))
                    ProcessLauncher.launch("pwvucontrol")
            }
            onTerminalRequested: {
                if (!ProcessLauncher.launch("alacritty"))
                    ProcessLauncher.launch("foot")
            }
            onIdeRequested: {
                if (!ProcessLauncher.launch("code"))
                    ProcessLauncher.launch("codium")
            }
            onSettingsRequested: {
                if (materialWindow.visible)
                    rootWindow.closeMaterialWindow()
                else
                    rootWindow.raiseMaterialWindow()
            }
        }
    }

    Shell.SystaliaFlowMenu {
        id: flowMenu
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
    }

    Connections {
        target: topBar
        function onLauncherRequested() { flowMenu.toggle() }
        function onControlCenterRequested() { controlCenter.toggle() }
    }

    Connections {
        target: waylandServer
        function onWindowSelected(toplevelObject) {
            for (let index = 0; index < clientWindowsLayer.children.length; ++index) {
                const windowContainer = clientWindowsLayer.children[index]
                if (windowContainer.toplevel === toplevelObject) {
                    rootWindow.raiseClientWindow(windowContainer)
                    break
                }
            }
        }
    }

    Shortcut {
        sequence: "Meta+Space"
        context: Qt.ApplicationShortcut
        onActivated: flowMenu.toggle()
    }

    Shell.SystaliaControlCenter {
        id: controlCenter
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
    }

    Shell.SystaliaNotificationHost {
        id: notificationHost
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
    }

    Shell.SystaliaTaskSwitcher {
        id: taskSwitcher
        anchors.fill: parent
        z: 200
        waylandServer: waylandServer
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
    }
}
