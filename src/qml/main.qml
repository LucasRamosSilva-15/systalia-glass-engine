import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine

ApplicationWindow {
    id: rootWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("Systalia Glass Engine (SGE) — Etapa 1.4: Backdrop Screen-Space")
    color: "#080b10"

    // Timer de animação suave para os elementos dinâmicos do backdrop a 60 FPS
    Timer {
        id: animTimer
        interval: 16
        running: true
        repeat: true
        property real tick: 0.0
        onTriggered: tick += 0.02
    }

    // 1. Cena de Fundo com Elementos Geométricos Dinâmicos
    Item {
        id: backgroundScene
        anchors.fill: parent

        // Grade de diagnóstico analítico
        Grid {
            anchors.fill: parent
            rows: 20
            columns: 32
            opacity: 0.15

            Repeater {
                model: 20 * 32
                Rectangle {
                    width: rootWindow.width / 32
                    height: rootWindow.height / 20
                    color: "transparent"
                    border.color: "#305080"
                    border.width: 1
                }
            }
        }

        // Disco 1: Magenta/Violeta oscilando suavemente em órbita
        Rectangle {
            x: 200 + Math.cos(animTimer.tick) * 90
            y: 140 + Math.sin(animTimer.tick * 0.8) * 70
            width: 320
            height: 320
            radius: 160
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#ff007f" }
                GradientStop { position: 1.0; color: "#7928ca" }
            }
        }

        // Disco 2: Ciano/Esmeralda oscilando em contra-fase
        Rectangle {
            x: 740 - Math.sin(animTimer.tick * 0.7) * 80
            y: 340 - Math.cos(animTimer.tick) * 60
            width: 340
            height: 340
            radius: 170
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00f2fe" }
                GradientStop { position: 1.0; color: "#4facfe" }
            }
        }

        // Linhas de diagnóstico de eixos
        Rectangle {
            x: 0
            y: rootWindow.height / 2
            width: rootWindow.width
            height: 2
            color: "#00e5ff"
            opacity: 0.25
        }

        Rectangle {
            x: rootWindow.width / 2
            y: 0
            width: 2
            height: rootWindow.height
            color: "#ff0055"
            opacity: 0.25
        }
    }

    // 2. Capturador FBO Dinâmico em Screen-Space (ShaderEffectSource)
    ShaderEffectSource {
        id: backdropGrab
        anchors.fill: backgroundScene
        sourceItem: backgroundScene
        textureSize: Qt.size(Math.max(1, rootWindow.width * rootWindow.devicePixelRatio),
                             Math.max(1, rootWindow.height * rootWindow.devicePixelRatio))
        live: true
        hideSource: false
        visible: false
        recursive: false
        format: ShaderEffectSource.RGBA8
    }

    // 3. Placa de Vidro VALO Glass com Amostragem Ativa do Backdrop
    SystaliaGlassItem {
        id: glassChassis
        width: 860
        height: 580
        x: Math.round((rootWindow.width - width) / 2)
        y: Math.round((rootWindow.height - height) / 2)

        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)

        surfaceOpacity: 0.85

        property real rTL: 24.0
        property real rTR: 24.0
        property real rBL: 24.0
        property real rBR: 24.0
        property real currentSlant: 0.0

        radii: Qt.vector4d(rTL, rTR, rBL, rBR)
        slant: currentSlant

        // Permite arrastar a janela livremente pelo ecrã
        DragHandler {
            id: windowDrag
            target: glassChassis
        }

        // Conteúdo interativo integrado sobre o vidro
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            // Barra de Título Superior
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: "#00e5ff"
                }

                Text {
                    text: "SYSTALIA GLASS ENGINE — BACKDROP SCREEN-SPACE ATIVO"
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1.5
                    color: "#e2e8f0"
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: 125
                    height: 22
                    radius: 4
                    color: "#0a2540"
                    border.color: "#00e5ff"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "SCREEN-SPACE LIVE"
                        font.pixelSize: 10
                        font.bold: true
                        color: "#00e5ff"
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#1e3a5f"
            }

            // Barra de Presets Rápidos
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "Presets de Curvatura:"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#38bdf8"
                }

                Button {
                    text: "Simétrico 24"
                    onClicked: {
                        glassChassis.rTL = 24.0;
                        glassChassis.rTR = 24.0;
                        glassChassis.rBL = 24.0;
                        glassChassis.rBR = 24.0;
                        glassChassis.currentSlant = 0.0;
                    }
                }

                Button {
                    text: "Domo 48 (Slant 0.35)"
                    onClicked: {
                        glassChassis.rTL = 48.0;
                        glassChassis.rTR = 48.0;
                        glassChassis.rBL = 12.0;
                        glassChassis.rBR = 12.0;
                        glassChassis.currentSlant = 0.35;
                    }
                }

                Button {
                    text: "Assimétrico (42, 8, 8, 42)"
                    onClicked: {
                        glassChassis.rTL = 42.0;
                        glassChassis.rTR = 8.0;
                        glassChassis.rBL = 8.0;
                        glassChassis.rBR = 42.0;
                        glassChassis.currentSlant = 0.0;
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "(Arraste a janela com o mouse)"
                    font.pixelSize: 11
                    color: "#94a3b8"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#132338"
            }

            // Painel de Sliders Interativos
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 8
                columnSpacing: 16

                Text { text: "Raio Superior Esquerdo (r_tl):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rTL
                    onMoved: glassChassis.rTL = value
                }
                Text { text: Math.round(glassChassis.rTL) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                Text { text: "Raio Superior Direito (r_tr):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rTR
                    onMoved: glassChassis.rTR = value
                }
                Text { text: Math.round(glassChassis.rTR) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                Text { text: "Raio Inferior Esquerdo (r_bl):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rBL
                    onMoved: glassChassis.rBL = value
                }
                Text { text: Math.round(glassChassis.rBL) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                Text { text: "Raio Inferior Direito (r_br):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rBR
                    onMoved: glassChassis.rBR = value
                }
                Text { text: Math.round(glassChassis.rBR) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                Text { text: "Opacidade do Cristal:"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.05; to: 1.0
                    value: glassChassis.surfaceOpacity
                    onMoved: glassChassis.surfaceOpacity = value
                }
                Text { text: Math.round(glassChassis.surfaceOpacity * 100) + " %"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }
            }

            Item { Layout.fillHeight: true }

            // Rodapé Técnico
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "Backdrop FBO RGBA8 • Amostragem gl_FragCoord / ScreenResolution • Alinhamento 1:1"
                    font.pixelSize: 11
                    color: "#64748b"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Screen-Space: Ativo"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#10b981"
                }
            }
        }
    }
}
