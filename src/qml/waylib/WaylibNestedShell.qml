import QtQuick
import QtQuick.Layouts
import Waylib.Server

Item {
    id: root
    required property var compositorAdapter

    OutputRenderWindow {
        id: renderWindow
        objectName: "waylibOutputRenderWindow"
        width: 1280
        height: 760
        visible: true

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#07111e" }
                GradientStop { position: 0.55; color: "#0c1d33" }
                GradientStop { position: 1.0; color: "#060c16" }
            }
        }

        // Outputs Waylib (nested ou headless)
        Row {
            id: outputsContainer
            anchors.fill: parent

            DynamicCreatorComponent {
                creator: root.compositorAdapter.outputAdapter.outputCreator

                OutputItem {
                    id: outputItem
                    required property WaylandOutput waylandOutput
                    output: waylandOutput
                    devicePixelRatio: waylandOutput.scale
                    layout: root.compositorAdapter.outputAdapter.outputLayout

                    OutputViewport {
                        id: outputViewport
                        anchors.fill: parent
                        output: waylandOutput
                        devicePixelRatio: parent.devicePixelRatio
                        live: true
                    }
                }
            }
        }

        // TopBar mínima com identidade VALO
        Rectangle {
            id: valoTopBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 36
            color: "#CC0b1626"
            border.width: 1
            border.color: "#2200f2fe"
            z: 20

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: "#00f2fe"
                }

                Text {
                    text: "SYSTALIA // Waylib Nested PoC"
                    color: "#e2f1ff"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Socket: " + (root.compositorAdapter.socketName.length > 0
                                      ? root.compositorAdapter.socketName : "starting")
                          + " | Surfaces: " + root.compositorAdapter.surfaceAdapter.surfaceCount
                    color: "#94a3b8"
                    font.pixelSize: 12
                }
            }
        }

        // Área de janelas Wayland clientes
        Item {
            id: clientLayer
            objectName: "waylibClientLayer"
            anchors.top: valoTopBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: valoDock.top
            anchors.margins: 16
            z: 10

            Repeater {
                id: surfaceRepeater
                model: root.compositorAdapter.surfaceAdapter.surfaces

                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    x: 24 + (index * 32)
                    y: 20 + (index * 28)
                    width: Math.max(320, toplevelItem.implicitWidth + 4)
                    height: Math.max(240, toplevelItem.implicitHeight + 30)
                    radius: 10
                    color: "#D90f172a"
                    border.width: 1
                    border.color: "#8000f2fe"

                    Rectangle {
                        id: titleChrome
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 28
                        radius: 10
                        color: "#3000f2fe"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            text: modelData.title.length > 0 ? modelData.title : (modelData.appId.length > 0 ? modelData.appId : "Wayland Client")
                            color: "#e2f1ff"
                            font.pixelSize: 12
                        }
                    }

                    XdgToplevelSurfaceItem {
                        id: toplevelItem
                        anchors.top: titleChrome.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        surface: modelData.surface
                        shellSurface: modelData
                    }
                }
            }
        }

        // Dock mínimo com identidade VALO
        Rectangle {
            id: valoDock
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            width: 320
            height: 48
            radius: 18
            color: "#CC0b1626"
            border.width: 1
            border.color: "#4400f2fe"
            z: 20

            Text {
                anchors.centerIn: parent
                text: "VALO Dock (Waylib Nested)"
                color: "#7dd3fc"
                font.pixelSize: 12
                font.weight: Font.Medium
            }
        }
    }
}
