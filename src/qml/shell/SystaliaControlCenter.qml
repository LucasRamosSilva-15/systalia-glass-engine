import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: controlCenter

    property bool opened: false
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0.0
    property vector4d cornerRadii: Qt.vector4d(22, 22, 22, 22)

    anchors.fill: parent
    z: 115
    visible: opened || opacity > 0.01
    opacity: opened ? 1.0 : 0.0

    function toggle() {
        opened = !opened
    }

    function close() {
        opened = false
    }

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        visible: controlCenter.opened
        color: "#32020a18"
        MouseArea {
            anchors.fill: parent
            onClicked: controlCenter.close()
        }
    }

    Item {
        id: panel
        width: 358
        height: 508
        x: controlCenter.width - width - 16
        y: controlCenter.opened ? 48 : -height - 12
        clip: true

        Behavior on y {
            NumberAnimation { duration: 210; easing.type: Easing.OutCubic }
        }

        SystaliaGlassItem {
            anchors.fill: parent
            backdropSource: controlCenter.backdropSource
            screenResolution: controlCenter.screenResolution
            radii: controlCenter.cornerRadii
            slant: 0.0
            ior: 1.27
            chromaticDispersion: 0.04
            causticIntensity: 0.62
            surfaceOpacity: 0.9
            time: controlCenter.animationTime
        }

        SystaliaG2Frame {
            anchors.fill: parent
            radii: controlCenter.cornerRadii
            fillColor: "#d00b1b2d"
            strokeColor: "#7880e9f5"
            strokeWidth: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: "ABYSS CONTROL"
                        color: "#f1fbff"
                        font.pixelSize: 15
                        font.bold: true
                        font.letterSpacing: 1.1
                    }
                    Text {
                        text: "Ajustes rápidos do sistema"
                        color: "#8bb2c4"
                        font.pixelSize: 9
                    }
                }
                ToolButton {
                    text: "×"
                    Accessible.name: "Fechar central de controlo"
                    onClicked: controlCenter.close()
                    contentItem: Text {
                        text: parent.text
                        color: "#d9f7ff"
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 9
                QuickToggle {
                    Layout.fillWidth: true
                    label: "Wi-Fi"
                    glyph: "⌁"
                    active: SystemControl.wifiEnabled
                    onClicked: SystemControl.toggleWifi()
                }
                QuickToggle {
                    Layout.fillWidth: true
                    label: "Bluetooth"
                    glyph: "ᛒ"
                    active: SystemControl.bluetoothEnabled
                    onClicked: SystemControl.toggleBluetooth()
                }
                QuickToggle {
                    Layout.fillWidth: true
                    label: "Não incomodar"
                    glyph: "◐"
                    active: SystemControl.doNotDisturb
                    onClicked: SystemControl.setDoNotDisturb(!SystemControl.doNotDisturb)
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#335e8496" }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: "VOLUME"
                        color: "#90b9cb"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Text {
                        text: SystemControl.isMuted ? "MUDO" : SystemControl.volume + "%"
                        color: SystemControl.isMuted ? "#fb7185" : "#dffaff"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    ToolButton {
                        text: SystemControl.isMuted ? "×" : "◖"
                        Accessible.name: SystemControl.isMuted ? "Ativar som" : "Silenciar"
                        onClicked: SystemControl.toggleMute()
                        contentItem: Text {
                            text: parent.text
                            color: "#c8f4ff"
                            font.pixelSize: 16
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: SystemControl.volume
                    onMoved: SystemControl.setVolume(Math.round(value))
                    background: Rectangle {
                        x: volumeSlider.leftPadding
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: volumeSlider.availableWidth
                        height: 5
                        radius: 3
                        color: "#46566f80"
                        Rectangle {
                            width: volumeSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: "#29c8e6"
                        }
                    }
                    handle: Rectangle {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition
                           * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: 17
                        height: 17
                        radius: 9
                        color: "#d6fbff"
                        border.color: "#41cce8"
                        border.width: 2
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: "BRILHO DO ECRÃ"
                        color: "#90b9cb"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Text {
                        text: SystemControl.brightness + "%"
                        color: "#dffaff"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
                Slider {
                    id: brightnessSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: SystemControl.brightness
                    onMoved: SystemControl.setBrightness(Math.round(value))
                    background: Rectangle {
                        x: brightnessSlider.leftPadding
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: brightnessSlider.availableWidth
                        height: 5
                        radius: 3
                        color: "#46566f80"
                        Rectangle {
                            width: brightnessSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: "#29c8e6"
                        }
                    }
                    handle: Rectangle {
                        x: brightnessSlider.leftPadding + brightnessSlider.visualPosition
                           * (brightnessSlider.availableWidth - width)
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: 17
                        height: 17
                        radius: 9
                        color: "#d6fbff"
                        border.color: "#41cce8"
                        border.width: 2
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#335e8496" }

            Text {
                Layout.fillWidth: true
                text: "ESTADO DO SISTEMA"
                color: "#90b9cb"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.8
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                StatusPill {
                    Layout.fillWidth: true
                    label: SystemControl.wifiEnabled ? "Wi-Fi ligado" : "Wi-Fi desligado"
                    active: SystemControl.wifiEnabled
                }
                StatusPill {
                    Layout.fillWidth: true
                    label: SystemControl.bluetoothEnabled ? "Bluetooth ligado" : "Bluetooth desligado"
                    active: SystemControl.bluetoothEnabled
                }
            }
            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                text: "VALO OCEANIC GLASS  •  SYSTALIA SDE"
                color: "#658ea4"
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    component QuickToggle: Button {
        id: toggleButton
        property string label: ""
        property string glyph: "•"
        property bool active: false
        implicitHeight: 74
        display: AbstractButton.TextUnderIcon
        spacing: 4
        contentItem: Column {
            spacing: 3
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: toggleButton.glyph
                color: toggleButton.active ? "#f1fcff" : "#a8c2d1"
                font.pixelSize: 19
                font.bold: true
            }
            Text {
                width: toggleButton.width - 8
                anchors.horizontalCenter: parent.horizontalCenter
                text: toggleButton.label
                color: toggleButton.active ? "#ecfaff" : "#91a9b8"
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
        background: Rectangle {
            radius: 13
            color: toggleButton.active ? "#3c26a8c8" : "#2925364b"
            border.color: toggleButton.active ? "#8168e1f0" : "#314d687a"
            border.width: 1
        }
    }

    component StatusPill: Rectangle {
        property string label: ""
        property bool active: false
        implicitHeight: 29
        radius: 14
        color: active ? "#252c9cbb" : "#202a3b4a"
        border.color: active ? "#4d74d8e7" : "#2c526274"
        Row {
            anchors.centerIn: parent
            spacing: 5
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 6
                height: 6
                radius: 3
                color: active ? "#4de3c6" : "#657d8c"
            }
            Text {
                text: parent.parent.label
                color: "#c3dbe7"
                font.pixelSize: 8
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: controlCenter.opened
        onActivated: controlCenter.close()
    }
}
