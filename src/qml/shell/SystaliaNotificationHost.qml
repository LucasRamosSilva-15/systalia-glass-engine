import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: notificationHost

    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0.0

    anchors.top: parent.top
    anchors.topMargin: 48
    anchors.right: parent.right
    anchors.rightMargin: 16
    width: 370
    height: Math.max(0, parent.height - 64)
    z: 220
    visible: Notifications.notificationCount > 0 || opacity > 0.01
    opacity: Notifications.notificationCount > 0 ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    ListView {
        id: notificationList
        anchors.fill: parent
        spacing: 10
        clip: false
        interactive: false
        model: Notifications
        delegate: Item {
            id: notificationCard
            required property int notificationId
            required property string appName
            required property string summary
            required property string body
            required property string appIcon
            required property int timeout

            width: notificationList.width
            height: 126

            SystaliaGlassItem {
                anchors.fill: parent
                backdropSource: notificationHost.backdropSource
                screenResolution: notificationHost.screenResolution
                radii: Qt.vector4d(18, 18, 18, 18)
                slant: 0.0
                ior: 1.25
                chromaticDispersion: 0.035
                causticIntensity: 0.52
                surfaceOpacity: 0.90
                opacity: 0.90
                time: notificationHost.animationTime
            }

            Rectangle {
                anchors.fill: parent
                radius: 18
                color: "#c20b1b2b"
                border.color: "#6e9de8f5"
                border.width: 1
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: 12
                    color: "#2b56c6dc"
                    border.color: "#70d7f6ff"
                    Image {
                        id: notificationIcon
                        anchors.centerIn: parent
                        width: 25
                        height: 25
                        source: notificationCard.appIcon.length > 0
                                ? "image://desktopIcons/" + encodeURIComponent(notificationCard.appIcon)
                                : ""
                        sourceSize: Qt.size(48, 48)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: notificationIcon.status !== Image.Ready
                        text: notificationCard.appName.length > 0
                              ? notificationCard.appName.substring(0, 1).toUpperCase() : "•"
                        color: "#eafaff"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: notificationCard.appName
                        color: "#84d8ee"
                        font.pixelSize: 9
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: notificationCard.summary
                        color: "#f2fbff"
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: notificationCard.body
                        color: "#c2dce8"
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }
                }

                ToolButton {
                    Layout.alignment: Qt.AlignTop
                    text: "×"
                    Accessible.name: "Fechar notificação"
                    onClicked: Notifications.closeNotificationFromUi(notificationCard.notificationId)
                    contentItem: Text {
                        text: parent.text
                        color: parent.hovered ? "#ffffff" : "#8db4c4"
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 10
                        color: parent.hovered ? "#3346a4b5" : "transparent"
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.bottomMargin: 9
                height: 2
                radius: 1
                visible: notificationCard.timeout > 0
                color: "#30446070"

                Rectangle {
                    id: lifetimeBar
                    property real remaining: 1.0
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * remaining
                    radius: 1
                    color: "#68e5f7"
                    NumberAnimation {
                        id: lifetimeAnimation
                        target: lifetimeBar
                        property: "remaining"
                        from: 1.0
                        to: 0.0
                        duration: notificationCard.timeout
                        running: notificationCard.timeout > 0
                        easing.type: Easing.Linear
                    }
                }
            }

            onTimeoutChanged: {
                lifetimeBar.remaining = 1.0
                if (notificationCard.timeout > 0)
                    lifetimeAnimation.restart()
            }
        }

        add: Transition {
            NumberAnimation { properties: "opacity,scale"; from: 0.0; to: 1.0; duration: 190; easing.type: Easing.OutBack }
        }
        remove: Transition {
            NumberAnimation { properties: "opacity,scale"; to: 0.0; duration: 150; easing.type: Easing.InCubic }
        }
        displaced: Transition {
            NumberAnimation { properties: "y"; duration: 190; easing.type: Easing.OutCubic }
        }
    }
}
