import QtQuick
import QtQuick.Layouts
import Waylib.Server
import Systalia.Engine
import Systalia.System 1.0
import "qrc:/qt/qml/Systalia/Engine/src/qml/graphics/v2" as GfxV2

Item {
    id: root
    required property var compositorAdapter
    property bool filesWindowMaximized: false
    property rect filesWindowRestoreGeometry: Qt.rect(48, 72, 701, 534)
    property bool materialWindowMaximized: false
    property rect materialWindowRestoreGeometry: Qt.rect(520, 72, 700, 534)
    property var orderedClientFrames: []
    readonly property int maxClientStages: 6

    Binding {
        target: ProcessLauncher
        property: "waylandDisplay"
        value: root.compositorAdapter ? root.compositorAdapter.socketName : ""
        when: root.compositorAdapter && root.compositorAdapter.socketName.length > 0
    }

    Binding {
        target: ProcessLauncher
        property: "isolatedNestedSession"
        value: true
        when: root.compositorAdapter !== null
    }

    function rebuildOrderedClientFrames() {
        let list = []
        for (let i = 0; i < surfaceRepeater.count; ++i) {
            const item = surfaceRepeater.itemAt(i)
            if (item && item.visible) {
                list.push(item)
            }
        }
        list.sort((a, b) => a.windowZOrder - b.windowZOrder)
        orderedClientFrames = list
    }

    function backdropForClientFrame(frameItem) {
        const idx = orderedClientFrames.indexOf(frameItem)
        if (idx <= 0) {
            return wallpaperOnlyBackdropSource
        }
        const stageIdx = Math.min(idx - 1, maxClientStages - 1)
        const stageItem = clientStageRepeater.itemAt(stageIdx)
        return (stageItem && stageItem.stageActive) ? stageItem.stageGrab : wallpaperOnlyBackdropSource
    }

    function launchDesktopApp(candidates, label) {
        for (let i = 0; i < candidates.length; ++i) {
            if (ProcessLauncher.launch(candidates[i])) {
                return true
            }
        }
        console.warn("[SGE-WAYLIB] Nenhum executável disponível para " + label + ":", candidates.join(", "))
        return false
    }

    function bringInternalWindowToFront(win) {
        if (!win) {
            return
        }
        if (win === filesWindow) {
            filesWindow.z = 32
            materialWindow.z = 31
        } else if (win === materialWindow) {
            materialWindow.z = 32
            filesWindow.z = 31
        }
        controlCenter.close()
        flowMenu.close()
    }

    function raiseFileWindow() {
        filesWindow.windowOpen = true
        filesWindow.visible = true
        bringInternalWindowToFront(filesWindow)
        filesWindow.forceActiveFocus()
    }

    function minimizeFileWindow() {
        filesWindow.windowOpen = false
        filesWindow.visible = false
    }

    function closeFileWindow() {
        if (filesWindowMaximized) {
            toggleFileWindowMaximized()
        }
        filesWindow.windowOpen = false
        filesWindow.visible = false
    }

    function toggleFileWindowMaximized() {
        if (filesWindowMaximized) {
            filesWindow.x = filesWindowRestoreGeometry.x
            filesWindow.y = filesWindowRestoreGeometry.y
            filesWindow.width = filesWindowRestoreGeometry.width
            filesWindow.height = filesWindowRestoreGeometry.height
            filesWindowMaximized = false
            return
        }
        filesWindowRestoreGeometry = Qt.rect(filesWindow.x, filesWindow.y,
                                             filesWindow.width, filesWindow.height)
        filesWindow.x = clientLayer.x + 8
        filesWindow.y = clientLayer.y + 8
        filesWindow.width = Math.max(480, clientLayer.width - 16)
        filesWindow.height = Math.max(360, clientLayer.height - 16)
        filesWindowMaximized = true
        raiseFileWindow()
    }

    function raiseMaterialWindow() {
        materialWindow.windowOpen = true
        materialWindow.visible = true
        bringInternalWindowToFront(materialWindow)
        materialWindow.forceActiveFocus()
    }

    function minimizeMaterialWindow() {
        materialWindow.windowOpen = false
        materialWindow.visible = false
    }

    function closeMaterialWindow() {
        if (materialWindowMaximized) {
            toggleMaterialWindowMaximized()
        }
        materialWindow.windowOpen = false
        materialWindow.visible = false
    }

    function toggleMaterialWindowMaximized() {
        if (materialWindowMaximized) {
            materialWindow.x = materialWindowRestoreGeometry.x
            materialWindow.y = materialWindowRestoreGeometry.y
            materialWindow.width = materialWindowRestoreGeometry.width
            materialWindow.height = materialWindowRestoreGeometry.height
            materialWindowMaximized = false
            return
        }
        materialWindowRestoreGeometry = Qt.rect(materialWindow.x, materialWindow.y,
                                                materialWindow.width, materialWindow.height)
        materialWindow.x = clientLayer.x + 8
        materialWindow.y = clientLayer.y + 8
        materialWindow.width = Math.max(480, clientLayer.width - 16)
        materialWindow.height = Math.max(360, clientLayer.height - 16)
        materialWindowMaximized = true
        raiseMaterialWindow()
    }

    Connections {
        target: root.compositorAdapter ? root.compositorAdapter.surfaceAdapter : null
        ignoreUnknownSignals: true
        function onActiveSurfaceChanged() { Qt.callLater(root.rebuildOrderedClientFrames) }
        function onSurfacesChanged() { Qt.callLater(root.rebuildOrderedClientFrames) }
    }

    Timer {
        id: animTimer
        interval: 16
        running: true
        repeat: true
        property real tick: 0.0
        onTriggered: tick += 0.02
    }

    OutputRenderWindow {
        id: renderWindow
        objectName: "waylibOutputRenderWindow"
        width: 1280
        height: 760
        visible: true

        Component.onCompleted: SystemTelemetry.attachWindow(renderWindow)
        onRenderEnd: SystemTelemetry.recordFrame()

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

        // 1. Camada de conteúdo capturável (Wallpaper real + Janelas Wayland XDG)
        Item {
            id: desktopContentLayer
            objectName: "desktopContentLayer"
            anchors.fill: parent
            z: 10

            // Wallpaper oficial VALO oceânico (idêntico ao ShellRoot.qml)
            Item {
                id: valoWallpaper
                objectName: "waylibWallpaper"
                anchors.fill: parent

                Rectangle {
                    anchors.fill: parent
                    color: ValoTheme.primitiveNavy950
                }

                Image {
                    id: wallpaperImage
                    objectName: "waylibWallpaperImage"
                    anchors.fill: parent
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-ocean-atmosphere.png"
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    smooth: true
                    mipmap: true
                    visible: status === Image.Ready
                }
            }

            // Fonte de backdrop exclusiva do wallpaper (base da cadeia acíclica de clientes)
            ShaderEffectSource {
                id: wallpaperOnlyBackdropSource
                objectName: "waylibWallpaperBackdropSource"
                anchors.fill: valoWallpaper
                sourceItem: valoWallpaper
                textureSize: Qt.size(
                    Math.max(1, Math.round(valoWallpaper.width * renderWindow.devicePixelRatio)),
                    Math.max(1, Math.round(valoWallpaper.height * renderWindow.devicePixelRatio)))
                live: true
                hideSource: false
                visible: true
                opacity: 0.0
                z: -100
                recursive: false
                format: ShaderEffectSource.RGBA8
            }

            // Área útil (workarea) para janelas Wayland clientes XDG
            Item {
                id: clientLayer
                objectName: "waylibClientLayer"
                anchors.fill: parent
                anchors.topMargin: 60
                anchors.bottomMargin: valoDock.height + 24
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                clip: true

                Repeater {
                    id: surfaceRepeater
                    model: root.compositorAdapter.surfaceAdapter.windowModel
                    onItemAdded: Qt.callLater(root.rebuildOrderedClientFrames)
                    onItemRemoved: Qt.callLater(root.rebuildOrderedClientFrames)

                    delegate: WaylibWindowFrame {
                        id: clientFrameDelegate
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
                        outputLayout: root.compositorAdapter.outputAdapter.outputLayout
                        backdropSource: root.backdropForClientFrame(clientFrameDelegate)
                        blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
                        userHighContrast: materialWindow.opticalSurface.highContrast
                        userPrefersReducedMotion: materialWindow.opticalSurface.prefersReducedMotion
                        refractiveIndex: materialWindow.refractiveIndex
                        effectiveDispersion: materialWindow.effectiveDispersion
                        caustics: materialWindow.caustics
                        slant: materialWindow.surfaceSlant
                        cornerRadius: materialWindow.cornerRadius
                        animationTime: animTimer.tick
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
                        onWindowZOrderChanged: Qt.callLater(root.rebuildOrderedClientFrames)
                        onVisibleChanged: Qt.callLater(root.rebuildOrderedClientFrames)
                    }
                }
            }
        }

        // Cadeia acíclica offscreen para transparência entre janelas clientes Wayland (fora de desktopContentLayer)
        Item {
            id: clientBackdropPool
            objectName: "waylibClientBackdropPool"
            anchors.fill: desktopContentLayer
            visible: false

            Repeater {
                id: clientStageRepeater
                model: root.maxClientStages

                Item {
                    id: stageItem
                    required property int index
                    readonly property Item prevSource: index === 0
                        ? wallpaperOnlyBackdropSource
                        : clientStageRepeater.itemAt(index - 1).stageGrab
                    readonly property Item lowerFrame: (index < root.orderedClientFrames.length)
                        ? root.orderedClientFrames[index] : null
                    readonly property bool stageActive: lowerFrame !== null && root.orderedClientFrames.length > (index + 1)
                    readonly property point framePos: lowerFrame
                        ? lowerFrame.mapToItem(desktopContentLayer, 0, 0) : Qt.point(0, 0)
                    readonly property alias stageGrab: stageGrabItem
                    anchors.fill: parent

                    Item {
                        id: stageComposition
                        anchors.fill: parent

                        ShaderEffect {
                            anchors.fill: parent
                            property var source: stageItem.prevSource
                            fragmentShader: "qrc:/shaders/v2/valo_backdrop_copy.frag.qsb"
                        }

                        ShaderEffectSource {
                            anchors.fill: parent
                            sourceRect: Qt.rect(-stageItem.framePos.x, -stageItem.framePos.y,
                                                stageItem.width, stageItem.height)
                            sourceItem: stageItem.stageActive ? stageItem.lowerFrame : null
                            opacity: stageItem.lowerFrame ? stageItem.lowerFrame.opacity : 0.0
                            textureSize: Qt.size(
                                Math.max(1, Math.round(stageItem.width * renderWindow.devicePixelRatio)),
                                Math.max(1, Math.round(stageItem.height * renderWindow.devicePixelRatio)))
                            live: stageItem.stageActive
                            recursive: false
                            hideSource: false
                        }
                    }

                    ShaderEffectSource {
                        id: stageGrabItem
                        anchors.fill: parent
                        sourceItem: stageItem.stageActive ? stageComposition : null
                        textureSize: Qt.size(
                            Math.max(1, Math.round(stageItem.width * renderWindow.devicePixelRatio)),
                            Math.max(1, Math.round(stageItem.height * renderWindow.devicePixelRatio)))
                        live: stageItem.stageActive
                        recursive: false
                        hideSource: false
                    }
                }
            }
        }

        // Fonte real do backdrop (captura wallpaper + janelas clientes, sem chrome/vidro)
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

        // Transparência real entre janelas internas (Files + Systalia Control Center) acima de desktopBackdropSource
        GfxV2.ValoInternalWindowBackdropV2 {
            id: internalWindowBackdrop
            objectName: "waylibInternalWindowBackdropV2"
            anchors.fill: parent
            firstWindow: filesWindow
            secondWindow: materialWindow
            wallpaperSource: desktopBackdropSource
            enabledForWorkspace: true
            externalClientsPresent: false
        }

        // 2. Janelas internas da shell (Files + Systalia Control Center) — sempre abaixo de TopBar/Dock
        Item {
            id: internalWindowsLayer
            objectName: "waylibInternalWindowsLayer"
            anchors.fill: parent
            z: 40

            SystaliaFilesWindowV2 {
                id: filesWindow
                objectName: "waylibFilesWindowV2"
                visible: false
                windowOpen: false
                x: Math.max(8, Math.min(48, renderWindow.width - width - 16))
                y: 68
                width: Math.min(701, Math.max(480, clientLayer.width - 16))
                height: Math.min(534, Math.max(360, clientLayer.height - 16))
                z: 31
                blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
                backdropSource: internalWindowBackdrop.sourceFor(filesWindow)
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
                animationTime: animTimer.tick
                refractiveIndex: materialWindow.refractiveIndex
                dispersion: materialWindow.effectiveDispersion
                caustics: materialWindow.caustics
                surfaceOpacity: materialWindow.surfaceOpacity
                surfaceSlant: materialWindow.surfaceSlant
                cornerRadius: materialWindow.cornerRadius
                materialActive: materialWindow.materialActive
                controller: root
            }

            SystaliaMaterialWindow {
                id: materialWindow
                objectName: "waylibMaterialWindow"
                visible: false
                windowOpen: false
                x: Math.max(8, Math.min(renderWindow.width - width - 32, Math.max(8, renderWindow.width - width - 16)))
                y: 68
                width: Math.min(700, Math.max(480, clientLayer.width - 16))
                height: Math.min(534, Math.max(360, clientLayer.height - 16))
                z: 32
                blurProfile: 1
                backdropSource: internalWindowBackdrop.sourceFor(materialWindow)
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
                animationTime: animTimer.tick
                controller: root
            }
        }

        // Composição de backdrop para Overlays (wallpaper + clientes + janelas internas, fora de shellChromeLayer)
        Item {
            id: overlayBackdropComposition
            anchors.fill: parent
            visible: false
            readonly property bool hasInternalWindows: filesWindow.visible || materialWindow.visible

            ShaderEffect {
                anchors.fill: parent
                property var source: desktopBackdropSource
                fragmentShader: "qrc:/shaders/v2/valo_backdrop_copy.frag.qsb"
            }

            ShaderEffectSource {
                anchors.fill: parent
                sourceItem: overlayBackdropComposition.hasInternalWindows ? internalWindowsLayer : null
                textureSize: Qt.size(
                    Math.max(1, Math.round(overlayBackdropComposition.width * renderWindow.devicePixelRatio)),
                    Math.max(1, Math.round(overlayBackdropComposition.height * renderWindow.devicePixelRatio)))
                live: overlayBackdropComposition.hasInternalWindows
                recursive: false
                hideSource: false
            }
        }

        ShaderEffectSource {
            id: overlayCompositeBackdropSource
            objectName: "waylibOverlayCompositeBackdropSource"
            anchors.fill: parent
            sourceItem: overlayBackdropComposition.hasInternalWindows ? overlayBackdropComposition : desktopContentLayer
            textureSize: Qt.size(
                Math.max(1, Math.round(renderWindow.width * renderWindow.devicePixelRatio)),
                Math.max(1, Math.round(renderWindow.height * renderWindow.devicePixelRatio)))
            live: true
            hideSource: false
            visible: true
            opacity: 0.0
            z: -100
            recursive: false
            format: ShaderEffectSource.RGBA8
        }

        // 3. Camada de chrome/vidro VALO V2 (sempre acima das janelas internas e clientes)
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
                blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
                backdropSource: overlayCompositeBackdropSource
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
                animationTime: animTimer.tick
                telemetry: SystemTelemetry
                workspaceManager: WorkspaceManager
                activeWindowTitle: root.compositorAdapter.surfaceAdapter.activeTitle
                activeWindowAppId: root.compositorAdapter.surfaceAdapter.activeAppId
                onLauncherRequested: {
                    controlCenter.close()
                    flowMenu.toggle()
                }
                onControlCenterRequested: {
                    flowMenu.close()
                    controlCenter.toggle()
                }
                onTerminalRequested: root.launchDesktopApp(["alacritty", "foot", "konsole", "xterm"], "terminal")
                onPowerRequested: {
                    flowMenu.close()
                    controlCenter.open()
                }

                // Impede vazamento de cliques para janelas abaixo da TopBar
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    acceptedButtons: Qt.AllButtons
                    onPressed: (mouse) => { mouse.accepted = true }
                }
            }

            SystaliaDockV2 {
                id: valoDock
                objectName: "valoDockV2"
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                anchors.horizontalCenter: parent.horizontalCenter
                z: 95
                performanceProfile: !materialWindow.materialActive || materialWindow.blurProfile === 0
                                    ? "performance" : materialWindow.blurProfile === 1 ? "balanced" : "quality"
                backdropSource: overlayCompositeBackdropSource
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
                animationTime: animTimer.tick
                filesVisible: filesWindow.visible
                settingsOpen: materialWindow.visible
                refractiveIndex: materialWindow.refractiveIndex
                dispersion: materialWindow.effectiveDispersion
                caustics: materialWindow.caustics
                materialActive: materialWindow.materialActive
                activeAppIds: root.compositorAdapter.surfaceAdapter.activeAppIds

                onFilesRequested: {
                    if (filesWindow.visible)
                        root.minimizeFileWindow()
                    else
                        root.raiseFileWindow()
                }
                onAudioRequested: root.launchDesktopApp(["pavucontrol", "pwvucontrol"], "áudio")
                onTerminalRequested: root.launchDesktopApp(["alacritty", "foot", "konsole", "xterm"], "terminal")
                onIdeRequested: root.launchDesktopApp(["code", "codium"], "IDE")
                onSettingsRequested: {
                    if (materialWindow.visible)
                        root.closeMaterialWindow()
                    else
                        root.raiseMaterialWindow()
                }

                // Impede vazamento de cliques na região da Dock
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    acceptedButtons: Qt.AllButtons
                    onPressed: (mouse) => { mouse.accepted = true }
                }
            }

            SystaliaControlCenterV2 {
                id: controlCenter
                objectName: "waylibControlCenterV2"
                z: 115
                blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
                backdropSource: overlayCompositeBackdropSource
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
                animationTime: animTimer.tick
            }

            SystaliaNotificationHost {
                id: notificationHost
                objectName: "waylibNotificationHost"
                z: 118
                blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
                backdropSource: overlayCompositeBackdropSource
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
                animationTime: animTimer.tick
            }

            SystaliaFlowMenu {
                id: flowMenu
                objectName: "waylibFlowMenu"
                z: 120
                blurProfile: materialWindow.materialActive ? materialWindow.blurProfile : 0
                backdropSource: overlayCompositeBackdropSource
                screenResolution: Qt.vector2d(renderWindow.width * renderWindow.devicePixelRatio,
                                              renderWindow.height * renderWindow.devicePixelRatio)
                animationTime: animTimer.tick
            }
        }

        Shortcut {
            sequence: "Meta+Space"
            context: Qt.ApplicationShortcut
            onActivated: {
                controlCenter.close()
                flowMenu.toggle()
            }
        }

        Shortcut {
            sequence: "Escape"
            context: Qt.ApplicationShortcut
            enabled: controlCenter.opened
            onActivated: controlCenter.close()
        }

        // 4. Camada de popups XDG (menus, tooltips, comboboxes acima de janelas e chrome)
        Item {
            id: popupLayer
            objectName: "waylibPopupLayer"
            anchors.fill: parent
            z: 120

            function originForParent(parentSurf) {
                if (!parentSurf) {
                    return Qt.point(clientLayer.x, clientLayer.y)
                }
                for (let i = 0; i < surfaceRepeater.count; ++i) {
                    const frameItem = surfaceRepeater.itemAt(i)
                    if (frameItem && frameItem.waylandSurface === parentSurf) {
                        return frameItem.clientOriginIn(popupLayer)
                    }
                }
                return Qt.point(clientLayer.x, clientLayer.y)
            }

            Repeater {
                id: popupRepeater
                model: root.compositorAdapter.surfaceAdapter.popupModel

                delegate: Item {
                    id: popupDelegate
                    required property var surface
                    required property var popupSurface
                    required property var parentSurface
                    required property real popupX
                    required property real popupY

                    readonly property point baseOrigin: popupLayer.originForParent(parentSurface)

                    x: baseOrigin.x + popupX
                    y: baseOrigin.y + popupY
                    width: popupItem.implicitWidth
                    height: popupItem.implicitHeight
                    z: 120

                    XdgPopupSurfaceItem {
                        id: popupItem
                        objectName: "waylibPopupItem"
                        surface: popupDelegate.surface
                        shellSurface: popupDelegate.popupSurface
                    }
                }
            }
        }

        Shortcut {
            sequence: "Alt+Tab"
            context: Qt.ApplicationShortcut
            onActivated: root.compositorAdapter.surfaceAdapter.cycleFocus()
        }

        // Camada de brilho em software para telas sem controle sysfs por hardware
        Rectangle {
            id: softwareBrightnessDimmer
            objectName: "waylibSoftwareBrightnessDimmer"
            anchors.fill: parent
            z: 9990
            color: "#000000"
            opacity: SystemControl.softwareBrightness
                     ? Math.max(0.0, Math.min(0.85, (100 - SystemControl.brightness) * 0.0085)) : 0
            visible: opacity > 0.01
            enabled: false
        }

        // 5. Camada de cursor acima do wallpaper, janelas, chrome VALO, popups e dimmer
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
