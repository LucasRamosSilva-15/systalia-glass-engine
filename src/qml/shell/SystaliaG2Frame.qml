import QtQuick
import QtQuick.Shapes

Shape {
    id: frame

    property real cornerRadius: 20
    property color fillColor: "transparent"
    property color strokeColor: "transparent"
    property real strokeWidth: 0
    property real controlFactor: 0.91
    property alias fillGradient: framePath.fillGradient

    readonly property real effectiveRadius: Math.max(
        0, Math.min(cornerRadius, Math.min(width, height) * 0.5 - 0.5))

    antialiasing: true

    ShapePath {
        id: framePath

        fillColor: frame.fillColor
        strokeColor: frame.strokeColor
        strokeWidth: frame.strokeWidth
        joinStyle: ShapePath.RoundJoin
        capStyle: ShapePath.FlatCap
        startX: frame.effectiveRadius
        startY: 0

        PathLine {
            x: frame.width - frame.effectiveRadius
            y: 0
        }
        PathCubic {
            x: frame.width
            y: frame.effectiveRadius
            control1X: frame.width - frame.effectiveRadius * (1 - frame.controlFactor)
            control1Y: 0
            control2X: frame.width
            control2Y: frame.effectiveRadius * (1 - frame.controlFactor)
        }
        PathLine {
            x: frame.width
            y: frame.height - frame.effectiveRadius
        }
        PathCubic {
            x: frame.width - frame.effectiveRadius
            y: frame.height
            control1X: frame.width
            control1Y: frame.height - frame.effectiveRadius * (1 - frame.controlFactor)
            control2X: frame.width - frame.effectiveRadius * (1 - frame.controlFactor)
            control2Y: frame.height
        }
        PathLine {
            x: frame.effectiveRadius
            y: frame.height
        }
        PathCubic {
            x: 0
            y: frame.height - frame.effectiveRadius
            control1X: frame.effectiveRadius * (1 - frame.controlFactor)
            control1Y: frame.height
            control2X: 0
            control2Y: frame.height - frame.effectiveRadius * (1 - frame.controlFactor)
        }
        PathLine {
            x: 0
            y: frame.effectiveRadius
        }
        PathCubic {
            x: frame.effectiveRadius
            y: 0
            control1X: 0
            control1Y: frame.effectiveRadius * (1 - frame.controlFactor)
            control2X: frame.effectiveRadius * (1 - frame.controlFactor)
            control2Y: 0
        }
    }
}
