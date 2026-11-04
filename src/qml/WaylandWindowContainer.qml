import QtQuick
import QtWayland.Compositor
import Systalia.Engine
import "shell" as Shell

Item {
    id: windowContainer

    property var toplevel: null
    property var waylandServer: null
    property var workspace: null
    property var tilingService: null
    property var outputManager: null
    property var backdropSource: null
    property point screenResolution: Qt.point(1280, 800)
    property int currentScreenIndex: 0
    property real currentScreenScale: 1.0
    property real animTime: 0.0
    property point restorePosition: Qt.point(0, 0)
    property size restoreFrameSize: Qt.size(0, 0)
    property size restoreClientSize: Qt.size(0, 0)
    property bool maximizedBySge: false
    property bool restoringGeometry: false
    property bool destructionScheduled: false
    property bool hadEverMapped: false
    property int workspaceIndex: 0
    property bool inActiveWorkspace: true
    property bool workspaceTransitioning: false
    property int snapState: 0
    property real baseCornerRadius: 24.0
    readonly property rect availableSnapArea: parent
                                              ? Qt.rect(0, 0, parent.width, parent.height)
                                              : Qt.rect(0, 0, 1662, 821)
    readonly property rect screenSnapBounds: (workspace && workspace.width > 0 && workspace.height > 0)
                                             ? Qt.rect(0, 0, workspace.width, workspace.height)
                                             : Qt.rect(0, 0, 1662, 975)

    width: 680
    height: 480
    readonly property bool withinFrustum: parent && parent.withinViewportFrustum !== undefined
                                          ? parent.withinViewportFrustum
                                          : true
    visible: !minimized && clientMapped && (inActiveWorkspace || (workspaceTransitioning && withinFrustum))
    enabled: !minimized && clientMapped && inActiveWorkspace && !workspaceTransitioning

    TapHandler {
        target: null
        gesturePolicy: TapHandler.WithinBounds
        onPressedChanged: {
            if (pressed) {
                if (windowContainer.workspace && typeof windowContainer.workspace.raiseClientWindow === "function")
                    windowContainer.workspace.raiseClientWindow(windowContainer)
                if (windowContainer.waylandServer && windowContainer.toplevel)
                    windowContainer.waylandServer.activateToplevel(windowContainer.toplevel)
            }
        }
    }

    property int minWindowWidth: 320
    property int minWindowHeight: 220
    readonly property int borderMargin: 8
    readonly property int frameMargin: 8
    readonly property int titleBarHeight: 36
    readonly property bool isXWaylandWindow: toplevel !== null && toplevel !== undefined
                                             && toplevel.isXWayland === true
    readonly property var xdgSurface: (toplevel && !isXWaylandWindow && toplevel.xdgSurface !== undefined)
                                      ? toplevel.xdgSurface : null
    readonly property var clientSurface: (xdgSurface && xdgSurface.surface)
                                         ? xdgSurface.surface
                                         : ((toplevel && toplevel.surface !== undefined)
                                            ? toplevel.surface : null)
    readonly property bool minimized: {
        if (!waylandServer || !toplevel)
            return false
        const revision = waylandServer.minimizedToplevelsRevision
        if (revision < 0)
            return false
        return waylandServer.isToplevelMinimized(toplevel)
    }
    readonly property bool clientMapped: clientSurface ? clientSurface.hasContent : false
    readonly property var opticalMetadata: {
        if (!waylandServer || !clientSurface)
            return null
        const extension = waylandServer.surfaceExtension
        // Track creation/removal as well as notifications on the metadata object.
        extension.surfaceCount
        return extension.resourceForSurface(clientSurface)
    }
    readonly property rect clientGeometry: xdgSurface
                                           ? xdgSurface.windowGeometry
                                           : ((toplevel && toplevel.windowGeometry !== undefined)
                                              ? toplevel.windowGeometry : Qt.rect(0, 0, 0, 0))
    readonly property int effectiveClientWidth: {
        if (clientGeometry.width > 0)
            return clientGeometry.width
        if (clientSurface) {
            if (clientSurface.destinationSize.width > 0)
                return clientSurface.destinationSize.width
            if (clientSurface.bufferSize.width > 0)
                return clientSurface.bufferSize.width
        }
        return 800
    }
    readonly property int effectiveClientHeight: {
        if (clientGeometry.height > 0)
            return clientGeometry.height
        if (clientSurface) {
            if (clientSurface.destinationSize.height > 0)
                return clientSurface.destinationSize.height
            if (clientSurface.bufferSize.height > 0)
                return clientSurface.bufferSize.height
        }
        return 500
    }
    function computeFusedRadiiForState(state, radius) {
        if (tilingService && typeof tilingService.calculateFusedRadii === "function")
            return tilingService.calculateFusedRadii(state, radius)
        const r = Math.max(0.0, radius)
        switch (state) {
        case 1: return Qt.vector4d(r, 0.0, r, 0.0)
        case 2: return Qt.vector4d(0.0, r, 0.0, r)
        case 3: return Qt.vector4d(0.0, 0.0, 0.0, 0.0)
        case 4: return Qt.vector4d(r, 0.0, 0.0, 0.0)
        case 5: return Qt.vector4d(0.0, r, 0.0, 0.0)
        case 6: return Qt.vector4d(0.0, 0.0, r, 0.0)
        case 7: return Qt.vector4d(0.0, 0.0, 0.0, r)
        default: return Qt.vector4d(r, r, r, r)
        }
    }

    readonly property vector4d frameRadii: {
        if (snapState !== 0)
            return computeFusedRadiiForState(snapState, baseCornerRadius)
        return opticalMetadata
               ? opticalMetadata.radii
               : Qt.vector4d(20.0, 20.0, 20.0, 20.0)
    }
    readonly property bool resizeEnabled: toplevel !== null && waylandServer !== null
                                          && !maximizedBySge && !toplevel.fullscreen
    property bool resizing: false
    property int resizeEdges: 0
    property bool resizeAnchored: false
    property point resizeOrigin: Qt.point(0, 0)
    property size resizeStartSize: Qt.size(0, 0)
    property size requestedClientSize: Qt.size(0, 0)
    property real fixedRight: 0
    property real fixedBottom: 0

    // Only committed client geometry sizes the glass; configure requests are asynchronous.
    function syncClientGeometry() {
        if (clientGeometry.width <= 0 || clientGeometry.height <= 0)
            return
        width = clientGeometry.width + 2 * frameMargin
        height = clientGeometry.height + titleBarHeight + 2 * frameMargin
        if (restoringGeometry) {
            if (Math.round(clientGeometry.width) === restoreClientSize.width
                    && Math.round(clientGeometry.height) === restoreClientSize.height)
                restoreGeometryTimer.restart()
            return
        }
        if (resizeAnchored && (resizeEdges & Qt.LeftEdge)) {
            x = fixedRight - width
        }
        if (resizeAnchored && (resizeEdges & Qt.TopEdge))
            y = fixedBottom - height
    }

    onClientGeometryChanged: syncClientGeometry()

    onClientMappedChanged: {
        if (clientMapped) {
            hadEverMapped = true
            syncClientGeometry()
            syncOutputRouting()
            if (inActiveWorkspace) {
                if (workspace && typeof workspace.raiseClientWindow === "function")
                    workspace.raiseClientWindow(windowContainer)
                if (waylandServer && toplevel)
                    waylandServer.activateToplevel(toplevel)
                clientSurfaceItem.takeFocus()
            }
        }
    }

    function dispatchClientResize(targetSize, interactive) {
        if (!waylandServer || !toplevel)
            return
        if (isXWaylandWindow && typeof waylandServer.requestWindowResize === "function") {
            waylandServer.requestWindowResize(toplevel, targetSize, Boolean(interactive))
        } else {
            waylandServer.requestToplevelResize(toplevel, targetSize, Boolean(interactive))
        }
    }

    function syncOutputRouting() {
        if (!outputManager || typeof outputManager.updateWindowOutputRouting !== "function")
            return
        const surfaceObj = clientSurface
        const winRect = Qt.rect(x, y, width, height)
        const nextScreen = outputManager.updateWindowOutputRouting(windowContainer, surfaceObj, winRect)
        if (nextScreen >= 0) {
            currentScreenIndex = nextScreen
            currentScreenScale = outputManager.screenScaleAt(nextScreen)
            const phys = outputManager.physicalResolutionAt(nextScreen)
            if (phys.x > 0 && phys.y > 0)
                screenResolution = Qt.point(phys.x, phys.y)
        }
    }

    function beginResize(edges, position) {
        if (!resizeEnabled || resizing)
            return
        resizeEdges = edges
        resizeAnchored = true
        resizeOrigin = position
        resizeStartSize = Qt.size(width, height)
        fixedRight = x + width
        fixedBottom = y + height
        requestedClientSize = Qt.size(Math.round(width - 2 * frameMargin),
                                      Math.round(height - titleBarHeight - 2 * frameMargin))
        resizing = true
        forceActiveFocus()
        dispatchClientResize(requestedClientSize, true)
    }

    function updateResize(position) {
        if (!resizing || !resizeEnabled)
            return
        const dx = position.x - resizeOrigin.x
        const dy = position.y - resizeOrigin.y
        let w = resizeStartSize.width
        let h = resizeStartSize.height
        if (resizeEdges & Qt.LeftEdge) w -= dx
        if (resizeEdges & Qt.RightEdge) w += dx
        if (resizeEdges & Qt.TopEdge) h -= dy
        if (resizeEdges & Qt.BottomEdge) h += dy
        const clientW = Math.max(240, minWindowWidth - 2 * frameMargin, toplevel.minSize.width)
        const clientH = Math.max(160, minWindowHeight - titleBarHeight - 2 * frameMargin,
                                 toplevel.minSize.height)
        w = Math.max(clientW, Math.round(w - 2 * frameMargin))
        h = Math.max(clientH, Math.round(h - titleBarHeight - 2 * frameMargin))
        if (toplevel.maxSize.width > 0) w = Math.min(w, Math.max(clientW, toplevel.maxSize.width))
        if (toplevel.maxSize.height > 0) h = Math.min(h, Math.max(clientH, toplevel.maxSize.height))
        if (w === requestedClientSize.width && h === requestedClientSize.height)
            return
        requestedClientSize = Qt.size(w, h)
        dispatchClientResize(requestedClientSize, true)
    }

    function finishResize() {
        if (!resizing)
            return
        resizing = false
        dispatchClientResize(requestedClientSize, false)
    }

    function snapToZone(targetSnap) {
        if (!tilingService || targetSnap === 0)
            return
        if (snapState === 0 && !maximizedBySge) {
            restorePosition = Qt.point(x, y)
            restoreFrameSize = Qt.size(width, height)
        }
        snapState = targetSnap
        if (typeof tilingService.registerWindowSnap === "function")
            tilingService.registerWindowSnap(windowContainer, targetSnap, workspaceIndex)
        updateSnappedGeometry(tilingService.splitRatio, false)
    }

    function updateSnappedGeometry(ratio, interactive, dispatchConfigure) {
        if (!tilingService || snapState === 0)
            return
        const targetGeo = tilingService.getSnapGeometry(snapState, availableSnapArea, ratio)
        if (targetGeo.width <= 0 || targetGeo.height <= 0)
            return
        x = Math.round(targetGeo.x)
        y = Math.round(targetGeo.y)
        width = Math.round(targetGeo.width)
        height = Math.round(targetGeo.height)
        const clientW = Math.max(1, Math.round(targetGeo.width - 2 * frameMargin))
        const clientH = Math.max(1, Math.round(targetGeo.height - titleBarHeight - 2 * frameMargin))
        requestedClientSize = Qt.size(clientW, clientH)
        const shouldSend = (dispatchConfigure === undefined) ? true : Boolean(dispatchConfigure)
        if (shouldSend)
            dispatchClientResize(requestedClientSize, Boolean(interactive))
    }

    function unsnapWindow() {
        if (snapState === 0)
            return
        const restoredW = restoreFrameSize.width > 0 ? restoreFrameSize.width : 680
        const restoredH = restoreFrameSize.height > 0 ? restoreFrameSize.height : 480
        const clientW = Math.max(240, Math.round(restoredW - 2 * frameMargin))
        const clientH = Math.max(160, Math.round(restoredH - titleBarHeight - 2 * frameMargin))
        snapState = 0
        if (tilingService && typeof tilingService.unregisterWindowSnap === "function")
            tilingService.unregisterWindowSnap(windowContainer)
        width = restoredW
        height = restoredH
        requestedClientSize = Qt.size(clientW, clientH)
        dispatchClientResize(requestedClientSize, false)
    }

    function disposeContainer() {
        if (destructionScheduled)
            return
        destructionScheduled = true
        visible = false
        enabled = false
        if (clientSurfaceItem) {
            clientSurfaceItem.paintEnabled = false
            clientSurfaceItem.surface = null
            clientSurfaceItem.shellSurface = null
        }
        if (clientTextureCapture) {
            clientTextureCapture.live = false
            clientTextureCapture.sourceItem = null
        }
        if (glassChassis) {
            glassChassis.clientTextureSource = null
            glassChassis.occluded = true
        }
        if (tilingService && typeof tilingService.unregisterWindowSnap === "function")
            tilingService.unregisterWindowSnap(windowContainer)
        finishResize()
        destroy()
    }

    function toggleMaximized() {
        if (!toplevel || !waylandServer || !parent)
            return

        const wasMaximized = maximizedBySge
        let clientWidth = 0
        let clientHeight = 0
        let nextX = x
        let nextY = y

        if (wasMaximized) {
            const frameWidth = restoreFrameSize.width > 0 ? restoreFrameSize.width : 816
            const frameHeight = restoreFrameSize.height > 0 ? restoreFrameSize.height : 552
            clientWidth = Math.round(frameWidth - 2 * frameMargin)
            clientHeight = Math.round(frameHeight - titleBarHeight - 2 * frameMargin)
            restoreClientSize = Qt.size(clientWidth, clientHeight)
            nextX = restorePosition.x
            nextY = restorePosition.y
        } else {
            restorePosition = Qt.point(x, y)
            restoreFrameSize = Qt.size(width, height)
            clientWidth = Math.round(parent.width - 2 * frameMargin)
            clientHeight = Math.round(parent.height - titleBarHeight - 2 * frameMargin)
            nextX = 0
            nextY = 0
        }

        if (clientWidth <= 0 || clientHeight <= 0)
            return
        maximizedBySge = !wasMaximized
        restoringGeometry = wasMaximized
        if (!waylandServer.toggleToplevelMaximized(toplevel, Qt.size(clientWidth, clientHeight))) {
            maximizedBySge = wasMaximized
            restoringGeometry = false
            return
        }

        x = nextX
        y = nextY
        if (restoringGeometry
                && Math.round(clientGeometry.width) === restoreClientSize.width
                && Math.round(clientGeometry.height) === restoreClientSize.height)
            restoreGeometryTimer.restart()
    }

    Timer {
        id: restoreGeometryTimer
        interval: 16
        repeat: false
        onTriggered: {
            if (!windowContainer.restoringGeometry)
                return
            windowContainer.x = windowContainer.restorePosition.x
            windowContainer.y = windowContainer.restorePosition.y
            windowContainer.restoringGeometry = false
        }
    }

    onResizeEnabledChanged: {
        if (!resizeEnabled)
            finishResize()
    }

    onToplevelChanged: {
        maximizedBySge = toplevel ? Boolean(toplevel.maximized) : false
        if (!toplevel)
            disposeContainer()
    }

    Timer {
        id: closeFallbackTimer
        interval: 220
        repeat: false
        onTriggered: {
            if (!windowContainer.destructionScheduled)
                windowContainer.disposeContainer()
        }
    }

    Connections {
        target: windowContainer.toplevel
        ignoreUnknownSignals: true
        function onMaximizedChanged() {
            windowContainer.maximizedBySge = windowContainer.toplevel
                                             ? Boolean(windowContainer.toplevel.maximized) : false
        }
    }

    Connections {
        target: windowContainer.waylandServer
        ignoreUnknownSignals: true
        function onWindowRemoved(toplevelObj) {
            if (toplevelObj === windowContainer.toplevel) {
                windowContainer.disposeContainer()
            }
        }
    }

    BorderImage {
        x: -60
        y: -60
        width: parent.width + 120
        height: parent.height + 160
        z: -1
        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-window-shadow.png"
        border.left: 60
        border.top: 60
        border.right: 60
        border.bottom: 100
        horizontalTileMode: BorderImage.Stretch
        verticalTileMode: BorderImage.Stretch
        smooth: true
    }

    // Chassi de Vidro Líquido Nativo do Systalia Glass Engine (SGE)
    SystaliaGlassItem {
        id: glassChassis
        anchors.fill: parent

        backdropSource: windowContainer.backdropSource
        clientTextureSource: clientTextureCapture
        screenResolution: Qt.vector2d(windowContainer.screenResolution.x, windowContainer.screenResolution.y)
        devicePixelRatio: windowContainer.currentScreenScale
        titleBarHeight: windowContainer.titleBarHeight
        occluded: !windowContainer.visible

        radii: windowContainer.frameRadii
        Behavior on radii {
            PropertyAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
        slant: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.slant : 0.0
        ior: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.ior : 1.25
        chromaticDispersion: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.dispersion : 0.040
        causticIntensity: 0.85
        surfaceOpacity: 0.85
        time: windowContainer.animTime

        // A fonte Wayland fica atrás do passe óptico; o shader a compõe no mesmo frame.
        Item {
            id: clientViewport
            objectName: "clientViewport"
            x: windowContainer.frameMargin
            y: windowContainer.titleBarHeight + windowContainer.frameMargin
            width: Math.max(0, parent.width - 2 * windowContainer.frameMargin)
            height: Math.max(0, parent.height - windowContainer.titleBarHeight
                             - 2 * windowContainer.frameMargin)
            z: 1
            clip: true

            ShellSurfaceItem {
                id: clientSurfaceItem
                objectName: "clientSurfaceItem"
                x: -windowContainer.clientGeometry.x
                y: -windowContainer.clientGeometry.y
                width: windowContainer.effectiveClientWidth
                height: windowContainer.effectiveClientHeight
                shellSurface: windowContainer.xdgSurface
                surface: windowContainer.clientSurface
                focus: true
                focusOnClick: true
                inputEventsEnabled: !windowContainer.resizing
                paintEnabled: true
                moveItem: windowContainer

                onActiveFocusChanged: {
                    if (activeFocus) {
                        if (windowContainer.workspace && typeof windowContainer.workspace.raiseClientWindow === "function")
                            windowContainer.workspace.raiseClientWindow(windowContainer)
                        if (windowContainer.waylandServer && windowContainer.toplevel)
                            windowContainer.waylandServer.activateToplevel(windowContainer.toplevel)
                    }
                }

                onSurfaceDestroyed: {
                    windowContainer.disposeContainer()
                }
            }

            ShaderEffectSource {
                id: clientTextureCapture
                objectName: "clientTextureCapture"
                sourceItem: clientSurfaceItem
                live: windowContainer.visible
                hideSource: false
                recursive: false
                visible: false
                format: ShaderEffectSource.RGBA8
            }
        }

        Shell.SystaliaWindowHeader {
            id: sgeTitleBar
            width: parent.width
            height: windowContainer.titleBarHeight
            z: 2
            iconGlyph: "▰"
            title: (windowContainer.toplevel && windowContainer.toplevel.title.length > 0)
                   ? windowContainer.toplevel.title
                   : ((windowContainer.toplevel && windowContainer.toplevel.appId.length > 0)
                      ? windowContainer.toplevel.appId
                      : "Aplicação Wayland")
            badgeText: "SDE-KWIN | VALO"
            badgeWidth: 0
            maximized: windowContainer.maximizedBySge
            glassSurface: true
            radii: windowContainer.frameRadii
            slant: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.slant : 0
            slantReferenceWidth: windowContainer.width
            slantReferenceHeight: windowContainer.height
            dragEnabled: !windowContainer.resizing && !windowContainer.maximizedBySge
                          && (!windowContainer.toplevel || !windowContainer.toplevel.fullscreen)
            dragTarget: windowContainer

            onMinimizeRequested: {
                if (windowContainer.toplevel && windowContainer.waylandServer)
                    windowContainer.waylandServer.minimizeToplevel(windowContainer.toplevel)
            }
            onMaximizeRequested: windowContainer.toggleMaximized()
            onCloseRequested: {
                if (windowContainer.toplevel && windowContainer.waylandServer)
                    windowContainer.waylandServer.closeToplevel(windowContainer.toplevel)
                closeFallbackTimer.restart()
            }
            onDragStarted: {
                windowContainer.resizeAnchored = false
                if (windowContainer.snapState !== 0)
                    windowContainer.unsnapWindow()
                if (windowContainer.workspace && typeof windowContainer.workspace.raiseClientWindow === "function")
                    windowContainer.workspace.raiseClientWindow(windowContainer)
                if (windowContainer.toplevel && windowContainer.waylandServer)
                    windowContainer.waylandServer.activateToplevel(windowContainer.toplevel)
                clientSurfaceItem.takeFocus()
            }
            onDragPositionChanged: scenePos => {
                if (windowContainer.tilingService
                        && typeof windowContainer.tilingService.updateSnapPreview === "function") {
                    windowContainer.tilingService.updateSnapPreview(
                        scenePos,
                        windowContainer.screenSnapBounds,
                        windowContainer.availableSnapArea,
                        windowContainer.baseCornerRadius)
                }
            }
            onDragFinished: scenePos => {
                if (windowContainer.tilingService) {
                    const snapTarget = windowContainer.tilingService.calculateSnapTarget(
                        scenePos, windowContainer.screenSnapBounds)
                    windowContainer.tilingService.clearSnapPreview()
                    if (snapTarget !== 0)
                        windowContainer.snapToZone(snapTarget)
                }
                windowContainer.syncOutputRouting()
            }
            onHeaderTapped: {
                if (windowContainer.workspace && typeof windowContainer.workspace.raiseClientWindow === "function")
                    windowContainer.workspace.raiseClientWindow(windowContainer)
                if (windowContainer.toplevel && windowContainer.waylandServer)
                    windowContainer.waylandServer.activateToplevel(windowContainer.toplevel)
                clientSurfaceItem.takeFocus()
            }
        }
    }

    Shell.SystaliaG2Frame {
        anchors.fill: parent
        z: 3
        radii: windowContainer.frameRadii
        Behavior on radii {
            PropertyAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
        slant: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.slant : 0
        strokeColor: "#c9ffffff"
        strokeWidth: 1.5
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
        delegate: MouseArea {
            required property var modelData
            readonly property bool horizontalEdge: (modelData.edges & (Qt.LeftEdge | Qt.RightEdge)) !== 0
            readonly property bool verticalEdge: (modelData.edges & (Qt.TopEdge | Qt.BottomEdge)) !== 0
            objectName: "resize_" + modelData.name
            z: 100
            x: (modelData.edges & Qt.RightEdge) ? windowContainer.width - width
               : (horizontalEdge ? 0 : windowContainer.borderMargin)
            y: (modelData.edges & Qt.BottomEdge) ? windowContainer.height - height
               : (verticalEdge ? 0 : windowContainer.borderMargin)
            width: horizontalEdge ? windowContainer.borderMargin
                                  : windowContainer.width - 2 * windowContainer.borderMargin
            height: verticalEdge ? windowContainer.borderMargin
                                 : windowContainer.height - 2 * windowContainer.borderMargin
            enabled: windowContainer.resizeEnabled
            hoverEnabled: true
            cursorShape: modelData.cursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: mouse => windowContainer.beginResize(modelData.edges,
                                    mapToItem(windowContainer.parent, mouse.x, mouse.y))
            onPositionChanged: mouse => {
                if (pressed)
                    windowContainer.updateResize(mapToItem(windowContainer.parent, mouse.x, mouse.y))
            }
            onReleased: windowContainer.finishResize()
            onCanceled: windowContainer.finishResize()
        }
    }

}
