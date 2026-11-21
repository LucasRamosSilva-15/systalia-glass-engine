import QtQuick
import QtQuick.Controls
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: dockItem

    property string iconSource: ""
    property string label: ""
    property string action: ""
    property var appIds: []
    property bool isRunning: false
    property bool isFocused: false
    property bool showKeyboardFocus: false
    property real iconSize: ValoTheme.sizeDockIcon

    readonly property bool isHovered: hoverHandler.hovered
    readonly property bool isPressed: tapHandler.pressed

    signal activated()

    width: iconSize
    height: iconSize + 16
    focus: false
    activeFocusOnTab: true
    Accessible.name: label
    Accessible.role: Accessible.Button

    // Microinterações de escala no hover e clique
    scale: isPressed ? 0.94 : (isHovered ? 1.08 : 1.0)
    transformOrigin: Item.Center

    Behavior on scale {
        NumberAnimation {
            duration: ValoTheme.motionDurationFastMs
            easing.type: Easing.OutCubic
        }
    }

    // Suporte a ativação por teclado
    Keys.onReturnPressed: dockItem.activated()
    Keys.onSpacePressed: dockItem.activated()

    // Indicador de foco por teclado (visível estritamente quando navegado via teclado)
    Rectangle {
        anchors.centerIn: iconImage
        width: iconSize + 10
        height: iconSize + 10
        radius: (iconSize + 10) / 2
        color: "transparent"
        border.color: ValoTheme.withAlpha(ValoTheme.accentPrimary, 0.60)
        border.width: 1.5
        visible: dockItem.activeFocus && dockItem.showKeyboardFocus
        opacity: (dockItem.activeFocus && dockItem.showKeyboardFocus) ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: ValoTheme.motionDurationFastMs
                easing.type: Easing.OutCubic
            }
        }
    }

    // Ícone vetorial SVG do Figma
    Image {
        id: iconImage
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: dockItem.iconSize
        height: dockItem.iconSize
        source: dockItem.iconSource
        sourceSize: Qt.size(dockItem.iconSize * 2, dockItem.iconSize * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: false
    }

    // Indicador de execução luminoso e adaptativo VALO
    Item {
        id: indicatorContainer
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: iconImage.bottom
        anchors.topMargin: 4
        width: 24
        height: 6

        // Glow ambiente da aplicação ativa
        Rectangle {
            anchors.centerIn: parent
            width: indicatorPill.width + 6
            height: indicatorPill.height + 4
            radius: height / 2
            color: ValoTheme.accentPrimary
            opacity: (dockItem.isRunning && dockItem.isFocused) ? 0.45 : 0.0
            visible: dockItem.isRunning && dockItem.isFocused
        }

        // Pílula (ativa) ou Dot (segundo plano)
        Rectangle {
            id: indicatorPill
            anchors.centerIn: parent
            visible: dockItem.isRunning
            width: !dockItem.isRunning ? 0 : (dockItem.isFocused ? 14 : 5)
            height: dockItem.isFocused ? 3.5 : 5
            radius: height / 2
            color: dockItem.isFocused
                   ? ValoTheme.accentBright
                   : ValoTheme.withAlpha(ValoTheme.accentPrimary, 0.75)
            opacity: dockItem.isRunning ? (dockItem.isHovered ? 1.0 : 0.85) : 0.0
            antialiasing: true

            Behavior on width {
                NumberAnimation {
                    duration: ValoTheme.motionDurationFastMs
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on height {
                NumberAnimation {
                    duration: ValoTheme.motionDurationFastMs
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on color {
                ColorAnimation {
                    duration: ValoTheme.motionDurationFastMs
                }
            }
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        id: tapHandler
        onTapped: dockItem.activated()
    }

    ToolTip.visible: hoverHandler.hovered
    ToolTip.text: dockItem.label
    ToolTip.delay: 300
}
