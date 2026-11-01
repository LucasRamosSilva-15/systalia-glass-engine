import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: fileWindow

    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0
    property var controller: null
    property var waylandServer: null
    property real refractiveIndex: 1.25
    property real dispersion: 0.04
    property real caustics: 0.85
    property real surfaceOpacity: 0.85
    property real surfaceSlant: 0
    property real cornerRadius: 20
    property int snapState: 0
    property rect restoreGeometry: Qt.rect(113, 193, 701, 534)
    property vector4d cornerRadii: snapState !== 0
                                   ? LiquidTiling.calculateFusedRadii(snapState, cornerRadius)
                                   : Qt.vector4d(cornerRadius, cornerRadius,
                                                 cornerRadius, cornerRadius)
    property bool materialActive: true
    property bool windowOpen: true
    property string selectedLocation: "Home"
    property string selectedFile: ""

    width: 701
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
        backdropSource: fileWindow.backdropSource
        screenResolution: fileWindow.screenResolution
        radii: fileWindow.cornerRadii
        Behavior on radii {
            PropertyAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
        slant: fileWindow.surfaceSlant
        ior: fileWindow.refractiveIndex
        chromaticDispersion: fileWindow.dispersion
        causticIntensity: fileWindow.caustics
        surfaceOpacity: fileWindow.materialActive ? fileWindow.surfaceOpacity : 0
        time: fileWindow.animationTime
    }

    SystaliaG2Frame {
        anchors.fill: parent
        radii: fileWindow.cornerRadii
        slant: fileWindow.surfaceSlant
        fillGradient: LinearGradient {
            x1: 0
            y1: 0
            x2: fileWindow.width
            y2: fileWindow.height * 0.75
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
            x: -8.92
            y: 99.96
            width: 565.335
            height: 492.371
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-files-prism.png"
            sourceSize: Qt.size(1056, 804)
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
            opacity: 1.85
        }

        // Fita espectral cáustica (dispersão ciano, dourada e violeta)
        Image {
            x: -88.44
            y: 67.92
            width: 599.68
            height: 512.41
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-files-ribbon.png"
            sourceSize: Qt.size(994, 868)
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
            opacity: 2.1
        }

        // Refração de borda violeta-ciano
        Image {
            x: 196.77
            y: 13.5
            width: 449.24
            height: 524.653
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-files-edge.png"
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
        radii: fileWindow.cornerRadii
        slant: fileWindow.surfaceSlant
        strokeColor: "#a8ffffff"
        strokeWidth: 1.25
    }

    SystaliaWindowHeader {
        width: parent.width
        height: 50
        iconGlyph: "▰"
        title: "Systalia Files"
        badgeText: "ABYSSUS VES"
        badgeWidth: 120
        maximized: fileWindow.controller ? fileWindow.controller.filesWindowMaximized : false
        glassSurface: false
        actionsRightMargin: 18.5

        onMinimizeRequested: {
            if (fileWindow.controller)
                fileWindow.controller.minimizeFileWindow()
            else
                fileWindow.visible = false
        }
        onMaximizeRequested: {
            if (fileWindow.controller)
                fileWindow.controller.toggleFileWindowMaximized()
        }
        onCloseRequested: {
            if (fileWindow.controller)
                fileWindow.controller.closeFileWindow()
            else
                fileWindow.visible = false
        }
    }

    Item {
        x: 0
        y: 0
        width: Math.max(200, fileWindow.width - 130)
        height: 49
        DragHandler {
            id: fileWindowDrag
            target: fileWindow
            enabled: fileWindow.controller ? !fileWindow.controller.filesWindowMaximized : true
            onActiveChanged: {
                if (active) {
                    if (fileWindow.controller) {
                        fileWindow.controller.raiseFileWindow()
                        if (fileWindow.snapState !== 0
                                && typeof fileWindow.controller.unsnapShellWindow === "function") {
                            fileWindow.controller.unsnapShellWindow(fileWindow)
                        }
                    }
                } else if (fileWindow.controller
                           && typeof fileWindow.controller.finishShellWindowDrag === "function") {
                    fileWindow.controller.finishShellWindowDrag(
                        fileWindow, fileWindowDrag.centroid.scenePosition)
                }
            }
            onCentroidChanged: {
                if (active && fileWindow.controller
                        && typeof fileWindow.controller.updateShellWindowDrag === "function") {
                    fileWindow.controller.updateShellWindowDrag(
                        fileWindow, fileWindowDrag.centroid.scenePosition)
                }
            }
        }
        TapHandler {
            onTapped: if (fileWindow.controller) fileWindow.controller.raiseFileWindow()
        }
    }

    Rectangle {
        x: 10
        y: 51
        width: 181
        height: Math.max(469, parent.height - 65)
        radius: 14
        color: "#e0081a2d"
        antialiasing: true
    }

    Repeater {
        model: [
            { name: "Recent", icon: "◷", top: 16 },
            { name: "Home", icon: "⌂", top: 50 },
            { name: "Desktop", icon: "▱", top: 84 },
            { name: "Documents", icon: "♧", top: 118 },
            { name: "Downloads", icon: "↓", top: 152 },
            { name: "Music", icon: "♫", top: 186 },
            { name: "Pictures", icon: "▧", top: 220 },
            { name: "Trash", icon: "♜", top: 254 },
            { name: "Other Items", icon: "+", top: 306 }
        ]

        delegate: Item {
            id: navigationEntry
            required property var modelData
            x: 19
            y: 51 + modelData.top - 6
            width: 163
            height: 33

            // Fundo do item selecionado (Active Home / Item no style.css: 135deg, inset shadows, raio 7px)
            Item {
                anchors.fill: parent
                visible: fileWindow.selectedLocation === navigationEntry.modelData.name

                // Sombra de queda base (0px 3px 5px rgba(0, 16, 29, 0.35))
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 2
                    radius: 7
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
                            x2: 163; y2: 33
                            GradientStop { position: 0.1389; color: "#3d9fefff" }
                            GradientStop { position: 0.2778; color: "#6e5bbce5" }
                            GradientStop { position: 0.5208; color: "#b819587d" }
                            GradientStop { position: 0.8333; color: "#e010334e" }
                        }
                        startX: 7; startY: 0
                        PathLine { x: 156; y: 0 }
                        PathArc { x: 163; y: 7; radiusX: 7; radiusY: 7 }
                        PathLine { x: 163; y: 26 }
                        PathArc { x: 156; y: 33; radiusX: 7; radiusY: 7 }
                        PathLine { x: 7; y: 33 }
                        PathArc { x: 0; y: 26; radiusX: 7; radiusY: 7 }
                        PathLine { x: 0; y: 7 }
                        PathArc { x: 7; y: 0; radiusX: 7; radiusY: 7 }
                    }
                }

                // Inset highlight superior (inset 0px 1px 1px rgba(255, 255, 255, 0.32))
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 1
                    width: parent.width - 10
                    height: 1
                    radius: 0.5
                    color: "#52ffffff"
                }

                // Inset shadow inferior (inset 0px -2px 4px rgba(6, 24, 45, 0.4))
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    width: parent.width - 8
                    height: 3
                    radius: 1.5
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0; color: "#0006182d" }
                        GradientStop { position: 1; color: "#6606182d" }
                    }
                }

                // Borda perimetral sutil
                Rectangle {
                    anchors.fill: parent
                    radius: 7
                    color: "transparent"
                    border.color: "#30ffffff"
                    border.width: 0.8
                    antialiasing: true
                }
            }

            Text {
                x: 9
                anchors.verticalCenter: parent.verticalCenter
                width: 150
                text: navigationEntry.modelData.icon + "  " + navigationEntry.modelData.name
                color: fileWindow.selectedLocation === navigationEntry.modelData.name ? "#eef9ff" : "#d5e2ee"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 16
                font.weight: fileWindow.selectedLocation === navigationEntry.modelData.name ? Font.Medium : Font.Normal
                antialiasing: true
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: fileWindow.selectedLocation = navigationEntry.modelData.name
                }
            }
        }
    }

    // Botões de navegação voltar/avançar (style.css: left: 207px, top: 60px, color: rgba(140, 241, 255, 0.92))
    Row {
        x: 207
        y: 60
        spacing: 12

        Text {
            text: "‹"
            color: "#eb8cf1ff"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 26
            font.weight: Font.Bold
            antialiasing: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }

        Text {
            text: "›"
            color: "#668cf1ff"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 26
            font.weight: Font.Bold
            antialiasing: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    // Pasta atual no breadcrumb (style.css: width: 90px, height: 30px, left: 276px, top: 60px, background: rgba(16, 56, 82, 0.85))
    Rectangle {
        x: 276
        y: 60
        width: Math.max(90, folderNameText.implicitWidth + 24)
        height: 30
        radius: 7
        color: "#d9103852"
        border.color: "#28ffffff"
        border.width: 0.8
        antialiasing: true

        Text {
            id: folderNameText
            anchors.centerIn: parent
            text: "⌂  " + fileWindow.selectedLocation
            color: "#e6fbff"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 15
            font.weight: Font.Medium
            antialiasing: true
        }
    }

    // Campo de busca (style.css: width: 157px, height: 28px, left: 526px, top: 61px, background: rgba(10, 37, 53, 0.73))
    TextField {
        id: searchField
        anchors.right: parent.right
        anchors.rightMargin: 18
        y: 61
        width: 157
        height: 28
        placeholderText: "⌕ Search"
        color: SystaliaTheme.text
        placeholderTextColor: "#bdcfdc"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 15
        leftPadding: 14
        topPadding: 0
        bottomPadding: 0
        verticalAlignment: TextInput.AlignVCenter
        background: Rectangle {
            radius: 14
            color: "#ba0a2535"
            border.color: "#26ffffff"
            border.width: 0.8
            antialiasing: true
        }
        selectionColor: "#7d42e9ee"
    }

    // Alternador de visualização em grade/lista (style.css: left: 490px, top: 65px, font-size: 21px, color: #C2EFFF)
    Text {
        anchors.right: searchField.left
        anchors.rightMargin: 6
        y: 62
        width: 30
        height: 28
        text: "≡"
        color: "#c2efff"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 22
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        antialiasing: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }

    // Linha divisória horizontal (style.css: width: 476px, height: 1px, left: 208px, top: 97px, background: rgba(178, 243, 253, 0.3))
    Rectangle {
        x: 208
        y: 97
        width: Math.max(476, parent.width - 226)
        height: 1
        color: "#4db2f3fd"
        antialiasing: true
    }

    GridView {
        id: filesGrid
        x: 216
        y: 115
        width: Math.max(460, parent.width - 235)
        height: Math.max(400, parent.height - 130)
        cellWidth: 118
        cellHeight: 120
        clip: true
        interactive: true
        boundsBehavior: Flickable.StopAtBounds

        model: [
            { name: "All Files", icon: "valo-file-document.png", glyph: "" },
            { name: "Documents", icon: "valo-file-documents-art.png", glyph: "◉" },
            { name: "Document", icon: "valo-file-document.png", glyph: "</>" },
            { name: "Download2", icon: "valo-file-download.png", glyph: "⬇" },
            { name: "Divides 1", icon: "valo-file-list.png", glyph: "≡" },
            { name: "Systion 4", icon: "valo-file-check.png", glyph: "✓" }
        ]

        delegate: Item {
            id: fileEntry
            required property var modelData
            width: filesGrid.cellWidth
            height: filesGrid.cellHeight

            Rectangle {
                anchors.centerIn: parent
                width: 96
                height: 106
                visible: fileWindow.selectedFile === fileEntry.modelData.name
                radius: 8
                color: "#182ae0ff"
                border.color: "#507ce9f4"
                border.width: 1
                antialiasing: true
            }

            Item {
                anchors.centerIn: parent
                width: 96
                height: 106

                Image {
                    x: -7
                    y: -10
                    width: 82
                    height: 87
                    visible: fileEntry.modelData.name === "All Files"
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-file-folder-body.png"
                    sourceSize: Qt.size(512, 543)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                Image {
                    x: -7
                    y: 10
                    width: 90
                    height: 67
                    visible: fileEntry.modelData.name === "All Files"
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-file-folder-lip.png"
                    sourceSize: Qt.size(512, 381)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                Image {
                    x: fileEntry.modelData.name === "Documents" ? 3 : 2
                    y: fileEntry.modelData.name === "Documents" ? -13.5 : -14.5
                    width: fileEntry.modelData.name === "Documents" ? 75 : 77
                    height: fileEntry.modelData.name === "Documents" ? 94 : 96
                    visible: fileEntry.modelData.name !== "All Files"
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + fileEntry.modelData.icon
                    sourceSize: Qt.size(512, 642)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                Text {
                    x: 20
                    y: 16
                    width: 55
                    height: 44
                    visible: fileEntry.modelData.glyph !== ""
                    text: fileEntry.modelData.glyph
                    color: "#8cf1ff"
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: fileEntry.modelData.glyph === "</>" ? 22 : 26
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    antialiasing: true
                }

                Text {
                    x: 0
                    y: 78
                    width: 96
                    height: 26
                    text: fileEntry.modelData.name
                    color: "#eefcff"
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    antialiasing: true
                }
            }

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: fileWindow.selectedFile = fileEntry.modelData.name }
        }
    }

}
