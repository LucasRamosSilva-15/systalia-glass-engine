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

    ValoGlass {
        id: glassChassis
        anchors.fill: parent
        active: fileWindow.visible
        backdropSource: fileWindow.backdropSource
        screenResolution: fileWindow.screenResolution
        titleBarHeight: 36
        cornerRadii: fileWindow.cornerRadii
        slant: fileWindow.surfaceSlant
        refractionStrength: Math.max(0.0, (fileWindow.refractiveIndex - 1.0) / 0.25)
        chromaticDispersion: fileWindow.dispersion
        time: fileWindow.animationTime
    }

    SystaliaWindowHeader {
        width: parent.width
        height: 50
        iconGlyph: "▰"
        title: "Systalia Files"
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
            { name: "Recent", icon: "valo-sidebar-recent.svg", top: 16 },
            { name: "Home", icon: "valo-sidebar-home.svg", top: 50 },
            { name: "Desktop", icon: "valo-sidebar-desktop.svg", top: 84 },
            { name: "Documents", icon: "valo-sidebar-documents.svg", top: 118 },
            { name: "Downloads", icon: "valo-sidebar-downloads.svg", top: 152 },
            { name: "Music", icon: "valo-sidebar-music.svg", top: 186 },
            { name: "Pictures", icon: "valo-sidebar-pictures.svg", top: 220 },
            { name: "Trash", icon: "valo-sidebar-trash.svg", top: 254 },
            { name: "Other Items", icon: "valo-sidebar-other.svg", top: 306 }
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

            Image {
                id: navIcon
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + navigationEntry.modelData.icon
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                antialiasing: true
            }

            Text {
                x: 38
                anchors.verticalCenter: parent.verticalCenter
                width: 116
                text: navigationEntry.modelData.name
                color: fileWindow.selectedLocation === navigationEntry.modelData.name ? "#eef9ff" : "#d5e2ee"
                font.family: ValoTheme.fontFamily
                font.pixelSize: 15
                font.weight: fileWindow.selectedLocation === navigationEntry.modelData.name ? Font.Medium : Font.Normal
                elide: Text.ElideRight
                antialiasing: true
            }

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler {
                onTapped: fileWindow.selectedLocation = navigationEntry.modelData.name
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
            font.family: ValoTheme.fontFamily
            font.pixelSize: 26
            font.weight: Font.Bold
            antialiasing: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }

        Text {
            text: "›"
            color: "#668cf1ff"
            font.family: ValoTheme.fontFamily
            font.pixelSize: 26
            font.weight: Font.Bold
            antialiasing: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    // Pasta atual no breadcrumb (style.css: width: 90px, height: 30px, left: 276px, top: 60px, background: rgba(16, 56, 82, 0.85))
    Rectangle {
        x: 275
        y: 59
        width: Math.max(90, breadcrumbRow.implicitWidth + 24)
        height: 30
        radius: 7
        color: "#d9103852"
        border.color: "#28ffffff"
        border.width: 0.8
        antialiasing: true

        Row {
            id: breadcrumbRow
            anchors.centerIn: parent
            spacing: 8

            Image {
                source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-breadcrumb-home.svg"
                width: 12
                height: 13
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }

            Text {
                id: folderNameText
                text: fileWindow.selectedLocation
                color: "#e6fbff"
                font.family: ValoTheme.fontFamily
                font.pixelSize: 15
                font.weight: Font.Medium
                anchors.verticalCenter: parent.verticalCenter
                antialiasing: true
            }
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
        color: ValoTheme.textPrimary
        placeholderTextColor: "#bdcfdc"
        font.family: ValoTheme.fontFamily
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
    Image {
        id: filesMenuButton
        anchors.right: searchField.left
        anchors.rightMargin: 10
        y: 61
        width: 24
        height: 24
        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-files-menu.svg"
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
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
            { name: "All Files", icon: "valo-file-folder.svg" },
            { name: "Documents", icon: "valo-file-documents-art.svg" },
            { name: "Document", icon: "valo-file-document.svg" },
            { name: "Download2", icon: "valo-file-download.svg" },
            { name: "Divides 1", icon: "valo-file-list.svg" },
            { name: "Systion 4", icon: "valo-file-check.svg" }
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
                    id: fileIconImg
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: fileEntry.modelData.name === "All Files" ? 2 : 0
                    width: fileEntry.modelData.name === "All Files" ? 76 : 58
                    height: 70
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + fileEntry.modelData.icon
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    antialiasing: true
                }

                Text {
                    x: 0
                    y: 76
                    width: 96
                    height: 26
                    text: fileEntry.modelData.name
                    color: "#eefcff"
                    font.family: ValoTheme.fontFamily
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
