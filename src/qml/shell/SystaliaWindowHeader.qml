import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Systalia.Engine

Item {
    id: header

    property string iconGlyph: "▰"
    property string title: ""
    property string badgeText: ""
    property int badgeWidth: 120
    property int titlePixelSize: 21
    property color iconColor: "#64d6ff"
    property color titleColor: "#f4fcff"
    property color badgeTextColor: "#86b7d9"
    property real actionsRightMargin: 18.5
    property bool maximized: false
    property bool glassSurface: false
    property bool dragEnabled: false
    property Item dragTarget: null
    property vector4d radii: Qt.vector4d(20, 20, 0, 0)
    property real slant: 0
    property real slantReferenceWidth: width
    property real slantReferenceHeight: height
    property real slantOffsetX: 0
    property real slantOffsetY: 0

    signal minimizeRequested()
    signal maximizeRequested()
    signal closeRequested()
    signal dragStarted()
    signal headerTapped()

    readonly property real headerRadiusLeft: Math.max(0, radii.x)
    readonly property real headerRadiusRight: Math.max(0, radii.y)
    readonly property real badgePreferredWidth: Math.min(
                                                   Math.max(badgeWidth, badgeLabel.implicitWidth + 18),
                                                   Math.max(90, width * 0.34))

    SystaliaG2Frame {
        anchors.fill: parent
        visible: header.glassSurface
        radii: Qt.vector4d(header.headerRadiusLeft, header.headerRadiusRight, 0, 0)
        slant: header.slant
        slantReferenceWidth: header.slantReferenceWidth
        slantReferenceHeight: header.slantReferenceHeight
        slantOffsetX: header.slantOffsetX
        slantOffsetY: header.slantOffsetY

        fillGradient: LinearGradient {
            x1: 0
            y1: 0
            x2: header.width
            y2: 0
            GradientStop { position: 0.0; color: "#63264f76" }
            GradientStop { position: 0.24; color: "#754bafc7" }
            GradientStop { position: 0.52; color: "#63133a5d" }
            GradientStop { position: 1.0; color: "#6809233f" }
        }
    }

    SystaliaG2Frame {
        x: 4.5
        y: 2.5
        width: Math.max(0, header.width - 9)
        height: Math.max(0, header.height - 5)
        visible: header.glassSurface
        radii: Qt.vector4d(14, 14, 0, 0)
        slant: header.slant
        slantReferenceWidth: header.slantReferenceWidth
        slantReferenceHeight: header.slantReferenceHeight
        slantOffsetX: header.slantOffsetX + 4.5
        slantOffsetY: header.slantOffsetY + 2.5

        fillGradient: LinearGradient {
            x1: 0
            y1: 0
            x2: 0
            y2: Math.max(1, header.height - 5)
            GradientStop { position: 0.0; color: "#40c8ffff" }
            GradientStop { position: 0.34; color: "#1f61f0ff" }
            GradientStop { position: 0.72; color: "#0b155279" }
            GradientStop { position: 1.0; color: "#001b1834" }
        }
    }

    Text {
        x: 16.5
        width: 28
        height: 36
        anchors.verticalCenter: parent.verticalCenter
        text: header.iconGlyph
        color: header.iconColor
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 24
        font.weight: Font.Bold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        antialiasing: true
    }

    RowLayout {
        id: titleContent
        anchors.left: parent.left
        anchors.leftMargin: 48.5
        anchors.right: actions.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Text {
            id: titleLabel
            text: header.title
            color: header.titleColor
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: header.titlePixelSize
            font.weight: Font.Medium
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            antialiasing: true
            Layout.minimumWidth: 48
            Layout.preferredWidth: Math.min(
                                       implicitWidth,
                                       Math.max(48, titleContent.width
                                                - (badge.visible
                                                   ? header.badgePreferredWidth + titleContent.spacing
                                                   : 0)))
            Layout.maximumWidth: Math.max(
                                     48, titleContent.width
                                     - (badge.visible
                                        ? header.badgePreferredWidth + titleContent.spacing
                                        : 0))
        }

        Rectangle {
            id: badge
            visible: header.badgeText.length > 0
            height: 26
            radius: 6
            color: "#b80a2c46"
            antialiasing: true
            Layout.preferredWidth: header.badgePreferredWidth
            Layout.maximumWidth: Math.max(90, header.width * 0.34)

            Text {
                id: badgeLabel
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                text: header.badgeText
                color: header.badgeTextColor
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                antialiasing: true
            }
        }
    }

    Row {
        id: actions
        anchors.right: parent.right
        anchors.rightMargin: header.actionsRightMargin
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        SystaliaWindowAction {
            objectName: "minimizeToplevelButton"
            glyph: "−"
            accessibleName: "Minimizar " + header.title
            onClicked: header.minimizeRequested()
        }

        SystaliaWindowAction {
            objectName: "maximizeToplevelButton"
            glyph: header.maximized ? "❐" : "□"
            accessibleName: (header.maximized ? "Restaurar " : "Maximizar ") + header.title
            onClicked: header.maximizeRequested()
        }

        SystaliaWindowAction {
            objectName: "closeToplevelButton"
            glyph: "×"
            closeAction: true
            accessibleName: "Fechar " + header.title
            onClicked: header.closeRequested()
        }
    }

    DragHandler {
        target: header.dragTarget
        enabled: header.dragEnabled && header.dragTarget !== null
        onActiveChanged: if (active) header.dragStarted()
    }

    TapHandler {
        enabled: header.dragEnabled
        onTapped: header.headerTapped()
    }
}
