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

    property bool filesWindowMaximized: false
    property rect filesWindowRestoreGeometry: Qt.rect(80, 80, 701, 534)
    property bool materialWindowMaximized: false
    property rect materialWindowRestoreGeometry: Qt.rect(810, 80, 700, 534)
    property int topZIndex: 20

    function bringWindowToFront(win) {
        if (!win)
            return
        topZIndex += 1
        win.z = topZIndex
        WorkspaceManager.setActiveWindow(win)
    }

    function raiseFileWindow() {
        WorkspaceManager.switchToWorkspace(0)
        glassChassis.windowOpen = true
        glassChassis.visible = true
        bringWindowToFront(glassChassis)
    }

    function minimizeFileWindow() {
        glassChassis.windowOpen = false
        glassChassis.visible = false
    }

    function closeFileWindow() {
        if (filesWindowMaximized)
            toggleFileWindowMaximized()
        glassChassis.windowOpen = false
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
        glassChassis.x = 8
        glassChassis.y = 8
        glassChassis.width = workspace.width - 16
        glassChassis.height = workspace.height - 16
        filesWindowMaximized = true
        raiseFileWindow()
    }

    function raiseMaterialWindow() {
        WorkspaceManager.switchToWorkspace(0)
        materialWindow.windowOpen = true
        materialWindow.visible = true
        bringWindowToFront(materialWindow)
    }

    function closeMaterialWindow() {
        if (materialWindowMaximized)
            toggleMaterialWindowMaximized()
        materialWindow.windowOpen = false
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
        materialWindow.x = 8
        materialWindow.y = 8
        materialWindow.width = workspace.width - 16
        materialWindow.height = workspace.height - 16
        materialWindowMaximized = true
        raiseMaterialWindow()
    }

    function raiseClientWindow(windowContainer) {
        if (!windowContainer)
            return
        bringWindowToFront(windowContainer)
        if (windowContainer.waylandServer && windowContainer.toplevel) {
            windowContainer.waylandServer.activateToplevel(windowContainer.toplevel)
        }
    }

    readonly property rect workspaceAvailableArea: Qt.rect(0, 0, workspace.width, workspace.height)
    readonly property rect shellCanvasAvailableArea: Qt.rect(8, 8, workspace.width - 16, workspace.height - 16)

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

    function syncSnappedWindowsInActiveWorkspace(ratio, interactive, forceDispatch) {
        const activeWs = WorkspaceManager.currentWorkspace
        const dispatchConfigure = Boolean(forceDispatch)
            || LiquidTiling.shouldDispatchInteractiveResize(Boolean(interactive))
        for (let i = 0; i < trackedClientContainers.length; ++i) {
            const win = trackedClientContainers[i]
            if (win && win.workspaceIndex === activeWs && win.snapState !== 0) {
                win.updateSnappedGeometry(ratio, interactive, dispatchConfigure)
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
        const page0 = clientWindowsLayer.workspacePageAt(0)
        if (page0) {
            if (glassChassis.parent !== page0)
                glassChassis.parent = page0
            if (materialWindow.parent !== page0)
                materialWindow.parent = page0
        }
        WorkspaceManager.setWindowWorkspace(glassChassis, 0)
        WorkspaceManager.setWindowWorkspace(materialWindow, 0)
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

        Component.onCompleted: {
            create()
            startXWayland(1)
        }
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
            fillMode: Image.PreserveAspectCrop
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
            fillMode: Image.PreserveAspectCrop
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

    Shell.SystaliaFilesWindow {
        id: glassChassis
        parent: clientWindowsLayer.workspacePageAt(0)
        x: 80
        y: 80
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
        parent: clientWindowsLayer.workspacePageAt(0)
        x: 810
        y: 80
        z: 11
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
        controller: rootWindow
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

    Shell.SystaliaTopBar {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 4
        height: 45
        z: 100
        telemetry: SystemTelemetry
        workspaceManager: WorkspaceManager
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
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
            width: modelData.width - 16
            height: 45
            z: 100
            telemetry: SystemTelemetry
            workspaceManager: WorkspaceManager
            backdropSource: backdropGrab
            screenResolution: OutputManager.physicalResolutionAt(index)
            animationTime: animTimer.tick
            onLauncherRequested: flowMenu.toggle()
            onControlCenterRequested: controlCenter.toggle()
            onTerminalRequested: {
                if (!ProcessLauncher.launch("alacritty"))
                    ProcessLauncher.launch("foot")
            }
            onPowerRequested: controlCenter.open()
        }
    }

    Item {
        id: dockCanvas
        readonly property bool isMultiScreenSpan: OutputManager.screenCount > 1
                                                  && rootWindow.width >= OutputManager.virtualCanvasBounds.width
        readonly property rect activeScreenRect: OutputManager.screenCount > 0
            ? OutputManager.screenGeometryAt(OutputManager.activeCursorScreen)
            : Qt.rect(0, 0, rootWindow.width, rootWindow.height)

        width: 555
        height: 125

        readonly property real dockScale: Math.min(1.0, Math.min((rootWindow.width - 32) / width,
                                                                 (rootWindow.height - 120) / 380))
        readonly property real bottomMargin: Math.max(12, Math.min(33, rootWindow.height * 0.035))

        scale: dockScale
        transformOrigin: Item.Bottom

        x: isMultiScreenSpan
            ? (activeScreenRect.x + (activeScreenRect.width - width * scale) / 2)
            : ((rootWindow.width - width * scale) / 2)
        y: isMultiScreenSpan
            ? (activeScreenRect.y + activeScreenRect.height - height * scale - bottomMargin)
            : (rootWindow.height - height * scale - bottomMargin)

        Behavior on x {
            NumberAnimation {
                duration: 240
                easing.type: Easing.OutCubic
            }
        }
        z: 95

        Shell.SystaliaDock {
            id: systemDock
            anchors.fill: parent
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
        function onTerminalRequested() {
            if (!ProcessLauncher.launch("alacritty"))
                ProcessLauncher.launch("foot")
        }
        function onPowerRequested() {
            controlCenter.open()
        }
    }

    // Camada de brilho em software para ecrãs e monitores externos sem controlo sysfs por hardware
    Rectangle {
        id: softwareBrightnessDimmer
        anchors.fill: parent
        z: 95
        color: "#000000"
        opacity: Math.max(0.0, Math.min(0.85, (100 - SystemControl.brightness) * 0.0085))
        visible: opacity > 0.01
        enabled: false
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

    Connections {
        target: LiquidTiling
        function onThrottledResizeFlushRequested(flushedRatio) {
            rootWindow.syncSnappedWindowsInActiveWorkspace(flushedRatio, true, true)
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
