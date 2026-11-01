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
        WorkspaceManager.switchToWorkspace(0)
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
        WorkspaceManager.switchToWorkspace(0)
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
        WorkspaceManager.setActiveWindow(windowContainer)
        windowCanvas.z = 10
        clientWindowsLayer.z = 40
        clientWindowsLayer.topZIndex += 1
        windowContainer.z = clientWindowsLayer.topZIndex
    }

    readonly property rect workspaceAvailableArea: Qt.rect(0, 0, workspace.width, workspace.height)
    readonly property rect shellCanvasAvailableArea: Qt.rect(18, 66, 1626, 720)

    function updateShellWindowDrag(win, scenePos) {
        if (!win)
            return
        const screenBounds = Qt.rect(0, 0, rootWindow.width, rootWindow.height)
        LiquidTiling.updateSnapPreview(scenePos, screenBounds, workspaceAvailableArea, win.cornerRadius)
    }

    function finishShellWindowDrag(win, scenePos) {
        if (!win)
            return
        const screenBounds = Qt.rect(0, 0, rootWindow.width, rootWindow.height)
        const targetSnap = LiquidTiling.calculateSnapTarget(scenePos, screenBounds)
        LiquidTiling.clearSnapPreview()
        if (targetSnap === 0)
            return
        if (win.snapState === 0) {
            win.restoreGeometry = Qt.rect(win.x, win.y, win.width, win.height)
        }
        win.snapState = targetSnap
        LiquidTiling.registerWindowSnap(win, targetSnap, 0)
        const targetGeo = LiquidTiling.getSnapGeometry(targetSnap, shellCanvasAvailableArea,
                                                       LiquidTiling.splitRatio)
        if (targetGeo.width > 0 && targetGeo.height > 0) {
            win.x = Math.round(targetGeo.x)
            win.y = Math.round(targetGeo.y)
            win.width = Math.round(targetGeo.width)
            win.height = Math.round(targetGeo.height)
        }
    }

    function unsnapShellWindow(win) {
        if (!win || win.snapState === 0)
            return
        win.snapState = 0
        LiquidTiling.unregisterWindowSnap(win)
        if (win.restoreGeometry.width > 0 && win.restoreGeometry.height > 0) {
            win.width = win.restoreGeometry.width
            win.height = win.restoreGeometry.height
        }
    }

    function syncSnappedWindowsInActiveWorkspace(ratio, interactive) {
        const activeWs = WorkspaceManager.currentWorkspace
        for (let i = 0; i < trackedClientContainers.length; ++i) {
            const win = trackedClientContainers[i]
            if (win && win.workspaceIndex === activeWs && win.snapState !== 0) {
                win.updateSnappedGeometry(ratio, interactive)
            }
        }
        if (activeWs === 0) {
            if (glassChassis.snapState !== 0) {
                const filesGeo = LiquidTiling.getSnapGeometry(
                    glassChassis.snapState, shellCanvasAvailableArea, ratio)
                if (filesGeo.width > 0 && filesGeo.height > 0) {
                    glassChassis.x = Math.round(filesGeo.x)
                    glassChassis.y = Math.round(filesGeo.y)
                    glassChassis.width = Math.round(filesGeo.width)
                    glassChassis.height = Math.round(filesGeo.height)
                }
            }
            if (materialWindow.snapState !== 0) {
                const matGeo = LiquidTiling.getSnapGeometry(
                    materialWindow.snapState, shellCanvasAvailableArea, ratio)
                if (matGeo.width > 0 && matGeo.height > 0) {
                    materialWindow.x = Math.round(matGeo.x)
                    materialWindow.y = Math.round(matGeo.y)
                    materialWindow.width = Math.round(matGeo.width)
                    materialWindow.height = Math.round(matGeo.height)
                }
            }
        }
    }

    property var trackedClientContainers: []

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
                    const wsIndex = WorkspaceManager.currentWorkspace
                    const targetPage = clientWindowsLayer.workspacePageAt(wsIndex)
                    const parentLayer = targetPage ? targetPage : clientWindowsLayer
                    const offset = (parentLayer.children.length * 36) % 180
                    const clientWindow = comp.createObject(parentLayer, {
                        toplevel: toplevelObject,
                        waylandServer: waylandServer,
                        workspace: rootWindow,
                        tilingService: LiquidTiling,
                        outputManager: OutputManager,
                        backdropSource: backdropGrab,
                        workspaceIndex: wsIndex,
                        screenResolution: Qt.point(rootWindow.width * rootWindow.devicePixelRatio,
                                                   rootWindow.height * rootWindow.devicePixelRatio),
                        animTime: animTimer.tick,
                        x: Math.round(160 + offset),
                        y: Math.round(80 + offset)
                    })
                    if (clientWindow) {
                        clientWindow.inActiveWorkspace = Qt.binding(() => {
                            return clientWindow.workspaceIndex === WorkspaceManager.currentWorkspace
                        })
                        clientWindow.workspaceTransitioning = Qt.binding(() => {
                            return WorkspaceManager.isTransitioning
                        })
                        WorkspaceManager.setWindowWorkspace(clientWindow, wsIndex)
                        const list = rootWindow.trackedClientContainers.slice()
                        list.push(clientWindow)
                        rootWindow.trackedClientContainers = list
                        clientWindow.Component.destruction.connect(() => {
                            const updated = rootWindow.trackedClientContainers.filter(w => w !== clientWindow)
                            rootWindow.trackedClientContainers = updated
                        })
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

    Item {
        id: wallpaperBackdropRoot
        anchors.fill: parent
        z: -10

        Image {
            id: wallpaper
            width: parent.width
            height: parent.height
            x: clientWindowsLayer.parallaxOffset * 0.12
            y: 0
            source: Qt.resolvedUrl("assets/figma/valo-ocean-atmosphere.png")
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            smooth: true
            mipmap: true
            visible: status === Image.Ready
        }

        Image {
            id: wallpaperParallaxExtension
            width: parent.width
            height: parent.height
            x: wallpaper.x + wallpaper.width
            y: 0
            mirror: true
            source: wallpaper.source
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            smooth: true
            mipmap: true
            visible: wallpaper.status === Image.Ready && wallpaper.x < 0
        }
    }

    ShaderEffectSource {
        id: backdropGrab
        anchors.fill: wallpaperBackdropRoot
        sourceItem: wallpaperBackdropRoot
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
        x: (rootWindow.width - width * scale) / 2 + clientWindowsLayer.viewportOffset
        y: (rootWindow.height - height * scale) / 2
        transformOrigin: Item.TopLeft
        z: 10
        visible: (WorkspaceManager.currentWorkspace === 0) || WorkspaceManager.isTransitioning
        enabled: (WorkspaceManager.currentWorkspace === 0) && !WorkspaceManager.isTransitioning

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

    WorkspaceViewport {
        id: clientWindowsLayer
        anchors.fill: workspace
        z: 20
        property int topZIndex: 0
        currentWorkspace: WorkspaceManager.currentWorkspace
        totalWorkspaces: WorkspaceManager.totalWorkspaces
        isTransitioning: WorkspaceManager.isTransitioning
        parallaxFactor: 0.35
    }

    Item {
        id: tilingOverlayLayer
        anchors.fill: workspace
        z: 45

        Shell.SnapPreviewOverlay {
            id: snapPreviewOverlay
            active: LiquidTiling.previewVisible
            targetGeometry: LiquidTiling.previewGeometry
            targetRadii: LiquidTiling.previewRadii
            backdropSource: backdropGrab
            screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                          rootWindow.height * rootWindow.devicePixelRatio)
            animationTime: animTimer.tick
        }

        Shell.TileSplitter {
            id: tileSplitter
            tilingService: LiquidTiling
            availableArea: rootWindow.workspaceAvailableArea
            splitRatio: LiquidTiling.splitRatio
            splitterGap: LiquidTiling.splitterGap
            activeSplit: {
                void(LiquidTiling.tilingRevision)
                return LiquidTiling.hasActiveSplitInWorkspace(WorkspaceManager.currentWorkspace)
            }
            onRatioUpdated: (newRatio, interactive) => {
                rootWindow.syncSnappedWindowsInActiveWorkspace(newRatio, interactive)
            }
        }
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
            workspaceManager: WorkspaceManager
            backdropSource: backdropGrab
            screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                          rootWindow.height * rootWindow.devicePixelRatio)
            animationTime: animTimer.tick
        }
    }

    // Réplica configurável da TopBar em monitores secundários via Instantiator
    Instantiator {
        id: multiMonitorTopBarInstantiator
        active: OutputManager.replicateTopBarOnAllScreens && OutputManager.screenCount > 1
        model: OutputManager.screenGeometries
        delegate: Shell.SystaliaTopBar {
            required property int index
            required property rect modelData
            parent: rootWindow.contentItem
            visible: index > 0 && multiMonitorTopBarInstantiator.active
            x: modelData.x + 8
            y: modelData.y + 4
            z: 100
            telemetry: SystemTelemetry
            workspaceManager: WorkspaceManager
            backdropSource: backdropGrab
            screenResolution: OutputManager.physicalResolutionAt(index)
            animationTime: animTimer.tick
        }
    }

    Item {
        id: dockCanvas
        width: 1662
        height: 975
        scale: rootWindow.designScale
        readonly property rect activeScreenRect: OutputManager.screenCount > 0
            ? OutputManager.screenGeometryAt(OutputManager.activeCursorScreen)
            : Qt.rect(0, 0, rootWindow.width, rootWindow.height)
        x: activeScreenRect.x + (rootWindow.width - width * scale) / 2
        y: (rootWindow.height - height * scale) / 2
        Behavior on x {
            NumberAnimation {
                duration: 240
                easing.type: Easing.OutCubic
            }
        }
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
            for (let index = 0; index < rootWindow.trackedClientContainers.length; ++index) {
                const windowContainer = rootWindow.trackedClientContainers[index]
                if (windowContainer && windowContainer.toplevel === toplevelObject) {
                    WorkspaceManager.switchToWorkspace(windowContainer.workspaceIndex)
                    rootWindow.raiseClientWindow(windowContainer)
                    break
                }
            }
        }
        function onToplevelWorkspaceChanged(toplevelObject, workspaceIndex) {
            for (let index = 0; index < rootWindow.trackedClientContainers.length; ++index) {
                const windowContainer = rootWindow.trackedClientContainers[index]
                if (windowContainer && windowContainer.toplevel === toplevelObject) {
                    windowContainer.workspaceIndex = workspaceIndex
                    const targetPage = clientWindowsLayer.workspacePageAt(workspaceIndex)
                    if (targetPage && windowContainer.parent !== targetPage) {
                        windowContainer.parent = targetPage
                    }
                    break
                }
            }
        }
    }

    Connections {
        target: WorkspaceManager
        function onWindowWorkspaceChanged(windowItem, workspaceIndex) {
            if (!windowItem)
                return
            const targetPage = clientWindowsLayer.workspacePageAt(workspaceIndex)
            if (targetPage && windowItem.parent !== targetPage) {
                windowItem.parent = targetPage
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
