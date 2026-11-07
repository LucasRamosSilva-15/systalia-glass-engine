import QtQuick
import QtQuick.Controls
import Systalia.Engine

/**
 * ValoDockV2.qml — Componente Oficial da Dock VALO Graphics 2.0 (Etapa 04)
 *
 * Encapsula a cápsula óptica (555x125, raio 47), material analítico SDF L2,
 * pipeline Dual-Kawase GPU multipass, iluminação especular zenital de 1.25px,
 * sombra externa suave, física Organic desacoplada e 5 lançadores vetoriais.
 */
Item {
    id: root

    // Dimensões nominais oficiais do conceito Figma
    readonly property real nominalWidth: 555.0
    readonly property real nominalHeight: 125.0
    readonly property real pillRadius: 47.0

    implicitWidth: nominalWidth
    implicitHeight: nominalHeight
    width: nominalWidth
    height: nominalHeight

    // Propriedades de Integração Gráfica e Óptica V2
    property alias backdropSource: backdropProvider.source
    property alias backdropProvider: glassItem.backdropProvider
    property alias opticalContext: glassItem.opticalContext

    // Perfis gráficos: 0 = Performance, 1 = Balanced, 2 = Quality
    property int blurProfile: 2

    // Modo de Diagnóstico:
    // "H" (0) = Composição Final Completa
    // "A" (1) = Geometria SDF (Campo analítico)
    // "B" (2) = Máscara analítica da cápsula
    // "C" (3) = Preenchimento volumétrico oceânico
    // "D" (4) = Refração e normais do menisco
    // "E" (5) = Blur Dual-Kawase GPU filtrado
    // "F" (6) = Contorno especular de 1.25px
    // "G" (7) = Sombra externa geométrica isolada
    property string debugPass: "H"
    property int debugMode: {
        if (debugPass === "A") return 1; // Normais/SDF (valo_diag_sdf)
        if (debugPass === "B") return 2; // Cobertura/Máscara (valo_diag_mask)
        if (debugPass === "C") return 3; // Tonalização volumétrica (valo_diag_volume)
        if (debugPass === "D") return 4; // Vetores de Refração (valo_diag_refraction)
        if (debugPass === "E") return 5; // Blur GPU amostrado (valo_diag_blur)
        if (debugPass === "F") return 6; // Contorno especular isolado (valo_diag_specular)
        return 0; // H = Composição final (valo_optical_v2)
    }

    // Estado Forçado para Testes e Diagnóstico Automatizado (0 = Normal, 1..7 = Estados canônicos)
    // 1: Idle, 2: Hover, 3: Press, 4: Active, 5: Background Running, 6: Focused, 7: Reduced Motion / High Contrast
    property int testState: 0

    // Acessibilidade e Adaptação
    property bool userPrefersReducedMotion: false
    property bool userHighContrast: false
    property bool prefersReducedMotion: userPrefersReducedMotion || (testState === 7)
    property bool highContrast: userHighContrast || (testState === 7)

    // Sinais de Ação
    signal filesRequested()
    signal audioRequested()
    signal terminalRequested()
    signal ideRequested()
    signal settingsRequested()
    signal launcherActivated(string action)

    // Provedor de Backdrop V2 com desfoque GPU real
    ValoBackdropProviderV2 {
        id: backdropProvider
        blurProfile: root.blurProfile
    }

    // Contexto óptico adaptativo
    ValoOpticalContext {
        id: defaultOpticalContext
        ambientLuminance: root.highContrast ? 0.95 : 0.35
        adaptiveOpacityOffset: root.highContrast ? 0.15 : 0.0
        highContrastEnabled: root.highContrast || (root.testState === 7)
        prefersReducedMotion: root.prefersReducedMotion || (root.testState === 7)
    }

    // 1. Sombra Externa Analítica VALO L2 (Offset Y=12px, Blur sigma=20px, rgba(0, 13, 33, 0.35))
    // Renderizada via shader analítico contínuo F(d_shadow, sigma) derivado da geometria SDF L2 nominal
    ValoShadowItemV2 {
        id: externalShadow
        anchors.fill: parent
        cornerRadii: Qt.vector4d(root.pillRadius, root.pillRadius, root.pillRadius, root.pillRadius)
        shadowOffsetY: 12.0
        shadowIntensity: 0.35
        isolatedPass: (root.debugPass === "G")
        visible: root.debugPass === "H" || root.debugPass === "G"
        z: 0
    }

    // 2. Chassi Óptico Unificado VALO Glass V2 (Material analítico SDF, refração, dispersão e contorno)
    ValoGlassItemV2 {
        id: glassItem
        anchors.fill: parent
        z: 5
        visible: root.debugPass !== "G" // No modo G (Sombra isolada), oculta o chassi

        cornerRadii: Qt.vector4d(root.pillRadius, root.pillRadius, root.pillRadius, root.pillRadius)
        geometryMode: ValoGlassItemV2.CircularL2
        specularStrokeWidth: 1.25
        surfaceOpacity: root.highContrast ? 0.82 : 0.34
        volumetricTint: root.highContrast
                        ? Qt.rgba(0.01, 0.06, 0.12, 0.92)
                        : Qt.rgba(0.036, 0.153, 0.255, 0.39)
        active: true
        debugMode: root.debugMode
        slant: 0.0
        ior: (root.blurProfile === 2) ? 1.25 : 1.15
        chromaticDispersion: (root.blurProfile === 2) ? 0.040 : 0.020
        causticIntensity: (root.blurProfile === 2) ? 0.18 : 0.05
        blurProfile: root.blurProfile
        backdropProvider: backdropProvider
        opticalContext: defaultOpticalContext
    }

    // 3. Fila dos Cinco Lançadores Vetoriais Oficiais do Figma
    Item {
        id: launcherRow
        anchors.fill: parent
        z: 10
        visible: root.debugPass === "H" // Visível na composição final

        readonly property var launchersData: [
            { action: "files",    label: "Arquivos",         icon: "valo-dock-folder.svg",   relX: 33.75 },
            { action: "audio",    label: "Áudio",            icon: "valo-dock-speaker.svg",  relX: 128.75 },
            { action: "terminal", label: "Terminal",         icon: "valo-dock-terminal.svg", relX: 222.75 },
            { action: "ide",      label: "Editor de Código", icon: "valo-dock-code.svg",     relX: 316.75 },
            { action: "settings", label: "Systalia Core",    icon: "valo-dock-spark.svg",    relX: 410.75 }
        ]

        Repeater {
            id: launcherRepeater
            model: launcherRow.launchersData

            delegate: ValoDockItemV2 {
                id: itemDelegate
                required property var modelData
                required property int index

                x: modelData.relX
                y: 23.5 // Centralizado verticalmente na cápsula de 125px: (125 - 78) / 2
                iconSize: 78.0
                iconSource: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + modelData.icon
                label: modelData.label
                action: modelData.action
                prefersReducedMotion: root.prefersReducedMotion || (root.testState === 7)

                // Simulação e roteamento de estados para validação dos 7 estados canônicos
                forceHovered: (root.testState === 2 && index === 2) // Terminal com hover no estado 2
                forcePressed: (root.testState === 3 && index === 2) // Terminal com press no estado 3
                forceActive:  (root.testState === 4 && index === 0) || (root.testState === 7 && index === 0) // Files ativo
                forceBackgroundRunning: (root.testState === 5 && (index === 1 || index === 3)) // Áudio e IDE em segundo plano
                isFocused: (root.testState === 6 && index === 2) || activeFocus
                showKeyboardFocus: (root.testState === 6 && index === 2) || activeFocus

                onActivated: {
                    root.launcherActivated(modelData.action);
                    if (modelData.action === "files") root.filesRequested();
                    else if (modelData.action === "audio") root.audioRequested();
                    else if (modelData.action === "terminal") root.terminalRequested();
                    else if (modelData.action === "ide") root.ideRequested();
                    else if (modelData.action === "settings") root.settingsRequested();
                }
            }
        }
    }

    // Função auxiliar de acesso ao lançador por índice para testes unitários
    function itemAt(index) {
        return launcherRepeater.itemAt(index);
    }
}

