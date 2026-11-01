import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Systalia.Engine
import Systalia.System 1.0

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
    property int snapState: 0
    property rect restoreGeometry: Qt.rect(864, 193, 700, 534)
    property vector4d cornerRadii: snapState !== 0
                                   ? LiquidTiling.calculateFusedRadii(snapState, cornerRadius)
                                   : Qt.vector4d(cornerRadius, cornerRadius,
                                                 cornerRadius, cornerRadius)
    property bool materialActive: true
    property bool windowOpen: true
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
        border.left: 60
        border.top: 60
        border.right: 60
        border.bottom: 100
        horizontalTileMode: BorderImage.Stretch
        verticalTileMode: BorderImage.Stretch
        smooth: true
    }

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: materialWindow.backdropSource
        screenResolution: materialWindow.screenResolution
        radii: materialWindow.cornerRadii
        Behavior on radii {
            PropertyAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
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
            y2: materialWindow.height * 0.75
            GradientStop { position: 0.08; color: "#4d264f76" }
            GradientStop { position: 0.24; color: "#554bafc7" }
            GradientStop { position: 0.52; color: "#48133a5d" }
            GradientStop { position: 0.88; color: "#5509233f" }
        }
    }

    // Camada de dispersão cromática e feixes cáusticos de refração
    Item {
        anchors.fill: parent
        clip: true

        // Prisma interno cáustico (âmbar dourado e ciano)
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
            opacity: 1.85
        }

        // Fita espectral cáustica (dispersão ciano, dourada e violeta)
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
            opacity: 2.1
        }

        // Refração de borda violeta-ciano
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
            opacity: 1.65
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

    SystaliaG2Frame {
        anchors.fill: parent
        radii: materialWindow.cornerRadii
        slant: materialWindow.surfaceSlant
        strokeColor: "#a8ffffff"
        strokeWidth: 1.25
    }

    SystaliaWindowHeader {
        width: parent.width
        height: 50
        iconGlyph: "ϟ"
        title: "Systalia Control Center"
        badgeText: "VALO & SOAR Engine"
        badgeWidth: 172
        titlePixelSize: 20
        iconColor: "#48c4ff"
        titleColor: "#f3fbff"
        badgeTextColor: "#87bada"
        maximized: materialWindow.controller ? materialWindow.controller.materialWindowMaximized : false
        glassSurface: false
        actionsRightMargin: 21.5

        onMinimizeRequested: materialWindow.visible = false
        onMaximizeRequested: {
            if (materialWindow.controller)
                materialWindow.controller.toggleMaterialWindowMaximized()
        }
        onCloseRequested: {
            if (materialWindow.controller)
                materialWindow.controller.closeMaterialWindow()
            else
                materialWindow.visible = false
        }
    }

    Item {
        x: 0
        y: 0
        width: Math.max(200, materialWindow.width - 134)
        height: 49
        DragHandler {
            id: materialWindowDrag
            target: materialWindow
            enabled: materialWindow.controller ? !materialWindow.controller.materialWindowMaximized : true
            onActiveChanged: {
                if (active) {
                    if (materialWindow.controller) {
                        materialWindow.controller.raiseMaterialWindow()
                        if (materialWindow.snapState !== 0
                                && typeof materialWindow.controller.unsnapShellWindow === "function") {
                            materialWindow.controller.unsnapShellWindow(materialWindow)
                        }
                    }
                } else if (materialWindow.controller
                           && typeof materialWindow.controller.finishShellWindowDrag === "function") {
                    materialWindow.controller.finishShellWindowDrag(
                        materialWindow, materialWindowDrag.centroid.scenePosition)
                }
            }
            onCentroidChanged: {
                if (active && materialWindow.controller
                        && typeof materialWindow.controller.updateShellWindowDrag === "function") {
                    materialWindow.controller.updateShellWindowDrag(
                        materialWindow, materialWindowDrag.centroid.scenePosition)
                }
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
            x: 12
            y: 61 + modelData.top - 6
            width: 180
            height: 37

            Item {
                anchors.fill: parent
                visible: navigationEntry.modelData.name === materialWindow.selectedSection

                // Sombra de queda base (0px 3px 5px rgba(0, 16, 29, 0.35))
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 2
                    radius: 8
                    color: "#5900101d"
                }

                // Corpo com gradiente 135deg nativo
                Shape {
                    anchors.fill: parent
                    layer.enabled: true
                    layer.samples: 4

                    ShapePath {
                        strokeWidth: 0
                        fillGradient: LinearGradient {
                            x1: 0; y1: 0
                            x2: 180; y2: 37
                            GradientStop { position: 0.1389; color: "#3d9fefff" }
                            GradientStop { position: 0.2778; color: "#6e5bbce5" }
                            GradientStop { position: 0.5208; color: "#b819587d" }
                            GradientStop { position: 0.8333; color: "#e010334e" }
                        }
                        startX: 8; startY: 0
                        PathLine { x: 172; y: 0 }
                        PathArc { x: 180; y: 8; radiusX: 8; radiusY: 8 }
                        PathLine { x: 180; y: 29 }
                        PathArc { x: 172; y: 37; radiusX: 8; radiusY: 8 }
                        PathLine { x: 8; y: 37 }
                        PathArc { x: 0; y: 29; radiusX: 8; radiusY: 8 }
                        PathLine { x: 0; y: 8 }
                        PathArc { x: 8; y: 0; radiusX: 8; radiusY: 8 }
                    }
                }

                // Inset highlight superior (inset 0px 1px 1px rgba(255, 255, 255, 0.32))
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 1
                    width: parent.width - 12
                    height: 1
                    color: "#52ffffff"
                }

                // Inset shadow inferior (inset 0px -2px 4px rgba(6, 24, 45, 0.4))
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    width: parent.width - 10
                    height: 3
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0; color: "#0006182d" }
                        GradientStop { position: 1; color: "#6606182d" }
                    }
                }

                // Borda perimetral
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: "transparent"
                    border.color: "#30ffffff"
                    border.width: 0.8
                    antialiasing: true
                }
            }

            Text {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 160
                text: navigationEntry.modelData.icon + "  " + navigationEntry.modelData.name
                color: navigationEntry.modelData.name === materialWindow.selectedSection ? "#42e9ee" : "#d4e8ed"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 16
                font.weight: navigationEntry.modelData.name === materialWindow.selectedSection ? Font.Medium : Font.Normal
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
            { label: "▣  Default", top: 380 },
            { label: "⚙  Buttons", top: 425 },
            { label: "Options", top: 470 }
        ]

        delegate: Item {
            id: optionButton
            required property var modelData
            x: 24
            y: optionButton.modelData.top
            width: 165
            height: 32

            // Sombra de queda base (0px 3px 5px rgba(0, 16, 29, 0.35))
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 2
                radius: 15
                color: "#5900101d"
            }

            // Corpo com gradiente 135deg (style.css: Dropdown 0/1/2)
            Shape {
                anchors.fill: parent
                layer.enabled: true
                layer.samples: 4

                ShapePath {
                    strokeWidth: 0
                    fillGradient: LinearGradient {
                        x1: 0; y1: 0
                        x2: 165; y2: 32
                        GradientStop { position: 0.1389; color: "#3d9fefff" }
                        GradientStop { position: 0.2778; color: "#6e5bbce5" }
                        GradientStop { position: 0.5208; color: "#b819587d" }
                        GradientStop { position: 0.8333; color: "#e010334e" }
                    }
                    startX: 15; startY: 0
                    PathLine { x: 150; y: 0 }
                    PathArc { x: 165; y: 15; radiusX: 15; radiusY: 15 }
                    PathLine { x: 165; y: 17 }
                    PathArc { x: 150; y: 32; radiusX: 15; radiusY: 15 }
                    PathLine { x: 15; y: 32 }
                    PathArc { x: 0; y: 17; radiusX: 15; radiusY: 15 }
                    PathLine { x: 0; y: 15 }
                    PathArc { x: 15; y: 0; radiusX: 15; radiusY: 15 }
                }
            }

            // Inset highlight superior (inset 0px 1px 1px rgba(255, 255, 255, 0.32))
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 1
                width: parent.width - 24
                height: 1
                color: "#52ffffff"
            }

            // Inset shadow inferior (inset 0px -2px 4px rgba(6, 24, 45, 0.4))
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 1
                width: parent.width - 20
                height: 3
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0; color: "#0006182d" }
                    GradientStop { position: 1; color: "#6606182d" }
                }
            }

            // Borda do botão
            Rectangle {
                anchors.fill: parent
                radius: 15
                color: "transparent"
                border.color: "#30ffffff"
                border.width: 0.8
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

            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    Rectangle {
        x: 222.5
        y: 57.5
        width: Math.max(454, parent.width - 245)
        height: Math.max(452, parent.height - 75)
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
            anchors.right: parent.right
            anchors.rightMargin: 25
            y: 13
            width: 85
            height: 31
            Accessible.role: Accessible.CheckBox
            Accessible.name: "Ativar material VALO"
            Accessible.checked: materialWindow.materialActive

            // Sombra de queda base do toggle track (0px 3px 5px rgba(0, 16, 29, 0.35))
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 2
                radius: 18
                color: "#5900101d"
            }

            // Trilho com gradiente 135deg nativo
            Shape {
                anchors.fill: parent
                layer.enabled: true
                layer.samples: 4

                ShapePath {
                    strokeWidth: 0
                    fillGradient: LinearGradient {
                        x1: 0; y1: 0
                        x2: 85; y2: 31
                        GradientStop { position: 0.1389; color: "#3d9fefff" }
                        GradientStop { position: 0.2778; color: "#6e5bbce5" }
                        GradientStop { position: 0.5208; color: "#b819587d" }
                        GradientStop { position: 0.8333; color: "#e010334e" }
                    }
                    startX: 15.5; startY: 0
                    PathLine { x: 69.5; y: 0 }
                    PathArc { x: 85; y: 15.5; radiusX: 15.5; radiusY: 15.5 }
                    PathArc { x: 69.5; y: 31; radiusX: 15.5; radiusY: 15.5 }
                    PathLine { x: 15.5; y: 31 }
                    PathArc { x: 0; y: 15.5; radiusX: 15.5; radiusY: 15.5 }
                    PathArc { x: 15.5; y: 0; radiusX: 15.5; radiusY: 15.5 }
                }
            }

            // Inset highlight superior (inset 0px 1px 1px rgba(255, 255, 255, 0.32))
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 1
                width: parent.width - 20
                height: 1
                color: "#52ffffff"
            }

            // Inset shadow inferior (inset 0px -2px 4px rgba(6, 24, 45, 0.4))
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 1
                width: parent.width - 20
                height: 3
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0; color: "#0006182d" }
                    GradientStop { position: 1; color: "#6606182d" }
                }
            }

            // Borda do trilho
            Rectangle {
                anchors.fill: parent
                radius: 18
                color: "transparent"
                border.color: "#30ffffff"
                border.width: 0.8
                antialiasing: true
            }

            // Manípulo esférico (Knob) de 23px x 23px com gradiente radial do style.css
            Item {
                x: materialWindow.materialActive ? 58 : 4
                y: 4
                width: 23
                height: 23
                Behavior on x {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }

                // Sombra do manípulo (0px 2px 4px rgba(0, 0, 0, 0.55))
                Rectangle {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 2
                    width: 21
                    height: 21
                    radius: 10.5
                    color: "#8c000000"
                }

                // Esfera 3D via RadialGradient
                Shape {
                    anchors.centerIn: parent
                    width: 23
                    height: 23
                    layer.enabled: true
                    layer.samples: 4

                    ShapePath {
                        strokeWidth: 0
                        fillGradient: RadialGradient {
                            centerX: 11.5
                            centerY: 11.5
                            focalX: 11.5
                            focalY: 11.5
                            centerRadius: 11.5
                            focalRadius: 0
                            // #FFFFFF 0%, #DBFBFF 30%, #8CE6F4 68%, #4387BA 100%
                            GradientStop { position: 0.0; color: "#ffffff" }
                            GradientStop { position: 0.30; color: "#dbfbff" }
                            GradientStop { position: 0.68; color: "#8ce6f4" }
                            GradientStop { position: 1.0; color: "#4387ba" }
                        }
                        startX: 0
                        startY: 11.5
                        PathArc { x: 23; y: 11.5; radiusX: 11.5; radiusY: 11.5 }
                        PathArc { x: 0; y: 11.5; radiusX: 11.5; radiusY: 11.5 }
                    }
                }

                // Borda do manípulo (border: 1px solid rgba(232, 255, 255, 0.8))
                Rectangle {
                    anchors.fill: parent
                    radius: 11.5
                    color: "transparent"
                    border.color: "#cce8ffff"
                    border.width: 1
                    antialiasing: true
                }
            }

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: materialWindow.materialActive = !materialWindow.materialActive }
        }

        SettingSlider {
            x: 22
            y: 55
            width: parent.width - 44
            label: "Backdrop Blur Filter"
            value: materialWindow.refractiveIndex
            from: 1.0
            to: 1.5
            valueText: Math.round(52 + (value - 1.0) * 26) + "%"
            onChanged: val => materialWindow.refractiveIndex = val
        }
        SettingSlider {
            x: 22
            y: 119
            width: parent.width - 44
            label: "Glass Layer Opacity"
            value: materialWindow.surfaceOpacity
            from: 0.2
            to: 1.0
            valueText: Math.round(value * 100).toString()
            onChanged: val => materialWindow.surfaceOpacity = val
        }
        SettingSlider {
            x: 22
            y: 183
            width: parent.width - 44
            label: "Glass Layer Opacity · Secondary"
            value: 0.75
            from: 0.2
            to: 1.0
            valueText: Math.round(value * 100) + "%"
        }
        SettingSlider {
            x: 22
            y: 247
            width: parent.width - 44
            label: "Ambient Halo Dispersion"
            value: materialWindow.dispersion
            from: 0
            to: 0.08
            valueText: value > 0.01 ? "On" : "Off"
            onChanged: val => materialWindow.dispersion = val
        }
        SettingSlider {
            x: 22
            y: 311
            width: parent.width - 44
            label: "Ambient Halo Filters"
            value: materialWindow.caustics
            from: 0
            to: 1.5
            valueText: Math.round(value * 12).toString()
            onChanged: val => materialWindow.caustics = val
        }
        SettingSlider {
            x: 22
            y: 375
            width: parent.width - 44
            label: "Ambient Halo Dispersion · Strength"
            value: 1.0
            from: 0.2
            to: 1.0
            valueText: Math.round(value * 100) + "%"
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
        width: 410
        height: 50

        Text {
            x: 0
            y: 0
            width: 320
            height: 24
            text: settingRow.label
            color: "#e4f7ff"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 16
            antialiasing: true
        }

        // Valor numérico (ex: 85%, 1.25, etc.) alinhado perfeitamente dentro do painel
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 0
            y: 20
            width: 55
            height: 24
            text: settingRow.valueText
            color: "#d9ecf7"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 15
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            antialiasing: true
        }

        Slider {
            id: slider
            x: 0
            y: 20
            width: Math.max(100, parent.width - 65)
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
                radius: 8
                // Fundo do trilho conforme style.css:
                // background: linear-gradient(135deg, rgba(3, 21, 41, 0.94) 13.89%, rgba(12, 39, 62, 0.96) 48.61%, rgba(23, 57, 85, 0.83) 83.33%);
                // border: 0.7px solid rgba(115, 186, 218, 0.26);
                // box-shadow: inset 0px -1px 1px rgba(230, 255, 255, 0.12), inset 0px 2px 3px rgba(0, 0, 0, 0.64);
                color: "#f0031529"
                border.color: "#4273bada"
                border.width: 0.7
                antialiasing: true

                // Inset shadow de profundidade interna no trilho
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: "transparent"
                    border.color: "#50000000"
                    border.width: 1
                }

                // Progresso preenchido conforme style.css:
                // background: linear-gradient(180deg, #14C4E7 0%, #17F5DF 100%);
                Rectangle {
                    x: 0
                    y: 0
                    width: Math.max(0, slider.visualPosition * parent.width)
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

            // Manípulo esférico 3D (Thumb) de 20px x 20px conforme style.css:
            // background: radial-gradient(50% 50% at 50% 50%, #FFFFFF 0%, #DBFBFF 30%, #8CE6F4 68%, #4387BA 100%);
            // border: 1px solid rgba(232, 255, 255, 0.8);
            // box-shadow: 0px 2px 4px rgba(0, 0, 0, 0.55), inset 0px 1px 1px rgba(255, 255, 255, 0.5);
            handle: Item {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.availableHeight / 2 - height / 2
                width: 20
                height: 20

                // Sombra de queda base (0px 2px 4px rgba(0, 0, 0, 0.55))
                Rectangle {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 2
                    width: 18
                    height: 18
                    radius: 9
                    color: "#8c000000"
                }

                // Esfera 3D via RadialGradient
                Shape {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    layer.enabled: true
                    layer.samples: 4

                    ShapePath {
                        strokeWidth: 0
                        fillGradient: RadialGradient {
                            centerX: 10
                            centerY: 10
                            focalX: 10
                            focalY: 10
                            centerRadius: 10
                            focalRadius: 0
                            GradientStop { position: 0.0; color: "#ffffff" }
                            GradientStop { position: 0.30; color: "#dbfbff" }
                            GradientStop { position: 0.68; color: "#8ce6f4" }
                            GradientStop { position: 1.0; color: "#4387ba" }
                        }
                        startX: 0
                        startY: 10
                        PathArc { x: 20; y: 10; radiusX: 10; radiusY: 10 }
                        PathArc { x: 0; y: 10; radiusX: 10; radiusY: 10 }
                    }
                }

                // Inset highlight no topo do manípulo (inset 0px 1px 1px rgba(255, 255, 255, 0.5))
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 1
                    width: 14
                    height: 6
                    radius: 3
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#80ffffff" }
                        GradientStop { position: 1.0; color: "#00ffffff" }
                    }
                }

                // Borda perimetral (border: 1px solid rgba(232, 255, 255, 0.8))
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

}
