import QtQuick

Item {
    id: splitterRoot
    objectName: "tileSplitter"

    property var tilingService: null
    property rect availableArea: Qt.rect(0, 0, 1662, 821)
    property real splitRatio: tilingService ? tilingService.splitRatio : 0.50
    property real splitterGap: tilingService ? tilingService.splitterGap : 8.0
    property bool activeSplit: false
    readonly property bool dragging: splitterMouseArea.pressed
    readonly property real usableWidth: Math.max(0.0, availableArea.width - splitterGap)
    readonly property real centerLineX: availableArea.x + usableWidth * splitRatio + splitterGap * 0.5

    signal ratioUpdated(real newRatio, bool interactive)

    width: Math.max(14, Math.round(splitterGap + 6))
    height: Math.max(0, availableArea.height)
    x: Math.round(centerLineX - width * 0.5)
    y: availableArea.y
    visible: activeSplit && availableArea.width > 0 && availableArea.height > 0
    enabled: visible

    // Linha vertical de contacto com menisco sutil
    Rectangle {
        id: meniscusLine
        width: (splitterMouseArea.containsMouse || splitterRoot.dragging) ? 2 : 1
        height: parent.height
        anchors.centerIn: parent
        color: splitterRoot.dragging
               ? "#ee00f0ff"
               : (splitterMouseArea.containsMouse ? "#aa00f0ff" : "#38c6e5f8")

        Behavior on color {
            ColorAnimation { duration: 140 }
        }
    }

    // Halo difuso da pega central
    Rectangle {
        width: 10
        height: grabHandlePill.height + 10
        radius: 5
        anchors.centerIn: parent
        color: "#3300f0ff"
        visible: splitterMouseArea.containsMouse || splitterRoot.dragging
    }

    // Pega central ergonómica com reação elástica
    Rectangle {
        id: grabHandlePill
        width: 4
        height: splitterRoot.dragging ? 72 : (splitterMouseArea.containsMouse ? 60 : 48)
        radius: 2
        anchors.centerIn: parent
        color: (splitterMouseArea.containsMouse || splitterRoot.dragging) ? "#00f0ff" : "#88d8f6ff"
        border.width: 1
        border.color: "#ccffffff"

        Behavior on height {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
        Behavior on color {
            ColorAnimation { duration: 140 }
        }
    }

    MouseArea {
        id: splitterMouseArea
        objectName: "tileSplitterMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.SplitHCursor
        acceptedButtons: Qt.LeftButton
        preventStealing: true

        onPositionChanged: mouse => {
            if (!pressed || !splitterRoot.parent || splitterRoot.usableWidth <= 0)
                return
            const pt = splitterMouseArea.mapToItem(splitterRoot.parent, mouse.x, mouse.y)
            const rawRatio = (pt.x - splitterRoot.availableArea.x - splitterRoot.splitterGap * 0.5)
                             / splitterRoot.usableWidth
            const clampedRatio = Math.max(0.20, Math.min(0.80, rawRatio))
            if (splitterRoot.tilingService && typeof splitterRoot.tilingService.setSplitRatio === "function")
                splitterRoot.tilingService.setSplitRatio(clampedRatio)
            else
                splitterRoot.splitRatio = clampedRatio
            splitterRoot.ratioUpdated(clampedRatio, true)
        }

        onReleased: {
            splitterRoot.ratioUpdated(splitterRoot.splitRatio, false)
        }

        onCanceled: {
            splitterRoot.ratioUpdated(splitterRoot.splitRatio, false)
        }
    }
}

