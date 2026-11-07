import QtQuick
import QtQuick.Controls
import Systalia.Engine

/**
 * ValoDockItemV2.qml — Lançador Oficial da Dock VALO Graphics 2.0 (Etapa 04)
 *
 * Encapsula ícones vetoriais SVG de alta definição (78x78), física elástica Organic
 * desacoplada do hit-testing, suporte a Reduced Motion, estados de foco acessível
 * por teclado e indicadores de execução luminescentes (Active / Background Running).
 */
Item {
    id: root

    property string iconSource: ""
    property string label: ""
    property string action: ""
    property var appIds: []
    property bool isRunning: false
    property bool isFocused: false
    property bool showKeyboardFocus: false
    property real iconSize: 78.0
    property bool prefersReducedMotion: false

    // Propriedades para simulação e validação determinística de estados
    property bool forceHovered: false
    property bool forcePressed: false
    property bool forceActive: false
    property bool forceBackgroundRunning: false

    readonly property bool effectiveHovered: forceHovered || (hoverHandler.hovered && !forcePressed)
    readonly property bool effectivePressed: forcePressed || tapHandler.pressed
    readonly property bool effectiveRunning: forceActive || forceBackgroundRunning || isRunning
    readonly property bool effectiveFocused: forceActive || (effectiveRunning && isFocused)
    readonly property bool effectiveBackground: forceBackgroundRunning || (effectiveRunning && !isFocused && !forceActive)

    signal activated()

    width: iconSize
    height: iconSize + 22 // 78px ícone + espaço para indicador inferior (top: 102px relativo à Dock)
    focus: false
    activeFocusOnTab: true
    Accessible.name: label
    Accessible.role: Accessible.Button

    // Alvo de escala conforme especificações Organic (Hover: 1.08, Press: 0.94, Idle: 1.0)
    readonly property real targetScale: effectivePressed ? 0.94 : (effectiveHovered ? 1.08 : 1.0)

    // Contêiner visual sujeito à transformação elástica (Hit-test permanece no root)
    Item {
        id: visualContainer
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.iconSize
        height: root.iconSize
        transformOrigin: Item.Center

        scale: root.targetScale

        Behavior on scale {
            enabled: !root.prefersReducedMotion
            SpringAnimation {
                spring: 4.5
                damping: 0.35
                epsilon: 0.005
                mass: 1.0
            }
        }

        // Anel de foco acessível por teclado (visível estritamente quando navegado por teclado)
        Rectangle {
            anchors.centerIn: parent
            width: root.iconSize + 8
            height: root.iconSize + 8
            radius: (root.iconSize + 8) / 2
            color: "transparent"
            border.color: Qt.rgba(0.22, 0.87, 0.98, 0.85) // Ciano elétrico #37DDFA
            border.width: 1.5
            visible: root.activeFocus && root.showKeyboardFocus
            opacity: (root.activeFocus && root.showKeyboardFocus) ? 1.0 : 0.0

            Behavior on opacity {
                enabled: !root.prefersReducedMotion
                NumberAnimation { duration: 120 }
            }
        }

        // Ícone vetorial SVG do Figma com drop shadow sutil
        Image {
            id: iconImage
            anchors.fill: parent
            source: root.iconSource
            sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
            fillMode: Image.PreserveAspectFit
            smooth: true
            asynchronous: false
        }
    }

    // Indicador de execução luminoso e adaptativo VALO (Posicionado sob o ícone a Y=102px)
    Item {
        id: indicatorContainer
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: visualContainer.bottom
        anchors.topMargin: 2
        width: 24
        height: 10

        // Glow ambiente da aplicação ativa
        Rectangle {
            anchors.centerIn: parent
            width: indicatorPill.width + 6
            height: indicatorPill.height + 4
            radius: height / 2
            color: "#2EE8F0"
            opacity: (root.effectiveRunning && root.effectiveFocused) ? 0.45 : 0.0
            visible: root.effectiveRunning && root.effectiveFocused
        }

        // Indicador: Barra ativa de 14x3.5px ou Dot em segundo plano de 5x5px
        Rectangle {
            id: indicatorPill
            anchors.centerIn: parent
            visible: root.effectiveRunning
            width: !root.effectiveRunning ? 0 : (root.effectiveFocused ? 14 : 5)
            height: root.effectiveFocused ? 3.5 : 5
            radius: height / 2
            color: root.effectiveFocused ? "#2EE8F0" : Qt.rgba(0.18, 0.91, 0.94, 0.70)
            opacity: root.effectiveRunning ? (root.effectiveHovered ? 1.0 : 0.85) : 0.0
            antialiasing: true

            Behavior on width {
                enabled: !root.prefersReducedMotion
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }
            Behavior on height {
                enabled: !root.prefersReducedMotion
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }
            Behavior on color {
                enabled: !root.prefersReducedMotion
                ColorAnimation { duration: 150 }
            }
        }
    }

    // Área invariante de Hit-Testing (garantia: a região clicável nunca é deformada)
    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        id: tapHandler
        onTapped: root.activated()
    }

    // Suporte à navegação e ativação por teclado
    Keys.onReturnPressed: root.activated()
    Keys.onSpacePressed: root.activated()

    ToolTip.visible: hoverHandler.hovered
    ToolTip.text: root.label
    ToolTip.delay: 300
}

