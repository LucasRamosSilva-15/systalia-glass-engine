import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: controlCenter

    property bool opened: false
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0.0
    property vector4d cornerRadii: Qt.vector4d(20, 20, 20, 20)

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
        width: 360
        height: 520
        x: controlCenter.width - width - 18
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
            ior: 1.25
            chromaticDispersion: 0.040
            causticIntensity: 0.85
            surfaceOpacity: 0.85
            time: controlCenter.animationTime
        }

        SystaliaG2Frame {
            anchors.fill: parent
            radii: controlCenter.cornerRadii
            fillColor: "#e6071628"
            strokeColor: "#c9ffffff"
            strokeWidth: 1.25
        }

        // Camada superior de brilho / gloss perimetral VALO
        Rectangle {
            x: 4
            y: 2.5
            width: parent.width - 8
            height: parent.height - 8
            radius: 18
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: "#40c8ffff" }
                GradientStop { position: 0.34; color: "#1f61f0ff" }
                GradientStop { position: 0.72; color: "#0b155279" }
                GradientStop { position: 1.0; color: "#001b1834" }
            }
            border.color: "#24c8ffff"
            border.width: 0.8
            antialiasing: true
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
                Item {
                    width: 26
                    height: 26
                    Rectangle {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        radius: 11
                        color: closeMouse.containsMouse ? "#66ff3b60" : "#30ff3b60"
                        border.color: closeMouse.containsMouse ? "#ffb1cb" : "#70ff3b60"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "#ffffff"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: controlCenter.close()
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
                        height: 12
                        radius: 8
                        color: "#f0031529"
                        border.color: "#4273bada"
                        border.width: 0.7
                        antialiasing: true

                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "transparent"
                            border.color: "#50000000"
                            border.width: 1
                        }

                        Rectangle {
                            x: 0
                            y: 0
                            width: Math.max(0, volumeSlider.visualPosition * parent.width)
                            height: parent.height
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: "#14c4e7" }
                                GradientStop { position: 1.0; color: "#17f5df" }
                            }
                            antialiasing: true
                        }
                    }
                    handle: Item {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition
                           * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: 20
                        height: 20

                        Rectangle {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: 2
                            width: 18
                            height: 18
                            radius: 9
                            color: "#8c000000"
                        }

                        Shape {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            layer.enabled: true
                            layer.samples: 4
                            ShapePath {
                                strokeWidth: 0
                                fillGradient: RadialGradient {
                                    centerX: 10; centerY: 10
                                    focalX: 10; focalY: 10
                                    centerRadius: 10; focalRadius: 0
                                    GradientStop { position: 0.0; color: "#ffffff" }
                                    GradientStop { position: 0.30; color: "#dbfbff" }
                                    GradientStop { position: 0.68; color: "#8ce6f4" }
                                    GradientStop { position: 1.0; color: "#4387ba" }
                                }
                                startX: 0; startY: 10
                                PathArc { x: 20; y: 10; radiusX: 10; radiusY: 10 }
                                PathArc { x: 0; y: 10; radiusX: 10; radiusY: 10 }
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: "transparent"
                            border.color: "#cce8ffff"
                            border.width: 1
                            antialiasing: true
                        }
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
                        height: 12
                        radius: 8
                        color: "#f0031529"
                        border.color: "#4273bada"
                        border.width: 0.7
                        antialiasing: true

                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "transparent"
                            border.color: "#50000000"
                            border.width: 1
                        }

                        Rectangle {
                            x: 0
                            y: 0
                            width: Math.max(0, brightnessSlider.visualPosition * parent.width)
                            height: parent.height
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: "#14c4e7" }
                                GradientStop { position: 1.0; color: "#17f5df" }
                            }
                            antialiasing: true
                        }
                    }
                    handle: Item {
                        x: brightnessSlider.leftPadding + brightnessSlider.visualPosition
                           * (brightnessSlider.availableWidth - width)
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: 20
                        height: 20

                        Rectangle {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: 2
                            width: 18
                            height: 18
                            radius: 9
                            color: "#8c000000"
                        }

                        Shape {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            layer.enabled: true
                            layer.samples: 4
                            ShapePath {
                                strokeWidth: 0
                                fillGradient: RadialGradient {
                                    centerX: 10; centerY: 10
                                    focalX: 10; focalY: 10
                                    centerRadius: 10; focalRadius: 0
                                    GradientStop { position: 0.0; color: "#ffffff" }
                                    GradientStop { position: 0.30; color: "#dbfbff" }
                                    GradientStop { position: 0.68; color: "#8ce6f4" }
                                    GradientStop { position: 1.0; color: "#4387ba" }
                                }
                                startX: 0; startY: 10
                                PathArc { x: 20; y: 10; radiusX: 10; radiusY: 10 }
                                PathArc { x: 0; y: 10; radiusX: 10; radiusY: 10 }
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: "transparent"
                            border.color: "#cce8ffff"
                            border.width: 1
                            antialiasing: true
                        }
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
                color: toggleButton.active ? "#ffffff" : "#a8c2d1"
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
        background: Item {
            anchors.fill: parent

            // Sombra quando ativo
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 2
                radius: 13
                visible: toggleButton.active
                color: "#5900101d"
            }

            // Corpo ativo com gradiente diagonal 135deg
            Shape {
                anchors.fill: parent
                visible: toggleButton.active
                layer.enabled: true
                layer.samples: 4
                ShapePath {
                    strokeWidth: 0
                    fillGradient: LinearGradient {
                        x1: 0; y1: 0
                        x2: toggleButton.width; y2: toggleButton.height
                        GradientStop { position: 0.1389; color: "#3d9fefff" }
                        GradientStop { position: 0.2778; color: "#6e5bbce5" }
                        GradientStop { position: 0.5208; color: "#b819587d" }
                        GradientStop { position: 0.8333; color: "#e010334e" }
                    }
                    startX: 13; startY: 0
                    PathLine { x: toggleButton.width - 13; y: 0 }
                    PathArc { x: toggleButton.width; y: 13; radiusX: 13; radiusY: 13 }
                    PathLine { x: toggleButton.width; y: toggleButton.height - 13 }
                    PathArc { x: toggleButton.width - 13; y: toggleButton.height; radiusX: 13; radiusY: 13 }
                    PathLine { x: 13; y: toggleButton.height }
                    PathArc { x: 0; y: toggleButton.height - 13; radiusX: 13; radiusY: 13 }
                    PathLine { x: 0; y: 13 }
                    PathArc { x: 13; y: 0; radiusX: 13; radiusY: 13 }
                }
            }

            // Inset highlight superior (ativo)
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 1
                width: parent.width - 14
                height: 1
                color: "#52ffffff"
                visible: toggleButton.active
            }

            // Inativo
            Rectangle {
                anchors.fill: parent
                radius: 13
                visible: !toggleButton.active
                color: toggleButton.hovered ? "#2419354d" : "#180d2235"
                border.color: toggleButton.hovered ? "#4a7e9fc0" : "#244d6c82"
                border.width: 1
            }

            // Borda ativa
            Rectangle {
                anchors.fill: parent
                radius: 13
                visible: toggleButton.active
                color: "transparent"
                border.color: "#c7f9ffff"
                border.width: 1.1
            }
        }
    }

    component StatusPill: Rectangle {
        property string label: ""
        property bool active: false
        implicitHeight: 30
        radius: 14
        color: active ? "#250b4460" : "#1a091c2c"
        border.color: active ? "#5edbf5" : "#2c526274"
        border.width: 1
        Row {
            anchors.centerIn: parent
            spacing: 6
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 7
                height: 7
                radius: 3.5
                color: active ? "#17f5df" : "#506e80"
                Rectangle {
                    anchors.centerIn: parent
                    width: 13
                    height: 13
                    radius: 6.5
                    color: "#3017f5df"
                    visible: active
                }
            }
            Text {
                text: parent.parent.label
                color: active ? "#ebf9ff" : "#9db3c1"
                font.pixelSize: 9
                font.bold: active
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: controlCenter.opened
        onActivated: controlCenter.close()
    }
}
