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

    readonly property int dockItemCount: launchers.length
    readonly property real dockWidth: width
    readonly property real dockHeight: height

    width: 555
    height: 125
    z: 95

    signal filesRequested()
    signal audioRequested()
    signal terminalRequested()
    signal ideRequested()
    signal settingsRequested()

    // Sombra de oclusão exterior suave VALO (box-shadow: 0px 12px 20px rgba(0, 13, 33, 0.35))
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 12
        anchors.bottomMargin: -6
        radius: dock.pillRadius
        color: ValoTheme.withAlpha(ValoTheme.primitiveBlack, 0.35)
        z: -2
    }

    // Glow difuso periférico ciano
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: dock.pillRadius + 1
        color: "transparent"
        border.color: ValoTheme.withAlpha(ValoTheme.accentPrimary, 0.12)
        border.width: 1
        z: -1
    }

    readonly property var launchers: [
        { action: "files", label: "Systalia Files", icon: "valo-dock-folder.svg", appIds: ["systalia-files"], relX: 33.75 },
        { action: "audio", label: "Áudio", icon: "valo-dock-speaker.svg", appIds: ["pavucontrol", "pwvucontrol", "org.pulseaudio.pavucontrol"], relX: 128.75 },
        { action: "terminal", label: "Terminal", icon: "valo-dock-terminal.svg", appIds: ["alacritty", "foot"], relX: 222.75 },
        { action: "ide", label: "Editor de código", icon: "valo-dock-code.svg", appIds: ["code", "codium", "com.visualstudio.code"], relX: 316.75 },
        { action: "settings", label: "Systalia Core", icon: "valo-dock-spark.svg", appIds: ["systalia-core"], relX: 410.75 }
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

    function isFocused(appIds) {
        if (appIds.length === 1 && appIds[0] === "systalia-files")
            return filesVisible
        if (appIds.length === 1 && appIds[0] === "systalia-core")
            return settingsOpen

        if (!activeAppIds || activeAppIds.length === 0)
            return false

        for (var i = 0; i < appIds.length; ++i) {
            var expected = String(appIds[i]).toLowerCase()
            for (var j = 0; j < activeAppIds.length; ++j) {
                if (String(activeAppIds[j]).toLowerCase() === expected)
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

    // Camada de vidro óptico (motor de dispersão e cáusticas)
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

    // Moldura orgânica VALO Glass com contorno especular de 1.25px e gradiente a 167.3°
    SystaliaG2Frame {
        anchors.fill: parent
        radii: dock.cornerRadii
        fillGradient: LinearGradient {
            x1: 0
            y1: 0
            x2: dock.width * 0.97
            y2: dock.height * 0.22
            GradientStop { position: 0.0714; color: ValoTheme.withAlpha(ValoTheme.ocean400, 0.29) }
            GradientStop { position: 0.3571; color: ValoTheme.withAlpha(ValoTheme.ocean600, 0.34) }
            GradientStop { position: 0.7857; color: ValoTheme.withAlpha(ValoTheme.backgroundDesktop, 0.39) }
        }
        strokeColor: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.38)
        strokeWidth: 1.25
    }

    // Inset highlight superior (inset 0px 2px 2px rgba(255, 255, 255, 0.25))
    Rectangle {
        x: dock.pillRadius
        y: 1.5
        width: parent.width - 2 * dock.pillRadius
        height: 1.5
        radius: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.2; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.25) }
            GradientStop { position: 0.8; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.25) }
            GradientStop { position: 1.0; color: "transparent" }
        }
        antialiasing: true
    }

    // Inset shadow inferior suave (inset 0px -3px 5px rgba(6, 19, 38, 0.35))
    Rectangle {
        x: dock.pillRadius
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        width: parent.width - 2 * dock.pillRadius
        height: 3
        radius: 1.5
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: ValoTheme.withAlpha(ValoTheme.primitiveBlack, 0.30) }
        }
        antialiasing: true
    }

    // Contêiner dos cinco itens da Dock com coordenadas originais Figma
    Item {
        id: itemsContainer
        objectName: "dockItemsContainer"
        anchors.fill: parent
        z: 2

        Repeater {
            model: dock.launchers

            delegate: ValoDockItem {
                id: dockItemDelegate
                objectName: "dockItem_" + modelData.action
                required property var modelData
                required property int index
                x: modelData.relX
                y: 13
                iconSource: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + modelData.icon
                label: modelData.label
                action: modelData.action
                appIds: modelData.appIds
                isRunning: dock.isRunning(modelData.appIds)
                isFocused: dock.isFocused(modelData.appIds)
                onActivated: dock.activate(modelData.action)
            }
        }
    }
}
