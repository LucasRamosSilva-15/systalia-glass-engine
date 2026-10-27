import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: flowMenu

    property bool opened: false
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0.0
    property vector4d cornerRadii: Qt.vector4d(24, 24, 24, 24)

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
        color: "#98020a18"

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
            ior: 1.28
            chromaticDispersion: 0.04
            causticIntensity: 0.78
            surfaceOpacity: 0.9
            time: flowMenu.animationTime
        }

        SystaliaG2Frame {
            anchors.fill: parent
            radii: flowMenu.cornerRadii
            fillColor: "#a8091b2c"
            strokeColor: "#725ceaff"
            strokeWidth: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: 19
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#b7f5ff" }
                        GradientStop { position: 0.38; color: "#29cce8" }
                        GradientStop { position: 1.0; color: "#12609c" }
                    }
                    border.color: "#b8ffffff"
                    Text {
                        anchors.centerIn: parent
                        text: "S"
                        color: "#f5fdff"
                        font.pixelSize: 21
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: "SYSTALIA FLOW"
                        color: "#f1fbff"
                        font.pixelSize: 17
                        font.bold: true
                        font.letterSpacing: 1.3
                    }
                    Text {
                        text: AppRegistry.applicationCount + " aplicações disponíveis"
                        color: "#8db8cb"
                        font.pixelSize: 10
                    }
                }

                Text {
                    text: "ESC"
                    color: "#8db8cb"
                    font.pixelSize: 10
                    font.family: "Monospace"
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
                    radius: 13
                    color: "#54203146"
                    border.width: searchField.activeFocus ? 1.5 : 1
                    border.color: searchField.activeFocus ? "#55dff8" : "#42627c"
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    anchors.verticalCenter: parent.verticalCenter
                    text: "⌕"
                    color: "#76dff5"
                    font.pixelSize: 23
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
                        color: pointer.containsMouse ? "#4b52cce8" : "#1e203e59"
                        border.width: pointer.containsMouse ? 1 : 0
                        border.color: "#8defffff"

                        Column {
                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 54
                                height: 54
                                radius: 17
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#b9f6ff" }
                                    GradientStop { position: 0.28; color: "#39d5ed" }
                                    GradientStop { position: 0.7; color: "#1584bd" }
                                    GradientStop { position: 1.0; color: "#123e69" }
                                }
                                border.color: "#aaffffff"

                                Text {
                                    anchors.centerIn: parent
                                    text: applicationDelegate.name.length > 0
                                          ? applicationDelegate.name.substring(0, 1).toUpperCase() : "•"
                                    color: "#f7fdff"
                                    font.pixelSize: 25
                                    font.bold: true
                                }
                                Image {
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

                            Text {
                                width: 108
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: applicationDelegate.name
                                color: "#ecf9ff"
                                font.pixelSize: 10
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
                                font.pixelSize: 8
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: pointer
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                ProcessLauncher.launch(applicationDelegate.command,
                                                       applicationDelegate.arguments)
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
