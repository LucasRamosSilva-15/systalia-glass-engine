import QtQuick
import Waylib.Server
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: frame
    objectName: "waylibWindowFrame"

    required property var surfaceAdapter
    required property var outputLayout
    required property var windowShellSurface
    required property var waylandSurface
    required property string windowTitle
    required property string windowAppId
    required property bool isActive
    required property bool isMinimized
    required property bool isMaximized
    required property int windowZOrder
    required property int windowIndex
    required property real workAreaWidth
    required property real workAreaHeight

    property var compositorAdapter: null
    property int workspaceIndex: 0
    property int activeWorkspace: 0
    property bool workspaceTransitioning: false
    property real desktopWidth: 1280
    property real desktopHeight: 760
    property real layerOriginX: 8
    property real layerOriginY: 60

    property var backdropSource: null
    property int blurProfile: 1
    property bool userHighContrast: false
    property bool userPrefersReducedMotion: false
    property real refractiveIndex: 1.25
    property real effectiveDispersion: 0.04
    property real caustics: 0.18
    property real slant: 0.0
    property real cornerRadius: 20.0
    property real animationTime: 0.0

    readonly property int borderMargin: 8
    readonly property int frameMargin: 8
    readonly property int titleBarHeight: sgeTitleBar.implicitHeight
    readonly property real radius: cornerRadius
    readonly property vector4d frameRadii: Qt.vector4d(cornerRadius, cornerRadius, cornerRadius, cornerRadius)

    property real normalX: Math.max(8, Math.min(24 + (windowIndex * 36), Math.max(8, workAreaWidth - 480)))
    property real normalY: Math.max(8, Math.min(16 + (windowIndex * 32), Math.max(8, workAreaHeight - 360)))
    property real normalWidth: 480
    property real normalHeight: 340
    property bool userSized: false
    property bool surfaceOpaque: false

    property bool resizing: false
    property bool clientInitiatedResizing: false
    property int resizeEdges: 0
    property bool resizeAnchored: false
    property point resizeOrigin: Qt.point(0, 0)
    property rect resizeStartRect: Qt.rect(0, 0, 480, 340)
    property real fixedRight: 0
    property real fixedBottom: 0
    property size requestedClientSize: Qt.size(0, 0)

    property real animatedWorkspaceOffsetX: (workspaceIndex - activeWorkspace) * desktopWidth
    Behavior on animatedWorkspaceOffsetX {
        enabled: !frame.userPrefersReducedMotion
        NumberAnimation {
            duration: 320
            easing.type: Easing.OutCubic
        }
    }

    x: (isMaximized ? 0 : normalX) + animatedWorkspaceOffsetX
    y: isMaximized ? 0 : normalY
    width: isMaximized ? Math.max(320, frame.desktopWidth - 16) : normalWidth
    height: isMaximized ? Math.max(220, frame.desktopHeight - 68) : normalHeight
    z: windowZOrder
    visible: !isMinimized && (workspaceIndex === activeWorkspace || workspaceTransitioning)
    clip: false

    function clientOriginIn(target) {
        return toplevelItem.mapToItem(target, 0, 0)
    }

    function refreshSurfaceOpaqueness() {
        if (frame.surfaceAdapter && frame.waylandSurface) {
            frame.surfaceOpaque = frame.surfaceAdapter.isSurfaceOpaque(frame.waylandSurface)
        } else {
            frame.surfaceOpaque = false
        }
    }

    onWaylandSurfaceChanged: refreshSurfaceOpaqueness()

    Connections {
        target: frame.waylandSurface
        ignoreUnknownSignals: true
        function onCommit() {
            frame.refreshSurfaceOpaqueness()
        }
    }

    Connections {
        target: frame.surfaceAdapter
        function onMoveRequested(surface) {
            if (surface === frame.windowShellSurface) {
                frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
                frame.surfaceAdapter.raiseSurface(frame.windowShellSurface)
            }
        }
        function onResizeRequested(surface, edgeFlags) {
            if (surface === frame.windowShellSurface && !frame.isMaximized && edgeFlags !== 0) {
                frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
                frame.surfaceAdapter.raiseSurface(frame.windowShellSurface)
                const curPos = (frame.compositorAdapter && frame.compositorAdapter.cursor)
                    ? frame.compositorAdapter.cursor.position
                    : Qt.point(frame.x + frame.width, frame.y + frame.height)
                const parentPt = frame.parent ? frame.parent.mapFromItem(null, curPos.x, curPos.y) : curPos
                frame.clientInitiatedResizing = true
                frame.beginResize(edgeFlags, parentPt)
            }
        }
    }

    Connections {
        target: frame.compositorAdapter ? frame.compositorAdapter.cursor : null
        enabled: frame.clientInitiatedResizing
        function onPositionChanged() {
            if (!frame.clientInitiatedResizing || !frame.compositorAdapter || !frame.compositorAdapter.cursor) {
                return
            }
            const curPos = frame.compositorAdapter.cursor.position
            const parentPt = frame.parent ? frame.parent.mapFromItem(null, curPos.x, curPos.y) : curPos
            frame.updateResize(parentPt)
        }
    }

    function cursorKeyForEdges(edges) {
        const hasLeft = (edges & Qt.LeftEdge) !== 0
        const hasRight = (edges & Qt.RightEdge) !== 0
        const hasTop = (edges & Qt.TopEdge) !== 0
        const hasBottom = (edges & Qt.BottomEdge) !== 0
        if ((hasLeft && hasTop) || (hasRight && hasBottom)) {
            return "resize-nwse"
        }
        if ((hasRight && hasTop) || (hasLeft && hasBottom)) {
            return "resize-nesw"
        }
        if (hasLeft || hasRight) {
            return "resize-ew"
        }
        if (hasTop || hasBottom) {
            return "resize-ns"
        }
        return "default"
    }

    function beginResize(edges, parentPoint) {
        if (frame.isMaximized || !frame.windowShellSurface || edges === 0) {
            return
        }
        frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
        frame.surfaceAdapter.raiseSurface(frame.windowShellSurface)
        frame.resizeEdges = edges
        frame.resizeAnchored = true
        frame.resizeOrigin = parentPoint
        frame.resizeStartRect = Qt.rect(frame.normalX, frame.normalY, frame.normalWidth, frame.normalHeight)
        frame.fixedRight = frame.normalX + frame.normalWidth
        frame.fixedBottom = frame.normalY + frame.normalHeight
        frame.requestedClientSize = Qt.size(
            Math.max(240, Math.round(frame.normalWidth - 2 * frame.frameMargin)),
            Math.max(160, Math.round(frame.normalHeight - frame.titleBarHeight - 2 * frame.frameMargin)))
        frame.resizing = true
        if (frame.compositorAdapter) {
            frame.compositorAdapter.compositorCursorOverride = cursorKeyForEdges(edges)
        }
    }

    function updateResize(parentPoint) {
        if (!frame.resizing || frame.isMaximized || !frame.windowShellSurface) {
            return
        }
        const dx = parentPoint.x - frame.resizeOrigin.x
        const dy = parentPoint.y - frame.resizeOrigin.y

        const minClientW = Math.max(240, toplevelItem.minimumSize ? toplevelItem.minimumSize.width : 0)
        const minClientH = Math.max(160, toplevelItem.minimumSize ? toplevelItem.minimumSize.height : 0)
        const maxClientW = (toplevelItem.maximumSize && toplevelItem.maximumSize.width > 0)
            ? Math.max(minClientW, toplevelItem.maximumSize.width) : 0
        const maxClientH = (toplevelItem.maximumSize && toplevelItem.maximumSize.height > 0)
            ? Math.max(minClientH, toplevelItem.maximumSize.height) : 0

        const minFrameW = minClientW + 2 * frame.frameMargin
        const minFrameH = minClientH + frame.titleBarHeight + 2 * frame.frameMargin
        const maxFrameW = maxClientW > 0 ? (maxClientW + 2 * frame.frameMargin) : 100000
        const maxFrameH = maxClientH > 0 ? (maxClientH + frame.titleBarHeight + 2 * frame.frameMargin) : 100000

        let nextW = frame.resizeStartRect.width
        let nextH = frame.resizeStartRect.height
        if (frame.resizeEdges & Qt.LeftEdge) {
            nextW = frame.resizeStartRect.width - dx
        } else if (frame.resizeEdges & Qt.RightEdge) {
            nextW = frame.resizeStartRect.width + dx
        }
        if (frame.resizeEdges & Qt.TopEdge) {
            nextH = frame.resizeStartRect.height - dy
        } else if (frame.resizeEdges & Qt.BottomEdge) {
            nextH = frame.resizeStartRect.height + dy
        }

        nextW = Math.max(minFrameW, Math.min(Math.round(nextW), maxFrameW))
        nextH = Math.max(minFrameH, Math.min(Math.round(nextH), maxFrameH))

        let nextX = frame.resizeStartRect.x
        let nextY = frame.resizeStartRect.y
        if (frame.resizeEdges & Qt.LeftEdge) {
            nextX = frame.fixedRight - nextW
        }
        if (frame.resizeEdges & Qt.TopEdge) {
            nextY = frame.fixedBottom - nextH
        }

        frame.userSized = true
        frame.normalX = nextX
        frame.normalY = nextY
        frame.normalWidth = nextW
        frame.normalHeight = nextH

        const clientW = Math.max(minClientW, Math.round(nextW - 2 * frame.frameMargin))
        const clientH = Math.max(minClientH, Math.round(nextH - frame.titleBarHeight - 2 * frame.frameMargin))
        if (clientW !== frame.requestedClientSize.width || clientH !== frame.requestedClientSize.height) {
            frame.requestedClientSize = Qt.size(clientW, clientH)
            if (typeof LiquidTiling === "undefined" || !LiquidTiling
                    || LiquidTiling.shouldDispatchInteractiveResize(true)) {
                frame.surfaceAdapter.resizeSurface(frame.windowShellSurface, clientW, clientH)
            }
        }
    }

    function finishResize() {
        if (!frame.resizing) {
            return
        }
        frame.resizing = false
        frame.clientInitiatedResizing = false
        if (frame.compositorAdapter) {
            frame.compositorAdapter.compositorCursorOverride = ""
        }
        if (frame.windowShellSurface && frame.requestedClientSize.width > 0 && frame.requestedClientSize.height > 0) {
            frame.surfaceAdapter.resizeSurface(frame.windowShellSurface,
                                               frame.requestedClientSize.width,
                                               frame.requestedClientSize.height)
        }
    }

    function maximizedClientSize() {
        const maxFrameW = Math.max(320, frame.desktopWidth - 16)
        const maxFrameH = Math.max(220, frame.desktopHeight - 68)
        return Qt.size(
            Math.max(240, Math.round(maxFrameW - 2 * frame.frameMargin)),
            Math.max(160, Math.round(maxFrameH - frame.titleBarHeight - 2 * frame.frameMargin)))
    }

    function syncMaximizedSurface() {
        if (!isMaximized || !windowShellSurface) {
            return
        }
        const sz = maximizedClientSize()
        frame.surfaceAdapter.resizeSurface(frame.windowShellSurface, sz.width, sz.height)
    }

    onIsMaximizedChanged: {
        if (isMaximized) {
            syncMaximizedSurface()
        } else if (windowShellSurface) {
            const targetW = Math.max(240, Math.round(frame.normalWidth - 2 * frame.frameMargin))
            const targetH = Math.max(160, Math.round(frame.normalHeight - frame.titleBarHeight - 2 * frame.frameMargin))
            frame.surfaceAdapter.resizeSurface(frame.windowShellSurface, targetW, targetH)
        }
    }
    onDesktopWidthChanged: syncMaximizedSurface()
    onDesktopHeightChanged: syncMaximizedSurface()

    // Chassi óptico unificado VALO Graphics V2 (idêntico ao WaylandWindowContainer.qml)
    ValoMaterialSurfaceV2 {
        id: clientDecorationV2
        objectName: "clientDecorationV2"
        anchors.fill: parent
        geometryMode: ValoGlassItemV2.SquircleG2
        backdropSource: frame.backdropSource
        blurProfile: frame.blurProfile
        cornerRadii: frame.frameRadii
        cornerRadius: frame.radius
        slant: frame.slant
        ior: frame.refractiveIndex
        chromaticDispersion: frame.effectiveDispersion
        causticIntensity: frame.caustics
        surfaceOpacity: frame.isActive ? ValoTheme.opacityGlassWindowActive : ValoTheme.opacityGlassWindowInactive
        volumetricTint: Qt.rgba(0.025, 0.12, 0.22, 0.52)
        shadowOffsetY: frame.isMaximized ? 0.0 : 16.0
        shadowIntensity: frame.isMaximized ? 0.0 : 0.38
        active: frame.visible
        highContrast: frame.userHighContrast
        prefersReducedMotion: frame.userPrefersReducedMotion
    }

    // Área cliente Wayland XDG (com suporte a subsurfaces, frame callbacks via OutputLayoutItem e opaque backing)
    Item {
        id: clientViewport
        objectName: "clientViewport"
        x: frame.frameMargin
        y: sgeTitleBar.height + frame.frameMargin
        width: Math.max(0, parent.width - 2 * frame.frameMargin)
        height: Math.max(0, parent.height - sgeTitleBar.height - 2 * frame.frameMargin)
        z: 1
        clip: true

        // Impede vazamento transparente quando buffers AB24 definem opaque_region (ex.: Electron / VS Code)
        Rectangle {
            id: opaqueClientBacking
            objectName: "opaqueClientBacking"
            x: 0
            y: 0
            width: toplevelItem.width > 0 ? Math.min(parent.width, toplevelItem.width) : parent.width
            height: toplevelItem.height > 0 ? Math.min(parent.height, toplevelItem.height) : parent.height
            radius: Math.max(0, frame.cornerRadius - frame.frameMargin - 2)
            color: "#0b111c"
            visible: frame.surfaceOpaque && toplevelItem.ready
        }

        OutputLayoutItem {
            anchors.fill: parent
            layout: frame.outputLayout
            onEnterOutput: (output) => {
                if (frame.waylandSurface && output) {
                    frame.waylandSurface.enterOutput(output)
                }
            }
            onLeaveOutput: (output) => {
                if (frame.waylandSurface && output) {
                    frame.waylandSurface.leaveOutput(output)
                }
            }
        }

        XdgToplevelSurfaceItem {
            id: toplevelItem
            objectName: "waylibToplevelSurfaceItem"
            x: 0
            y: 0
            surface: frame.waylandSurface
            shellSurface: frame.windowShellSurface
            subsurfacesVisible: true
            enabled: !frame.resizing

            function syncInitialSize() {
                if (frame.userSized || frame.isMaximized || implicitWidth <= 0 || implicitHeight <= 0) {
                    return
                }
                const targetW = Math.max(320, Math.min(implicitWidth + 2 * frame.frameMargin, frame.workAreaWidth))
                const targetH = Math.max(220, Math.min(implicitHeight + frame.titleBarHeight + 2 * frame.frameMargin, frame.workAreaHeight))
                frame.normalWidth = targetW
                frame.normalHeight = targetH
                frame.normalX = Math.max(0, Math.min(frame.normalX, Math.max(0, frame.workAreaWidth - targetW)))
                frame.normalY = Math.max(0, Math.min(frame.normalY, Math.max(0, frame.workAreaHeight - targetH)))
                if (implicitWidth + 2 * frame.frameMargin > frame.workAreaWidth
                        || implicitHeight + frame.titleBarHeight + 2 * frame.frameMargin > frame.workAreaHeight) {
                    frame.surfaceAdapter.resizeSurface(frame.windowShellSurface,
                                                       Math.max(240, Math.round(targetW - 2 * frame.frameMargin)),
                                                       Math.max(160, Math.round(targetH - frame.titleBarHeight - 2 * frame.frameMargin)))
                }
            }

            onImplicitWidthChanged: syncInitialSize()
            onImplicitHeightChanged: syncInitialSize()
        }

        // Ativa a janela ao clicar na área cliente propagando o evento ao cliente
        MouseArea {
            anchors.fill: parent
            z: 1
            acceptedButtons: Qt.AllButtons
            propagateComposedEvents: true
            onPressed: (mouse) => {
                frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
                mouse.accepted = false
            }
        }
    }

    // Barra de título oficial VALO Glass V2 (losango ciano + 3 botões esféricos brilhantes)
    SystaliaWindowHeader {
        id: sgeTitleBar
        objectName: "sgeTitleBar"
        width: parent.width
        height: implicitHeight
        z: 2
        iconGlyph: "▰"
        title: frame.windowTitle.length > 0
               ? frame.windowTitle
               : (frame.windowAppId.length > 0 ? frame.windowAppId : "Aplicação Wayland")
        maximized: frame.isMaximized
        glassSurface: false
        radii: frame.frameRadii
        slant: frame.slant
        slantReferenceWidth: frame.width
        slantReferenceHeight: frame.height
        dragEnabled: false

        onMinimizeRequested: frame.surfaceAdapter.toggleMinimize(frame.windowShellSurface)
        onMaximizeRequested: {
            if (!frame.windowShellSurface) {
                return
            }
            if (!frame.isMaximized) {
                const sz = frame.maximizedClientSize()
                frame.surfaceAdapter.setSurfaceMaximized(frame.windowShellSurface, true, sz.width, sz.height)
            } else {
                const targetW = Math.max(240, Math.round(frame.normalWidth - 2 * frame.frameMargin))
                const targetH = Math.max(160, Math.round(frame.normalHeight - frame.titleBarHeight - 2 * frame.frameMargin))
                frame.surfaceAdapter.setSurfaceMaximized(frame.windowShellSurface, false, targetW, targetH)
            }
        }
        onCloseRequested: frame.surfaceAdapter.requestClose(frame.windowShellSurface)
        onHeaderTapped: frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
    }

    // Arraste controlado pela barra de título permitindo passagem livre sob TopBar e Dock
    MouseArea {
        id: titleDragArea
        objectName: "ssdDragArea"
        x: 0
        y: 0
        width: Math.max(120, frame.width - 134)
        height: frame.titleBarHeight
        z: 3
        hoverEnabled: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        acceptedButtons: Qt.LeftButton
        property point pressOffset: Qt.point(0, 0)

        onPressed: (mouse) => {
            frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
            pressOffset = Qt.point(mouse.x, mouse.y)
            if (frame.compositorAdapter) {
                frame.compositorAdapter.compositorCursorOverride = "grabbing"
            }
            mouse.accepted = true
        }

        onPositionChanged: (mouse) => {
            if (!pressed || frame.isMaximized) {
                return
            }
            const nextX = frame.normalX + (mouse.x - pressOffset.x)
            const nextY = frame.normalY + (mouse.y - pressOffset.y)
            const visibleKeepW = Math.min(120, frame.width * 0.3)
            const minX = -frame.layerOriginX - frame.width + visibleKeepW
            const maxX = frame.desktopWidth - frame.layerOriginX - visibleKeepW
            const minY = -frame.layerOriginY
            const maxY = frame.desktopHeight - frame.layerOriginY - Math.max(36, frame.titleBarHeight)
            frame.normalX = Math.max(minX, Math.min(nextX, maxX))
            frame.normalY = Math.max(minY, Math.min(nextY, maxY))
        }

        onReleased: {
            if (frame.compositorAdapter) {
                frame.compositorAdapter.compositorCursorOverride = ""
            }
        }
        onCanceled: {
            if (frame.compositorAdapter) {
                frame.compositorAdapter.compositorCursorOverride = ""
            }
        }
        onContainsMouseChanged: {
            if (!frame.compositorAdapter) {
                return
            }
            if (containsMouse) {
                frame.compositorAdapter.hoveredCursorKey = "grab"
            } else if (frame.compositorAdapter.hoveredCursorKey === "grab") {
                frame.compositorAdapter.hoveredCursorKey = ""
            }
        }
    }

    // Sistema de redimensionamento multi-direcional (8 bordas e cantos)
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
            readonly property bool isCorner: horizontalEdge && verticalEdge
            readonly property int cornerMargin: 12
            readonly property string hoverKey: frame.cursorKeyForEdges(modelData.edges)
            objectName: "resize_" + modelData.name
            z: isCorner ? 102 : 101
            x: (modelData.edges & Qt.RightEdge) ? frame.width - width
               : (horizontalEdge ? 0 : cornerMargin)
            y: (modelData.edges & Qt.BottomEdge) ? frame.height - height
               : (verticalEdge ? 0 : cornerMargin)
            width: isCorner ? cornerMargin
                            : (horizontalEdge ? frame.borderMargin : Math.max(0, frame.width - 2 * cornerMargin))
            height: isCorner ? cornerMargin
                             : (verticalEdge ? frame.borderMargin : Math.max(0, frame.height - 2 * cornerMargin))
            visible: !frame.isMaximized
            enabled: !frame.isMaximized
            hoverEnabled: true
            cursorShape: modelData.cursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true

            onContainsMouseChanged: {
                if (!frame.compositorAdapter) {
                    return
                }
                if (containsMouse) {
                    frame.compositorAdapter.hoveredCursorKey = hoverKey
                } else if (frame.compositorAdapter.hoveredCursorKey === hoverKey) {
                    frame.compositorAdapter.hoveredCursorKey = ""
                }
            }
            onPressed: (mouse) => {
                frame.beginResize(modelData.edges, mapToItem(frame.parent, mouse.x, mouse.y))
                mouse.accepted = true
            }
            onPositionChanged: (mouse) => {
                if (pressed) {
                    frame.updateResize(mapToItem(frame.parent, mouse.x, mouse.y))
                }
            }
            onReleased: frame.finishResize()
            onCanceled: frame.finishResize()
        }
    }
}
