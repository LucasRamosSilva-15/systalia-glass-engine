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
    property real slant: 0
    readonly property real controlFactor: 0.91
    property alias fillGradient: framePath.fillGradient

    readonly property real maxRadius: Math.max(0, Math.min(width, height) * 0.5 - 0.5)
    readonly property vector4d effectiveRadii: Qt.vector4d(
        Math.max(0, Math.min(radii.x, maxRadius)),
        Math.max(0, Math.min(radii.y, maxRadius)),
        Math.max(0, Math.min(radii.z, maxRadius)),
        Math.max(0, Math.min(radii.w, maxRadius)))

    function xAt(x, y) {
        const halfHeight = Math.max(height * 0.5, 1)
        const slantFactor = 1 - ((y - halfHeight) / halfHeight) * slant * 0.25
        return width * 0.5 + (x - width * 0.5) * Math.max(slantFactor, 0.05)
    }

    antialiasing: true
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        id: framePath

        fillColor: frame.fillColor
        strokeColor: frame.strokeColor
        strokeWidth: frame.strokeWidth
        joinStyle: ShapePath.RoundJoin
        capStyle: ShapePath.FlatCap
        startX: frame.xAt(frame.effectiveRadii.x, 0)
        startY: 0

        PathLine {
            x: frame.xAt(frame.width - frame.effectiveRadii.y, 0)
            y: 0
        }
        PathCubic {
            x: frame.xAt(frame.width, frame.effectiveRadii.y)
            y: frame.effectiveRadii.y
            control1X: frame.xAt(frame.width - frame.effectiveRadii.y * (1 - frame.controlFactor), 0)
            control1Y: 0
            control2X: frame.xAt(frame.width, frame.effectiveRadii.y * (1 - frame.controlFactor))
            control2Y: frame.effectiveRadii.y * (1 - frame.controlFactor)
        }
        PathLine {
            x: frame.xAt(frame.width, frame.height - frame.effectiveRadii.w)
            y: frame.height - frame.effectiveRadii.w
        }
        PathCubic {
            x: frame.xAt(frame.width - frame.effectiveRadii.w, frame.height)
            y: frame.height
            control1X: frame.xAt(frame.width, frame.height - frame.effectiveRadii.w * (1 - frame.controlFactor))
            control1Y: frame.height - frame.effectiveRadii.w * (1 - frame.controlFactor)
            control2X: frame.xAt(frame.width - frame.effectiveRadii.w * (1 - frame.controlFactor), frame.height)
            control2Y: frame.height
        }
        PathLine {
            x: frame.xAt(frame.effectiveRadii.z, frame.height)
            y: frame.height
        }
        PathCubic {
            x: frame.xAt(0, frame.height - frame.effectiveRadii.z)
            y: frame.height - frame.effectiveRadii.z
            control1X: frame.xAt(frame.effectiveRadii.z * (1 - frame.controlFactor), frame.height)
            control1Y: frame.height
            control2X: frame.xAt(0, frame.height - frame.effectiveRadii.z * (1 - frame.controlFactor))
            control2Y: frame.height - frame.effectiveRadii.z * (1 - frame.controlFactor)
        }
        PathLine {
            x: frame.xAt(0, frame.effectiveRadii.x)
            y: frame.effectiveRadii.x
        }
        PathCubic {
            x: frame.xAt(frame.effectiveRadii.x, 0)
            y: 0
            control1X: frame.xAt(0, frame.effectiveRadii.x * (1 - frame.controlFactor))
            control1Y: frame.effectiveRadii.x * (1 - frame.controlFactor)
            control2X: frame.xAt(frame.effectiveRadii.x * (1 - frame.controlFactor), 0)
            control2Y: 0
        }
    }
}
