import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine

ApplicationWindow {
    id: rootWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("Systalia Glass Engine (SGE) — Etapa 1.2: Primeiro Quad Vulkan")
    color: "#080b10"

    // Fundo com elementos gráficos geométricos para verificação de transparência e oclusão
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

    // Placa de Vidro Vulkan nativo perfeitamente centralizada via anchors.centerIn
    SystaliaGlassItem {
        id: glassChassis
        width: 860
        height: 580
        anchors.centerIn: parent
        surfaceOpacity: 0.85
        radii: Qt.vector4d(24.0, 24.0, 24.0, 24.0)

        // Conteúdo visual interno repousando rigorosamente sobre a placa de vidro
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
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
                    text: "SYSTALIA GLASS ENGINE (SGE) — VALO GLASS RHI QUAD"
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1.5
                    color: "#e2e8f0"
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: 80
                    height: 22
                    radius: 4
                    color: "#0a2540"
                    border.color: "#00e5ff"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "VK RHI 1.4"
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

            Item { Layout.fillHeight: true }

            // Centro do Conteúdo
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "PLACA DE VIDRO PERFEITAMENTE CENTRALIZADA"
                font.bold: true
                font.pixelSize: 18
                font.letterSpacing: 1.2
                color: "#38bdf8"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Geometria local QRectF(0.0, 0.0, 860, 580) • Coordenadas Sincronizadas com o SceneGraph"
                font.pixelSize: 12
                color: "#94a3b8"
            }

            Item { Layout.fillHeight: true }

            // Rodapé Informativo
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "Dimensões: 860 × 580 px | Opacidade: 85% | Alinhamento Local: (0,0)"
                    font.pixelSize: 11
                    color: "#64748b"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Vulkan Pipeline: Active"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#10b981"
                }
            }
        }
    }
}
