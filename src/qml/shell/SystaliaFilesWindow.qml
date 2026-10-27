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
    property vector4d cornerRadii: Qt.vector4d(cornerRadius, cornerRadius,
                                                cornerRadius, cornerRadius)
    property bool materialActive: true
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
            x: -8.92
            y: 99.96
            width: 565.335
            height: 492.371
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-files-prism.png"
            sourceSize: Qt.size(1056, 804)
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }

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
        }

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
        }
    }

    SystaliaG2Frame {
        anchors.fill: parent
        radii: fileWindow.cornerRadii
        slant: fileWindow.surfaceSlant
        strokeColor: "#c9ffffff"
        strokeWidth: 1.5
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
        width: 570
        height: 49
        DragHandler {
            target: fileWindow
            enabled: fileWindow.controller ? !fileWindow.controller.filesWindowMaximized : true
            onActiveChanged: {
                if (active && fileWindow.controller)
                    fileWindow.controller.raiseFileWindow()
            }
        }
        TapHandler {
            onTapped: if (fileWindow.controller) fileWindow.controller.raiseFileWindow()
        }
    }

    Rectangle {
        x: 8.5
        y: 49.5
        width: 181
        height: 469
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
            x: 17.5
            y: 49.5 + modelData.top - 6
            width: 163
            height: 33

            Rectangle {
                anchors.fill: parent
                visible: fileWindow.selectedLocation === navigationEntry.modelData.name
                radius: 7
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
                    radius: 7
                    color: "transparent"
                    border.color: "#52ffffff"
                    border.width: 0.7
                    antialiasing: true
                }
            }

            Text {
                x: 0.5
                y: 6
                width: 150
                height: 27
                text: navigationEntry.modelData.icon + "  " + navigationEntry.modelData.name
                color: fileWindow.selectedLocation === navigationEntry.modelData.name ? "#eef9ff" : "#d5e2ee"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 16
                antialiasing: true
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: fileWindow.selectedLocation = navigationEntry.modelData.name
                }
            }
        }
    }

    Text {
        x: 207.5
        y: 57
        width: 83
        height: 44
        text: "‹   ›"
        color: "#c5eeff"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 26
        antialiasing: true
    }

    Rectangle {
        x: 274.5
        y: 58.5
        width: 90
        height: 30
        radius: 7
        color: "#d8103852"
        antialiasing: true
        Text {
            x: 11
            anchors.verticalCenter: parent.verticalCenter
            text: "⌂ " + fileWindow.selectedLocation
            color: "#e6fbff"
            font.family: SystaliaTheme.fontFamily
            font.pixelSize: 16
            font.weight: Font.Medium
            antialiasing: true
        }
    }

    Text {
        x: 488.5
        y: 57
        width: 30
        height: 38
        text: "☷"
        color: "#c2efff"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 21
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        antialiasing: true
    }

    TextField {
        id: searchField
        x: 524.5
        y: 59.5
        width: 157
        height: 28
        placeholderText: "⌕ Search"
        color: SystaliaTheme.text
        placeholderTextColor: "#bdcfdc"
        font.family: SystaliaTheme.fontFamily
        font.pixelSize: 15
        leftPadding: 13
        background: Rectangle {
            radius: 14
            color: "#ba0a2535"
            antialiasing: true
        }
        selectionColor: "#7d42e9ee"
    }

    Rectangle {
        x: 206.5
        y: 95.5
        width: 476
        height: 1
        color: "#4db2f3fd"
        antialiasing: true
    }

    Repeater {
        model: [
            { name: "All Files", icon: "valo-file-document.png", glyph: "", left: 216.5, top: 125.5 },
            { name: "Documents", icon: "valo-file-documents-art.png", glyph: "◉", left: 334.5, top: 125.5 },
            { name: "Document", icon: "valo-file-document.png", glyph: "</>", left: 452.5, top: 125.5 },
            { name: "Download2", icon: "valo-file-download.png", glyph: "⬇", left: 570.5, top: 125.5 },
            { name: "Divides 1", icon: "valo-file-list.png", glyph: "≡", left: 216.5, top: 245.5 },
            { name: "Systion 4", icon: "valo-file-check.png", glyph: "✓", left: 334.5, top: 245.5 }
        ]

        delegate: Item {
            id: fileEntry
            required property var modelData
            x: fileEntry.modelData.left
            y: fileEntry.modelData.top
            width: 96
            height: 106

            Rectangle {
                anchors.fill: parent
                visible: fileWindow.selectedFile === fileEntry.modelData.name
                radius: 8
                color: "#182ae0ff"
                border.color: "#507ce9f4"
                border.width: 1
                antialiasing: true
            }

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

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: fileWindow.selectedFile = fileEntry.modelData.name }
        }
    }

}
