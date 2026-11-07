import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: dock

    // =========================================================================
    // 1. CONTRATOS FUNCIONAIS E PROPRIEDADES DA SHELL
    // =========================================================================
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property var activeAppIds: []
    property var launchedAppIds: ProcessLauncher.runningApplications
    property real animationTime: 0.0
    property bool filesVisible: true
    property bool settingsOpen: true
    property string performanceProfile: "balanced"
    property real refractiveIndex: 1.25
    property real dispersion: 0.040
    property real caustics: 0.65
    property real surfaceOpacity: ValoTheme.opacityDock
    property bool materialActive: true
    readonly property real pillRadius: ValoTheme.radiusDock
    property vector4d cornerRadii: Qt.vector4d(pillRadius, pillRadius, pillRadius, pillRadius)
    property bool keyboardNavActive: false

    readonly property int dockItemCount: launchers.length
    readonly property real dockWidth: width
    readonly property real dockHeight: height
    readonly property alias valoGlassInstance: valoGlass

    width: 555
    height: 125
    z: 95

    signal filesRequested()
    signal audioRequested()
    signal terminalRequested()
    signal ideRequested()
    signal settingsRequested()

    // Suporte a detecção de navegação por teclado vs ponteiro
    Keys.onTabPressed: (event) => {
        dock.keyboardNavActive = true
        event.accepted = false
    }
    Keys.onBacktabPressed: (event) => {
        dock.keyboardNavActive = true
        event.accepted = false
    }
    HoverHandler {
        onHoveredChanged: if (hovered) dock.keyboardNavActive = false
    }
    TapHandler {
        onTapped: dock.keyboardNavActive = false
    }

    // =========================================================================
    // 2. MODELO DE INTERAÇÃO (DockInteractionModel)
    // =========================================================================
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

    function dockItemAt(index) {
        return launcherRepeater.itemAt(index)
    }

    // =========================================================================
    // 3. RAIZ VISUAL (DockVisualRoot)
    // =========================================================================
    Item {
        id: dockVisualRoot
        objectName: "dockVisualRoot"
        anchors.fill: parent

        // GlassChassis — Único elemento responsável pelo chassi principal
        Item {
            id: glassChassis
            objectName: "glassChassis"
            anchors.fill: parent

            // Sombra externa independente de oclusão (projetada suavemente sob a base)
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -12
                width: parent.width - 24
                height: 36
                radius: 18
                z: 0
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.35; color: ValoTheme.withAlpha(ValoTheme.primitiveBlack, 0.28) }
                    GradientStop { position: 0.70; color: ValoTheme.withAlpha(ValoTheme.primitiveBlack, 0.14) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            // Gradiente oceânico VALO da Dock calibrado conforme Figma
            LinearGradient {
                id: dockOceanicGradient
                x1: 0
                y1: 0
                x2: dock.width
                y2: dock.height * 0.85
                GradientStop { position: 0.00; color: "#4A5D9FB8" } // #5d9fb8 @ 0.29
                GradientStop { position: 0.29; color: "#4A5D9FB8" }
                GradientStop { position: 0.34; color: "#57194C70" } // #194c70 @ 0.34
                GradientStop { position: 1.00; color: "#630A2741" } // #0a2741 @ 0.39
            }

            // Chassi de vidro óptico unificado VALO Glass
            ValoGlass {
                id: valoGlass
                objectName: "valoGlassChassis"
                anchors.fill: parent
                z: 1
                showShadow: false
                cornerRadius: dock.pillRadius
                circularCorners: true
                strokeWidth: 1.25
                fillGradient: dockOceanicGradient
                glassOpacity: dock.materialActive ? dock.surfaceOpacity : 0.0
                backdropSource: dock.backdropSource
                screenResolution: dock.screenResolution
                time: dock.animationTime
                performanceProfile: dock.performanceProfile
                chromaticDispersion: dock.dispersion
                refractionStrength: (dock.refractiveIndex - 1.0) / 0.25
                active: true
                borderIntensity: 1.0
                glowIntensity: 1.0
            }
        }

        // LauncherRow — Fila dos 5 lançadores vetoriais com coordenadas Figma
        Item {
            id: launcherRow
            objectName: "launcherRow"
            anchors.fill: parent
            z: 10

            Repeater {
                id: launcherRepeater
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
                    showKeyboardFocus: dock.keyboardNavActive
                    onActivated: dock.activate(modelData.action)
                }
            }
        }
    }
}
