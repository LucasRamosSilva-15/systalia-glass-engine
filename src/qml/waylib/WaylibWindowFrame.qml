import QtQuick
import Waylib.Server
import Systalia.Engine

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

    readonly property int frameMargin: isMaximized ? 0 : 8
    readonly property int titleBarHeight: sgeTitleBar.implicitHeight
    readonly property real radius: isMaximized ? 0.0 : cornerRadius
    readonly property vector4d frameRadii: isMaximized
                                           ? Qt.vector4d(0, 0, 0, 0)
                                           : Qt.vector4d(cornerRadius, cornerRadius, cornerRadius, cornerRadius)

    property real normalX: Math.max(8, Math.min(24 + (windowIndex * 36), Math.max(8, workAreaWidth - 480)))
    property real normalY: Math.max(8, Math.min(16 + (windowIndex * 32), Math.max(8, workAreaHeight - 360)))
    property real normalWidth: 480
    property real normalHeight: 340
    property bool userSized: false
    property bool surfaceOpaque: false

    x: isMaximized ? 0 : normalX
    y: isMaximized ? 0 : normalY
    width: isMaximized ? Math.max(320, workAreaWidth) : normalWidth
    height: isMaximized ? Math.max(220, workAreaHeight) : normalHeight
    z: windowZOrder
    visible: !isMinimized
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
            if (surface === frame.windowShellSurface) {
                frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
                frame.surfaceAdapter.raiseSurface(frame.windowShellSurface)
            }
        }
    }

    function syncMaximizedSurface() {
        if (!isMaximized || !windowShellSurface) {
            return
        }
        const targetW = Math.max(240, Math.round(frame.width - 2 * frame.frameMargin))
        const targetH = Math.max(160, Math.round(frame.height - frame.titleBarHeight - 2 * frame.frameMargin))
        frame.surfaceAdapter.resizeSurface(frame.windowShellSurface, targetW, targetH)
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
    onWorkAreaWidthChanged: syncMaximizedSurface()
    onWorkAreaHeightChanged: syncMaximizedSurface()

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
        surfaceOpacity: frame.isActive ? 0.50 : 0.42
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
            anchors.fill: parent
            radius: frame.isMaximized ? 0 : Math.max(0, frame.cornerRadius - frame.frameMargin - 2)
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
        onMaximizeRequested: frame.surfaceAdapter.toggleMaximize(frame.windowShellSurface)
        onCloseRequested: frame.surfaceAdapter.requestClose(frame.windowShellSurface)
        onHeaderTapped: frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
    }

    // Arraste controlado pela barra de título preservando bindings de normalX/normalY e clamping na workarea
    MouseArea {
        id: titleDragArea
        objectName: "ssdDragArea"
        x: 0
        y: 0
        width: Math.max(120, frame.width - 134)
        height: frame.titleBarHeight
        z: 3
        acceptedButtons: Qt.LeftButton
        property point pressOffset: Qt.point(0, 0)

        onPressed: (mouse) => {
            frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
            pressOffset = Qt.point(mouse.x, mouse.y)
            mouse.accepted = true
        }

        onPositionChanged: (mouse) => {
            if (!pressed || frame.isMaximized) {
                return
            }
            const nextX = frame.normalX + (mouse.x - pressOffset.x)
            const nextY = frame.normalY + (mouse.y - pressOffset.y)
            frame.normalX = Math.max(0, Math.min(nextX, Math.max(0, frame.workAreaWidth - frame.width)))
            frame.normalY = Math.max(0, Math.min(nextY, Math.max(0, frame.workAreaHeight - frame.height)))
        }
    }

    // Alça de redimensionamento no canto inferior direito
    MouseArea {
        objectName: "resizeCornerHandle"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 14
        height: 14
        z: 100
        visible: !frame.isMaximized
        cursorShape: Qt.SizeFDiagCursor
        property point startPos: Qt.point(0, 0)
        property real startW: 0
        property real startH: 0

        onPressed: (mouse) => {
            frame.surfaceAdapter.activateSurface(frame.windowShellSurface)
            startPos = mapToItem(frame.parent, mouse.x, mouse.y)
            startW = frame.normalWidth
            startH = frame.normalHeight
            mouse.accepted = true
        }

        onPositionChanged: (mouse) => {
            if (!pressed || !frame.parent) {
                return
            }
            const cur = mapToItem(frame.parent, mouse.x, mouse.y)
            const nextW = Math.max(320, Math.min(startW + (cur.x - startPos.x), frame.workAreaWidth - frame.normalX))
            const nextH = Math.max(220, Math.min(startH + (cur.y - startPos.y), frame.workAreaHeight - frame.normalY))
            frame.userSized = true
            frame.normalWidth = nextW
            frame.normalHeight = nextH
            frame.surfaceAdapter.resizeSurface(frame.windowShellSurface,
                                               Math.max(240, Math.round(nextW - 2 * frame.frameMargin)),
                                               Math.max(160, Math.round(nextH - frame.titleBarHeight - 2 * frame.frameMargin)))
        }
    }
}
