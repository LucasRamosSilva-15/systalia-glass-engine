import QtQuick
import Systalia.Engine

/**
 * ValoMaterialSurfaceV2.qml — Superfície Base Unificada do VALO Graphics 2.0 (Etapa 05)
 *
 * Encapsula a infraestrutura óptica completa:
 * - ValoGlassItemV2 (Material analítico SDF, refração Cauchy, dispersão, contorno especular unificado)
 * - ValoShadowItemV2 (Sombra analítica contínua F(d_shadow, sigma) sem oclusão duplicada)
 * - ValoBackdropProviderV2 (Pipeline Dual-Kawase GPU multipass no beforeRendering)
 * - ValoOpticalContext (Políticas adaptativas: contraste, luminância, reduced motion)
 *
 * Garante as regras arquiteturais:
 * 1. Uma única definição geométrica por superfície (Circular L2 ou Squircle G2).
 * 2. Uma única borda especular sem sobreposição de retângulos.
 * 3. Máscaras analíticas compartilhadas via std140 UBO.
 * 4. Nenhum ShapePath ou blur CPU redundante.
 */
Item {
    id: root

    // =========================================================================
    // 1. PROPRIEDADES GEOMÉTRICAS E ÓPTICAS V2
    // =========================================================================
    property int geometryMode: ValoGlassItemV2.CircularL2
    property real cornerRadius: 20.0
    property vector4d cornerRadii: Qt.vector4d(cornerRadius, cornerRadius, cornerRadius, cornerRadius)
    property real surfaceOpacity: 0.34
    property real specularStrokeWidth: 1.25
    property color volumetricTint: Qt.rgba(0.036, 0.153, 0.255, 0.39)
    property real slant: 0.0

    // Perfis gráficos: 0 = Performance, 1 = Balanced, 2 = Quality
    property int blurProfile: 2

    // Constantes físicas calibradas por perfil
    property real ior: (blurProfile === 2) ? 1.25 : 1.15
    property real chromaticDispersion: (blurProfile === 2) ? 0.040 : 0.020
    property real causticIntensity: (blurProfile === 2) ? 0.18 : 0.05
    property bool active: true
    // Independent suspended-light overlay; never changes caustic intensity or UBO ABI.
    property real particleIntensity: 0.35
    readonly property real particleSeed: Math.random() * 1000

    // =========================================================================
    // 2. SOMBRA ANALÍTICA EXTERNA
    // =========================================================================
    property bool showShadow: true
    property real shadowOffsetY: 12.0
    property real shadowIntensity: 0.35

    // =========================================================================
    // 3. DIAGNÓSTICO E AUDITORIA DE PASSES
    // =========================================================================
    property string debugPass: "H"
    property int debugMode: {
        if (debugPass === "A") return 1; // SDF analítica
        if (debugPass === "B") return 2; // Máscara de cobertura
        if (debugPass === "C") return 3; // Volume oceânico
        if (debugPass === "D") return 4; // Vetores de refração
        if (debugPass === "E") return 5; // Blur GPU amostrado
        if (debugPass === "F") return 6; // Borda especular isolada
        if (debugPass === "I") return 7; // Cáusticas isoladas
        if (debugPass === "J") return 8; // Reflexão prismática isolada
        if (debugPass === "K") return 9; // Material básico (sem cáusticas/reflexões)
        return 0; // H = Composição final completa
    }

    // =========================================================================
    // 4. ACESSIBILIDADE E POLÍTICAS ADAPTATIVAS (Adaptive & Lucid)
    // =========================================================================
    property bool userHighContrast: false
    property bool userPrefersReducedMotion: false
    property bool highContrast: userHighContrast
    property bool prefersReducedMotion: userPrefersReducedMotion

    // Contexto óptico adaptativo
    property ValoOpticalContext externalOpticalContext: null
    readonly property ValoOpticalContext effectiveOpticalContext: externalOpticalContext ? externalOpticalContext : internalOpticalContext

    ValoOpticalContext {
        id: internalOpticalContext
        ambientLuminance: root.highContrast ? 0.95 : 0.35
        adaptiveOpacityOffset: root.highContrast ? 0.15 : 0.0
        highContrastEnabled: root.highContrast
        prefersReducedMotion: root.prefersReducedMotion
    }

    // =========================================================================
    // 5. PROVEDOR DE BACKDROP E DESFOQUE DUAL-KAWASE GPU
    // =========================================================================
    property var backdropSource: null
    property ValoBackdropProviderV2 externalBackdropProvider: null
    readonly property ValoBackdropProviderV2 effectiveBackdropProvider: externalBackdropProvider ? externalBackdropProvider : internalBackdropProvider

    ValoBackdropProviderV2 {
        id: internalBackdropProvider
        source: root.backdropSource
        blurProfile: root.blurProfile
    }

    // =========================================================================
    // 6. CAMADAS GRÁFICAS DO SCENE GRAPH
    // =========================================================================

    // Sombra Externa Analítica VALO L2/G2 (Offset Y, sigma 20px, rgba(0, 13, 33, 0.35))
    ValoShadowItemV2 {
        id: shadowItem
        anchors.fill: parent
        cornerRadii: root.cornerRadii
        geometryMode: root.geometryMode
        shadowOffsetY: root.shadowOffsetY
        shadowIntensity: root.highContrast ? 0.0 : root.shadowIntensity
        isolatedPass: (root.debugPass === "G")
        visible: root.showShadow && (root.debugPass === "H" || root.debugPass === "G")
        z: 0
    }

    // Chassi de Vidro Óptico Analítico VALO Glass V2
    ValoGlassItemV2 {
        id: glassItem
        anchors.fill: parent
        z: 5
        visible: root.debugPass !== "G" && root.debugPass !== "L" // Oculta o chassi no passe G (sombra isolada)

        cornerRadii: root.cornerRadii
        geometryMode: root.geometryMode
        specularStrokeWidth: root.specularStrokeWidth
        surfaceOpacity: root.highContrast
                        ? Math.min(1.0, root.surfaceOpacity + 0.35)
                        : root.surfaceOpacity
        volumetricTint: root.highContrast
                        ? Qt.rgba(0.01, 0.06, 0.12, 0.92)
                        : root.volumetricTint
        active: root.active
        debugMode: root.debugMode
        slant: root.slant
        ior: root.ior
        chromaticDispersion: root.chromaticDispersion
        causticIntensity: root.causticIntensity
        particleIntensity: root.particleIntensity
        blurProfile: root.blurProfile
        backdropProvider: root.effectiveBackdropProvider
        opticalContext: root.effectiveOpticalContext
    }

    ValoOpticalParticlesV2 {
        objectName: "valoSuspendedParticlesV2"
        anchors.fill: parent
        z: 6
        visible: root.active && root.blurProfile !== 0
                 && root.particleIntensity > 0 && (root.debugPass === "H" || root.debugPass === "L")
        animationTime: glassItem.animationTime
        intensity: root.particleIntensity
        quality: root.blurProfile
        seedOffset: root.particleSeed
        radii: root.cornerRadii
        geometryMode: root.geometryMode
        slant: root.slant
    }

    // =========================================================================
    // 7. CONTAINER DE CONTEÚDO (Filhos adicionados à superfície)
    // =========================================================================
    default property alias content: contentContainer.data
    readonly property alias contentItem: contentContainer
    readonly property alias glassItemInstance: glassItem
    readonly property alias shadowItemInstance: shadowItem

    Item {
        id: contentContainer
        anchors.fill: parent
        z: 10
        visible: root.debugPass === "H"
    }
}

