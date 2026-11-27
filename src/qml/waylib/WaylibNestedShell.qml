import QtQuick
import QtQuick.Layouts
import Waylib.Server
import Systalia.Engine
import Systalia.System 1.0

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

        // TopBar real VALO V2
        SystaliaTopBarV2 {
            id: valoTopBar
            objectName: "topBar"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 8
            height: 45
            z: 100
            blurProfile: 0
            backdropSource: null
            telemetry: SystemTelemetry
            workspaceManager: WorkspaceManager
            activeWindowTitle: root.compositorAdapter.surfaceAdapter.activeTitle
            activeWindowAppId: root.compositorAdapter.surfaceAdapter.activeAppId
            onLauncherRequested: root.compositorAdapter.surfaceAdapter.cycleFocus()
            onPowerRequested: root.compositorAdapter.surfaceAdapter.closeActiveSurface()

            // Impede vazamento de cliques para janelas abaixo da TopBar
            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.AllButtons
                onPressed: (mouse) => { mouse.accepted = true }
            }
        }

        // Área útil (workarea) para janelas Wayland clientes XDG
        Item {
            id: clientLayer
            objectName: "waylibClientLayer"
            anchors.top: valoTopBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: valoDock.top
            anchors.margins: 8
            clip: true
            z: 10

            Repeater {
                id: surfaceRepeater
                model: root.compositorAdapter.surfaceAdapter.windowModel

                delegate: WaylibWindowFrame {
                    required property var surface
                    required property var shellSurface
                    required property string title
                    required property string appId
                    required property bool activated
                    required property bool minimized
                    required property bool maximized
                    required property int zOrder
                    required property int index

                    surfaceAdapter: root.compositorAdapter.surfaceAdapter
                    waylandSurface: surface
                    windowShellSurface: shellSurface
                    windowTitle: title
                    windowAppId: appId
                    isActive: activated
                    isMinimized: minimized
                    isMaximized: maximized
                    windowZOrder: zOrder
                    windowIndex: index
                    workAreaWidth: clientLayer.width
                    workAreaHeight: clientLayer.height
                }
            }
        }

        // Faixa interativa de janelas Wayland abertas/minimizadas integrada ao Dock
        Rectangle {
            id: windowStrip
            objectName: "waylibWindowStrip"
            visible: root.compositorAdapter.surfaceAdapter.surfaceCount > 0
            anchors.bottom: valoDock.top
            anchors.bottomMargin: 6
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 48, windowStripRow.implicitWidth + 20)
            height: 32
            radius: 16
            color: "#CC071628"
            border.width: 1
            border.color: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.30)
            z: 96

            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.AllButtons
                onPressed: (mouse) => { mouse.accepted = true }
            }

            Row {
                id: windowStripRow
                anchors.centerIn: parent
                spacing: 8

                Repeater {
                    model: root.compositorAdapter.surfaceAdapter.windowModel

                    delegate: Rectangle {
                        required property var shellSurface
                        required property string title
                        required property string appId
                        required property bool activated
                        required property bool minimized

                        width: Math.min(180, pillLabel.implicitWidth + 20)
                        height: 24
                        radius: 12
                        color: activated
                               ? ValoTheme.withAlpha(ValoTheme.accentPrimary, 0.25)
                               : (minimized ? "#40102030" : "#80102840")
                        border.width: 1
                        border.color: activated ? ValoTheme.accentPrimary : ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.25)

                        Text {
                            id: pillLabel
                            anchors.centerIn: parent
                            width: Math.min(156, implicitWidth)
                            text: title.length > 0 ? title : (appId.length > 0 ? appId : "Wayland")
                            color: minimized ? ValoTheme.textSecondary : ValoTheme.textBright
                            font.family: ValoTheme.fontFamily
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.compositorAdapter.surfaceAdapter.focusOrRestore(shellSurface)
                        }
                    }
                }
            }
        }

        // Dock real VALO V2
        SystaliaDockV2 {
            id: valoDock
            objectName: "valoDockV2"
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            z: 95
            performanceProfile: "performance"
            backdropSource: null
            filesVisible: false
            settingsOpen: false
            activeAppIds: root.compositorAdapter.surfaceAdapter.activeAppIds

            onTerminalRequested: root.compositorAdapter.surfaceAdapter.cycleFocus()

            // Impede vazamento de cliques na região da Dock
            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.AllButtons
                onPressed: (mouse) => { mouse.accepted = true }
            }
        }
    }
}
