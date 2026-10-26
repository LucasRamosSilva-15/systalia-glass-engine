import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine

Item {
    id: materialWindow

    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0
    property var controller: null
    property real refractiveIndex: 1.25
    property real dispersion: 0.04
    property real caustics: 0.85
    property real surfaceOpacity: 0.85
    property real surfaceSlant: 0
    property real cornerRadius: 20
    property bool materialActive: true

    width: 700
    height: 534
    visible: true

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: materialWindow.backdropSource
        screenResolution: materialWindow.screenResolution
        radii: Qt.vector4d(materialWindow.cornerRadius, materialWindow.cornerRadius,
                            materialWindow.cornerRadius, materialWindow.cornerRadius)
        slant: materialWindow.surfaceSlant
        ior: materialWindow.refractiveIndex
        chromaticDispersion: materialWindow.dispersion
        causticIntensity: materialWindow.caustics
        surfaceOpacity: materialWindow.materialActive ? materialWindow.surfaceOpacity : 0
        time: materialWindow.animationTime
    }

    Rectangle {
        anchors.fill: parent
        radius: materialWindow.cornerRadius
        color: "#490b2039"
        border.color: SystaliaTheme.border
        border.width: 1.2
        antialiasing: true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 62
        radius: materialWindow.cornerRadius
        color: "#250d2844"
        antialiasing: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#497bb9d1"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 19
            anchors.rightMargin: 15
            spacing: 11

            Image {
                source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/systalia-mark.svg"
                sourceSize: Qt.size(30, 30)
                width: 30
                height: 30
                smooth: true
                mipmap: true
            }

            ColumnLayout {
                spacing: 1
                Text {
                    text: "Systalia Control Center"
                    color: SystaliaTheme.text
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    antialiasing: true
                }
                Text {
                    text: "VALO & SOAR Engine"
                    color: SystaliaTheme.textMuted
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 9
                    font.letterSpacing: 1.15
                    antialiasing: true
                }
            }

            Item { Layout.fillWidth: true }

            WindowAction {
                accessibleName: "Minimizar Control Center"
                glyph: "−"
                onClicked: materialWindow.visible = false
            }
            WindowAction {
                accessibleName: "Maximizar Control Center"
                glyph: "□"
                onClicked: if (materialWindow.controller) materialWindow.controller.toggleMaterialWindowMaximized()
            }
            WindowAction {
                accessibleName: "Fechar Control Center"
                glyph: "×"
                closeAction: true
                onClicked: {
                    if (materialWindow.controller)
                        materialWindow.controller.closeMaterialWindow()
                    else
                        materialWindow.visible = false
                }
            }
        }

        DragHandler {
            target: materialWindow
            enabled: materialWindow.controller ? !materialWindow.controller.materialWindowMaximized : true
            onActiveChanged: {
                if (active && materialWindow.controller)
                    materialWindow.controller.raiseMaterialWindow()
            }
        }
        TapHandler {
            onTapped: if (materialWindow.controller) materialWindow.controller.raiseMaterialWindow()
        }
    }

    Rectangle {
        id: settingsNavigation
        x: 15
        y: 75
        width: 174
        height: materialWindow.height - y - 15
        radius: 17
        color: "#75112640"
        border.color: "#5178b4cf"
        border.width: 1
        antialiasing: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Text {
                text: "SYSTEM"
                color: SystaliaTheme.textMuted
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.1
                Layout.leftMargin: 8
                Layout.bottomMargin: 5
                antialiasing: true
            }

            Repeater {
                model: [
                    { name: "General", icon: "◉" },
                    { name: "Status", icon: "⌁" },
                    { name: "Control Center", icon: "◈" },
                    { name: "Software", icon: "▧" },
                    { name: "Drivers", icon: "⌘" },
                    { name: "About", icon: "ⓘ" }
                ]

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    height: 37
                    radius: 9
                    color: modelData.name === "Control Center" ? "#3d2bd4e8" : navHover.hovered ? "#1cffffff" : "transparent"
                    border.color: modelData.name === "Control Center" ? "#697af2fa" : "transparent"
                    border.width: 1
                    antialiasing: true

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 11
                        spacing: 10
                        Text {
                            text: modelData.icon
                            color: modelData.name === "Control Center" ? SystaliaTheme.aqua : "#9fc4dc"
                            font.pixelSize: 15
                            Layout.preferredWidth: 17
                            horizontalAlignment: Text.AlignHCenter
                            antialiasing: true
                        }
                        Text {
                            text: modelData.name
                            color: modelData.name === "Control Center" ? SystaliaTheme.text : SystaliaTheme.textSecondary
                            font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 16
                            font.weight: modelData.name === "Control Center" ? Font.DemiBold : Font.Normal
                            antialiasing: true
                        }
                    }
                    HoverHandler { id: navHover }
                }
            }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#387ba4c1"
            }
            Text {
                text: "SYSTALIA  ·  VALO"
                color: SystaliaTheme.textMuted
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 9
                font.letterSpacing: .7
                Layout.leftMargin: 7
                antialiasing: true
            }
        }
    }

    Item {
        id: settingsContent
        x: 207
        y: 75
        width: materialWindow.width - x - 14
        height: materialWindow.height - y - 15

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 43
                ColumnLayout {
                    spacing: 1
                    Text {
                        text: "Live Material Controls"
                        color: SystaliaTheme.text
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        antialiasing: true
                    }
                    Text {
                        text: "VALO optical surface parameters"
                        color: SystaliaTheme.textMuted
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 12
                        antialiasing: true
                    }
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 9
                    height: 9
                    radius: 5
                    color: materialWindow.materialActive ? "#6df1c0" : "#8aa3b8"
                    antialiasing: true
                }
                Text {
                    text: materialWindow.materialActive ? "LIVE" : "PAUSED"
                    color: materialWindow.materialActive ? "#8ef4d0" : SystaliaTheme.textMuted
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: .8
                    antialiasing: true
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                radius: 11
                color: "#29152d4b"
                border.color: "#407eafc9"
                border.width: 1
                antialiasing: true

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 13
                    anchors.rightMargin: 10
                    Text {
                        text: "VALO material"
                        color: SystaliaTheme.textSecondary
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        antialiasing: true
                    }
                    Text {
                        text: materialWindow.materialActive ? "On" : "Off"
                        color: materialWindow.materialActive ? SystaliaTheme.aqua : SystaliaTheme.textMuted
                        font.pixelSize: 10
                        antialiasing: true
                    }
                    Switch {
                        id: materialSwitch
                        checked: materialWindow.materialActive
                        onToggled: materialWindow.materialActive = checked
                        implicitWidth: 42
                        implicitHeight: 26
                        indicator: Rectangle {
                            x: (materialSwitch.width - width) / 2
                            y: (materialSwitch.height - height) / 2
                            width: 38
                            height: 22
                            radius: 12
                            color: materialSwitch.checked ? "#208cabc2" : "#38496178"
                            border.color: materialSwitch.checked ? "#9673eaf0" : "#516f8798"
                            border.width: 1
                            antialiasing: true
                            Rectangle {
                                x: materialSwitch.checked ? parent.width - width - 3 : 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                radius: 8
                                color: materialSwitch.checked ? "#55eced" : "#abc4d4"
                                border.color: "#c7faff"
                                border.width: 1
                                antialiasing: true
                                Behavior on x { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                            }
                        }
                        background: Item {}
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                SettingSlider {
                    Layout.fillWidth: true
                    label: "Glass layer opacity"
                    value: materialWindow.surfaceOpacity
                    from: 0.2
                    to: 1
                    valueText: Math.round(materialWindow.surfaceOpacity * 100) + "%"
                    onChanged: materialWindow.surfaceOpacity = value
                }
                SettingSlider {
                    Layout.fillWidth: true
                    label: "Refractive index (IOR)"
                    value: materialWindow.refractiveIndex
                    from: 1
                    to: 1.5
                    valueText: materialWindow.refractiveIndex.toFixed(2)
                    onChanged: materialWindow.refractiveIndex = value
                }
                SettingSlider {
                    Layout.fillWidth: true
                    label: "Chromatic dispersion"
                    value: materialWindow.dispersion
                    from: 0
                    to: 0.08
                    valueText: materialWindow.dispersion.toFixed(3)
                    onChanged: materialWindow.dispersion = value
                }
                SettingSlider {
                    Layout.fillWidth: true
                    label: "Caustic intensity"
                    value: materialWindow.caustics
                    from: 0
                    to: 1.5
                    valueText: materialWindow.caustics.toFixed(2)
                    onChanged: materialWindow.caustics = value
                }
                SettingSlider {
                    Layout.fillWidth: true
                    label: "Corner radius"
                    value: materialWindow.cornerRadius
                    from: 10
                    to: 34
                    valueText: Math.round(materialWindow.cornerRadius) + " px"
                    onChanged: materialWindow.cornerRadius = value
                }
                SettingSlider {
                    Layout.fillWidth: true
                    label: "Surface slant"
                    value: materialWindow.surfaceSlant
                    from: 0
                    to: 0.2
                    valueText: materialWindow.surfaceSlant.toFixed(2)
                    onChanged: materialWindow.surfaceSlant = value
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                radius: 8
                color: "#1f213f5b"
                border.color: "#316e9cb8"
                border.width: 1
                antialiasing: true
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    spacing: 7
                    Text { text: "✧"; color: SystaliaTheme.aqua; font.pixelSize: 12; antialiasing: true }
                    Text {
                        text: "Edge smoothing  ·  adaptive SDF fwidth"
                        color: SystaliaTheme.textMuted
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 9
                        antialiasing: true
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    component SettingSlider: ColumnLayout {
        id: settingRow
        property string label: ""
        property real value: 0
        property real from: 0
        property real to: 1
        property string valueText: ""
        signal changed(real value)
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: settingRow.label
                color: SystaliaTheme.textSecondary
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 14
                Layout.fillWidth: true
                antialiasing: true
            }
            Text {
                text: settingRow.valueText
                color: SystaliaTheme.text
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 14
                font.weight: Font.DemiBold
                antialiasing: true
            }
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            implicitHeight: 24
            from: settingRow.from
            to: settingRow.to
            value: settingRow.value
            onMoved: settingRow.changed(value)
            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 4
                radius: 2
                color: "#4771a3bc"
                antialiasing: true
                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#14c4e7" }
                        GradientStop { position: 1; color: "#17f5df" }
                    }
                    antialiasing: true
                }
            }
            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 13
                height: 13
                radius: 7
                color: "#dffcff"
                border.color: "#61dce8"
                border.width: 1.5
                antialiasing: true
            }
        }
    }

    component WindowAction: ToolButton {
        id: action
        property string glyph: "•"
        property string accessibleName: "Ação da janela"
        property bool closeAction: false
        Accessible.name: accessibleName
        Layout.preferredWidth: 27
        Layout.preferredHeight: 25
        hoverEnabled: true
        contentItem: Text {
            text: action.glyph
            color: action.closeAction && action.hovered ? "#fff" : SystaliaTheme.textSecondary
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            antialiasing: true
        }
        background: Rectangle {
            radius: 8
            color: action.closeAction && action.hovered ? "#c94f5d78" : action.hovered ? "#4373b8d2" : "#1e8bb6ce"
            border.color: action.hovered ? "#768ac9dc" : "#3189b6ce"
            border.width: 1
            antialiasing: true
        }
    }
}
