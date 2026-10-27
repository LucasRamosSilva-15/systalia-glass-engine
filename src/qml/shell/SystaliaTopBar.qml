import QtQuick
import QtQuick.Controls
import Systalia.Engine

Item {
    id: topBar

    property var telemetry: null
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0
    property date dateNow: new Date()

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
        color: "#db071628"
        border.color: "#3d74d5ee"
        border.width: 1
        antialiasing: true
    }

    Image {
        x: 17
        y: 9
        width: 18
        height: 23
        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/systalia-mark.png"
        sourceSize: Qt.size(112, 112)
        fillMode: Image.PreserveAspectFit
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
        color: "#c6e5f8"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 19
        font.weight: Font.Medium
        antialiasing: true
    }

    Text {
        x: 189
        anchors.verticalCenter: parent.verticalCenter
        text: "SDE-KWIN | VALO"
        color: "#9cbad3"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 17
        antialiasing: true
    }

    Text {
        x: 392
        anchors.verticalCenter: parent.verticalCenter
        width: 318
        text: topBar.telemetry
              ? "CPU %1%  |  RAM %2GB  |  FPS %3"
                .arg(Math.round(topBar.telemetry.cpuUsage))
                .arg(topBar.telemetry.ramUsageGb.toFixed(1))
                .arg(topBar.telemetry.fps)
              : "CPU --%  |  RAM --GB  |  FPS --"
        color: "#9cbad3"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 17
        elide: Text.ElideRight
        antialiasing: true
    }

    Text {
        x: 724
        anchors.verticalCenter: parent.verticalCenter
        width: 300
        text: topBar.telemetry
              ? topBar.telemetry.systemTime
              : topBar.dateNow.toLocaleDateString(Qt.locale("en_US"), "ddd dd MMM").toUpperCase()
                + "  >  "
                + topBar.dateNow.toLocaleTimeString(Qt.locale("en_US"), "HH:mm:ss")
        color: "#c2d7df"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 17
        antialiasing: true
    }

    Row {
        x: 1331
        y: 7.5
        width: 290
        height: 30

        Repeater {
            model: [
                { glyph: "▣", name: "Estado do sistema" },
                { glyph: ">_", name: "Terminal" },
                { glyph: "▦", name: "Aplicativos" },
                { glyph: "☼", name: "Brilho" },
                { glyph: "▣", name: "Rede e som" },
                { glyph: "⏻", name: "Energia" }
            ]

            delegate: StatusButton {
                required property var modelData
                width: 48
                height: 30
                glyph: modelData.glyph
                accessibleName: modelData.name
                onClicked: topBar.controlCenterRequested()
            }
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: topBar.dateNow = new Date()
    }

    component StatusButton: ToolButton {
        id: statusButton
        property string glyph: "•"
        property string accessibleName: "Controle do sistema"
        Accessible.name: accessibleName
        hoverEnabled: true

        contentItem: Text {
            text: statusButton.glyph
            color: statusButton.hovered ? "#f4fcff" : "#a0e7fa"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 15
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            antialiasing: true
        }
        background: Rectangle {
            radius: 5
            color: statusButton.hovered ? "#2747a9c2" : "transparent"
            border.color: statusButton.hovered ? "#3d9de0f0" : "transparent"
            border.width: 1
            antialiasing: true
        }
    }
}
