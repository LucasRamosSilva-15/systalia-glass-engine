import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
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
    property vector4d cornerRadii: Qt.vector4d(cornerRadius, cornerRadius,
                                                cornerRadius, cornerRadius)
    property bool materialActive: true
    property string selectedSection: "Control Center"

    width: 700
    height: 534
    visible: true
    clip: false

    BorderImage {
        x: -60
        y: -60
        width: parent.width + 120
        height: parent.height + 160
        z: -1
        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-window-shadow.png"
        sourceSize: Qt.size(1642, 1388)
        border.left: 60
        border.top: 60
        border.right: 60
        border.bottom: 100
        horizontalTileMode: BorderImage.Stretch
        verticalTileMode: BorderImage.Stretch
        smooth: true
        mipmap: true
    }

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: materialWindow.backdropSource
        screenResolution: materialWindow.screenResolution
        radii: materialWindow.cornerRadii
        slant: materialWindow.surfaceSlant
        ior: materialWindow.refractiveIndex
        chromaticDispersion: materialWindow.dispersion
        causticIntensity: materialWindow.caustics
        surfaceOpacity: materialWindow.materialActive ? materialWindow.surfaceOpacity : 0
        time: materialWindow.animationTime
    }

    SystaliaG2Frame {
        anchors.fill: parent
        radii: materialWindow.cornerRadii
        slant: materialWindow.surfaceSlant
        fillGradient: LinearGradient {
            x1: 0
            y1: 0
            x2: materialWindow.width
            y2: 0
            GradientStop { position: 0.0; color: "#63264f76" }
            GradientStop { position: 0.24; color: "#754bafc7" }
            GradientStop { position: 0.52; color: "#63133a5d" }
            GradientStop { position: 1.0; color: "#6809233f" }
        }
    }

    Rectangle {
        x: 4.5
        y: 2.5
        width: parent.width - 9
        height: parent.height - 10
        radius: 14
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

    Item {
        anchors.fill: parent
        clip: true

        Image {
            x: -9.11
            y: 99.96
            width: 564.729
            height: 492.062
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-control-prism.png"
            sourceSize: Qt.size(1054, 804)
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }

        Image {
            x: -88.52
            y: 67.92
            width: 599.029
            height: 512.018
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-control-ribbon.png"
            sourceSize: Qt.size(992, 870)
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }

        Image {
            x: 196.23
            y: 13.5
            width: 448.856
            height: 524.482
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-control-edge.png"
            sourceSize: Qt.size(764, 962)
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }
    }

    SystaliaG2Frame {
        anchors.fill: parent
        radii: materialWindow.cornerRadii
        slant: materialWindow.surfaceSlant
        strokeColor: "#c9ffffff"
        strokeWidth: 1.5
    }

    Text {
        x: 16.5
        y: 8.5
        width: 32
        height: 36
        text: "ϟ"
        color: "#48c4ff"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 23
        font.weight: Font.Bold
        antialiasing: true
    }

    Text {
        x: 48.5
        y: 9
        width: 275
        height: 36
        text: "Systalia Control Center"
        color: "#f3fbff"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 20
        font.weight: Font.Medium
        antialiasing: true
    }

    Rectangle {
        x: 273.5
        y: 12.5
        width: 172
        height: 25
        radius: 6
        color: "#c70c314f"
        antialiasing: true
        Text {
            anchors.centerIn: parent
            text: "VALO & SOAR Engine"
            color: "#87bada"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 14
            antialiasing: true
        }
    }

    WindowAction {
        x: 574.5
        y: 12.5
        accessibleName: "Minimizar Control Center"
        glyph: "−"
        onClicked: materialWindow.visible = false
    }
    WindowAction {
        x: 612.5
        y: 12.5
        accessibleName: "Maximizar Control Center"
        glyph: materialWindow.controller && materialWindow.controller.materialWindowMaximized ? "❐" : "□"
        onClicked: {
            if (materialWindow.controller)
                materialWindow.controller.toggleMaterialWindowMaximized()
        }
    }
    WindowAction {
        x: 650.5
        y: 12.5
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

    Item {
        x: 0
        y: 0
        width: 566
        height: 49
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

    Repeater {
        model: [
            { name: "General", icon: "⚙", top: 6 },
            { name: "Status", icon: "♙", top: 50 },
            { name: "Control Center", icon: "☷", top: 94 },
            { name: "Software", icon: "⚙", top: 138 },
            { name: "Drivers", icon: "▣", top: 182 },
            { name: "About", icon: "ⓘ", top: 226 }
        ]

        delegate: Item {
            id: navigationEntry
            required property var modelData
            x: 10.5
            y: 59.5 + modelData.top
            width: 197
            height: modelData.name === materialWindow.selectedSection ? 37 : 28

            Rectangle {
                x: -0.5
                y: modelData.name === materialWindow.selectedSection ? -7 : 0
                width: 180
                height: 37
                visible: navigationEntry.modelData.name === materialWindow.selectedSection
                radius: 8
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0; color: "#3d9fefff" }
                    GradientStop { position: 0.5; color: "#6e19587d" }
                    GradientStop { position: 1; color: "#e010334e" }
                }
                border.color: "#c7f9ffff"
                border.width: 1
                antialiasing: true
            }

            Text {
                x: 10
                y: 0
                width: 181
                height: 27
                text: navigationEntry.modelData.icon + "  " + navigationEntry.modelData.name
                color: navigationEntry.modelData.name === materialWindow.selectedSection ? "#42e9ee" : "#d4e8ed"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 16
                antialiasing: true
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: materialWindow.selectedSection = navigationEntry.modelData.name
                }
            }
        }
    }

    Rectangle {
        x: 204.5
        y: 62.5
        width: 1
        height: 440
        color: "#47c8f7fb"
        antialiasing: true
    }

    Text {
        x: 23.5
        y: 347.5
        width: 165
        height: 29
        text: "Options"
        color: "#dceff7"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 17
        antialiasing: true
    }

    Repeater {
        model: [
            { label: "▣  Default", top: 378.5 },
            { label: "⚙  Buttons", top: 423.5 },
            { label: "Options", top: 468.5 }
        ]

        delegate: Rectangle {
            id: optionButton
            required property var modelData
            x: 22.5
            y: optionButton.modelData.top
            width: 165
            height: 32
            radius: 15
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0; color: "#3d9fefff" }
                GradientStop { position: 0.5; color: "#6e19587d" }
                GradientStop { position: 1; color: "#e010334e" }
            }
            border.color: "#bff9ffff"
            border.width: 1
            antialiasing: true

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 14
                color: "transparent"
                border.color: "#2bffffff"
                border.width: 0.7
                antialiasing: true
            }
            Text {
                x: 17
                anchors.verticalCenter: parent.verticalCenter
                text: optionButton.modelData.label
                color: "#e9f8fc"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 15
                antialiasing: true
            }
        }
    }

    Rectangle {
        x: 222.5
        y: 57.5
        width: 454
        height: 452
        radius: 13
        color: "#c2071d32"
        antialiasing: true

        Text {
            x: 22
            y: 14
            width: 200
            height: 36
            text: "Options"
            color: "#f1faff"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 21
            font.weight: Font.Medium
            antialiasing: true
        }

        Item {
            x: 344
            y: 13
            width: 85
            height: 31
            Accessible.role: Accessible.CheckBox
            Accessible.name: "Ativar material VALO"
            Accessible.checked: materialWindow.materialActive

            Rectangle {
                anchors.fill: parent
                radius: 18
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0; color: "#3d9fefff" }
                    GradientStop { position: 0.5; color: "#6e19587d" }
                    GradientStop { position: 1; color: "#e010334e" }
                }
                border.color: "#c7f9ffff"
                border.width: 1
                antialiasing: true
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: 17
                    color: "transparent"
                    border.color: "#30ffffff"
                    border.width: 0.7
                    antialiasing: true
                }
            }

            Rectangle {
                x: materialWindow.materialActive ? 60 : 4
                y: 4
                width: 23
                height: 23
                radius: 12
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0; color: "#efffffff" }
                    GradientStop { position: 1; color: "#a8d9f4ff" }
                }
                border.color: "#d9ffffff"
                border.width: 1
                antialiasing: true
                Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
            }

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: materialWindow.materialActive = !materialWindow.materialActive }
        }

        SettingSlider {
            x: 22
            y: 55
            width: 420
            label: "Glass Layer Opacity"
            value: materialWindow.surfaceOpacity
            from: 0.2
            to: 1
            valueText: Math.round(materialWindow.surfaceOpacity * 100) + "%"
            onChanged: materialWindow.surfaceOpacity = value
        }
        SettingSlider {
            x: 22
            y: 119
            width: 420
            label: "Refractive Index (IOR)"
            value: materialWindow.refractiveIndex
            from: 1
            to: 1.5
            valueText: materialWindow.refractiveIndex.toFixed(2)
            onChanged: materialWindow.refractiveIndex = value
        }
        SettingSlider {
            x: 22
            y: 183
            width: 420
            label: "Chromatic Dispersion"
            value: materialWindow.dispersion
            from: 0
            to: 0.08
            valueText: materialWindow.dispersion.toFixed(3)
            onChanged: materialWindow.dispersion = value
        }
        SettingSlider {
            x: 22
            y: 247
            width: 420
            label: "Caustic Intensity"
            value: materialWindow.caustics
            from: 0
            to: 1.5
            valueText: materialWindow.caustics.toFixed(2)
            onChanged: materialWindow.caustics = value
        }
        SettingSlider {
            x: 22
            y: 311
            width: 420
            label: "Corner Radius"
            value: materialWindow.cornerRadius
            from: 10
            to: 34
            valueText: Math.round(materialWindow.cornerRadius) + " px"
            onChanged: materialWindow.cornerRadius = value
        }
        SettingSlider {
            x: 22
            y: 375
            width: 420
            label: "Surface Slant"
            value: materialWindow.surfaceSlant
            from: 0
            to: 0.2
            valueText: materialWindow.surfaceSlant.toFixed(2)
            onChanged: materialWindow.surfaceSlant = value
        }
    }

    component SettingSlider: Item {
        id: settingRow
        property string label: ""
        property real value: 0
        property real from: 0
        property real to: 1
        property string valueText: ""
        signal changed(real value)
        height: 50

        Text {
            x: 0
            y: 0
            width: 345
            height: 24
            text: settingRow.label
            color: "#e4f7ff"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 16
            antialiasing: true
        }

        Text {
            x: 399
            y: 25
            width: 50
            height: 26
            text: settingRow.valueText
            color: "#d9ecf7"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 15
            horizontalAlignment: Text.AlignRight
            antialiasing: true
        }

        Slider {
            id: slider
            x: 0
            y: 25
            width: 367
            height: 24
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0
            from: settingRow.from
            to: settingRow.to
            value: settingRow.value
            Accessible.name: settingRow.label
            onMoved: settingRow.changed(value)

            background: Rectangle {
                x: slider.leftPadding
                y: slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 12
                radius: 7
                color: "#f0031529"
                border.color: "#4273bada"
                border.width: 0.7
                antialiasing: true

                Rectangle {
                    x: 1
                    y: 1
                    width: Math.max(0, slider.visualPosition * (parent.width - 2))
                    height: parent.height - 2
                    radius: 6
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
                y: slider.availableHeight / 2 - height / 2
                width: 20
                height: 20
                radius: 10
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0; color: "#f0ffffff" }
                    GradientStop { position: 1; color: "#a8d9f4ff" }
                }
                border.color: "#d8ffffff"
                border.width: 1
                antialiasing: true
                Rectangle {
                    anchors.centerIn: parent
                    width: 7
                    height: 7
                    radius: 4
                    color: "#55c8dff2"
                    antialiasing: true
                }
            }
        }
    }

    component WindowAction: ToolButton {
        id: action
        property string glyph: "•"
        property string accessibleName: "Ação da janela"
        property bool closeAction: false
        Accessible.name: accessibleName
        width: 27
        height: 27
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: accessibleName
        ToolTip.delay: 450

        background: Rectangle {
            anchors.centerIn: parent
            width: 27
            height: 27
            color: "transparent"
            Image {
                anchors.centerIn: parent
                width: 38
                height: 38
                source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-window-action-"
                        + (action.closeAction ? "close" : action.glyph === "−" ? "minimize" : "maximize")
                        + "-bg.png"
                sourceSize: Qt.size(128, 128)
                smooth: true
                mipmap: true
                opacity: action.hovered ? 0.92 : 1
            }
        }

        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-window-"
                        + (action.closeAction ? "close" : action.glyph === "−" ? "minimize" : "maximize")
                        + ".png"
                sourceSize: Qt.size(512, 512)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                opacity: action.hovered ? 0.92 : 1
            }
        }
    }
}
