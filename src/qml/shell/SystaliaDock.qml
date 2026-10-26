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
    property bool settingsOpen: true
    property real refractiveIndex: 1.25
    property real dispersion: 0.04
    property real caustics: 0.65
    property real surfaceOpacity: 0.88
    property bool materialActive: true
    readonly property real pillRadius: height / 2.0

    width: 555
    height: 125
    z: 95

    signal filesRequested()
    signal audioRequested()
    signal terminalRequested()
    signal ideRequested()
    signal settingsRequested()

    readonly property var launchers: [
        { action: "files", label: "Systalia Files", icon: "dock-folder.svg", appIds: ["systalia-files"] },
        { action: "audio", label: "Áudio", icon: "dock-speaker.svg", appIds: ["pavucontrol", "pwvucontrol", "org.pulseaudio.pavucontrol"] },
        { action: "terminal", label: "Terminal", icon: "dock-terminal.svg", appIds: ["alacritty", "foot"] },
        { action: "ide", label: "Editor de código", icon: "dock-code.svg", appIds: ["code", "codium", "com.visualstudio.code"] },
        { action: "settings", label: "Systalia Core", icon: "dock-spark.svg", appIds: ["systalia-core"] }
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
        if (action === "files") filesRequested()
        else if (action === "audio") audioRequested()
        else if (action === "terminal") terminalRequested()
        else if (action === "ide") ideRequested()
        else if (action === "settings") settingsRequested()
    }

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: dock.backdropSource
        screenResolution: dock.screenResolution
        radii: Qt.vector4d(dock.pillRadius, dock.pillRadius, dock.pillRadius, dock.pillRadius)
        slant: 0
        ior: dock.refractiveIndex
        chromaticDispersion: dock.dispersion
        causticIntensity: dock.caustics
        surfaceOpacity: dock.materialActive ? dock.surfaceOpacity : 0
        time: dock.animationTime
    }

    Rectangle {
        anchors.fill: parent
        radius: dock.pillRadius
        color: "#2f0a213a"
        border.color: "#9b8fcce0"
        border.width: 1.2
        antialiasing: true
    }

    Rectangle {
        x: 25
        y: 9
        width: parent.width - 50
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: .18; color: "#78edffff" }
            GradientStop { position: .82; color: "#78edffff" }
            GradientStop { position: 1; color: "transparent" }
        }
        antialiasing: true
    }

    Row {
        id: iconsRow
        x: 35
        y: 22
        spacing: 17
        z: 2

        Repeater {
            model: dock.launchers

            delegate: Item {
                id: launcher
                required property var modelData
                width: 78
                height: 88
                scale: pointer.hovered ? 1.08 : 1.0

                Behavior on scale {
                    NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
                }

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    width: 78
                    height: 78
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + launcher.modelData.icon
                    sourceSize: Qt.size(78, 78)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: 15
                    height: 3
                    radius: 2
                    visible: dock.isRunning(launcher.modelData.appIds)
                    color: "#70f5f2"
                    opacity: .95
                    antialiasing: true
                }

                HoverHandler { id: pointer }
                ToolTip.visible: pointer.hovered
                ToolTip.text: launcher.modelData.label
                ToolTip.delay: 400

                TapHandler { onTapped: dock.activate(launcher.modelData.action) }
            }
        }
    }
}
