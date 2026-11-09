import QtQuick

// Bounded to three independent, explicitly supplied internal producers.
// Producers must share the consumer's workspace and be ordered behind it.
Item {
    id: root
    required property Item wallpaperSource
    required property Item workspacePage
    required property Item consumer
    property list<Item> candidates
    property bool enabledForWorkspace: true
    property bool externalClientsPresent: false
    property real mappingRevision: 0
    // When consumer is outside workspacePage its caller must establish that it
    // is above every candidate (e.g. a shell overlay).
    property bool consumerAboveWorkspace: false
    readonly property var selected: {
        void(mappingRevision)
        if (!enabledForWorkspace || externalClientsPresent || !consumer || !consumer.visible)
            return []
        if (!consumerAboveWorkspace && consumer.parent !== workspacePage)
            return []
        void(consumer.x); void(consumer.y); void(consumer.width); void(consumer.height); void(consumer.z)
        if (consumer.parent) {
            void(consumer.parent.x); void(consumer.parent.y); void(consumer.parent.scale); void(consumer.parent.rotation)
        }
        const a = consumer.mapToItem(root, 0, 0)
        const b = consumer.mapToItem(root, consumer.width, consumer.height)
        let items = []
        for (let i = 0; i < candidates.length; ++i) {
            const item = candidates[i]
            if (item) { void(item.x); void(item.y); void(item.width); void(item.height); void(item.z) }
            if (!item || items.indexOf(item) !== -1 || item === consumer || item.parent !== workspacePage || !item.visible
                    || item.opacity <= 0 || item.scale !== 1 || item.rotation !== 0
                    || (!consumerAboveWorkspace && (item.z > consumer.z
                        || (item.z === consumer.z && workspacePage.children.indexOf(item) >= workspacePage.children.indexOf(consumer)))))
                continue
            const p = item.mapToItem(root, 0, 0)
            if (p.x >= Math.max(a.x,b.x) || p.x + item.width <= Math.min(a.x,b.x)
                    || p.y >= Math.max(a.y,b.y) || p.y + item.height <= Math.min(a.y,b.y))
                continue
            items.push(item)
        }
        items.sort((a,b) => a.z === b.z ? workspacePage.children.indexOf(a) - workspacePage.children.indexOf(b) : a.z - b.z)
        return items.slice(Math.max(0, items.length - 3))
    }
    readonly property int selectedCount: selected.length
    readonly property bool compositeAvailable: selectedCount > 0
    readonly property alias compositeSource: compositeGrab
    readonly property var selectedNames: selected.map(item => item.objectName)
    visible: false

    Item {
        id: composition
        anchors.fill: parent
        ShaderEffect {
            anchors.fill: parent
            property var source: root.wallpaperSource
            fragmentShader: "qrc:/shaders/v2/valo_backdrop_copy.frag.qsb"
        }
        Repeater {
            // Keep the capture items stable while geometry or ordering changes.
            model: 3
            ShaderEffectSource {
                required property int index
                readonly property Item producer: root.selected[index] || null
                anchors.fill: parent
                readonly property point position: {
                    void(root.mappingRevision)
                    if (!producer) return Qt.point(0,0)
                    void(producer.x); void(producer.y)
                    return producer.mapToItem(root,0,0)
                }
                sourceRect: Qt.rect(-position.x, -position.y, root.width, root.height)
                sourceItem: producer
                opacity: producer ? producer.opacity : 0
                textureSize: Qt.size(Math.max(1,width*Screen.devicePixelRatio),Math.max(1,height*Screen.devicePixelRatio))
                live: producer !== null && root.compositeAvailable
                recursive: false
                hideSource: false
            }
        }
    }
    ShaderEffectSource {
        id: compositeGrab
        anchors.fill: parent
        sourceItem: root.compositeAvailable ? composition : null
        textureSize: Qt.size(Math.max(1,width*Screen.devicePixelRatio),Math.max(1,height*Screen.devicePixelRatio))
        live: root.compositeAvailable
        recursive: false
        hideSource: false
    }
}
