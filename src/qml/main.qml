import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine

ApplicationWindow {
    id: rootWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("Systalia Glass Engine (SGE) — Etapa 1.3: Recorte SDF Squircle G2")
    color: "#080b10"

    // Fundo com elementos gráficos geométricos para verificação de transparência e corte SDF
    Item {
        id: backgroundScene
        anchors.fill: parent

        // Grade sutil
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

        // Disco 1: Magenta/Violeta no quadrante superior esquerdo
        Rectangle {
            x: 180
            y: 120
            width: 320
            height: 320
            radius: 160
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#ff007f" }
                GradientStop { position: 1.0; color: "#7928ca" }
            }
        }

        // Disco 2: Ciano/Esmeralda no quadrante inferior direito
        Rectangle {
            x: 760
            y: 360
            width: 340
            height: 340
            radius: 170
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00f2fe" }
                GradientStop { position: 1.0; color: "#4facfe" }
            }
        }

        // Linhas de diagnóstico de centralização
        Rectangle {
            x: 0
            y: rootWindow.height / 2
            width: rootWindow.width
            height: 2
            color: "#00e5ff"
            opacity: 0.3
        }

        Rectangle {
            x: rootWindow.width / 2
            y: 0
            width: 2
            height: rootWindow.height
            color: "#ff0055"
            opacity: 0.3
        }
    }

    // Placa de Vidro Vulkan nativo com recorte analítico SDF G2 Squircle de 4ª ordem
    SystaliaGlassItem {
        id: glassChassis
        width: 860
        height: 580
        anchors.centerIn: parent
        surfaceOpacity: 0.85

        // Propriedades reativas controladas pelo painel
        property real rTL: 24.0
        property real rTR: 24.0
        property real rBL: 24.0
        property real rBR: 24.0
        property real currentSlant: 0.0

        radii: Qt.vector4d(rTL, rTR, rBL, rBR)
        slant: currentSlant

        // Conteúdo interativo integrado
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            // 1. Barra de Título Superior
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
                    text: "SYSTALIA GLASS ENGINE — RECORTE SQUIRCLE SDF G2 (4ª ORDEM)"
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1.5
                    color: "#e2e8f0"
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: 90
                    height: 22
                    radius: 4
                    color: "#0a2540"
                    border.color: "#00e5ff"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "SDF G2 ACTIVE"
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

            // 2. Barra de Presets Rápidos
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
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#132338"
            }

            // 3. Painel de Sliders Interativos (Tempo Real a 60+ FPS)
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 8
                columnSpacing: 16

                // Raio Top-Left
                Text { text: "Raio Superior Esquerdo (r_tl):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rTL
                    onMoved: glassChassis.rTL = value
                }
                Text { text: Math.round(glassChassis.rTL) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                // Raio Top-Right
                Text { text: "Raio Superior Direito (r_tr):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rTR
                    onMoved: glassChassis.rTR = value
                }
                Text { text: Math.round(glassChassis.rTR) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                // Raio Bottom-Left
                Text { text: "Raio Inferior Esquerdo (r_bl):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rBL
                    onMoved: glassChassis.rBL = value
                }
                Text { text: Math.round(glassChassis.rBL) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                // Raio Bottom-Right
                Text { text: "Raio Inferior Direito (r_br):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0; to: 64.0
                    value: glassChassis.rBR
                    onMoved: glassChassis.rBR = value
                }
                Text { text: Math.round(glassChassis.rBR) + " px"; color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }

                // Slant (Inclinação)
                Text { text: "Inclinação Lateral (Slant):"; color: "#94a3b8"; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: -1.0; to: 1.0
                    value: glassChassis.currentSlant
                    onMoved: glassChassis.currentSlant = value
                }
                Text { text: glassChassis.currentSlant.toFixed(2); color: "#00e5ff"; font.pixelSize: 12; Layout.preferredWidth: 50 }
            }

            Item { Layout.fillHeight: true }

            // 4. Rodapé Técnico
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "SDF Squircle G2 • Subpixel AA: fwidth(dist) • Descarte: alpha <= 0.001"
                    font.pixelSize: 11
                    color: "#64748b"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Curvatura Contínua: Ativa"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#10b981"
                }
            }
        }
    }
}
