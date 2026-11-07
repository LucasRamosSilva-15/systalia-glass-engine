import QtQuick
import QtQuick.Shapes
import Systalia.Engine

Item {
    id: root

    // =========================================================================
    // 1. PROPRIEDADES PRINCIPAIS (Requisitos Etapa 3 e 4)
    // =========================================================================

    // Estado ativo/focado da superfície (controla iluminação, reflexos e opacidade)
    property bool active: true

    // Opacidade da camada de vidro translúcido (conforme tokens window-min e window-max)
    property real glassOpacity: active ? ValoTheme.opacityWindowMax : ValoTheme.opacityWindowMin

    // Raio de blur para o filtro de fundo (px)
    property real blurRadius: ValoTheme.blurBackdrop

    // Raio de curvatura dos cantos (px)
    property real cornerRadius: ValoTheme.radiusWindow

    // Vetor de raios individuais (topo-esq, topo-dir, inf-esq, inf-dir)
    property vector4d cornerRadii: Qt.vector4d(cornerRadius, cornerRadius, cornerRadius, cornerRadius)
    property alias radii: root.cornerRadii

    // Intensidade das bordas especulares
    property real borderIntensity: 1.0

    // Força dos reflexos prismáticos espectrais
    property real iridescenceStrength: 1.0

    // Fator de refração óptica (escala o IOR de 1.0 a 1.25)
    property real refractionStrength: 1.0

    // Dispersão cromática de Cauchy
    property real chromaticDispersion: 0.040

    // Intensidade do brilho/sombra ambiente
    property real glowIntensity: 1.0

    // Perfil gráfico de desempenho: "quality", "balanced", "performance"
    property string performanceProfile: "balanced"

    // =========================================================================
    // 2. PARÂMETROS DE CONTEXTO E ÓPTICA
    // =========================================================================

    // Fonte de backdrop para refração em tempo real
    property var backdropSource: null

    // Fonte de textura do cliente (para janelas Wayland compostas)
    property var clientTextureSource: null

    // Resolução física/screen-space de renderização
    property vector2d screenResolution: Qt.vector2d(width > 0 ? width : 640, height > 0 ? height : 480)

    // Escala HiDPI do ecrã
    property real devicePixelRatio: 1.0

    // Inclinação geométrica analítica (slant 3D)
    property real slant: 0.0

    // Tempo de animação para cáusticas subaquáticas
    property real time: 0.0

    // Altura da barra de título (para recorte interno de clientes)
    property real titleBarHeight: 0.0

    // Alterna entre cantos euclidianos circulares (L2) ou Squircle G2 analítico
    property bool circularCorners: false

    // Controla se a janela está ocluída (otimização de renderização)
    property bool occluded: false

    // Controla a exibição da sombra projetada
    property bool showShadow: true

    // Controla a exibição da camada de prisma espectral
    property bool showPrism: true

    // Ativa transições animadas suaves de geometria
    property bool animateTransitions: true

    // =========================================================================
    // 3. DIMENSÕES E HIERARQUIA DE CONTEÚDO
    // =========================================================================

    width: 480
    height: 320

    // Permite que componentes filhos sejam inseridos diretamente no ValoGlass
    default property alias contentData: contentContainer.data
    property alias contentItem: contentContainer
    property alias glassItem: glassItem

    // =========================================================================
    // 4. CÁLCULO DINÂMICO CONFORME PERFIL DE DESEMPENHO
    // =========================================================================

    readonly property bool isQuality: performanceProfile === "quality"
    readonly property bool isPerformance: performanceProfile === "performance"

    // IOR efetivo mapeado: 1.0 (sem distorção) a 1.25 (refração total de água/vidro)
    readonly property real effectiveIor: {
        if (isPerformance) return 1.0
        if (isQuality) return 1.0 + (0.25 * Math.max(0.0, refractionStrength))
        return 1.0 + (0.15 * Math.max(0.0, refractionStrength))
    }

    // Dispersão cromática efetiva
    readonly property real effectiveDispersion: {
        if (isPerformance) return 0.0
        if (isQuality) return chromaticDispersion
        return chromaticDispersion * 0.65
    }

    // Intensidade das cáusticas ondulantes
    readonly property real effectiveCaustics: {
        if (isPerformance) return 0.0
        const base = isQuality ? 0.85 : 0.50
        return base * (active ? 1.0 : 0.25)
    }

    // Opacidade superficial transmitida ao shader
    readonly property real effectiveSurfaceOpacity: {
        const base = Math.min(1.0, Math.max(0.0, glassOpacity))
        return active ? base : base * 0.75
    }

    readonly property vector4d effectiveRadii: cornerRadii

    Behavior on cornerRadii {
        enabled: root.animateTransitions
        PropertyAnimation {
            duration: ValoTheme.motionDurationNormalMs
            easing.type: Easing.OutCubic
        }
    }

    // =========================================================================
    // 5. CAMADAS VISUAIS (Z-ORDER: Sombra < Óptico < Gradiente < Prisma < Borda < Conteúdo)
    // =========================================================================

    // Camada A: Sombra Externa de Projeção (Drop Shadow + Âncora)
    BorderImage {
        id: dropShadow
        x: -60
        y: -60
        width: parent.width + 120
        height: parent.height + 160
        z: 0
        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-window-shadow.png"
        border.left: 60
        border.top: 60
        border.right: 60
        border.bottom: 100
        horizontalTileMode: BorderImage.Stretch
        verticalTileMode: BorderImage.Stretch
        smooth: true
        opacity: root.glowIntensity * (root.isPerformance ? 0.35 : (root.active ? 1.0 : 0.45))
        visible: root.showShadow && root.glowIntensity > 0.001
    }

    // Camada B: Passo Óptico Nativo (SystaliaGlassItem via SPIR-V)
    SystaliaGlassItem {
        id: glassItem
        anchors.fill: parent
        z: 1

        backdropSource: root.backdropSource
        clientTextureSource: root.clientTextureSource
        screenResolution: root.screenResolution
        devicePixelRatio: root.devicePixelRatio
        titleBarHeight: root.titleBarHeight
        circularCorners: root.circularCorners
        occluded: root.occluded || !root.visible

        radii: root.effectiveRadii
        slant: root.slant
        ior: root.effectiveIor
        chromaticDispersion: root.effectiveDispersion
        causticIntensity: root.effectiveCaustics
        surfaceOpacity: root.effectiveSurfaceOpacity
        time: root.isPerformance ? 0.0 : root.time
    }

    // Camada C: Gradiente Volumétrico do Corpo (windowFillStops do VALO)
    SystaliaG2Frame {
        id: volumetricBodyFrame
        anchors.fill: parent
        z: 2
        radii: root.effectiveRadii
        slant: root.slant
        opacity: Math.min(1.0, root.glassOpacity / ValoTheme.opacityWindowMax)

        fillGradient: LinearGradient {
            x1: 0
            y1: 0
            x2: volumetricBodyFrame.width
            y2: volumetricBodyFrame.height * 0.75

            // Paragens extraídas do Figma e valo.tokens.json
            GradientStop { position: 0.00; color: "#63194F76" } // #194F76 @ 0.39
            GradientStop { position: 0.20; color: "#454BAFC7" } // #4BAFC7 @ 0.27
            GradientStop { position: 0.53; color: "#45133A5D" } // #133A5D @ 0.27
            GradientStop { position: 1.00; color: "#6909233F" } // #09233F @ 0.41
        }
    }

    // Camada D: Reflexos Prismáticos e Espectrais (spectralOverlayStops em ângulo -31°)
    SystaliaG2Frame {
        id: spectralPrismFrame
        anchors.fill: parent
        z: 3
        radii: root.effectiveRadii
        slant: root.slant
        visible: root.showPrism && root.iridescenceStrength > 0.001 && !root.isPerformance
        opacity: root.iridescenceStrength * (root.active ? 1.0 : 0.40)

        fillGradient: LinearGradient {
            x1: 0
            y1: spectralPrismFrame.height
            x2: spectralPrismFrame.width
            y2: 0

            // Paragens do espectro prismático (Ciano, Branco, Âmbar, Violeta)
            GradientStop { position: 0.00; color: "#1A00EFFF" } // #00EFFF @ 0.10
            GradientStop { position: 0.29; color: "#1FF5FFFF" } // #F5FFFF @ 0.12
            GradientStop { position: 0.60; color: "#12F8C47B" } // #F8C47B @ 0.07
            GradientStop { position: 1.00; color: "#149E77FF" } // #9E77FF @ 0.08
        }
    }

    // Camada E: Borda Especular Analítica G2 (Bisel e Chanfro de Luz de 1.5px)
    SystaliaG2Frame {
        id: specularStrokeFrame
        anchors.fill: parent
        z: 4
        radii: root.effectiveRadii
        slant: root.slant
        fillColor: "transparent"
        strokeWidth: ValoTheme.strokeWindow
        strokeColor: ValoTheme.withAlpha(
            root.active ? ValoTheme.borderHighlight : ValoTheme.borderSubtle,
            (root.active ? 0.45 : 0.20) * root.borderIntensity
        )
        visible: root.borderIntensity > 0.001
    }

    // Camada F: Content Container para itens e controlos da aplicação
    Item {
        id: contentContainer
        anchors.fill: parent
        z: 10
        clip: false
    }
}
