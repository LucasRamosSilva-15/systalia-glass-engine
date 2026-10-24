import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine

ApplicationWindow {
    id: rootWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("Systalia Glass Engine (SGE) — Etapa 1.5: Física Óptica VALO Glass Completa")
    color: "#080b10"

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

        // Grade analítica de alta precisão
        Grid {
            anchors.fill: parent
            rows: 20
            columns: 32
            opacity: 0.20

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

        // Padrão de barras listradas de alta frequência para evidenciar a distorção da lente
        Row {
            x: 140
            y: 80
            spacing: 8
            Repeater {
                model: 24
                Rectangle {
                    width: 8
                    height: 280
                    color: (index % 2 === 0) ? "#00e5ff" : "#ff007f"
                    opacity: 0.85
                }
            }
        }

        // Disco 1: Magenta/Violeta em órbita suave cruzando a borda esquerda
        Rectangle {
            x: 120 + Math.cos(animTimer.tick) * 90
            y: 140 + Math.sin(animTimer.tick * 0.8) * 70
            width: 320
            height: 320
            radius: 160
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#ff007f" }
                GradientStop { position: 1.0; color: "#7928ca" }
            }
        }

        // Disco 2: Ciano/Esmeralda em contra-órbita
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

        // Anel de diagnóstico concêntrico
        Rectangle {
            x: 500
            y: 260
            width: 260
            height: 260
            radius: 130
            color: "transparent"
            border.color: "#f59e0b"
            border.width: 3
            opacity: 0.7
        }

        // Linhas guia de eixos
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

    // 3. Placa de Vidro com Física Óptica Completa VALO Glass
    SystaliaGlassItem {
        id: glassChassis
        width: 860
        height: 580
        x: Math.round((rootWindow.width - width) / 2)
        y: Math.round((rootWindow.height - height) / 2)

        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)

        // Propriedades reativas controladas pelo painel
        property real rTL: 24.0
        property real rTR: 24.0
        property real rBL: 24.0
        property real rBR: 24.0
        property real currentSlant: 0.0
        property real currentIOR: 1.45
        property real currentDispersion: 0.050
        property real currentCaustic: 0.65
        property real currentOpacity: 0.85

        radii: Qt.vector4d(rTL, rTR, rBL, rBR)
        slant: currentSlant
        ior: currentIOR
        chromaticDispersion: currentDispersion
        causticIntensity: currentCaustic
        surfaceOpacity: currentOpacity

        DragHandler {
            id: windowDrag
            target: glassChassis
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            // 3.1 Barra de Título Superior
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
                    text: "SYSTALIA GLASS ENGINE — FÍSICA ÓPTICA VALO GLASS NATIVA"
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1.5
                    color: "#e2e8f0"
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: 150
                    height: 22
                    radius: 4
                    color: "#0a2540"
                    border.color: "#00e5ff"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "VALO OPTICAL VK 1.4"
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

            // 3.2 Presets Rápidos
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: "Presets:"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#38bdf8"
                }

                Button {
                    text: "Simétrico 24"
                    onClicked: {
                        glassChassis.rTL = 24.0; glassChassis.rTR = 24.0;
                        glassChassis.rBL = 24.0; glassChassis.rBR = 24.0;
                        glassChassis.currentSlant = 0.0;
                    }
                }

                Button {
                    text: "Domo 48 (Slant 0.35)"
                    onClicked: {
                        glassChassis.rTL = 48.0; glassChassis.rTR = 48.0;
                        glassChassis.rBL = 12.0; glassChassis.rBR = 12.0;
                        glassChassis.currentSlant = 0.35;
                    }
                }

                Button {
                    text: "Assimétrico (42, 8, 8, 42)"
                    onClicked: {
                        glassChassis.rTL = 42.0; glassChassis.rTR = 8.0;
                        glassChassis.rBL = 8.0; glassChassis.rBR = 42.0;
                        glassChassis.currentSlant = 0.0;
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "(Arraste para inspecionar refração móvel)"
                    font.pixelSize: 11
                    color: "#94a3b8"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#132338"
            }

            // 3.3 Controles de Física Óptica e Geometria em 2 Colunas
            RowLayout {
                Layout.fillWidth: true
                spacing: 24

                // Coluna Esquerda: Física Óptica
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "FÍSICA ÓPTICA SNELLIANA & CAUCHY"
                        font.bold: true
                        font.pixelSize: 11
                        color: "#00e5ff"
                    }

                    // IOR
                    RowLayout {
                        Text { text: "Índice de Refração (IOR):"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 150 }
                        Slider {
                            Layout.fillWidth: true
                            from: 1.00; to: 1.50
                            value: glassChassis.currentIOR
                            onMoved: glassChassis.currentIOR = value
                            onValueChanged: if (pressed) glassChassis.currentIOR = value
                        }
                        Text { text: glassChassis.currentIOR.toFixed(2); color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }

                    // Dispersão de Cauchy
                    RowLayout {
                        Text { text: "Dispersão Cromática:"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 150 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.000; to: 0.080
                            value: glassChassis.currentDispersion
                            onMoved: glassChassis.currentDispersion = value
                            onValueChanged: if (pressed) glassChassis.currentDispersion = value
                        }
                        Text { text: glassChassis.currentDispersion.toFixed(3); color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }

                    // Cáustica a 45°
                    RowLayout {
                        Text { text: "Feixe Cáustico a 45°:"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 150 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.00; to: 1.50
                            value: glassChassis.currentCaustic
                            onMoved: glassChassis.currentCaustic = value
                            onValueChanged: if (pressed) glassChassis.currentCaustic = value
                        }
                        Text { text: glassChassis.currentCaustic.toFixed(2); color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }

                    // Opacidade
                    RowLayout {
                        Text { text: "Opacidade do Vidro:"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 150 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.05; to: 1.00
                            value: glassChassis.currentOpacity
                            onMoved: glassChassis.currentOpacity = value
                            onValueChanged: if (pressed) glassChassis.currentOpacity = value
                        }
                        Text { text: Math.round(glassChassis.currentOpacity * 100) + " %"; color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }
                }

                // Coluna Direita: Geometria & Curvatura G2
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "GEOMETRIA SQUIRCLE SDF G2"
                        font.bold: true
                        font.pixelSize: 11
                        color: "#00e5ff"
                    }

                    // Raio TL
                    RowLayout {
                        Text { text: "Raio Sup. Esq. (TL):"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.0; to: 64.0
                            value: glassChassis.rTL
                            onMoved: glassChassis.rTL = value
                        }
                        Text { text: Math.round(glassChassis.rTL) + " px"; color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }

                    // Raio TR
                    RowLayout {
                        Text { text: "Raio Sup. Dir. (TR):"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.0; to: 64.0
                            value: glassChassis.rTR
                            onMoved: glassChassis.rTR = value
                        }
                        Text { text: Math.round(glassChassis.rTR) + " px"; color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }

                    // Raio BL/BR
                    RowLayout {
                        Text { text: "Raios Inf. (BL/BR):"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.0; to: 64.0
                            value: glassChassis.rBL
                            onMoved: {
                                glassChassis.rBL = value;
                                glassChassis.rBR = value;
                            }
                        }
                        Text { text: Math.round(glassChassis.rBL) + " px"; color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }

                    // Slant
                    RowLayout {
                        Text { text: "Inclinação (Slant):"; color: "#94a3b8"; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        Slider {
                            Layout.fillWidth: true
                            from: -1.0; to: 1.0
                            value: glassChassis.currentSlant
                            onMoved: glassChassis.currentSlant = value
                        }
                        Text { text: glassChassis.currentSlant.toFixed(2); color: "#00e5ff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            // 3.4 Rodapé Técnico com Status
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "Lente de Menisco 3D 28.0 px • Cauchy RGB • Especular 1.2 px • Blending RGBA8"
                    font.pixelSize: 11
                    color: "#64748b"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "VALO Optics: 100% Active"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#10b981"
                }
            }
        }
    }
}
