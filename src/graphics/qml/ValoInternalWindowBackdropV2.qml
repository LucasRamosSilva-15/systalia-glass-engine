import QtQuick

// Deliberately bounded to two internal windows. Never capture their common
// ancestor: that would include the consumer, clients, and create a texture loop.
Item {
    id: root
    required property Item firstWindow
    required property Item secondWindow
    required property Item wallpaperSource
    property bool enabledForWorkspace: true
    property bool externalClientsPresent: false
    property real mappingRevision: 0
    readonly property bool eligible: enabledForWorkspace && !externalClientsPresent && firstWindow && secondWindow
        && firstWindow.visible && secondWindow.visible
        && firstWindow.parent === secondWindow.parent
        && firstWindow.scale === 1 && secondWindow.scale === 1
        && firstWindow.rotation === 0 && secondWindow.rotation === 0
    readonly property Item frontWindow: eligible
        ? (firstWindow.z > secondWindow.z ? firstWindow : secondWindow) : null
    readonly property Item rearWindow: eligible
        ? (frontWindow === firstWindow ? secondWindow : firstWindow) : null
    readonly property point rearPosition: {
        const revision = mappingRevision
        const x = rearWindow ? rearWindow.x : 0
        const y = rearWindow ? rearWindow.y : 0
        return rearWindow ? rearWindow.mapToItem(root, 0, 0) : Qt.point(0, 0)
    }
    readonly property bool compositeAvailable: eligible
        && firstWindow.x < secondWindow.x + secondWindow.width
        && secondWindow.x < firstWindow.x + firstWindow.width
        && firstWindow.y < secondWindow.y + secondWindow.height
        && secondWindow.y < firstWindow.y + firstWindow.height
    readonly property string coverageMode: compositeAvailable
        ? "internal-pair" : "wallpaper-only"
    readonly property alias compositeSource: compositeGrab
    function sourceFor(consumer) {
        return compositeAvailable && consumer === frontWindow ? compositeGrab : wallpaperSource
    }
    visible: false

    Item {
        id: composition
        anchors.fill: parent
        ShaderEffect {
            anchors.fill: parent
            property var source: root.wallpaperSource
            fragmentShader: "qrc:/shaders/v2/valo_backdrop_copy.frag.qsb"
        }
        ShaderEffectSource {
            id: rearCapture
            // Optical shaders use viewport gl_FragCoord. A window-sized target
            // would shift its backdrop UV while rendering this offscreen copy.
            anchors.fill: parent
            sourceRect: Qt.rect(-root.rearPosition.x, -root.rearPosition.y, root.width, root.height)
            opacity: root.rearWindow ? root.rearWindow.opacity : 1
            sourceItem: root.compositeAvailable ? root.rearWindow : null
            textureSize: Qt.size(Math.max(1, width * Screen.devicePixelRatio),
                                 Math.max(1, height * Screen.devicePixelRatio))
            live: root.compositeAvailable
            recursive: false
            hideSource: false
        }
    }
    ShaderEffectSource {
        id: compositeGrab
        objectName: "internalPairBackdropCaptureV2"
        anchors.fill: parent
        sourceItem: root.compositeAvailable ? composition : null
        textureSize: Qt.size(Math.max(1, width * Screen.devicePixelRatio),
                             Math.max(1, height * Screen.devicePixelRatio))
        live: root.compositeAvailable
        recursive: false
        hideSource: false
    }
}
