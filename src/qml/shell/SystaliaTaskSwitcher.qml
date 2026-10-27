import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor
import Systalia.Engine

Item {
    id: taskSwitcher

    property var waylandServer: null
    property var backdropSource: null
    property vector2d screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0.0
    property vector4d cornerRadii: Qt.vector4d(24, 24, 24, 24)
    readonly property real tileWidth: 224
    readonly property real tileHeight: 168
    readonly property real tileSpacing: 14
    readonly property int windowCount: waylandServer ? waylandServer.windowCount : 0
    readonly property real frameWidth: Math.min(
        width * 0.85,
        Math.max(320, windowCount * tileWidth
                 + Math.max(0, windowCount - 1) * tileSpacing + 48))

    anchors.fill: parent
    z: 200
    opacity: waylandServer && waylandServer.taskSwitcherVisible ? 1.0 : 0.0
    scale: waylandServer && waylandServer.taskSwitcherVisible ? 1.0 : 0.96
    visible: opacity > 0.01
    enabled: waylandServer ? waylandServer.taskSwitcherVisible : false

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        NumberAnimation { duration: 180; easing.type: Easing.OutBack }
    }

    SystaliaGlassItem {
        id: switcherFrame
        anchors.centerIn: parent
        width: taskSwitcher.frameWidth
        height: 220
        z: -1
        radii: taskSwitcher.cornerRadii
        slant: 0.0
        ior: 1.28
        chromaticDispersion: 0.035
        surfaceOpacity: 0.92
        causticIntensity: 0.72
        backdropSource: taskSwitcher.backdropSource
        screenResolution: taskSwitcher.screenResolution
        time: taskSwitcher.animationTime
    }

    SystaliaG2Frame {
        anchors.centerIn: parent
        width: switcherFrame.width
        height: switcherFrame.height
        radii: taskSwitcher.cornerRadii
        fillColor: "#d20a1829"
        strokeWidth: 1
        strokeColor: "#8a70d9ed"
    }

    ListView {
        id: itemsList
        anchors.centerIn: parent
        width: switcherFrame.width - 48
        height: tileHeight + 12
        orientation: ListView.Horizontal
        spacing: taskSwitcher.tileSpacing
        clip: true
        interactive: false
        model: taskSwitcher.waylandServer ? taskSwitcher.waylandServer.windowStackModel : null
        currentIndex: taskSwitcher.waylandServer ? taskSwitcher.waylandServer.taskSwitcherIndex : -1
        contentWidth: count > 0 ? count * (taskSwitcher.tileWidth + spacing) - spacing : 0
        preferredHighlightBegin: (width - taskSwitcher.tileWidth) / 2
        preferredHighlightEnd: preferredHighlightBegin + taskSwitcher.tileWidth
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: 100

        delegate: Item {
            id: taskCard
            required property int index
            required property string title
            required property string appId
            required property string iconName
            required property var toplevel
            required property var surface

            width: taskSwitcher.tileWidth
            height: taskSwitcher.tileHeight
            scale: index === itemsList.currentIndex ? 1.0 : 0.97
            opacity: index === itemsList.currentIndex ? 1.0 : 0.74

            Behavior on scale {
                NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
            }
            Behavior on opacity {
                NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.fill: parent
                radius: 16
                color: index === itemsList.currentIndex ? "#ce102b43" : "#a90a1829"
                border.width: index === itemsList.currentIndex ? 2 : 1
                border.color: index === itemsList.currentIndex ? "#00f0ff" : "#516b91a4"
            }

            Item {
                id: thumbnail
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 9
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                height: 105
                clip: true

                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: "#071321"
                }

                WaylandQuickItem {
                    id: liveSurfacePreview
                    anchors.fill: parent
                    surface: taskCard.surface
                    paintEnabled: true
                    inputEventsEnabled: false
                    touchEventsEnabled: false
                    focusOnClick: false
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: index === itemsList.currentIndex ? "#0800cfe8" : "#2a0a1420"
                    border.width: 1
                    border.color: index === itemsList.currentIndex ? "#9068edff" : "#4d7390a4"
                }
            }

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.bottomMargin: 9
                height: 38
                spacing: 9

                Rectangle {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    radius: 9
                    color: "#3b23718a"
                    border.width: 1
                    border.color: "#7d9be9f4"

                    Image {
                        id: appIcon
                        anchors.fill: parent
                        anchors.margins: 4
                        source: taskCard.iconName.length > 0
                                ? "image://desktopIcons/" + encodeURIComponent(taskCard.iconName) : ""
                        sourceSize: Qt.size(48, 48)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: appIcon.status !== Image.Ready
                        text: taskCard.appId.length > 0
                              ? taskCard.appId.substring(0, 1).toUpperCase() : "▣"
                        color: "#effcff"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: taskCard.title.length > 0 ? taskCard.title : qsTr("Janela")
                    color: "#f0fbff"
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            TapHandler {
                onTapped: {
                    if (taskSwitcher.waylandServer)
                        taskSwitcher.waylandServer.selectWindow(taskCard.index)
                }
            }
        }
    }

    Connections {
        target: taskSwitcher.waylandServer
        function onTaskSwitcherIndexChanged() {
            if (itemsList.currentIndex >= 0)
                itemsList.positionViewAtIndex(itemsList.currentIndex, ListView.Contain)
        }
    }
}
