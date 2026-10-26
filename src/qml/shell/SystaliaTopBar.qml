import QtQuick
import QtQuick.Controls
import Systalia.Engine

Item {
    id: topBar

    property var telemetry: null
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0

    signal launcherRequested()
    signal controlCenterRequested()

    width: 1646
    height: 45
    z: 100

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: topBar.backdropSource
        screenResolution: topBar.screenResolution
        radii: Qt.vector4d(22, 22, 22, 22)
        slant: 0
        ior: 1.12
        chromaticDispersion: 0.018
        causticIntensity: 0.18
        surfaceOpacity: 0.72
        time: topBar.animationTime
    }

    Rectangle {
        anchors.fill: parent
        radius: 22
        color: "#d9071628"
        border.color: "#3d74d5ee"
        border.width: 1
        antialiasing: true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 25
        anchors.rightMargin: 25
        height: 1
        opacity: .7
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: .18; color: "#5bc5e7f0" }
            GradientStop { position: .82; color: "#5bc5e7f0" }
            GradientStop { position: 1; color: "transparent" }
        }
        antialiasing: true
    }

    Image {
        x: 14
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/systalia-mark.svg"
        sourceSize: Qt.size(28, 28)
        smooth: true
        mipmap: true
        TapHandler {
            cursorShape: Qt.PointingHandCursor
            onTapped: topBar.launcherRequested()
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }

    Text {
        x: 47
        anchors.verticalCenter: parent.verticalCenter
        text: "SYSTALIA"
        color: SystaliaTheme.text
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 19
        font.weight: Font.DemiBold
        font.letterSpacing: 1.0
        antialiasing: true
    }

    Text {
        x: 189
        anchors.verticalCenter: parent.verticalCenter
        text: "SDE-KWIN  |  VALO"
        color: SystaliaTheme.textSecondary
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 17
        font.letterSpacing: .55
        antialiasing: true
    }

    Text {
        x: 392
        anchors.verticalCenter: parent.verticalCenter
        width: 302
        text: topBar.telemetry
              ? "CPU %1%   RAM %2 GB   FPS %3"
                .arg(Math.round(topBar.telemetry.cpuUsage))
                .arg(topBar.telemetry.ramUsageGb.toFixed(1))
                .arg(topBar.telemetry.fps)
              : "CPU --%   RAM -- GB   FPS --"
        color: SystaliaTheme.textSecondary
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 17
        font.letterSpacing: .2
        elide: Text.ElideRight
        antialiasing: true
    }

    Text {
        x: 724
        anchors.verticalCenter: parent.verticalCenter
        text: topBar.telemetry ? topBar.telemetry.systemTime : "--:--:--"
        color: SystaliaTheme.text
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 17
        font.weight: Font.Medium
        antialiasing: true
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 17
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        StatusButton { glyph: "⌁"; accessibleName: "Rede"; onClicked: topBar.controlCenterRequested() }
        StatusButton { glyph: "◖"; accessibleName: "Som"; onClicked: topBar.controlCenterRequested() }
        StatusButton { glyph: "☼"; accessibleName: "Brilho"; onClicked: topBar.controlCenterRequested() }
        StatusButton { glyph: "⌘"; accessibleName: "Sessão"; onClicked: topBar.controlCenterRequested() }
    }

    component StatusButton: ToolButton {
        id: statusButton
        property string glyph: "•"
        property string accessibleName: "Controle do sistema"
        Accessible.name: accessibleName
        width: 32
        height: 30
        hoverEnabled: true
        contentItem: Text {
            text: statusButton.glyph
            color: statusButton.hovered ? "#f4fcff" : "#c6e5f8"
            font.pixelSize: 16
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            antialiasing: true
        }
        background: Rectangle {
            radius: 9
            color: statusButton.hovered ? "#3d47a9c2" : "#1f295373"
            border.color: statusButton.hovered ? "#7b9de0f0" : "#285e8ca8"
            border.width: 1
            antialiasing: true
        }
    }
}
