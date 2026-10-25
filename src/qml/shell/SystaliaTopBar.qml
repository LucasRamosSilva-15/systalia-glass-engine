import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: topBar

    property var telemetry: null
    readonly property int barHeight: 40

    signal launcherRequested()
    signal controlCenterRequested()

    height: barHeight
    z: 100

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "#e6162c46" }
            GradientStop { position: 0.55; color: "#d70b1c32" }
            GradientStop { position: 1.0; color: "#eb071225" }
        }
        border.width: 0
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#705ceaff"
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        height: 1
        color: "#28b8f7ff"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 14
        spacing: 8

        RowLayout {
            id: identityBlock
            Layout.alignment: Qt.AlignVCenter
            spacing: 7

            TapHandler {
                cursorShape: Qt.PointingHandCursor
                onTapped: topBar.launcherRequested()
            }

            Rectangle {
                Layout.preferredWidth: 23
                Layout.preferredHeight: 25
                radius: 12
                rotation: -8
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#80ecfeff" }
                    GradientStop { position: 0.24; color: "#7044d9e8" }
                    GradientStop { position: 1.0; color: "#d00873a8" }
                }
                border.color: "#a0d9fbff"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "S"
                    color: "#f4fdff"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Inter, Noto Sans, sans-serif"
                }
            }

            Text {
                text: "SYSTALIA"
                color: "#f1fbff"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1.2
            }

            Text {
                text: "|"
                color: "#668da8"
                font.pixelSize: 12
            }

            Text {
                text: "SDE | VALO"
                color: "#91bdd0"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.9
            }
        }

        Item { Layout.fillWidth: true }

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: topBar.telemetry
                  ? "CPU %1%  |  RAM %2 GB  |  FPS %3  |  %4"
                    .arg(Math.round(topBar.telemetry.cpuUsage))
                    .arg(topBar.telemetry.ramUsageGb.toFixed(1))
                    .arg(topBar.telemetry.fps)
                    .arg(topBar.telemetry.systemTime)
                  : "CPU --%  |  RAM -- GB  |  FPS --  |  -- -- --- > --:--:--"
            color: "#d6edf6"
            font.pixelSize: 10
            font.family: "Monospace"
            font.letterSpacing: 0.25
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            id: sessionControls
            Layout.alignment: Qt.AlignVCenter
            spacing: 3

            ToolButton {
                id: networkButton
                text: "⌁"
                Accessible.name: "Rede"
                ToolTip.visible: hovered
                ToolTip.text: "Rede"
                onClicked: topBar.controlCenterRequested()
                contentItem: Text {
                    text: networkButton.text
                    color: networkButton.hovered ? "#e9fbff" : "#a8c7d6"
                    font.pixelSize: 17
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: networkButton.hovered ? "#2638bdf8" : "transparent"
                }
            }

            ToolButton {
                id: soundButton
                text: "◖"
                Accessible.name: "Som"
                ToolTip.visible: hovered
                ToolTip.text: "Som"
                onClicked: topBar.controlCenterRequested()
                contentItem: Text {
                    text: soundButton.text
                    color: soundButton.hovered ? "#e9fbff" : "#a8c7d6"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: soundButton.hovered ? "#2638bdf8" : "transparent"
                }
            }

            ToolButton {
                id: brightnessButton
                text: "☼"
                Accessible.name: "Brilho"
                ToolTip.visible: hovered
                ToolTip.text: "Brilho"
                onClicked: topBar.controlCenterRequested()
                contentItem: Text {
                    text: brightnessButton.text
                    color: brightnessButton.hovered ? "#e9fbff" : "#a8c7d6"
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: brightnessButton.hovered ? "#2638bdf8" : "transparent"
                }
            }

            ToolButton {
                id: profileButton
                text: "LR"
                Accessible.name: "Perfil de sessão"
                ToolTip.visible: hovered
                ToolTip.text: "Perfil de sessão"
                onClicked: topBar.controlCenterRequested()
                contentItem: Text {
                    text: profileButton.text
                    color: "#e8faff"
                    font.pixelSize: 9
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 11
                    color: profileButton.hovered ? "#3947c9e8" : "#2633a3bd"
                    border.color: "#4867c9d8"
                    border.width: 1
                }
            }
        }
    }
}
