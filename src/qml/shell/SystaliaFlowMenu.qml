import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: flowMenu

    property bool opened: false
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0.0
    property vector4d cornerRadii: Qt.vector4d(20, 20, 20, 20)

    anchors.fill: parent
    z: 120
    visible: opened || opacity > 0.001
    opacity: opened ? 1.0 : 0.0
    scale: opened ? 1.0 : 0.97

    function toggle() {
        opened = !opened
    }

    function close() {
        opened = false
    }

    onOpenedChanged: {
        if (opened)
            Qt.callLater(() => searchField.forceActiveFocus())
    }

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 0.35 }
    }

    Rectangle {
        anchors.fill: parent
        color: "#40020a18"

        MouseArea {
            anchors.fill: parent
            onClicked: flowMenu.close()
        }
    }

    Item {
        id: panel
        width: Math.min(640, flowMenu.width - 36)
        height: Math.min(610, flowMenu.height - 96)
        anchors.centerIn: parent
        clip: true

        SystaliaGlassItem {
            anchors.fill: parent
            backdropSource: flowMenu.backdropSource
            screenResolution: flowMenu.screenResolution
            radii: flowMenu.cornerRadii
            slant: 0.0
            ior: 1.25
            chromaticDispersion: 0.040
            causticIntensity: 0.85
            surfaceOpacity: 0.85
            time: flowMenu.animationTime
        }

        SystaliaG2Frame {
            anchors.fill: parent
            radii: flowMenu.cornerRadii
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
            radius: 17
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
            anchors.margins: 22
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38

                    // Sombra da esfera de ícone
                    Rectangle {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: 2
                        width: 36
                        height: 36
                        radius: 14
                        color: "#60000000"
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 14
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#b9f6ff" }
                            GradientStop { position: 0.28; color: "#39d5ed" }
                            GradientStop { position: 0.70; color: "#1584bd" }
                            GradientStop { position: 1.0; color: "#123e69" }
                        }
                        border.color: "#cce8ffff"
                        border.width: 1
                        antialiasing: true

                        Text {
                            anchors.centerIn: parent
                            text: "S"
                            color: "#f5fdff"
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    RowLayout {
                        spacing: 8
                        Text {
                            text: "SYSTALIA FLOW"
                            color: "#f1fbff"
                            font.pixelSize: 17
                            font.bold: true
                            font.letterSpacing: 1.3
                        }
                        Rectangle {
                            radius: 6
                            color: "#ba0a2c46"
                            border.color: "#307db4cf"
                            border.width: 0.8
                            implicitWidth: badgeText.implicitWidth + 12
                            implicitHeight: 18
                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: "VALO LAUNCHER"
                                color: "#74d5ee"
                                font.pixelSize: 8
                                font.bold: true
                                font.letterSpacing: 0.6
                            }
                        }
                    }
                    Text {
                        text: AppRegistry.applicationCount + " aplicações disponíveis"
                        color: "#8db8cb"
                        font.pixelSize: 10
                    }
                }

                Rectangle {
                    radius: 6
                    color: "#20092236"
                    border.color: "#355a8299"
                    border.width: 0.8
                    implicitWidth: 36
                    implicitHeight: 22
                    Text {
                        anchors.centerIn: parent
                        text: "ESC"
                        color: "#9fc0d1"
                        font.pixelSize: 10
                        font.bold: true
                        font.family: "Monospace"
                    }
                }
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                placeholderText: "Pesquisar aplicações..."
                leftPadding: 42
                color: "#f2fbff"
                placeholderTextColor: "#7698ad"
                selectionColor: "#4ad7f2"
                onTextChanged: AppRegistry.filterText = text
                background: Rectangle {
                    radius: 12
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: "#45082035" }
                        GradientStop { position: 1.0; color: "#25020a14" }
                    }
                    border.color: searchField.activeFocus ? "#78e0f8" : "#4273bada"
                    border.width: searchField.activeFocus ? 1.5 : 1
                    antialiasing: true

                    // Inset highlight superior
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 1
                        width: parent.width - 24
                        height: 1
                        color: "#40ffffff"
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    anchors.verticalCenter: parent.verticalCenter
                    text: "⌕"
                    color: "#76dff5"
                    font.pixelSize: 21
                }
            }

            Text {
                Layout.fillWidth: true
                text: AppRegistry.applicationCount > 0 ? "APLICAÇÕES" : "NENHUMA APLICAÇÃO ENCONTRADA"
                color: "#82aac0"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 1.1
            }

            GridView {
                id: applicationGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: 132
                cellHeight: 126
                model: AppRegistry
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    id: applicationDelegate
                    required property string name
                    required property bool terminal
                    required property string command
                    required property var arguments
                    required property string icon
                    required property var categories
                    width: applicationGrid.cellWidth
                    height: applicationGrid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 5
                        radius: 15
                        color: pointer.containsMouse ? "#2b1f85aa" : "#14092235"
                        border.width: pointer.containsMouse ? 1.2 : 0.8
                        border.color: pointer.containsMouse ? "#8defffff" : "#1e60859c"
                        antialiasing: true

                        // Highlight superior no hover
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 1
                            width: parent.width - 16
                            height: 1
                            color: "#52ffffff"
                            visible: pointer.containsMouse
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: 8

                            Item {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 54
                                height: 54

                                // Sombra do ícone
                                Rectangle {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: 2
                                    width: 50
                                    height: 50
                                    radius: 16
                                    color: "#70000000"
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 16
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#b9f6ff" }
                                        GradientStop { position: 0.28; color: "#39d5ed" }
                                        GradientStop { position: 0.70; color: "#1584bd" }
                                        GradientStop { position: 1.0; color: "#123e69" }
                                    }
                                    border.color: "#cce8ffff"
                                    border.width: 1
                                    antialiasing: true

                                    Text {
                                        anchors.centerIn: parent
                                        text: applicationDelegate.name.length > 0
                                              ? applicationDelegate.name.substring(0, 1).toUpperCase() : "•"
                                        color: "#f7fdff"
                                        font.pixelSize: 24
                                        font.bold: true
                                        visible: iconImage.status !== Image.Ready
                                    }

                                    Image {
                                        id: iconImage
                                        anchors.centerIn: parent
                                        width: 34
                                        height: 34
                                        source: applicationDelegate.icon.length > 0
                                                ? "image://desktopIcons/" + encodeURIComponent(applicationDelegate.icon)
                                                : ""
                                        sourceSize: Qt.size(64, 64)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                    }
                                }
                            }

                            Text {
                                width: 108
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: applicationDelegate.name
                                color: "#ecf9ff"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            Text {
                                width: 108
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: applicationDelegate.categories.length > 0
                                      ? applicationDelegate.categories[0]
                                      : applicationDelegate.command
                                color: "#85aec2"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: pointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const launched = applicationDelegate.terminal
                                    ? ProcessLauncher.launchInTerminal(applicationDelegate.command,
                                                                       applicationDelegate.arguments)
                                    : ProcessLauncher.launch(applicationDelegate.command,
                                                             applicationDelegate.arguments)
                                if (launched)
                                    flowMenu.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: flowMenu.opened
        onActivated: flowMenu.close()
    }
}
