import QtQuick
import QtQuick.Controls
import Systalia.Engine
import Systalia.System 1.0
import "../graphics/v2" as GfxV2

/**
 * SystaliaDockV2.qml — Componente Oficial da Dock V2 integrado à Shell SYSTALIA (Etapa 05)
 *
 * Encapsula a ValoDockV2 conectando-a aos serviços da Shell:
 * - ProcessLauncherService (aplicativos em execução)
 * - SystaliaCompositor (activeAppIds / janelas ativas)
 * - WorkspaceManager (espaços de trabalho)
 * - Sinais e slots de controle de janelas
 */
Item {
    id: root

    // =========================================================================
    // 1. CONTRATOS FUNCIONAIS E PROPRIEDADES DA SHELL
    // =========================================================================
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property var activeAppIds: []
    property var launchedAppIds: (typeof ProcessLauncher !== "undefined") ? ProcessLauncher.runningApplications : []
    property real animationTime: 0.0
    property bool filesVisible: true
    property bool settingsOpen: true
    property string performanceProfile: "quality"
    property real refractiveIndex: 1.25
    property real dispersion: 0.040
    property real caustics: 0.18
    property real surfaceOpacity: ValoTheme.opacityDock
    property bool materialActive: true
    readonly property real pillRadius: 47.0
    property vector4d cornerRadii: Qt.vector4d(pillRadius, pillRadius, pillRadius, pillRadius)
    property bool keyboardNavActive: false

    readonly property real dockWidth: 555.0
    readonly property real dockHeight: 125.0
    readonly property int dockItemCount: 5

    readonly property var launchers: [
        { action: "files", label: "Systalia Files", icon: "valo-dock-folder.svg", appIds: ["systalia-files"], relX: 33.75 },
        { action: "audio", label: "Áudio", icon: "valo-dock-speaker.svg", appIds: ["pavucontrol", "pwvucontrol", "org.pulseaudio.pavucontrol"], relX: 128.75 },
        { action: "terminal", label: "Terminal", icon: "valo-dock-terminal.svg", appIds: ["alacritty", "foot"], relX: 222.75 },
        { action: "ide", label: "Editor de Código", icon: "valo-dock-code.svg", appIds: ["code", "codium", "com.visualstudio.code"], relX: 316.75 },
        { action: "settings", label: "Systalia Core", icon: "valo-dock-spark.svg", appIds: ["systalia-core"], relX: 410.75 }
    ]

    readonly property var valoGlassInstance: ({
        circularCorners: true,
        cornerRadius: 47.0,
        glassOpacity: 0.34,
        showShadow: false
    })

    // Perfil de desfoque GPU (0 = Performance, 1 = Balanced, 2 = Quality)
    property int blurProfile: {
        if (performanceProfile === "performance") return 0;
        if (performanceProfile === "balanced") return 1;
        return 2; // quality
    }

    // Acessibilidade
    property bool prefersReducedMotion: false
    property bool highContrast: false

    implicitWidth: 555
    implicitHeight: 125
    width: 555
    height: 125
    z: 95

    signal filesRequested()
    signal audioRequested()
    signal terminalRequested()
    signal ideRequested()
    signal settingsRequested()

    // Instância oficial da Dock V2 com motor óptico analítico
    GfxV2.ValoDockV2 {
        id: valoDockInstance
        anchors.fill: parent

        backdropSource: root.backdropSource
        blurProfile: root.blurProfile
        activeAppIds: root.activeAppIds
        launchedAppIds: root.launchedAppIds
        filesVisible: root.filesVisible
        settingsOpen: root.settingsOpen
        keyboardNavActive: root.keyboardNavActive
        userPrefersReducedMotion: root.prefersReducedMotion
        userHighContrast: root.highContrast

        onFilesRequested: root.filesRequested()
        onAudioRequested: root.audioRequested()
        onTerminalRequested: root.terminalRequested()
        onIdeRequested: root.ideRequested()
        onSettingsRequested: root.settingsRequested()
    }

    function dockItemAt(index) {
        return valoDockInstance.itemAt(index);
    }

    function isRunning(appIds) {
        return valoDockInstance.isRunning(appIds);
    }

    function isFocused(appIds) {
        return valoDockInstance.isFocused(appIds);
    }

    readonly property alias valoDockInstance: valoDockInstance
}
