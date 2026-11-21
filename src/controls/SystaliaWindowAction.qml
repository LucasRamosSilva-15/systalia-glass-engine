import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

ToolButton {
    id: action

    property string glyph: "•"
    property string accessibleName: "Ação da janela"
    property string actionType: ""
    property bool closeAction: actionType === "close" || glyph === "×" || glyph === "x" || glyph === "X"
    property bool maximizeAction: actionType === "maximize" || glyph === "□" || glyph === "❐" || glyph === "+"
    property bool minimizeAction: actionType === "minimize" || glyph === "−" || glyph === "-" || glyph === "_"

    Accessible.name: accessibleName
    width: 28
    height: 28
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.35
    ToolTip.visible: hovered
    ToolTip.text: accessibleName
    ToolTip.delay: 450

    // Física elástica de clique e hover
    scale: pressed ? 0.90 : (hovered ? 1.08 : 1.0)
    Behavior on scale {
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    background: Item {
        anchors.centerIn: parent
        width: 28
        height: 28

        // Halo / Glow luminoso exterior no hover
        Rectangle {
            anchors.centerIn: parent
            width: 32
            height: 32
            radius: 16
            color: action.closeAction ? "#ff3b60" : "#00c8ff"
            opacity: action.pressed ? 0.45 : (action.hovered ? 0.35 : 0.0)
            visible: opacity > 0
            Behavior on opacity {
                NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
            }
        }

        // Sombra de queda base conforme Figma (cx=19, cy=16, r=14 em viewBox 38x38, offset dy=3, blur 2.5)
        Rectangle {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 3
            width: 28
            height: 28
            radius: 14
            color: "#8000101b"
            opacity: action.pressed ? 0.3 : 0.5
        }

        // Esfera base de diâmetro 28px desenhada puramente via Shape circular
        Shape {
            id: sphereShape
            anchors.centerIn: parent
            width: 32
            height: 32
            layer.enabled: true
            layer.samples: 4

            ShapePath {
                strokeWidth: 0
                fillGradient: RadialGradient {
                    centerX: 16
                    centerY: 16
                    focalX: 16
                    focalY: 16
                    centerRadius: 14
                    focalRadius: 0

                    GradientStop {
                        position: 0.0
                        color: action.closeAction ? "#ffb1cb" : "#b0faff"
                    }
                    GradientStop {
                        position: action.closeAction ? 0.27 : 0.25
                        color: action.closeAction ? "#ff3b60" : "#00c8ff"
                    }
                    GradientStop {
                        position: action.closeAction ? 0.72 : 0.62
                        color: action.closeAction ? "#9e1448" : "#007dfc"
                    }
                    GradientStop {
                        position: 1.0
                        color: action.closeAction ? "#540d30" : "#073e9e"
                    }
                }

                // Círculo fechado perfeito sem singularidade no topo
                startX: 2
                startY: 16
                PathArc {
                    x: 30
                    y: 16
                    radiusX: 14
                    radiusY: 14
                    useLargeArc: false
                }
                PathArc {
                    x: 2
                    y: 16
                    radiusX: 14
                    radiusY: 14
                    useLargeArc: false
                }
            }
        }

        // Brilho superior / Especular interno (Inset highlight: 0px 1.5px 2px rgba(255,255,255,0.46))
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 1
            width: 22
            height: 12
            radius: 6
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#75ffffff" }
                GradientStop { position: 1.0; color: "#00ffffff" }
            }
        }

        // Sombra interna inferior (Inset shadow: 0px -1.5px 2px rgba(0, 35, 76, 0.55))
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            width: 22
            height: 10
            radius: 5
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 1.0; color: action.closeAction ? "#8c3b001a" : "#8c001b3d" }
            }
        }

        // Borda perimetral de vidro (border: 1px solid rgba(217, 249, 255, 0.56))
        Rectangle {
            anchors.centerIn: parent
            width: 28
            height: 28
            radius: 14
            color: "transparent"
            border.width: 1
            border.color: action.hovered ? "#c7ffffff" : "#8fd9f9ff"
            Behavior on border.color {
                ColorAnimation { duration: 150 }
            }
        }

        // Anel de foco acessível
        Rectangle {
            anchors.centerIn: parent
            width: 32
            height: 32
            radius: 16
            color: "transparent"
            border.width: 1.5
            border.color: "#49f3fc"
            visible: action.activeFocus
        }
    }

    contentItem: Item {
        width: 16
        height: 16

        Item {
            anchors.centerIn: parent
            width: 16
            height: 16
            scale: action.pressed ? 0.92 : (action.hovered ? 1.05 : 1.0)
            opacity: action.pressed ? 0.78 : (action.hovered ? 1.0 : 0.93)
            Behavior on scale {
                NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
            }
            Behavior on opacity {
                NumberAnimation { duration: 120 }
            }

            // Sombra do ícone (drop-shadow(0px 1px 1px rgba(0, 0, 0, 0.4)))
            Item {
                anchors.fill: parent
                anchors.topMargin: 1

                // Sombra: Minimizar
                Rectangle {
                    visible: action.minimizeAction
                    anchors.centerIn: parent
                    width: 10
                    height: 1.65
                    radius: 0.8
                    color: "#66000000"
                }

                // Sombra: Maximizar
                Rectangle {
                    visible: action.maximizeAction
                    anchors.centerIn: parent
                    width: 9.5
                    height: 9.5
                    radius: 1.5
                    color: "transparent"
                    border.width: 1.5
                    border.color: "#66000000"
                }

                // Sombra: Fechar
                Shape {
                    visible: action.closeAction
                    anchors.fill: parent
                    layer.enabled: true
                    layer.samples: 4
                    ShapePath {
                        strokeWidth: 1.6
                        strokeColor: "#66000000"
                        capStyle: ShapePath.RoundCap
                        startX: 3.5; startY: 3.5
                        PathLine { x: 12.5; y: 12.5 }
                    }
                    ShapePath {
                        strokeWidth: 1.6
                        strokeColor: "#66000000"
                        capStyle: ShapePath.RoundCap
                        startX: 12.5; startY: 3.5
                        PathLine { x: 3.5; y: 12.5 }
                    }
                }
            }

            // Ícone Vetorial Nativo QML
            // 1. Minimizar: Traço horizontal com pontas arredondadas (M3 8H13, stroke: #F9FEFF 0.93, width: 1.65)
            Rectangle {
                visible: action.minimizeAction
                anchors.centerIn: parent
                width: 10
                height: 1.65
                radius: 0.8
                color: "#edf9feff"
            }

            // 2. Maximizar: Retângulo com cantos arredondados (stroke: #F9FEFF 0.93, width: 1.5, radius: 1.5)
            Rectangle {
                visible: action.maximizeAction
                anchors.centerIn: parent
                width: 9.5
                height: 9.5
                radius: 1.5
                color: "transparent"
                border.width: 1.5
                border.color: "#edf9feff"
            }

            // 3. Fechar: X cruzado com traço de precisão (M3.5 3.5L12.5 12.5 / M12.5 3.5L3.5 12.5, stroke: white 0.94, width: 1.6)
            Shape {
                visible: action.closeAction
                anchors.fill: parent
                layer.enabled: true
                layer.samples: 4
                ShapePath {
                    strokeWidth: 1.6
                    strokeColor: "#f0ffffff"
                    capStyle: ShapePath.RoundCap
                    startX: 3.5; startY: 3.5
                    PathLine { x: 12.5; y: 12.5 }
                }
                ShapePath {
                    strokeWidth: 1.6
                    strokeColor: "#f0ffffff"
                    capStyle: ShapePath.RoundCap
                    startX: 12.5; startY: 3.5
                    PathLine { x: 3.5; y: 12.5 }
                }
            }
        }
    }
}
