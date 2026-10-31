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
    property vector4d cornerRadii: Qt.vector4d(22, 22, 22, 22)

    signal launcherRequested()
    signal controlCenterRequested()

    width: 1646
    height: 45
    z: 100

    // Sombra luminosa externa ciano conforme style.css: box-shadow: 0px 4px 18px rgba(9, 186, 237, 0.2)
    Rectangle {
        anchors.fill: parent
        radius: topBar.cornerRadii.x
        color: "#3309baed"
        z: -1
    }

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: topBar.backdropSource
        screenResolution: topBar.screenResolution
        radii: topBar.cornerRadii
        circularCorners: true
        slant: 0
        ior: 1.10
        chromaticDispersion: 0.012
        causticIntensity: 0.12
        surfaceOpacity: 0.86
        time: topBar.animationTime
    }

    // Contorno único de precisão conforme style.css: border: 1px solid rgba(116, 213, 238, 0.24)
    Rectangle {
        anchors.fill: parent
        radius: topBar.cornerRadii.x
        color: "#db071628"
        border.color: "#3d74d5ee"
        border.width: 1
    }

    Image {
        x: 17
        y: 9
        width: 18
        height: 23
        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-mark.png"
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
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        height: 30
        spacing: 4

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
                width: 44
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
            radius: 8
            color: statusButton.hovered ? "#2b3d8ea8" : "transparent"
            border.color: statusButton.hovered ? "#60a8eaf5" : "transparent"
            border.width: 1
            antialiasing: true
        }
    }
}
