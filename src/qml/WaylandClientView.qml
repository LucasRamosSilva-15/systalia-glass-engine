import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor

Item {
    id: rootClientView
    property var toplevel: null
    property alias surface: quickItem.surface

    width: (toplevel && toplevel.maxSize.width > 0) ? Math.min(toplevel.maxSize.width, 720) : 640
    height: (toplevel && toplevel.maxSize.height > 0) ? Math.min(toplevel.maxSize.height, 460) : 420

    // Chassi da janela cliente com acabamento estilo Systalia
    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "#0f172a"
        border.color: rootClientView.activeFocus ? "#00f2fe" : "#30ffffff"
        border.width: 1

        // Barra de Título da Janela Cliente
        Rectangle {
            id: clientTitleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 32
            radius: 12
            color: rootClientView.activeFocus ? "#2500f2fe" : "#1a2332"

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 10
                color: parent.color
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: rootClientView.activeFocus ? "#00f2fe" : "#64748b"
                }

                Text {
                    Layout.fillWidth: true
                    text: (rootClientView.toplevel && rootClientView.toplevel.title.length > 0)
                          ? rootClientView.toplevel.title
                          : ((rootClientView.toplevel && rootClientView.toplevel.appId.length > 0)
                             ? rootClientView.toplevel.appId
                             : "Wayland Client")
                    font.pixelSize: 11
                    font.bold: true
                    color: "#f8fafc"
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: 20
                    height: 20
                    radius: 10
                    color: closeBtnMouse.containsMouse ? "#ff5c7a" : "#20ffffff"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: 9
                        color: "#f8fafc"
                    }

                    MouseArea {
                        id: closeBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (rootClientView.toplevel) {
                                rootClientView.toplevel.sendClose()
                            }
                        }
                    }
                }
            }

            DragHandler {
                id: clientDrag
                target: rootClientView
            }
        }

        // Renderizador da Superfície Wayland (ShellSurfaceItem herda de WaylandQuickItem)
        ShellSurfaceItem {
            id: quickItem
            anchors.top: clientTitleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            shellSurface: rootClientView.toplevel ? rootClientView.toplevel.xdgSurface : null
            focus: true
            focusOnClick: true
            inputEventsEnabled: true
            paintEnabled: true
            moveItem: rootClientView

            onSurfaceDestroyed: {
                rootClientView.destroy()
            }
        }
    }

    TapHandler {
        onTapped: {
            rootClientView.forceActiveFocus()
            quickItem.forceActiveFocus()
        }
    }
}
