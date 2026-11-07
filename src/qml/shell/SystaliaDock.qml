import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
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
    property real dispersion: 0.040
    property real caustics: 0.65
    property real surfaceOpacity: ValoTheme.opacityDock
    property bool materialActive: true
    readonly property real pillRadius: ValoTheme.radiusDock
    property vector4d cornerRadii: Qt.vector4d(pillRadius, pillRadius, pillRadius, pillRadius)

    width: 555
    height: 125
    z: 95

    signal filesRequested()
    signal audioRequested()
    signal terminalRequested()
    signal ideRequested()
    signal settingsRequested()

    // Sombra de queda base conforme style.css: box-shadow: 0px 12px 20px rgba(0, 13, 33, 0.35)
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 12
        radius: dock.pillRadius
        color: ValoTheme.withAlpha(ValoTheme.primitiveBlack, 0.35)
        z: -1
    }

    readonly property var launchers: [
        { action: "files", label: "Systalia Files", icon: "valo-dock-folder.png", appIds: ["systalia-files"] },
        { action: "audio", label: "Áudio", icon: "valo-dock-speaker.png", appIds: ["pavucontrol", "pwvucontrol", "org.pulseaudio.pavucontrol"] },
        { action: "terminal", label: "Terminal", icon: "valo-dock-terminal.png", appIds: ["alacritty", "foot"] },
        { action: "ide", label: "Editor de código", icon: "valo-dock-code.png", appIds: ["code", "codium", "com.visualstudio.code"] },
        { action: "settings", label: "Systalia Core", icon: "valo-dock-spark.png", appIds: ["systalia-core"] }
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
        radii: dock.cornerRadii
        circularCorners: true
        slant: 0
        ior: dock.refractiveIndex
        chromaticDispersion: dock.dispersion
        causticIntensity: dock.caustics
        surfaceOpacity: dock.materialActive ? dock.surfaceOpacity : 0
        time: dock.animationTime
    }

    SystaliaG2Frame {
        anchors.fill: parent
        radii: dock.cornerRadii
        // Gradiente a 135deg conforme style.css e tokens VALO
        fillGradient: LinearGradient {
            x1: 0
            y1: 0
            x2: dock.width
            y2: dock.height
            GradientStop { position: 0.0714; color: ValoTheme.withAlpha(ValoTheme.ocean400, 0.29) }
            GradientStop { position: 0.3571; color: ValoTheme.withAlpha(ValoTheme.ocean600, 0.34) }
            GradientStop { position: 0.7857; color: ValoTheme.withAlpha(ValoTheme.backgroundDesktop, 0.39) }
        }
        strokeColor: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.18)
        strokeWidth: ValoTheme.strokeFine
    }

    // Inset highlight superior conforme style.css (inset 0px 2px 2px rgba(255, 255, 255, 0.25))
    Rectangle {
        x: dock.pillRadius
        y: 2
        width: parent.width - 2 * dock.pillRadius
        height: 2
        radius: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 0.2; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.25) }
            GradientStop { position: 0.8; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.25) }
            GradientStop { position: 1; color: "transparent" }
        }
        antialiasing: true
    }

    // Inset shadow inferior conforme style.css (inset 0px -3px 5px rgba(6, 19, 38, 0.35))
    Rectangle {
        x: dock.pillRadius
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        width: parent.width - 2 * dock.pillRadius
        height: 4
        radius: 2
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 1; color: ValoTheme.withAlpha(ValoTheme.backgroundDesktop, 0.35) }
        }
        antialiasing: true
    }

    Row {
        id: iconsRow
        x: (parent.width - implicitWidth) / 2
        y: 21.75
        spacing: 16
        z: 2

        Repeater {
            model: dock.launchers

            delegate: Item {
                id: launcher
                required property var modelData
                readonly property bool activeRunning: dock.isRunning(launcher.modelData.appIds)
                width: ValoTheme.sizeDockIcon
                height: ValoTheme.sizeDockIcon + 8
                scale: pointer.hovered ? 1.08 : 1.0

                Behavior on scale {
                    NumberAnimation {
                        duration: ValoTheme.motionDurationFastMs
                        easing.type: Easing.OutCubic
                    }
                }

                Image {
                    id: iconImg
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    width: ValoTheme.sizeDockIcon
                    height: ValoTheme.sizeDockIcon
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + launcher.modelData.icon
                    sourceSize: Qt.size(312, 312)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                // Indicador luminoso de aplicativo aberto/ativo (dot ciano de 4px)
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: iconImg.bottom
                    anchors.topMargin: 2
                    width: launcher.activeRunning ? 16 : 0
                    height: 3
                    radius: 1.5
                    color: ValoTheme.accentBright
                    visible: launcher.activeRunning
                    opacity: pointer.hovered ? 1.0 : 0.85
                    antialiasing: true

                    Behavior on width {
                        NumberAnimation {
                            duration: ValoTheme.motionDurationFastMs
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                HoverHandler { id: pointer }

                ToolTip.visible: pointer.hovered
                ToolTip.text: launcher.modelData.label
                ToolTip.delay: 350

                TapHandler { onTapped: dock.activate(launcher.modelData.action) }
            }
        }
    }
}
