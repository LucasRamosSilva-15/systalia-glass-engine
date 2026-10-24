import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor
import Systalia.Engine

Item {
    id: windowContainer

    property var toplevel: null
    property var backdropSource: null
    property point screenResolution: Qt.point(1280, 800)
    property real animTime: 0.0

    width: 680
    height: 480

    // Chassi de Vidro Líquido Nativo do Systalia Glass Engine (SGE)
    SystaliaGlassItem {
        id: glassChassis
        anchors.fill: parent

        backdropSource: windowContainer.backdropSource
        screenResolution: Qt.vector2d(windowContainer.screenResolution.x, windowContainer.screenResolution.y)

        radii: Qt.vector4d(18.0, 18.0, 18.0, 18.0)
        slant: 0.0
        ior: 1.25
        chromaticDispersion: 0.040
        causticIntensity: 0.70
        surfaceOpacity: 0.90
        time: windowContainer.animTime

        // Substrato protetor abissal para contraste óptico de texto e janelas externas
        Rectangle {
            anchors.fill: parent
            radius: 18
            color: Qt.rgba(0.04, 0.08, 0.15, 0.75)
            border.color: windowContainer.activeFocus ? "#00f2fe" : "#25ffffff"
            border.width: 1

            // 1. Barra de Título SGE com Decoração Server-Side Nativa (SSD)
            Rectangle {
                id: sgeTitleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 36
                radius: 18
                color: windowContainer.activeFocus ? "#3000f2fe" : "#181e293b"

                // Ocultar cantos inferiores arredondados da barra de título
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 12
                    color: parent.color
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 10
                    spacing: 8

                    // Ícone do Aplicativo SGE
                    Rectangle {
                        width: 20
                        height: 20
                        radius: 5
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#38bdf8" }
                            GradientStop { position: 1.0; color: "#0284c7" }
                        }
                        border.color: "#80ffffff"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: ">_"
                            font.pixelSize: 10
                            font.bold: true
                            color: "#f8fafc"
                        }
                    }

                    // Título do Aplicativo
                    Text {
                        Layout.fillWidth: true
                        text: (windowContainer.toplevel && windowContainer.toplevel.title.length > 0)
                              ? windowContainer.toplevel.title
                              : ((windowContainer.toplevel && windowContainer.toplevel.appId.length > 0)
                                 ? windowContainer.toplevel.appId
                                 : "Aplicação Externa (SGE)")
                        font.pixelSize: 12
                        font.bold: true
                        color: "#f8fafc"
                        elide: Text.ElideRight
                    }

                    // Badge de Decoração Server-Side
                    Rectangle {
                        height: 18
                        implicitWidth: badgeTxt.implicitWidth + 10
                        radius: 4
                        color: "#1500f2fe"
                        border.color: "#4000f2fe"
                        border.width: 1

                        Text {
                            id: badgeTxt
                            anchors.centerIn: parent
                            text: "SGE SSD"
                            font.pixelSize: 8
                            font.bold: true
                            color: "#00f2fe"
                        }
                    }

                    // Botões Canônicos de Controle de Janela SGE
                    Row {
                        spacing: 6

                        // Botão Minimizar
                        Rectangle {
                            width: 24
                            height: 20
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: minMouse.containsMouse ? "#40ffffff" : "#20ffffff" }
                                GradientStop { position: 1.0; color: minMouse.containsMouse ? "#300284c7" : "#0d1b2a" }
                            }
                            border.color: minMouse.containsMouse ? "#00f2fe" : "#30ffffff"
                            border.width: 1

                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 2
                                color: "#f8fafc"
                            }

                            MouseArea {
                                id: minMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        // Botão Maximizar
                        Rectangle {
                            width: 24
                            height: 20
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: maxMouse.containsMouse ? "#40ffffff" : "#20ffffff" }
                                GradientStop { position: 1.0; color: maxMouse.containsMouse ? "#300284c7" : "#0d1b2a" }
                            }
                            border.color: maxMouse.containsMouse ? "#00f2fe" : "#30ffffff"
                            border.width: 1

                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 8
                                color: "transparent"
                                border.color: "#f8fafc"
                                border.width: 1.2
                            }

                            MouseArea {
                                id: maxMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (windowContainer.toplevel) {
                                        if (windowContainer.toplevel.maximized) {
                                            windowContainer.toplevel.sendUnmaximized()
                                        } else {
                                            windowContainer.toplevel.sendMaximized(Qt.size(windowContainer.width, windowContainer.height))
                                        }
                                    }
                                }
                            }
                        }

                        // Botão Fechar
                        Rectangle {
                            width: 24
                            height: 20
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: closeMouse.containsMouse ? "#ff5c7a" : "#25ffffff" }
                                GradientStop { position: 1.0; color: closeMouse.containsMouse ? "#b91c1c" : "#0d1b2a" }
                            }
                            border.color: closeMouse.containsMouse ? "#fca5a5" : "#35ffffff"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                font.pixelSize: 10
                                color: "#f8fafc"
                            }

                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (windowContainer.toplevel) {
                                        windowContainer.toplevel.sendClose()
                                    }
                                }
                            }
                        }
                    }
                }

                DragHandler {
                    id: windowDrag
                    target: windowContainer
                }
            }

            // 2. Área Útil do Buffer Wayland (ShellSurfaceItem herda de WaylandQuickItem)
            ShellSurfaceItem {
                id: clientSurfaceItem
                anchors.top: sgeTitleBar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                shellSurface: windowContainer.toplevel ? windowContainer.toplevel.xdgSurface : null
                focus: true
                focusOnClick: true
                inputEventsEnabled: true
                paintEnabled: true
                moveItem: windowContainer

                onSurfaceDestroyed: {
                    windowContainer.destroy()
                }
            }
        }
    }

    TapHandler {
        onTapped: {
            windowContainer.forceActiveFocus()
            clientSurfaceItem.forceActiveFocus()
        }
    }
}
