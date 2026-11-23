import QtQuick
import QtQuick.Controls
import Systalia.Engine

/**
 * ValoDockV2Scene.qml — Cena de Demonstração e Validação Isolada da Dock V2 (Etapa 04)
 *
 * Permite inspecionar a Dock V2 de forma totalmente autônoma (sem carregar o compositor Wayland),
 * alternando dinamicamente entre múltiplos backdrops de teste, 8 passes diagnósticos e 7 estados.
 */
Item {
    id: root

    width: 1280
    height: 720

    property int backgroundMode: 0
    property bool showHud: true
    property alias dock: dockInstance

    // 1. Texturas e Cenários de Fundo para Amostragem Óptica
    Item {
        id: backdropContainer
        anchors.fill: parent

        // 0: Wallpaper Oceânico Deep VALO (#010C1A -> #052445)
        Rectangle {
            anchors.fill: parent
            visible: root.backgroundMode === 0
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#010C1A" }
                GradientStop { position: 0.5; color: "#031830" }
                GradientStop { position: 1.0; color: "#052445" }
            }

            // Elementos luminosos circulares suaves de ambiência oceânica
            Rectangle {
                x: 180; y: 200; width: 340; height: 340; radius: 170
                color: Qt.rgba(0.06, 0.35, 0.55, 0.12)
            }
            Rectangle {
                x: 760; y: 280; width: 380; height: 380; radius: 190
                color: Qt.rgba(0.08, 0.42, 0.62, 0.10)
            }
        }

        // 1: Grade de Alto Contraste (para avaliar curvatura refrativa e normais)
        Rectangle {
            anchors.fill: parent
            visible: root.backgroundMode === 1
            color: "#05090F"

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.strokeStyle = "rgba(0, 239, 255, 0.35)";
                    ctx.lineWidth = 1;
                    for (var x = 0; x < width; x += 30) {
                        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke();
                    }
                    for (var y = 0; y < height; y += 30) {
                        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke();
                    }
                    // Círculos concêntricos sob a dock
                    ctx.strokeStyle = "rgba(248, 196, 123, 0.55)";
                    ctx.lineWidth = 2;
                    ctx.beginPath(); ctx.arc(width/2, height - 120, 80, 0, 2*Math.PI); ctx.stroke();
                    ctx.strokeStyle = "rgba(158, 119, 255, 0.55)";
                    ctx.beginPath(); ctx.arc(width/2, height - 120, 140, 0, 2*Math.PI); ctx.stroke();
                }
            }
        }

        // 2: Fundo Claro (Light Studio #E8EDF2)
        Rectangle {
            anchors.fill: parent
            visible: root.backgroundMode === 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#F0F4F8" }
                GradientStop { position: 1.0; color: "#D9E2EC" }
            }
            Rectangle {
                anchors.centerIn: parent
                width: 700; height: 300
                color: Qt.rgba(0.2, 0.5, 0.8, 0.15)
                radius: 40
            }
        }

        // 3: Fundo Escuro (Obsidian Dark #0A0D12)
        Rectangle {
            anchors.fill: parent
            visible: root.backgroundMode === 3
            color: "#080B10"
            Rectangle {
                anchors.centerIn: parent
                width: 600; height: 200
                color: Qt.rgba(0.12, 0.18, 0.25, 0.4)
                radius: 30
            }
        }

        // 4: Detalhes Coloridos (Prism Spectrum)
        Rectangle {
            anchors.fill: parent
            visible: root.backgroundMode === 4
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#0F2027" }
                GradientStop { position: 0.3; color: "#203A43" }
                GradientStop { position: 0.6; color: "#2C5364" }
                GradientStop { position: 1.0; color: "#5F2C82" }
            }
            // Bolas coloridas atravessando a área da dock
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 80
                spacing: 60
                Repeater {
                    model: ["#FF2D75", "#49F3FC", "#FAD961", "#A8FF78", "#9B51E0"]
                    Rectangle {
                        width: 70; height: 70; radius: 35
                        color: modelData
                    }
                }
            }
        }
    }

    // Provedor de textura do backdrop via ShaderEffectSource (isTextureProvider() = true)
    ShaderEffectSource {
        id: backdropEffectSource
        anchors.fill: parent
        sourceItem: backdropContainer
        live: true
        hideSource: false
        recursive: false
    }

    // 2. Instância da Dock VALO V2 (Centralizada na parte inferior da tela)
    ValoDockV2 {
        id: dockInstance
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40

        // Vincula o ShaderEffectSource para refração e blur em GPU real
        backdropSource: backdropEffectSource
    }

    // 3. Painel Superior de Controle Interativo e Diagnóstico (HUD)
    Rectangle {
        id: controlHud
        visible: root.showHud
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 16
        width: 1040
        height: 64
        radius: 16
        color: Qt.rgba(0.04, 0.09, 0.16, 0.85)
        border.color: Qt.rgba(0.2, 0.6, 0.8, 0.35)
        border.width: 1

        Row {
            anchors.centerIn: parent
            spacing: 24

            // Seletor de Fundo
            Row {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter
                Text { text: "Fundo:"; color: "#8AE8FF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                ComboBox {
                    id: bgSelector
                    model: ["Oceânico", "Grade", "Claro", "Escuro", "Prisma"]
                    currentIndex: root.backgroundMode
                    onCurrentIndexChanged: root.backgroundMode = currentIndex
                }
            }

            // Seletor dos 8 Passes Diagnósticos (A a H)
            Row {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter
                Text { text: "Passe:"; color: "#8AE8FF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                ComboBox {
                    id: passSelector
                    model: ["H (Final)", "A (SDF)", "B (Máscara)", "C (Volume)", "D (Refração)", "E (Blur GPU)", "F (Contorno)", "G (Sombra)"]
                    onCurrentIndexChanged: {
                        var p = ["H", "A", "B", "C", "D", "E", "F", "G"];
                        dockInstance.debugPass = p[currentIndex];
                    }
                }
            }

            // Seletor dos 7 Estados Canônicos
            Row {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter
                Text { text: "Estado:"; color: "#8AE8FF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                ComboBox {
                    id: stateSelector
                    model: ["Normal", "1: Idle", "2: Hover", "3: Press", "4: Active", "5: Running", "6: Focused", "7: Reduced/HighContrast"]
                    onCurrentIndexChanged: {
                        dockInstance.testState = currentIndex;
                        dockInstance.highContrast = (currentIndex === 7);
                        dockInstance.prefersReducedMotion = (currentIndex === 7);
                    }
                }
            }

            // Seletor de Perfil
            Row {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter
                Text { text: "Perfil:"; color: "#8AE8FF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                ComboBox {
                    id: profileSelector
                    model: ["Performance", "Balanced", "Quality"]
                    currentIndex: 2 // Quality default
                    onCurrentIndexChanged: dockInstance.blurProfile = currentIndex
                }
            }
        }
    }
}

