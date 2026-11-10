import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor
import Systalia.Engine
import Systalia.Compositor 1.0
import Systalia.System 1.0
import "shell" as Shell
import "graphics/v2" as GfxV2

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
    readonly property bool useGraphicsV2: true

    // Integration adapter: reach the typed V2 render item without changing migrated components.
    function opticalItemIn(item) {
        if (!item) return null
        if (item instanceof ValoGlassItemV2) return item
        for (let child of item.children) {
            const found = opticalItemIn(child)
            if (found) return found
        }
        return null
    }
    readonly property var filesOpticalItem: opticalItemIn(glassChassis)
    readonly property var dockOpticalItem: opticalItemIn(systemDock)
    readonly property var topBarOpticalItem: opticalItemIn(topBar)

    component MaterialOpticalBindings: Item {
        required property var opticalItem
        visible: false
        Binding { target: opticalItem; property: "ior"; value: materialWindow.materialActive ? materialWindow.refractiveIndex : 1.0; when: opticalItem !== null }
        Binding { target: opticalItem; property: "chromaticDispersion"; value: materialWindow.materialActive ? materialWindow.effectiveDispersion : 0.0; when: opticalItem !== null }
        Binding { target: opticalItem; property: "causticIntensity"; value: materialWindow.materialActive ? materialWindow.caustics : 0.0; when: opticalItem !== null }
    }
    MaterialOpticalBindings { opticalItem: rootWindow.filesOpticalItem }
    MaterialOpticalBindings { opticalItem: rootWindow.dockOpticalItem }
    Binding { target: rootWindow.filesOpticalItem; property: "slant"; value: glassChassis.surfaceSlant; when: rootWindow.filesOpticalItem !== null }
    Binding { target: rootWindow.dockOpticalItem; property: "surfaceOpacity"; value: systemDock.materialActive ? (systemDock.highContrast ? 0.82 : systemDock.surfaceOpacity) : 0.0; when: rootWindow.dockOpticalItem !== null }

    function bringWindowToFront(win) {
        if (!win)
            return
        topZIndex += 1
        win.z = topZIndex
        WorkspaceManager.setActiveWindow(win)
    }

    function raiseFileWindow() {
        WorkspaceManager.switchToWorkspace(WorkspaceManager.windowWorkspace(glassChassis))
        glassChassis.windowOpen = true
        glassChassis.visible = true
        bringWindowToFront(glassChassis)
    }

    function minimizeFileWindow() {
        glassChassis.windowOpen = false
        glassChassis.visible = false
    }

    function closeFileWindow() {
        unsnapShellWindow(glassChassis)
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
        WorkspaceManager.switchToWorkspace(WorkspaceManager.windowWorkspace(materialWindow))
        materialWindow.windowOpen = true
        materialWindow.visible = true
        bringWindowToFront(materialWindow)
    }

    function minimizeMaterialWindow() {
        materialWindow.windowOpen = false
        materialWindow.visible = false
    }

    function closeMaterialWindow() {
        unsnapShellWindow(materialWindow)
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

    // Shared handles change only internal window geometry; client resize is separate.
    component ShellResizeHandles: Item {
        id: handles
        required property Item targetWindow
        property real minimumWidth: 610
        property real minimumHeight: 530
        property point startPointer
        property rect startGeometry
        anchors.fill: parent
        z: 1000
        visible: enabled
        function begin(point) {
            rootWindow.bringWindowToFront(targetWindow)
            startPointer = point
            startGeometry = Qt.rect(targetWindow.x, targetWindow.y, targetWindow.width, targetWindow.height)
        }
        function move(edges, point) {
            const dx = point.x - startPointer.x
            const dy = point.y - startPointer.y
            let width = startGeometry.width
            let height = startGeometry.height
            if (edges & Qt.LeftEdge) width = Math.max(minimumWidth, startGeometry.width - dx)
            if (edges & Qt.RightEdge) width = Math.max(minimumWidth, startGeometry.width + dx)
            if (edges & Qt.TopEdge) height = Math.max(minimumHeight, startGeometry.height - dy)
            if (edges & Qt.BottomEdge) height = Math.max(minimumHeight, startGeometry.height + dy)
            if (edges & Qt.LeftEdge) targetWindow.x = startGeometry.x + startGeometry.width - width
            if (edges & Qt.TopEdge) targetWindow.y = startGeometry.y + startGeometry.height - height
            targetWindow.width = width
            targetWindow.height = height
        }
        function finish() {
            targetWindow.restoreGeometry = Qt.rect(targetWindow.x, targetWindow.y,
                                                    targetWindow.width, targetWindow.height)
        }
        Repeater {
            model: [
                { name: "top", edges: Qt.TopEdge, cursor: Qt.SizeVerCursor },
                { name: "bottom", edges: Qt.BottomEdge, cursor: Qt.SizeVerCursor },
                { name: "left", edges: Qt.LeftEdge, cursor: Qt.SizeHorCursor },
                { name: "right", edges: Qt.RightEdge, cursor: Qt.SizeHorCursor },
                { name: "topLeft", edges: Qt.TopEdge | Qt.LeftEdge, cursor: Qt.SizeFDiagCursor },
                { name: "bottomRight", edges: Qt.BottomEdge | Qt.RightEdge, cursor: Qt.SizeFDiagCursor },
                { name: "topRight", edges: Qt.TopEdge | Qt.RightEdge, cursor: Qt.SizeBDiagCursor },
                { name: "bottomLeft", edges: Qt.BottomEdge | Qt.LeftEdge, cursor: Qt.SizeBDiagCursor }
            ]
            MouseArea {
                required property var modelData
                readonly property bool horizontal: (modelData.edges & (Qt.LeftEdge | Qt.RightEdge)) !== 0
                readonly property bool vertical: (modelData.edges & (Qt.TopEdge | Qt.BottomEdge)) !== 0
                objectName: "internalResize_" + modelData.name
                z: horizontal && vertical ? 1 : 0
                x: (modelData.edges & Qt.RightEdge) ? handles.width - width : (horizontal ? 0 : 8)
                y: (modelData.edges & Qt.BottomEdge) ? handles.height - height : (vertical ? 0 : 8)
                width: horizontal ? 8 : Math.max(0, handles.width - 16)
                height: vertical ? 8 : Math.max(0, handles.height - 16)
                hoverEnabled: true
                cursorShape: modelData.cursor
                acceptedButtons: Qt.LeftButton
                preventStealing: true
                onPressed: mouse => handles.begin(mapToItem(handles.targetWindow.parent, mouse.x, mouse.y))
                onPositionChanged: mouse => {
                    if (pressed) handles.move(modelData.edges, mapToItem(handles.targetWindow.parent, mouse.x, mouse.y))
                }
                onReleased: handles.finish()
                onCanceled: handles.finish()
            }
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
        LiquidTiling.registerWindowSnap(win, targetSnap, WorkspaceManager.windowWorkspace(win))
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
        for (const win of [glassChassis, materialWindow]) {
            if (WorkspaceManager.windowWorkspace(win) !== activeWs || win.snapState === 0)
                continue
            const geo = LiquidTiling.getSnapGeometry(win.snapState, shellCanvasAvailableArea, ratio)
            if (geo.width > 0 && geo.height > 0) {
                win.x = Math.round(geo.x)
                win.y = Math.round(geo.y)
                win.width = Math.round(geo.width)
                win.height = Math.round(geo.height)
            }
        }
    }

    property var trackedClientContainers: []

    // Single source of truth for the nearest intersecting lower window.
    // External windows participate in ordering, but are never captured.
    function nearestBackdropWindow(consumer, consumerAboveWorkspace) {
        const page = clientWindowsLayer.workspacePageAt(WorkspaceManager.currentWorkspace)
        void(clientWindowsLayer.viewportOffset)
        if (!consumer || !page || !consumer.visible || WorkspaceManager.isTransitioning || OutputManager.screenCount > 1)
            return null
        if (!consumerAboveWorkspace && consumer.parent !== page) return null
        function bounds(item) {
            void(item.x); void(item.y); void(item.width); void(item.height); void(item.scale); void(item.rotation)
            if (item.parent) { void(item.parent.x); void(item.parent.y); void(item.parent.scale); void(item.parent.rotation) }
            const p = [item.mapToItem(null,0,0), item.mapToItem(null,item.width,0),
                       item.mapToItem(null,0,item.height), item.mapToItem(null,item.width,item.height)]
            return {l:Math.min(...p.map(v=>v.x)), r:Math.max(...p.map(v=>v.x)),
                    t:Math.min(...p.map(v=>v.y)), b:Math.max(...p.map(v=>v.y))}
        }
        const area = bounds(consumer)
        let nearest = null
        const candidates = [glassChassis, materialWindow].concat(trackedClientContainers)
        for (const item of candidates) {
            if (!item || item === consumer) continue
            void(item.x); void(item.y); void(item.width); void(item.height); void(item.z)
            void(item.visible); void(item.opacity); void(item.parent); void(item.scale); void(item.rotation)
            if (item.workspaceIndex !== undefined) void(item.workspaceIndex)
            if (!item.visible || item.opacity <= 0) continue
            if (item.workspaceIndex !== undefined && item.workspaceIndex !== WorkspaceManager.currentWorkspace) continue
            if (item.parent !== page) {
                if (trackedClientContainers.indexOf(item) !== -1) return item
                continue
            }
            const index = page.children.indexOf(item)
            if (!consumerAboveWorkspace && (item.z > consumer.z ||
                (item.z === consumer.z && index >= page.children.indexOf(consumer)))) continue
            const box = bounds(item)
            if (Math.max(area.l,box.l) >= Math.min(area.r,box.r) ||
                Math.max(area.t,box.t) >= Math.min(area.b,box.b)) continue
            if (!nearest || item.z > nearest.z ||
                (item.z === nearest.z && index > page.children.indexOf(nearest))) nearest = item
        }
        return nearest
    }

    // Never sample a lower internal window through an external client that we
    // cannot capture. Restrict the fallback to the affected consumer region.
    function externalBackdropBlocked(consumer, producers, consumerAboveWorkspace) {
        const page = clientWindowsLayer.workspacePageAt(WorkspaceManager.currentWorkspace)
        void(clientWindowsLayer.viewportOffset)
        if (!consumer || !page) return true
        function bounds(item) {
            void(item.x); void(item.y); void(item.width); void(item.height)
            void(item.scale); void(item.rotation)
            if (item.parent) { void(item.parent.x); void(item.parent.y); void(item.parent.scale); void(item.parent.rotation) }
            const p = [item.mapToItem(null, 0, 0), item.mapToItem(null, item.width, 0),
                       item.mapToItem(null, 0, item.height), item.mapToItem(null, item.width, item.height)]
            return {left: Math.min(...p.map(v => v.x)), right: Math.max(...p.map(v => v.x)),
                    top: Math.min(...p.map(v => v.y)), bottom: Math.max(...p.map(v => v.y))}
        }
        function above(a, b) {
            return a.z > b.z || (a.z === b.z && page.children.indexOf(a) > page.children.indexOf(b))
        }
        const target = bounds(consumer)
        for (const client of trackedClientContainers) {
            if (!client) continue
            // Read all changing fields even when a different condition excludes it.
            void(client.x); void(client.y); void(client.width); void(client.height); void(client.z)
            void(client.visible); void(client.opacity); void(client.parent); void(client.workspaceIndex)
            if (!client.visible || client.opacity <= 0 || client.workspaceIndex !== WorkspaceManager.currentWorkspace)
                continue
            if (client.parent !== page || (!consumerAboveWorkspace && consumer.parent !== page))
                return true
            if (!consumerAboveWorkspace && !above(consumer, client)) continue
            const c = bounds(client)
            for (const producer of producers) {
                if (!producer) continue
                void(producer.x); void(producer.y); void(producer.width); void(producer.height); void(producer.z)
                void(producer.visible); void(producer.opacity); void(producer.parent)
                if (!producer.visible || producer.opacity <= 0 || producer.parent !== page || !above(client, producer)) continue
                const p = bounds(producer)
                // Fallback for the entire consumer if a client intersects a
                // contributing producer. A remote part of that producer can
                // reach the consumer through blur/refraction; do not guess the
                // sampling radius or clip an uncaptured client to a fake mask.
                if (Math.max(target.left, p.left) < Math.min(target.right, p.right)
                        && Math.max(target.top, p.top) < Math.min(target.bottom, p.bottom)
                        && Math.max(c.left, p.left) < Math.min(c.right, p.right)
                        && Math.max(c.top, p.top) < Math.min(c.bottom, p.bottom))
                    return true
            }
        }
        return false
    }

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
                        blurProfile: Qt.binding(() => materialWindow.materialActive ? materialWindow.blurProfile : 0),
                        userHighContrast: Qt.binding(() => materialWindow.opticalSurface.highContrast),
                        userPrefersReducedMotion: Qt.binding(() => materialWindow.opticalSurface.prefersReducedMotion),
                        workspaceIndex: wsIndex,
                        screenResolution: Qt.binding(() => Qt.point(
                            rootWindow.width * rootWindow.devicePixelRatio,
                            rootWindow.height * rootWindow.devicePixelRatio)),
                        animTime: Qt.binding(() => animTimer.tick),
                        x: Math.round(160 + offset),
                        y: Math.round(80 + offset)
                    })
                    if (clientWindow) {
                        clientWindow.backdropSelection = Qt.binding(() => rootWindow.nearestBackdropWindow(clientWindow, false))
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
        visible: true
        opacity: 0.0
        z: -100
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

    GfxV2.ValoInternalWindowBackdropV2 {
        id: internalWindowBackdrop
        objectName: "internalWindowBackdropV2"
        anchors.fill: parent
        firstWindow: glassChassis
        secondWindow: materialWindow
        wallpaperSource: backdropGrab
        // External buffers are deliberately outside this internal-pair pilot.
        externalClientsPresent: rootWindow.externalBackdropBlocked(
            glassChassis.z > materialWindow.z ? glassChassis : materialWindow,
            [glassChassis.z > materialWindow.z ? materialWindow : glassChassis], false)
            || rootWindow.nearestBackdropWindow(glassChassis.z > materialWindow.z ? glassChassis : materialWindow, false)
                !== (glassChassis.z > materialWindow.z ? materialWindow : glassChassis)
        enabledForWorkspace: !WorkspaceManager.isTransitioning
            && OutputManager.screenCount <= 1
            && glassChassis.parent === clientWindowsLayer.workspacePageAt(WorkspaceManager.currentWorkspace)
        mappingRevision: clientWindowsLayer.viewportOffset
    }

    Item {
        id: overlayBackdrop
        objectName: "limitedOverlayBackdrop"
        anchors.fill: parent
        visible: false
        property bool enabledForWorkspace: !WorkspaceManager.isTransitioning && !LiquidTiling.previewVisible
            && !LiquidTiling.hasActiveSplitInWorkspace(WorkspaceManager.currentWorkspace)
            && OutputManager.screenCount <= 1
        readonly property bool bothVisible: glassChassis.visible && materialWindow.visible
        readonly property bool dockAvailable: bothVisible ? dockStack.compositeAvailable : singleStack.dockAvailable
        readonly property bool topBarAvailable: bothVisible ? topStack.compositeAvailable : singleStack.topBarAvailable
        readonly property bool requiresBoth: bothVisible && (dockStack.selectedCount === 2 || topStack.selectedCount === 2)
        readonly property string coverageMode: bothVisible && (dockAvailable || topBarAvailable)
            ? "limited-internal-windows" : singleStack.coverageMode
        readonly property var producer: singleStack.producer
        function producerFor(consumer) {
            if (!bothVisible) return singleStack.producer
            const selected = consumer === topBar ? topStack.selected : dockStack.selected
            return selected.length ? selected[selected.length - 1] : null
        }
        function sourceFor(consumer) {
            if (!bothVisible) return singleStack.sourceFor(consumer)
            const stack = consumer === topBar ? topStack : dockStack
            return stack.compositeAvailable ? stack.compositeSource : backdropGrab
        }
        GfxV2.ValoSingleWindowBackdropV2 {
            id: singleStack
            anchors.fill: parent
            firstWindow: glassChassis; secondWindow: materialWindow
            wallpaperSource: backdropGrab
            workspacePage: clientWindowsLayer.workspacePageAt(WorkspaceManager.currentWorkspace)
            topBar: topBar; dock: systemDock
            mappingRevision: clientWindowsLayer.viewportOffset
            topBarBlocked: rootWindow.externalBackdropBlocked(topBar, [glassChassis, materialWindow], true)
                || rootWindow.nearestBackdropWindow(topBar, true) !== singleStack.producer
            dockBlocked: rootWindow.externalBackdropBlocked(systemDock, [glassChassis, materialWindow], true)
                || rootWindow.nearestBackdropWindow(systemDock, true) !== singleStack.producer
            enabledForWorkspace: overlayBackdrop.enabledForWorkspace
        }
        GfxV2.ValoLimitedBackdropV2 {
            id: dockStack
            objectName: "dockLimitedBackdrop"
            anchors.fill: parent
            wallpaperSource: backdropGrab
            workspacePage: clientWindowsLayer.workspacePageAt(WorkspaceManager.currentWorkspace)
            consumer: systemDock
            candidates: [glassChassis, materialWindow]
            consumerAboveWorkspace: true
            externalClientsPresent: rootWindow.externalBackdropBlocked(systemDock, [glassChassis, materialWindow], true)
                || (rootWindow.nearestBackdropWindow(systemDock, true) !== glassChassis
                    && rootWindow.nearestBackdropWindow(systemDock, true) !== materialWindow)
            enabledForWorkspace: overlayBackdrop.enabledForWorkspace && singleStack.overlaysDisjoint && overlayBackdrop.bothVisible
            mappingRevision: clientWindowsLayer.viewportOffset
        }
        GfxV2.ValoLimitedBackdropV2 {
            id: topStack
            objectName: "topBarLimitedBackdrop"
            anchors.fill: parent
            wallpaperSource: backdropGrab
            workspacePage: clientWindowsLayer.workspacePageAt(WorkspaceManager.currentWorkspace)
            consumer: topBar
            candidates: [glassChassis, materialWindow]
            consumerAboveWorkspace: true
            externalClientsPresent: rootWindow.externalBackdropBlocked(topBar, [glassChassis, materialWindow], true)
                || (rootWindow.nearestBackdropWindow(topBar, true) !== glassChassis
                    && rootWindow.nearestBackdropWindow(topBar, true) !== materialWindow)
            enabledForWorkspace: overlayBackdrop.enabledForWorkspace && singleStack.overlaysDisjoint && overlayBackdrop.bothVisible
            mappingRevision: clientWindowsLayer.viewportOffset
        }
    }
    Binding {
        target: rootWindow.topBarOpticalItem ? rootWindow.topBarOpticalItem.backdropProvider : null
        property: "upstreamBlurProvider"
        value: overlayBackdrop.topBarAvailable && overlayBackdrop.producerFor(topBar)
               && rootWindow.opticalItemIn(overlayBackdrop.producerFor(topBar))
            ? rootWindow.opticalItemIn(overlayBackdrop.producerFor(topBar)).backdropProvider : null
        when: rootWindow.topBarOpticalItem !== null
    }
    Binding {
        target: rootWindow.dockOpticalItem ? rootWindow.dockOpticalItem.backdropProvider : null
        property: "upstreamBlurProvider"
        value: overlayBackdrop.dockAvailable && overlayBackdrop.producerFor(systemDock)
               && rootWindow.opticalItemIn(overlayBackdrop.producerFor(systemDock))
            ? rootWindow.opticalItemIn(overlayBackdrop.producerFor(systemDock)).backdropProvider : null
        when: rootWindow.dockOpticalItem !== null
    }

    Binding {
        target: rootWindow.filesOpticalItem ? rootWindow.filesOpticalItem.backdropProvider : null
        property: "upstreamBlurProvider"
        value: (internalWindowBackdrop.compositeAvailable && internalWindowBackdrop.frontWindow === glassChassis)
            || (overlayBackdrop.requiresBoth && glassChassis.z > materialWindow.z)
            ? materialWindow.opticalSurface.effectiveBackdropProvider : null
        when: rootWindow.filesOpticalItem !== null
    }
    Binding {
        target: materialWindow.opticalSurface.effectiveBackdropProvider
        property: "upstreamBlurProvider"
        value: (internalWindowBackdrop.compositeAvailable && internalWindowBackdrop.frontWindow === materialWindow)
            || (overlayBackdrop.requiresBoth && glassChassis.z <= materialWindow.z)
            ? (rootWindow.filesOpticalItem ? rootWindow.filesOpticalItem.backdropProvider : null) : null
    }

    Shell.SystaliaFilesWindowV2 {
        id: glassChassis
        parent: clientWindowsLayer.workspacePageAt(0)
        x: 80
        y: 80
        z: 10
        backdropSource: internalWindowBackdrop.sourceFor(glassChassis)
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
        controller: rootWindow
        waylandServer: waylandServer
        refractiveIndex: materialWindow.refractiveIndex
        dispersion: materialWindow.effectiveDispersion
        caustics: materialWindow.caustics
        surfaceOpacity: materialWindow.surfaceOpacity
        surfaceSlant: materialWindow.surfaceSlant
        cornerRadius: materialWindow.cornerRadius
        materialActive: materialWindow.materialActive
        blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
    }

    Shell.SystaliaMaterialWindow {
        id: materialWindow
        parent: clientWindowsLayer.workspacePageAt(0)
        x: 810
        y: 80
        z: 11
        backdropSource: internalWindowBackdrop.sourceFor(materialWindow)
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
        controller: rootWindow
    }

    ShellResizeHandles {
        objectName: "filesResizeHandles"
        parent: glassChassis
        targetWindow: glassChassis
        minimumWidth: 610
        minimumHeight: 530
        enabled: glassChassis.visible && !rootWindow.filesWindowMaximized && glassChassis.snapState === 0
    }
    ShellResizeHandles {
        objectName: "materialResizeHandles"
        parent: materialWindow
        targetWindow: materialWindow
        minimumWidth: 610
        minimumHeight: 525
        enabled: materialWindow.visible && !rootWindow.materialWindowMaximized && materialWindow.snapState === 0
    }

    Item {
        id: tilingOverlayLayer
        anchors.fill: workspace
        z: 45

        Shell.SnapPreviewOverlay {
            blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
            userHighContrast: materialWindow.opticalSurface.highContrast
            userPrefersReducedMotion: materialWindow.opticalSurface.prefersReducedMotion
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

    Shell.SystaliaTopBarV2 {
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
        blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
        backdropSource: overlayBackdrop.sourceFor(topBar)
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
    }

    // Réplica configurável da TopBar em monitores secundários via Instantiator
    Instantiator {
        id: multiMonitorTopBarInstantiator
        active: OutputManager.replicateTopBarOnAllScreens && OutputManager.screenCount > 1
        model: OutputManager.screenGeometries
        delegate: Shell.SystaliaTopBarV2 {
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
            blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
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

        Shell.SystaliaDockV2 {
            id: systemDock
            anchors.fill: parent
            visible: !waylandServer.hasMaximizedToplevel
            backdropSource: overlayBackdrop.sourceFor(systemDock)
            screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                          rootWindow.height * rootWindow.devicePixelRatio)
            activeAppIds: waylandServer.activeAppIds
            animationTime: animTimer.tick
            filesVisible: glassChassis.visible
            settingsOpen: materialWindow.visible
            refractiveIndex: materialWindow.refractiveIndex
            dispersion: materialWindow.effectiveDispersion
            caustics: materialWindow.caustics
            surfaceOpacity: ValoTheme.opacityDock
            materialActive: materialWindow.materialActive
            performanceProfile: !materialWindow.materialActive || materialWindow.blurProfile === 0
                                ? "performance" : materialWindow.blurProfile === 1 ? "balanced" : "quality"

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
        blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
        userHighContrast: materialWindow.opticalSurface.highContrast
        userPrefersReducedMotion: materialWindow.opticalSurface.prefersReducedMotion
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
        z: 1000
        color: "#000000"
        opacity: SystemControl.softwareBrightness
                 ? Math.max(0.0, Math.min(0.85, (100 - SystemControl.brightness) * 0.0085)) : 0
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
                    WorkspaceManager.setWindowWorkspace(windowContainer, workspaceIndex)
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
            if (windowItem.snapState !== undefined && windowItem.snapState !== 0)
                LiquidTiling.registerWindowSnap(windowItem, windowItem.snapState, workspaceIndex)
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

    Shell.SystaliaControlCenterV2 {
        id: controlCenter
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
    }

    Shell.SystaliaNotificationHost {
        blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
        userHighContrast: materialWindow.opticalSurface.highContrast
        userPrefersReducedMotion: materialWindow.opticalSurface.prefersReducedMotion
        id: notificationHost
        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)
        animationTime: animTimer.tick
    }

    Shell.SystaliaTaskSwitcher {
        blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
        userHighContrast: materialWindow.opticalSurface.highContrast
        userPrefersReducedMotion: materialWindow.opticalSurface.prefersReducedMotion
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
