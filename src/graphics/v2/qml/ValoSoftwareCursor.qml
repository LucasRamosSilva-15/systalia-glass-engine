import QtQuick

/**
 * ValoSoftwareCursor.qml — Motor de Cursores Dinâmicos VALO em QML (Canvas 2D)
 *
 * Implementa 11 estados contextuais vetoriais com hotspot dinâmico (x, y)
 * sem dependência de arquivos .svg externos (pronto para migração futura para .svg).
 */
Item {
    id: root

    property string cursorState: "default"
    property real devicePixelRatio: 1.0
    property real spinnerAngle: 0.0

    readonly property var cursorMetrics: {
        switch (cursorState) {
        case "pointer":
            return { w: 24, h: 24, hx: 7, hy: 2 }
        case "grab":
        case "grabbing":
            return { w: 24, h: 24, hx: 12, hy: 12 }
        case "resize-ew":
            return { w: 26, h: 24, hx: 13, hy: 12 }
        case "resize-ns":
            return { w: 24, h: 26, hx: 12, hy: 13 }
        case "resize-nwse":
        case "resize-nesw":
            return { w: 24, h: 24, hx: 12, hy: 12 }
        case "text":
            return { w: 20, h: 24, hx: 10, hy: 12 }
        case "busy":
        case "not-allowed":
            return { w: 24, h: 24, hx: 12, hy: 12 }
        case "default":
        default:
            return { w: 24, h: 24, hx: 1, hy: 1 }
        }
    }

    readonly property real hotspotX: cursorMetrics.hx
    readonly property real hotspotY: cursorMetrics.hy

    width: cursorMetrics.w
    height: cursorMetrics.h
    implicitWidth: cursorMetrics.w
    implicitHeight: cursorMetrics.h

    onCursorStateChanged: cursorCanvas.requestPaint()
    onDevicePixelRatioChanged: cursorCanvas.requestPaint()
    onSpinnerAngleChanged: {
        if (cursorState === "busy") {
            cursorCanvas.requestPaint()
        }
    }

    NumberAnimation on spinnerAngle {
        from: 0
        to: Math.PI * 2
        duration: 850
        loops: Animation.Infinite
        running: root.visible && root.cursorState === "busy"
    }

    Canvas {
        id: cursorCanvas
        anchors.fill: parent
        renderTarget: Canvas.Image
        canvasSize: Qt.size(Math.max(1, Math.round(width * Math.max(1.0, root.devicePixelRatio))),
                            Math.max(1, Math.round(height * Math.max(1.0, root.devicePixelRatio))))

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var dpr = Math.max(1.0, root.devicePixelRatio)
            ctx.scale(dpr, dpr)

            var fillDark = "#0b1626"
            var strokeLight = "#e2f1ff"
            var accentCyan = "#39F5F4"
            var accentAlert = "#ff5f6d"

            ctx.lineJoin = "round"
            ctx.lineCap = "round"

            var state = root.cursorState
            if (state === "pointer") {
                // Mão apontadora com dedo indicador em (7, 2)
                ctx.beginPath()
                ctx.moveTo(7, 2)
                ctx.lineTo(9.4, 2)
                ctx.lineTo(9.4, 9.2)
                ctx.lineTo(11.6, 8.6)
                ctx.lineTo(13.8, 9.2)
                ctx.lineTo(16.0, 9.8)
                ctx.lineTo(18.2, 10.6)
                ctx.lineTo(18.2, 17.0)
                ctx.lineTo(15.6, 21.0)
                ctx.lineTo(8.2, 21.0)
                ctx.lineTo(5.0, 15.8)
                ctx.lineTo(5.8, 13.6)
                ctx.lineTo(7.0, 14.4)
                ctx.closePath()
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()

                ctx.beginPath()
                ctx.arc(7, 2.8, 1.1, 0, Math.PI * 2)
                ctx.fillStyle = accentCyan
                ctx.fill()
            } else if (state === "grab" || state === "grabbing") {
                var closed = (state === "grabbing")
                ctx.beginPath()
                if (closed) {
                    ctx.moveTo(6.5, 11.5)
                    ctx.lineTo(8.5, 8.5)
                    ctx.lineTo(11.5, 8.0)
                    ctx.lineTo(14.5, 8.2)
                    ctx.lineTo(17.5, 9.5)
                    ctx.lineTo(17.8, 16.5)
                    ctx.lineTo(15.2, 20.0)
                    ctx.lineTo(8.8, 20.0)
                    ctx.lineTo(6.2, 16.0)
                } else {
                    ctx.moveTo(6.0, 12.0)
                    ctx.lineTo(6.8, 6.2)
                    ctx.lineTo(8.8, 6.2)
                    ctx.lineTo(9.3, 10.0)
                    ctx.lineTo(10.5, 4.8)
                    ctx.lineTo(12.6, 4.8)
                    ctx.lineTo(13.0, 10.0)
                    ctx.lineTo(14.2, 5.5)
                    ctx.lineTo(16.2, 5.8)
                    ctx.lineTo(16.4, 10.8)
                    ctx.lineTo(18.0, 7.6)
                    ctx.lineTo(19.5, 8.4)
                    ctx.lineTo(18.0, 16.8)
                    ctx.lineTo(15.2, 20.4)
                    ctx.lineTo(8.8, 20.4)
                    ctx.lineTo(5.6, 15.6)
                }
                ctx.closePath()
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()

                if (closed) {
                    ctx.beginPath()
                    ctx.arc(12, 14, 1.8, 0, Math.PI * 2)
                    ctx.fillStyle = accentCyan
                    ctx.fill()
                }
            } else if (state === "resize-ew") {
                // ↔ (26x24, hotspot 13, 12)
                ctx.beginPath()
                ctx.moveTo(2, 12)
                ctx.lineTo(8, 7)
                ctx.lineTo(8, 10)
                ctx.lineTo(18, 10)
                ctx.lineTo(18, 7)
                ctx.lineTo(24, 12)
                ctx.lineTo(18, 17)
                ctx.lineTo(18, 14)
                ctx.lineTo(8, 14)
                ctx.lineTo(8, 17)
                ctx.closePath()
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(13, 8.5)
                ctx.lineTo(13, 15.5)
                ctx.lineWidth = 1.2
                ctx.strokeStyle = accentCyan
                ctx.stroke()
            } else if (state === "resize-ns") {
                // ↕ (24x26, hotspot 12, 13)
                ctx.beginPath()
                ctx.moveTo(12, 2)
                ctx.lineTo(17, 8)
                ctx.lineTo(14, 8)
                ctx.lineTo(14, 18)
                ctx.lineTo(17, 18)
                ctx.lineTo(12, 24)
                ctx.lineTo(7, 18)
                ctx.lineTo(10, 18)
                ctx.lineTo(10, 8)
                ctx.lineTo(7, 8)
                ctx.closePath()
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(8.5, 13)
                ctx.lineTo(15.5, 13)
                ctx.lineWidth = 1.2
                ctx.strokeStyle = accentCyan
                ctx.stroke()
            } else if (state === "resize-nwse" || state === "resize-nesw") {
                ctx.save()
                ctx.translate(12, 12)
                if (state === "resize-nesw") {
                    ctx.rotate(Math.PI / 2)
                }
                ctx.beginPath()
                ctx.moveTo(-7.5, -7.5)
                ctx.lineTo(-1.0, -7.5)
                ctx.lineTo(-3.2, -5.3)
                ctx.lineTo(5.3, 3.2)
                ctx.lineTo(7.5, 1.0)
                ctx.lineTo(7.5, 7.5)
                ctx.lineTo(1.0, 7.5)
                ctx.lineTo(3.2, 5.3)
                ctx.lineTo(-5.3, -3.2)
                ctx.lineTo(-7.5, -1.0)
                ctx.closePath()
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()
                ctx.restore()
            } else if (state === "text") {
                // I-beam (20x24, hotspot 10, 12)
                ctx.beginPath()
                ctx.moveTo(6, 4)
                ctx.lineTo(14, 4)
                ctx.moveTo(10, 4)
                ctx.lineTo(10, 20)
                ctx.moveTo(6, 20)
                ctx.lineTo(14, 20)
                ctx.lineWidth = 3.6
                ctx.strokeStyle = fillDark
                ctx.stroke()
                ctx.lineWidth = 1.5
                ctx.strokeStyle = strokeLight
                ctx.stroke()
            } else if (state === "busy") {
                // Anel duplo VALO animado (24x24, hotspot 12, 12)
                ctx.beginPath()
                ctx.arc(12, 12, 8.0, 0, Math.PI * 2)
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()

                ctx.beginPath()
                ctx.arc(12, 12, 5.2, root.spinnerAngle, root.spinnerAngle + Math.PI * 1.25)
                ctx.lineWidth = 2.2
                ctx.strokeStyle = accentCyan
                ctx.stroke()
            } else if (state === "not-allowed") {
                // Círculo proibido (24x24, hotspot 12, 12)
                ctx.beginPath()
                ctx.arc(12, 12, 8.2, 0, Math.PI * 2)
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(6.8, 17.2)
                ctx.lineTo(17.2, 6.8)
                ctx.lineWidth = 2.2
                ctx.strokeStyle = accentAlert
                ctx.stroke()
            } else {
                // Default arrow VALO (24x24, hotspot 1, 1)
                ctx.beginPath()
                ctx.moveTo(1, 1)
                ctx.lineTo(1, 17.5)
                ctx.lineTo(5.6, 13.4)
                ctx.lineTo(9.2, 20.6)
                ctx.lineTo(11.8, 19.3)
                ctx.lineTo(8.2, 12.2)
                ctx.lineTo(14.0, 12.2)
                ctx.closePath()
                ctx.fillStyle = fillDark
                ctx.fill()
                ctx.lineWidth = 1.4
                ctx.strokeStyle = strokeLight
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(2.4, 3.6)
                ctx.lineTo(2.4, 13.6)
                ctx.lineWidth = 1.0
                ctx.strokeStyle = accentCyan
                ctx.stroke()
            }
        }

        Component.onCompleted: requestPaint()
    }
}
