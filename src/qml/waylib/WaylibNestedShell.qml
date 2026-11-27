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

        // Outputs Waylib (nested ou headless)
        Row {
            id: outputsContainer
            anchors.fill: parent
            z: 0

            DynamicCreatorComponent {
                creator: root.compositorAdapter.outputAdapter.outputCreator

                OutputItem {
                    id: outputItem
                    required property WaylandOutput waylandOutput
                    output: waylandOutput
                    devicePixelRatio: waylandOutput.scale
                    layout: root.compositorAdapter.outputAdapter.outputLayout
                    width: Math.max(renderWindow.width, waylandOutput.size.width / Math.max(1.0, waylandOutput.scale))
                    height: Math.max(renderWindow.height, waylandOutput.size.height / Math.max(1.0, waylandOutput.scale))

                    OutputViewport {
                        id: outputViewport
                        objectName: "waylibOutputViewport"
                        anchors.fill: parent
                        output: waylandOutput
                        devicePixelRatio: parent.devicePixelRatio
                        live: true
                    }
                }
            }
        }

        // 1. Camada de conteúdo capturável (Wallpaper + Janelas Wayland XDG)
        Item {
            id: desktopContentLayer
            objectName: "desktopContentLayer"
            anchors.fill: parent
            z: 10

            // Wallpaper VALO oceânico com iluminação sutil para refração/blur reais
            Rectangle {
                id: valoWallpaper
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#07111e" }
                    GradientStop { position: 0.55; color: "#0c1d33" }
                    GradientStop { position: 1.0; color: "#060c16" }
                }

                Rectangle {
                    width: parent.width * 0.55
                    height: width
                    radius: width * 0.5
                    x: -width * 0.15
                    y: -height * 0.25
                    opacity: 0.28
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#1490d8" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                Rectangle {
                    width: parent.width * 0.45
                    height: width
                    radius: width * 0.5
                    x: parent.width * 0.62
                    y: parent.height * 0.48
                    opacity: 0.22
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#00d2ff" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
            }

            // Área útil (workarea) para janelas Wayland clientes XDG
            Item {
                id: clientLayer
                objectName: "waylibClientLayer"
                anchors.fill: parent
                anchors.topMargin: 61
                anchors.bottomMargin: valoDock.height + 20
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                clip: true

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
        }

        // Fonte real do backdrop (captura apenas desktopContentLayer, sem chrome/vidro)
        ShaderEffectSource {
            id: desktopBackdropSource
            objectName: "waylibDesktopBackdropSource"
            anchors.fill: desktopContentLayer
            sourceItem: desktopContentLayer
            textureSize: Qt.size(
                Math.max(1, Math.round(desktopContentLayer.width * renderWindow.devicePixelRatio)),
                Math.max(1, Math.round(desktopContentLayer.height * renderWindow.devicePixelRatio)))
            live: true
            hideSource: false
            visible: true
            opacity: 0.0
            z: -100
            recursive: false
            format: ShaderEffectSource.RGBA8
        }

        // 2. Camada de chrome/vidro VALO V2 (fora de desktopContentLayer -> sem feedback loop)
        Item {
            id: shellChromeLayer
            objectName: "shellChromeLayer"
            anchors.fill: parent
            z: 90

            SystaliaTopBarV2 {
                id: valoTopBar
                objectName: "topBar"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                height: 45
                z: 100
                blurProfile: 1
                backdropSource: desktopBackdropSource
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
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

            SystaliaDockV2 {
                id: valoDock
                objectName: "valoDockV2"
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                anchors.horizontalCenter: parent.horizontalCenter
                z: 95
                performanceProfile: "balanced"
                backdropSource: desktopBackdropSource
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
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

        // 3. Camada de cursor acima do wallpaper, janelas e chrome VALO
        Item {
            id: cursorOverlayLayer
            objectName: "waylibCursorOverlayLayer"
            anchors.fill: parent
            z: 10000

            Canvas {
                id: softwarePointerArrow
                objectName: "waylibSoftwarePointer"
                visible: root.compositorAdapter.cursor && root.compositorAdapter.cursor.visible
                width: 20
                height: 24
                x: root.compositorAdapter.cursor ? root.compositorAdapter.cursor.position.x : 0
                y: root.compositorAdapter.cursor ? root.compositorAdapter.cursor.position.y : 0
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.beginPath()
                    ctx.moveTo(1, 1)
                    ctx.lineTo(1, 17)
                    ctx.lineTo(5.5, 13)
                    ctx.lineTo(9, 20)
                    ctx.lineTo(11.5, 18.8)
                    ctx.lineTo(8, 12)
                    ctx.lineTo(13.5, 12)
                    ctx.closePath()
                    ctx.fillStyle = "#0b1626"
                    ctx.fill()
                    ctx.lineWidth = 1.4
                    ctx.strokeStyle = "#e2f1ff"
                    ctx.stroke()
                }
                Component.onCompleted: requestPaint()
            }
        }
    }
}
