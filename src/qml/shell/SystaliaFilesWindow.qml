import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    property bool materialActive: true
    property string selectedLocation: "Home"
    property string selectedFile: "All Files"

    width: 701
    height: 534
    visible: true

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: fileWindow.backdropSource
        screenResolution: fileWindow.screenResolution
        radii: Qt.vector4d(fileWindow.cornerRadius, fileWindow.cornerRadius,
                            fileWindow.cornerRadius, fileWindow.cornerRadius)
        slant: fileWindow.surfaceSlant
        ior: fileWindow.refractiveIndex
        chromaticDispersion: fileWindow.dispersion
        causticIntensity: fileWindow.caustics
        surfaceOpacity: fileWindow.materialActive ? fileWindow.surfaceOpacity : 0
        time: fileWindow.animationTime
    }

    Rectangle {
        anchors.fill: parent
        radius: fileWindow.cornerRadius
        color: "#4a0c2540"
        border.color: SystaliaTheme.border
        border.width: 1.2
        antialiasing: true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 62
        radius: fileWindow.cornerRadius
        color: "#260d2844"
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
            anchors.leftMargin: 18
            anchors.rightMargin: 15
            spacing: 10

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
                    text: "Systalia Files"
                    color: SystaliaTheme.text
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 21
                    font.weight: Font.DemiBold
                    antialiasing: true
                }
                Text {
                    text: "ABYSSUS VES"
                    color: SystaliaTheme.textMuted
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 9
                    font.letterSpacing: 1.25
                    antialiasing: true
                }
            }

            Item { Layout.fillWidth: true }

            WindowAction {
                accessibleName: "Minimizar Systalia Files"
                glyph: "−"
                onClicked: {
                    if (fileWindow.controller)
                        fileWindow.controller.minimizeFileWindow()
                    else
                        fileWindow.visible = false
                }
            }
            WindowAction {
                accessibleName: "Maximizar Systalia Files"
                glyph: fileWindow.controller && fileWindow.controller.filesWindowMaximized ? "❐" : "□"
                onClicked: {
                    if (fileWindow.controller)
                        fileWindow.controller.toggleFileWindowMaximized()
                }
            }
            WindowAction {
                accessibleName: "Fechar Systalia Files"
                glyph: "×"
                closeAction: true
                onClicked: {
                    if (fileWindow.controller)
                        fileWindow.controller.closeFileWindow()
                    else
                        fileWindow.visible = false
                }
            }
        }

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
        id: navigation
        x: 15
        y: 75
        width: 174
        height: fileWindow.height - y - 15
        radius: 17
        color: "#75112640"
        border.color: "#5178b4cf"
        border.width: 1
        antialiasing: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 3

            Text {
                text: "FAVORITES"
                color: SystaliaTheme.textMuted
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.1
                Layout.leftMargin: 8
                Layout.bottomMargin: 4
                antialiasing: true
            }

            Repeater {
                model: [
                    { name: "Recent", icon: "◷" },
                    { name: "Home", icon: "⌂" },
                    { name: "Desktop", icon: "▧" },
                    { name: "Documents", icon: "▤" },
                    { name: "Downloads", icon: "↓" },
                    { name: "Music", icon: "♫" },
                    { name: "Pictures", icon: "▣" },
                    { name: "Trash", icon: "⌫" },
                    { name: "Other Items", icon: "···" }
                ]

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    height: 34
                    radius: 9
                    color: fileWindow.selectedLocation === modelData.name
                           ? "#3d2bd4e8" : navHover.hovered ? "#1cffffff" : "transparent"
                    border.color: fileWindow.selectedLocation === modelData.name ? "#697af2fa" : "transparent"
                    border.width: 1
                    antialiasing: true

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 11
                        Text {
                            text: modelData.icon
                            color: fileWindow.selectedLocation === modelData.name ? SystaliaTheme.aqua : "#9fc4dc"
                            font.pixelSize: 15
                            Layout.preferredWidth: 16
                            horizontalAlignment: Text.AlignHCenter
                            antialiasing: true
                        }
                        Text {
                            text: modelData.name
                            color: fileWindow.selectedLocation === modelData.name ? SystaliaTheme.text : SystaliaTheme.textSecondary
                            font.family: SystaliaTheme.fontFamily
                            font.pixelSize: 16
                            font.weight: fileWindow.selectedLocation === modelData.name ? Font.DemiBold : Font.Normal
                            Layout.fillWidth: true
                            antialiasing: true
                        }
                    }

                    HoverHandler { id: navHover }
                    TapHandler { onTapped: fileWindow.selectedLocation = modelData.name }
                }
            }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#387ba4c1"
                Layout.bottomMargin: 5
            }
            Text {
                text: fileWindow.waylandServer
                      ? "WAYLAND  ·  " + (fileWindow.waylandServer.created ? "ATIVO" : "INATIVO")
                      : "WAYLAND  ·  SGE"
                color: fileWindow.waylandServer && fileWindow.waylandServer.created ? "#82f2d2" : SystaliaTheme.textMuted
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 9
                font.letterSpacing: .6
                Layout.leftMargin: 7
                antialiasing: true
            }
        }
    }

    Item {
        id: filesContent
        x: 207
        y: 75
        width: fileWindow.width - x - 14
        height: fileWindow.height - y - 15

        ColumnLayout {
            anchors.fill: parent
            spacing: 15

            RowLayout {
                Layout.fillWidth: true
                spacing: 9

                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    radius: 10
                    color: "#53112640"
                    border.color: "#3d86b9d0"
                    border.width: 1
                    antialiasing: true
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 13
                        spacing: 9
                        Text { text: "Home"; color: SystaliaTheme.text; font.pixelSize: 12; font.bold: true; antialiasing: true }
                        Text { text: "›"; color: SystaliaTheme.textMuted; font.pixelSize: 15; antialiasing: true }
                        Text { text: "All files"; color: SystaliaTheme.textMuted; font.pixelSize: 12; antialiasing: true }
                        Item { Layout.fillWidth: true }
                        Text { text: "⌄"; color: SystaliaTheme.textMuted; font.pixelSize: 15; Layout.rightMargin: 11; antialiasing: true }
                    }
                }

                TextField {
                    id: searchField
                    Layout.preferredWidth: 174
                    Layout.fillWidth: false
                    height: 38
                    placeholderText: "Search files"
                    color: SystaliaTheme.text
                    placeholderTextColor: SystaliaTheme.textMuted
                    font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 12
                    leftPadding: 32
                    background: Rectangle {
                        radius: 10
                        color: "#53112640"
                        border.color: searchField.activeFocus ? "#9b50e6f7" : "#3d86b9d0"
                        border.width: 1
                        antialiasing: true
                    }
                    Text { x: 12; anchors.verticalCenter: parent.verticalCenter; text: "⌕"; color: SystaliaTheme.textMuted; font.pixelSize: 19; antialiasing: true }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Home"
                    color: SystaliaTheme.text
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 21
                    font.weight: Font.DemiBold
                    antialiasing: true
                }
                Item { Layout.fillWidth: true }
                Text { text: "6 items"; color: SystaliaTheme.textMuted; font.pixelSize: 10; antialiasing: true }
                Rectangle {
                    width: 60
                    height: 28
                    radius: 8
                    color: "#281b3a58"
                    border.color: "#4a79b4cd"
                    border.width: 1
                    antialiasing: true
                    Text { anchors.centerIn: parent; text: "▦   ≡"; color: SystaliaTheme.textSecondary; font.pixelSize: 13; antialiasing: true }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 3
                rowSpacing: 9
                columnSpacing: 8

                Repeater {
                    model: [
                        { name: "All Files", hint: "Folders and files", icon: "file-folder.svg" },
                        { name: "Documents", hint: "12 items", icon: "file-folder.svg" },
                        { name: "Document", hint: "Modified today", icon: "file-document.svg" },
                        { name: "Download2", hint: "Downloads", icon: "file-download.svg" },
                        { name: "Divides 1", hint: "Image · 4.2 MB", icon: "file-image.svg" },
                        { name: "Systion 4", hint: "Source files", icon: "file-code.svg" }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 128
                        radius: 13
                        color: fileWindow.selectedFile === modelData.name ? "#3b1b5070" : fileHover.hovered ? "#211a4868" : "#11172e4b"
                        border.color: fileWindow.selectedFile === modelData.name ? "#7662d8ef" : "#2b8db7ce"
                        border.width: 1
                        antialiasing: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6
                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 55
                                Layout.preferredHeight: 55
                                source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + modelData.icon
                                sourceSize: Qt.size(62, 64)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: SystaliaTheme.text
                                font.family: SystaliaTheme.fontFamily
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                antialiasing: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.hint
                                color: SystaliaTheme.textMuted
                                font.family: SystaliaTheme.fontFamily
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                antialiasing: true
                            }
                        }

                        HoverHandler { id: fileHover }
                        TapHandler { onTapped: fileWindow.selectedFile = modelData.name }
                    }
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
