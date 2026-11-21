import QtQuick

Item {
    id: viewport

    property int currentWorkspace: 0
    property int totalWorkspaces: 4
    property bool isTransitioning: false
    property real parallaxFactor: 0.35

    readonly property real viewportOffset: workspaceContainer.x
    readonly property real parallaxOffset: workspaceContainer.x * parallaxFactor
    property alias container: workspaceContainer

    function workspacePageAt(index) {
        if (index < 0 || index >= workspaceRepeater.count)
            return workspaceRepeater.itemAt(0)
        return workspaceRepeater.itemAt(index)
    }

    Item {
        id: workspaceContainer
        objectName: "workspaceContainer"
        width: viewport.width * Math.max(1, viewport.totalWorkspaces)
        height: viewport.height
        x: -viewport.currentWorkspace * viewport.width

        Behavior on x {
            NumberAnimation {
                duration: 320
                easing.type: Easing.OutCubic
            }
        }

        Repeater {
            id: workspaceRepeater
            model: Math.max(1, viewport.totalWorkspaces)

            delegate: Item {
                id: workspacePage
                required property int index
                objectName: "workspacePage_" + index
                x: index * viewport.width
                y: 0
                width: viewport.width
                height: viewport.height
                readonly property real xViewport: workspaceContainer.x + x
                readonly property bool withinViewportFrustum: viewport.width <= 0
                                                              || Math.abs(xViewport) < viewport.width
                visible: (index === viewport.currentWorkspace)
                         || (viewport.isTransitioning && withinViewportFrustum)
                enabled: (index === viewport.currentWorkspace) && !viewport.isTransitioning
            }
        }
    }
}

