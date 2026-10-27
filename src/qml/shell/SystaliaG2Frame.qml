import QtQuick
import QtQuick.Shapes

Shape {
    id: frame

    // Ordem igual a SystaliaGlassItem.radii: topo esquerdo, topo direito,
    // inferior esquerdo e inferior direito.
    property vector4d radii: Qt.vector4d(20, 20, 20, 20)
    property color fillColor: "transparent"
    property color strokeColor: "transparent"
    property real strokeWidth: 0
    readonly property real controlFactor: 0.91
    property alias fillGradient: framePath.fillGradient

    readonly property real maxRadius: Math.max(0, Math.min(width, height) * 0.5 - 0.5)
    readonly property vector4d effectiveRadii: Qt.vector4d(
        Math.max(0, Math.min(radii.x, maxRadius)),
        Math.max(0, Math.min(radii.y, maxRadius)),
        Math.max(0, Math.min(radii.z, maxRadius)),
        Math.max(0, Math.min(radii.w, maxRadius)))

    antialiasing: true
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        id: framePath

        fillColor: frame.fillColor
        strokeColor: frame.strokeColor
        strokeWidth: frame.strokeWidth
        joinStyle: ShapePath.RoundJoin
        capStyle: ShapePath.FlatCap
        startX: frame.effectiveRadii.x
        startY: 0

        PathLine {
            x: frame.width - frame.effectiveRadii.y
            y: 0
        }
        PathCubic {
            x: frame.width
            y: frame.effectiveRadii.y
            control1X: frame.width - frame.effectiveRadii.y * (1 - frame.controlFactor)
            control1Y: 0
            control2X: frame.width
            control2Y: frame.effectiveRadii.y * (1 - frame.controlFactor)
        }
        PathLine {
            x: frame.width
            y: frame.height - frame.effectiveRadii.w
        }
        PathCubic {
            x: frame.width - frame.effectiveRadii.w
            y: frame.height
            control1X: frame.width
            control1Y: frame.height - frame.effectiveRadii.w * (1 - frame.controlFactor)
            control2X: frame.width - frame.effectiveRadii.w * (1 - frame.controlFactor)
            control2Y: frame.height
        }
        PathLine {
            x: frame.effectiveRadii.z
            y: frame.height
        }
        PathCubic {
            x: 0
            y: frame.height - frame.effectiveRadii.z
            control1X: frame.effectiveRadii.z * (1 - frame.controlFactor)
            control1Y: frame.height
            control2X: 0
            control2Y: frame.height - frame.effectiveRadii.z * (1 - frame.controlFactor)
        }
        PathLine {
            x: 0
            y: frame.effectiveRadii.x
        }
        PathCubic {
            x: frame.effectiveRadii.x
            y: 0
            control1X: 0
            control1Y: frame.effectiveRadii.x * (1 - frame.controlFactor)
            control2X: frame.effectiveRadii.x * (1 - frame.controlFactor)
            control2Y: 0
        }
    }
}
