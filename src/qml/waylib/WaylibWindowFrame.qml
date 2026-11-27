import QtQuick
import QtQuick.Layouts
import Waylib.Server
import Systalia.Engine

Rectangle {
    id: frame
    objectName: "waylibWindowFrame"

    required property var surfaceAdapter
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

    readonly property int titleBarHeight: 34
    property real normalX: Math.max(8, Math.min(24 + (windowIndex * 36), Math.max(8, workAreaWidth - 420)))
    property real normalY: Math.max(8, Math.min(16 + (windowIndex * 32), Math.max(8, workAreaHeight - 320)))
    property real normalWidth: 480
    property real normalHeight: 320
    property bool userSized: false

    x: isMaximized ? 0 : normalX
    y: isMaximized ? 0 : normalY
    width: isMaximized ? Math.max(320, workAreaWidth) : normalWidth
    height: isMaximized ? Math.max(220, workAreaHeight) : normalHeight
    z: windowZOrder
    visible: !isMinimized

    radius: isMaximized ? 0 : 14
    color: frame.isActive ? "#EB091A2E" : "#CC071222"
    border.width: frame.isActive ? 1.5 : 1.0
    border.color: frame.isActive
                  ? ValoTheme.withAlpha(ValoTheme.accentPrimary, 0.85)
                  : ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.28)
    clip: true

    onIsMaximizedChanged: {
        if (!windowShellSurface) {
            return
        }
        const targetW = Math.max(240, Math.round(frame.width - 2))
        const targetH = Math.max(160, Math.round(frame.height - frame.titleBarHeight - 2))
        frame.surfaceAdapter.resizeSurface(frame.windowShellSurface, targetW, targetH)
    }

    // Barra de título SSD VALO
    Rectangle {
        id: titleBar
        objectName: "ssdTitleBar"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: frame.titleBarHeight
        color: frame.isActive
               ? ValoTheme.withAlpha(ValoTheme.primitiveOcean700, 0.65)
               : ValoTheme.withAlpha(ValoTheme.primitiveOcean900, 0.55)

        // Absorve cliques e permite arrastar a janela sem vazar para superfícies abaixo
        MouseArea {
            id: dragArea
            anchors.fill: parent
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

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            spacing: 8

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: frame.isActive ? ValoTheme.accentPrimary : ValoTheme.withAlpha(ValoTheme.textSecondary, 0.5)
            }

            Text {
                objectName: "windowTitleText"
                Layout.fillWidth: true
                text: frame.windowTitle.length > 0
                      ? frame.windowTitle
                      : (frame.windowAppId.length > 0 ? frame.windowAppId : "Wayland Client")
                color: frame.isActive ? ValoTheme.textBright : ValoTheme.textSecondary
                font.family: ValoTheme.fontFamily
                font.pixelSize: 12
                font.weight: frame.isActive ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }

            // Botão Minimizar
            Rectangle {
                objectName: "minimizeButton"
                width: 22
                height: 22
                radius: 6
                color: minMouse.containsMouse ? ValoTheme.withAlpha(ValoTheme.primitiveOcean600, 0.7) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "—"
                    color: ValoTheme.textPrimary
                    font.pixelSize: 11
                }

                MouseArea {
                    id: minMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: frame.surfaceAdapter.toggleMinimize(frame.windowShellSurface)
                }
            }

            // Botão Maximizar / Restaurar
            Rectangle {
                objectName: "maximizeButton"
                width: 22
                height: 22
                radius: 6
                color: maxMouse.containsMouse ? ValoTheme.withAlpha(ValoTheme.primitiveOcean600, 0.7) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: frame.isMaximized ? "❐" : "□"
                    color: ValoTheme.textPrimary
                    font.pixelSize: 11
                }

                MouseArea {
                    id: maxMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: frame.surfaceAdapter.toggleMaximize(frame.windowShellSurface)
                }
            }

            // Botão Fechar
            Rectangle {
                objectName: "closeButton"
                width: 22
                height: 22
                radius: 6
                color: closeMouse.containsMouse ? "#C0EF4444" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: closeMouse.containsMouse ? "#FFFFFF" : ValoTheme.textPrimary
                    font.pixelSize: 11
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: frame.surfaceAdapter.requestClose(frame.windowShellSurface)
                }
            }
        }
    }

    // Conteúdo da superfície cliente Wayland XDG
    Item {
        id: clientArea
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        clip: true

        XdgToplevelSurfaceItem {
            id: toplevelItem
            anchors.fill: parent
            surface: frame.waylandSurface
            shellSurface: frame.windowShellSurface

            onImplicitWidthChanged: {
                if (!frame.userSized && !frame.isMaximized && implicitWidth > 0) {
                    frame.normalWidth = Math.max(280, Math.min(implicitWidth + 2, frame.workAreaWidth))
                }
            }
            onImplicitHeightChanged: {
                if (!frame.userSized && !frame.isMaximized && implicitHeight > 0) {
                    frame.normalHeight = Math.max(200, Math.min(implicitHeight + frame.titleBarHeight + 2, frame.workAreaHeight))
                }
            }
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

    // Alça de redimensionamento no canto inferior direito
    MouseArea {
        objectName: "resizeCornerHandle"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 12
        height: 12
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
            const nextW = Math.max(260, Math.min(startW + (cur.x - startPos.x), frame.workAreaWidth - frame.normalX))
            const nextH = Math.max(180, Math.min(startH + (cur.y - startPos.y), frame.workAreaHeight - frame.normalY))
            frame.userSized = true
            frame.normalWidth = nextW
            frame.normalHeight = nextH
            frame.surfaceAdapter.resizeSurface(frame.windowShellSurface,
                                               Math.round(nextW - 2),
                                               Math.round(nextH - frame.titleBarHeight - 2))
        }
    }
}
