import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor
import Systalia.Engine

Item {
    id: windowContainer

    property var toplevel: null
    property var waylandServer: null
    property var backdropSource: null
    property point screenResolution: Qt.point(1280, 800)
    property real animTime: 0.0

    width: 680
    height: 480

    property int minWindowWidth: 320
    property int minWindowHeight: 220
    readonly property int borderMargin: 8
    readonly property int frameMargin: 4
    readonly property int titleBarHeight: 36
    readonly property var xdgSurface: toplevel ? toplevel.xdgSurface : null
    readonly property var opticalMetadata: {
        if (!waylandServer || !xdgSurface)
            return null
        const extension = waylandServer.surfaceExtension
        // Track creation/removal as well as notifications on the metadata object.
        extension.surfaceCount
        return extension.resourceForSurface(xdgSurface.surface)
    }
    readonly property rect clientGeometry: xdgSurface ? xdgSurface.windowGeometry : Qt.rect(0, 0, 0, 0)
    readonly property bool resizeEnabled: toplevel !== null && waylandServer !== null
                                          && !toplevel.maximized && !toplevel.fullscreen
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
        if (resizeAnchored && (resizeEdges & Qt.LeftEdge))
            x = fixedRight - width
        if (resizeAnchored && (resizeEdges & Qt.TopEdge))
            y = fixedBottom - height
    }

    onClientGeometryChanged: syncClientGeometry()

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
        waylandServer.requestToplevelResize(toplevel, requestedClientSize, true)
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
        waylandServer.requestToplevelResize(toplevel, requestedClientSize, true)
    }

    function finishResize() {
        if (!resizing)
            return
        resizing = false
        if (toplevel && waylandServer)
            waylandServer.requestToplevelResize(toplevel, requestedClientSize, false)
    }

    onResizeEnabledChanged: {
        if (!resizeEnabled)
            finishResize()
    }

    // Chassi de Vidro Líquido Nativo do Systalia Glass Engine (SGE)
    SystaliaGlassItem {
        id: glassChassis
        anchors.fill: parent

        backdropSource: windowContainer.backdropSource
        screenResolution: Qt.vector2d(windowContainer.screenResolution.x, windowContainer.screenResolution.y)

        radii: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.radii : Qt.vector4d(18.0, 18.0, 18.0, 18.0)
        slant: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.slant : 0.0
        ior: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.ior : 1.25
        chromaticDispersion: windowContainer.opticalMetadata ? windowContainer.opticalMetadata.dispersion : 0.040
        causticIntensity: 0.70
        surfaceOpacity: 0.90
        time: windowContainer.animTime

        // Substrato protetor abissal para contraste óptico de texto e janelas externas
        Rectangle {
            anchors.fill: parent
            radius: 18
            color: Qt.rgba(0.04, 0.08, 0.15, 0.75)
            border.color: windowContainer.activeFocus ? "#00f2fe" : "#25ffffff"
            border.width: 1

            // 1. Barra de Título SGE com Decoração Server-Side Nativa (SSD)
            Rectangle {
                id: sgeTitleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: windowContainer.titleBarHeight
                radius: 18
                color: windowContainer.activeFocus ? "#3000f2fe" : "#181e293b"

                // Ocultar cantos inferiores arredondados da barra de título
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 12
                    color: parent.color
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 10
                    spacing: 8

                    // Ícone do Aplicativo SGE
                    Rectangle {
                        width: 20
                        height: 20
                        radius: 5
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#38bdf8" }
                            GradientStop { position: 1.0; color: "#0284c7" }
                        }
                        border.color: "#80ffffff"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: ">_"
                            font.pixelSize: 10
                            font.bold: true
                            color: "#f8fafc"
                        }
                    }

                    // Título do Aplicativo
                    Text {
                        Layout.fillWidth: true
                        text: (windowContainer.toplevel && windowContainer.toplevel.title.length > 0)
                              ? windowContainer.toplevel.title
                              : ((windowContainer.toplevel && windowContainer.toplevel.appId.length > 0)
                                 ? windowContainer.toplevel.appId
                                 : "Aplicação Externa (SGE)")
                        font.pixelSize: 12
                        font.bold: true
                        color: "#f8fafc"
                        elide: Text.ElideRight
                    }

                    // Badge de Decoração Server-Side
                    Rectangle {
                        height: 18
                        implicitWidth: badgeTxt.implicitWidth + 10
                        radius: 4
                        color: "#1500f2fe"
                        border.color: "#4000f2fe"
                        border.width: 1

                        Text {
                            id: badgeTxt
                            anchors.centerIn: parent
                            text: "SGE SSD"
                            font.pixelSize: 8
                            font.bold: true
                            color: "#00f2fe"
                        }
                    }

                    // Botões Canônicos de Controle de Janela SGE
                    Row {
                        spacing: 6

                        // Botão Minimizar
                        Rectangle {
                            width: 24
                            height: 20
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: minMouse.containsMouse ? "#40ffffff" : "#20ffffff" }
                                GradientStop { position: 1.0; color: minMouse.containsMouse ? "#300284c7" : "#0d1b2a" }
                            }
                            border.color: minMouse.containsMouse ? "#00f2fe" : "#30ffffff"
                            border.width: 1

                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 2
                                color: "#f8fafc"
                            }

                            MouseArea {
                                id: minMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        // Botão Maximizar
                        Rectangle {
                            width: 24
                            height: 20
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: maxMouse.containsMouse ? "#40ffffff" : "#20ffffff" }
                                GradientStop { position: 1.0; color: maxMouse.containsMouse ? "#300284c7" : "#0d1b2a" }
                            }
                            border.color: maxMouse.containsMouse ? "#00f2fe" : "#30ffffff"
                            border.width: 1

                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 8
                                color: "transparent"
                                border.color: "#f8fafc"
                                border.width: 1.2
                            }

                            MouseArea {
                                id: maxMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (windowContainer.toplevel) {
                                        if (windowContainer.toplevel.maximized) {
                                            windowContainer.toplevel.sendUnmaximized()
                                        } else {
                                            windowContainer.toplevel.sendMaximized(Qt.size(windowContainer.width, windowContainer.height))
                                        }
                                    }
                                }
                            }
                        }

                        // Botão Fechar
                        Rectangle {
                            width: 24
                            height: 20
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: closeMouse.containsMouse ? "#ff5c7a" : "#25ffffff" }
                                GradientStop { position: 1.0; color: closeMouse.containsMouse ? "#b91c1c" : "#0d1b2a" }
                            }
                            border.color: closeMouse.containsMouse ? "#fca5a5" : "#35ffffff"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                font.pixelSize: 10
                                color: "#f8fafc"
                            }

                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (windowContainer.toplevel) {
                                        windowContainer.toplevel.sendClose()
                                    }
                                }
                            }
                        }
                    }
                }

                DragHandler {
                    id: windowDrag
                    target: windowContainer
                    enabled: !windowContainer.resizing
                    onActiveChanged: if (active) windowContainer.resizeAnchored = false
                }
            }

            // Clip buffer extents to xdg windowGeometry without stretching uncommitted buffers.
            Item {
                id: clientViewport
                anchors.top: sgeTitleBar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: windowContainer.frameMargin
                clip: true

                ShellSurfaceItem {
                    id: clientSurfaceItem
                    objectName: "clientSurfaceItem"
                    x: -windowContainer.clientGeometry.x
                    y: -windowContainer.clientGeometry.y
                    shellSurface: windowContainer.xdgSurface
                    focus: true
                    focusOnClick: true
                    inputEventsEnabled: !windowContainer.resizing
                    paintEnabled: true
                    moveItem: windowContainer

                    onSurfaceDestroyed: {
                        windowContainer.finishResize()
                        windowContainer.destroy()
                    }
                }
            }
        }
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

    TapHandler {
        onTapped: {
            windowContainer.forceActiveFocus()
            clientSurfaceItem.forceActiveFocus()
        }
    }
}
