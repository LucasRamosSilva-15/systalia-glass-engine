import QtQuick
import QtQuick.Controls
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: dock

    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property var activeAppIds: []
    property var launchedAppIds: ProcessLauncher.runningApplications
    property real animationTime: 0.0
    property bool filesVisible: true
    property bool settingsOpen: false
    readonly property int itemExtent: 58
    readonly property real pillRadius: height / 2.0

    width: iconsRow.implicitWidth + pillRadius * 1.5
    height: 64
    z: 95

    signal filesRequested()
    signal audioRequested()
    signal terminalRequested()
    signal ideRequested()
    signal settingsRequested()

    readonly property var launchers: [
        { action: "files", label: "Systalia Files", glyph: "▰", accent: "#22d3ee", appIds: ["systalia-files"] },
        { action: "audio", label: "Áudio", glyph: "♫", accent: "#38bdf8", appIds: ["pavucontrol", "pwvucontrol", "org.pulseaudio.pavucontrol"] },
        { action: "terminal", label: "Terminal", glyph: ">_", accent: "#00f2fe", appIds: ["alacritty", "foot"] },
        { action: "ide", label: "Editor de código", glyph: "</>", accent: "#818cf8", appIds: ["code", "codium", "com.visualstudio.code"] },
        { action: "settings", label: "Systalia Core", glyph: "✦", accent: "#f0abfc", appIds: ["systalia-core"] }
    ]

    function isRunning(appIds) {
        if (appIds.length === 1 && appIds[0] === "systalia-files")
            return filesVisible
        if (appIds.length === 1 && appIds[0] === "systalia-core")
            return settingsOpen

        for (var i = 0; i < appIds.length; ++i) {
            var expected = String(appIds[i]).toLowerCase()
            for (var j = 0; j < activeAppIds.length; ++j) {
                if (String(activeAppIds[j]).toLowerCase() === expected)
                    return true
            }
            for (var k = 0; k < launchedAppIds.length; ++k) {
                if (String(launchedAppIds[k]).toLowerCase() === expected)
                    return true
            }
        }
        return false
    }

    function activate(action) {
        if (action === "files")
            filesRequested()
        else if (action === "audio")
            audioRequested()
        else if (action === "terminal")
            terminalRequested()
        else if (action === "ide")
            ideRequested()
        else if (action === "settings")
            settingsRequested()
    }

    SystaliaGlassItem {
        id: glassCapsule
        anchors.fill: parent
        z: 0
        backdropSource: dock.backdropSource
        screenResolution: dock.screenResolution
        radii: Qt.vector4d(dock.pillRadius, dock.pillRadius,
                            dock.pillRadius, dock.pillRadius)
        slant: 0.0
        ior: 1.25
        chromaticDispersion: 0.038
        causticIntensity: 0.65
        surfaceOpacity: 0.88
        time: dock.animationTime
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: dock.pillRadius * 0.75
        anchors.rightMargin: dock.pillRadius * 0.75
        anchors.top: parent.top
        anchors.topMargin: 10
        height: 1
        z: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.18; color: "#48efffff" }
            GradientStop { position: 0.82; color: "#48efffff" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Row {
        id: iconsRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: dock.pillRadius * 0.75
        anchors.rightMargin: dock.pillRadius * 0.75
        anchors.top: parent.top
        anchors.topMargin: 4
        spacing: 12
        z: 2

        Repeater {
            model: dock.launchers

            delegate: Item {
                id: launcher
                required property var modelData
                width: dock.itemExtent
                height: 60
                scale: pointer.hovered ? 1.14 : 1.0

                Behavior on scale {
                    NumberAnimation {
                        duration: 150
                        easing.type: Easing.OutCubic
                    }
                }

                Rectangle {
                    id: iconTile
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    width: 44
                    height: 44
                    radius: 14
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: Qt.lighter(launcher.modelData.accent, 1.38) }
                        GradientStop { position: 0.44; color: launcher.modelData.accent }
                        GradientStop { position: 1.0; color: "#073b61" }
                    }
                    border.color: pointer.hovered ? "#d0ffffff" : "#80ffffff"
                    border.width: 1

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 2
                        height: 18
                        radius: 11
                        color: "#42ffffff"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: launcher.modelData.glyph
                        color: "#f4fdff"
                        font.pixelSize: launcher.modelData.action === "terminal" ? 17 : 21
                        font.bold: true
                        font.family: launcher.modelData.action === "terminal"
                                     || launcher.modelData.action === "ide" ? "Monospace" : "sans-serif"
                        style: Text.Outline
                        styleColor: "#70020a18"
                    }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: iconTile.bottom
                    anchors.topMargin: 5
                    width: 16
                    height: 3
                    radius: 2
                    visible: dock.isRunning(launcher.modelData.appIds)
                    color: "#00f0ff"
                    opacity: 0.95

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: "#9000f0ff"
                        scale: 1.8
                        opacity: 0.3
                    }
                }

                HoverHandler {
                    id: pointer
                }

                ToolTip.visible: pointer.hovered
                ToolTip.text: launcher.modelData.label
                ToolTip.delay: 400

                MouseArea {
                    anchors.fill: parent
                    onClicked: dock.activate(launcher.modelData.action)
                }
            }
        }
    }
}
