import QtQuick
import QtQuick.Shapes

Shape {
    id: frame

    // Ordem igual a SystaliaGlassItem.radii: topo esquerdo, topo direito,
    // inferior esquerdo e inferior direito.
    property vector4d radii: Qt.vector4d(20, 20, 20, 20)
    property bool circularCorners: false
    property color fillColor: "transparent"
    property color strokeColor: "transparent"
    property real strokeWidth: 0
    property real slant: 0
    property real slantReferenceWidth: width
    property real slantReferenceHeight: height
    property real slantOffsetX: 0
    property real slantOffsetY: 0
    readonly property real controlFactor: 0.91
    property ShapeGradient fillGradient: null

    readonly property real maxRadius: Math.max(0, Math.min(width, height) * 0.5 - 0.5)
    readonly property vector4d effectiveRadii: Qt.vector4d(
        Math.max(0, Math.min(radii.x, maxRadius)),
        Math.max(0, Math.min(radii.y, maxRadius)),
        Math.max(0, Math.min(radii.z, maxRadius)),
        Math.max(0, Math.min(radii.w, maxRadius)))

    function xAt(x, y) {
        if (slant === 0) return x
        const halfHeight = Math.max(slantReferenceHeight * 0.5, 1)
        const surfaceY = y + slantOffsetY
        const slantFactor = 1 - ((surfaceY - halfHeight) / halfHeight) * slant * 0.25
        const halfWidth = slantReferenceWidth * 0.5
        const surfaceX = x + slantOffsetX
        const transformedSurfaceX = halfWidth + (surfaceX - halfWidth) * Math.max(slantFactor, 0.05)
        return transformedSurfaceX - slantOffsetX
    }

    antialiasing: true
    preferredRendererType: Shape.CurveRenderer

    // =========================================================================
    // 1. Caminho Squircle G2 com continuidade de curvatura analítica (Windows)
    // =========================================================================
    ShapePath {
        id: g2Path
        fillColor: !frame.circularCorners ? frame.fillColor : "transparent"
        strokeColor: !frame.circularCorners ? frame.strokeColor : "transparent"
        strokeWidth: !frame.circularCorners ? frame.strokeWidth : 0
        fillGradient: !frame.circularCorners ? frame.fillGradient : null
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

    // =========================================================================
    // 2. Caminho Circular L2 Euclidiano exato para elementos orgânicos (Dock/Pill)
    // =========================================================================
    ShapePath {
        id: circPath
        fillColor: frame.circularCorners ? frame.fillColor : "transparent"
        strokeColor: frame.circularCorners ? frame.strokeColor : "transparent"
        strokeWidth: frame.circularCorners ? frame.strokeWidth : 0
        fillGradient: frame.circularCorners ? frame.fillGradient : null
        joinStyle: ShapePath.RoundJoin
        capStyle: ShapePath.FlatCap

        startX: frame.xAt(frame.effectiveRadii.x, 0)
        startY: 0

        PathLine {
            x: frame.xAt(frame.width - frame.effectiveRadii.y, 0)
            y: 0
        }
        PathArc {
            x: frame.xAt(frame.width, frame.effectiveRadii.y)
            y: frame.effectiveRadii.y
            radiusX: frame.effectiveRadii.y
            radiusY: frame.effectiveRadii.y
            direction: PathArc.Clockwise
        }
        PathLine {
            x: frame.xAt(frame.width, frame.height - frame.effectiveRadii.w)
            y: frame.height - frame.effectiveRadii.w
        }
        PathArc {
            x: frame.xAt(frame.width - frame.effectiveRadii.w, frame.height)
            y: frame.height
            radiusX: frame.effectiveRadii.w
            radiusY: frame.effectiveRadii.w
            direction: PathArc.Clockwise
        }
        PathLine {
            x: frame.xAt(frame.effectiveRadii.z, frame.height)
            y: frame.height
        }
        PathArc {
            x: frame.xAt(0, frame.height - frame.effectiveRadii.z)
            y: frame.height - frame.effectiveRadii.z
            radiusX: frame.effectiveRadii.z
            radiusY: frame.effectiveRadii.z
            direction: PathArc.Clockwise
        }
        PathLine {
            x: frame.xAt(0, frame.effectiveRadii.x)
            y: frame.effectiveRadii.x
        }
        PathArc {
            x: frame.xAt(frame.effectiveRadii.x, 0)
            y: 0
            radiusX: frame.effectiveRadii.x
            radiusY: frame.effectiveRadii.x
            direction: PathArc.Clockwise
        }
    }
}
