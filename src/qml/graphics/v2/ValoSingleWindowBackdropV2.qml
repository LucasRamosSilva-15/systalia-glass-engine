import QtQuick

// Bounded overlay backdrop: exactly one internal producer and no clients.
// Never captures a common ancestor or any consumer.
Item {
    id: root
    required property Item firstWindow
    required property Item secondWindow
    required property Item wallpaperSource
    required property Item topBar
    required property Item dock
    required property Item workspacePage
    property bool enabledForWorkspace: true
    property bool externalClientsPresent: false
    property real mappingRevision: 0
    readonly property Item producer: firstWindow && secondWindow && firstWindow.visible !== secondWindow.visible
        ? (firstWindow.visible ? firstWindow : secondWindow) : null
    readonly property bool overlaysDisjoint: {
        if (!topBar || !dock || !topBar.visible || !dock.visible) return true
        if (dock.parent) { void(dock.parent.x); void(dock.parent.y); void(dock.parent.scale) }
        void(topBar.x); void(topBar.y); void(topBar.width); void(topBar.height)
        void(dock.width); void(dock.height)
        const a=topBar.mapToItem(root,0,0), b=topBar.mapToItem(root,topBar.width,topBar.height)
        const c=dock.mapToItem(root,0,0), d=dock.mapToItem(root,dock.width,dock.height)
        return Math.max(a.x,b.x) <= Math.min(c.x,d.x) || Math.max(c.x,d.x) <= Math.min(a.x,b.x)
            || Math.max(a.y,b.y) <= Math.min(c.y,d.y) || Math.max(c.y,d.y) <= Math.min(a.y,b.y)
    }
    readonly property bool eligible: enabledForWorkspace && !externalClientsPresent && producer && overlaysDisjoint
        && producer.parent === workspacePage && producer.scale === 1 && producer.rotation === 0
    readonly property point producerPosition: {
        void(mappingRevision)
        if (!producer) return Qt.point(0,0)
        void(producer.x); void(producer.y)
        return producer.mapToItem(root,0,0)
    }
    function overlaps(item) {
        if (!eligible || !producer || !item || !item.visible) return false
        // Read ancestor geometry to track the dock's animated position and scale.
        if (item.parent) { void(item.parent.x); void(item.parent.y); void(item.parent.scale) }
        void(item.x); void(item.y); void(item.width); void(item.height)
        const p=item.mapToItem(root,0,0)
        const q=item.mapToItem(root,item.width,item.height)
        return producerPosition.x < Math.max(p.x,q.x) && Math.min(p.x,q.x) < producerPosition.x + producer.width
            && producerPosition.y < Math.max(p.y,q.y) && Math.min(p.y,q.y) < producerPosition.y + producer.height
    }
    readonly property bool topBarAvailable: overlaps(topBar)
    readonly property bool dockAvailable: overlaps(dock)
    readonly property bool compositeAvailable: topBarAvailable || dockAvailable
    readonly property string coverageMode: compositeAvailable ? "single-internal-window" : "wallpaper-only"
    function sourceFor(item) {
        return ((item === topBar && topBarAvailable) || (item === dock && dockAvailable))
            ? compositeGrab : wallpaperSource
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
            anchors.fill: parent
            sourceRect: Qt.rect(-root.producerPosition.x,-root.producerPosition.y,root.width,root.height)
            opacity: root.producer ? root.producer.opacity : 1
            sourceItem: root.compositeAvailable ? root.producer : null
            textureSize: Qt.size(Math.max(1,width*Screen.devicePixelRatio),Math.max(1,height*Screen.devicePixelRatio))
            live: root.compositeAvailable
            recursive: false
            hideSource: false
        }
    }
    ShaderEffectSource {
        id: compositeGrab
        objectName: "singleWindowOverlayBackdropV2"
        anchors.fill: parent
        sourceItem: root.compositeAvailable ? composition : null
        textureSize: Qt.size(Math.max(1,width*Screen.devicePixelRatio),Math.max(1,height*Screen.devicePixelRatio))
        live: root.compositeAvailable
        recursive: false
        hideSource: false
    }
}
