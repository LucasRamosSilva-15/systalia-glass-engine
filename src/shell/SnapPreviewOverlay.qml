import QtQuick
import Systalia.Engine

Item {
    id: previewRoot
    objectName: "snapPreviewOverlay"

    property bool active: false
    property rect targetGeometry: Qt.rect(0, 0, 0, 0)
    property vector4d targetRadii: Qt.vector4d(24.0, 24.0, 24.0, 24.0)
    property int blurProfile: 1
    property bool userHighContrast: false
    property bool userPrefersReducedMotion: false
    property var backdropSource: null
    property vector2d screenResolution: Qt.vector2d(1662, 975)
    property real animationTime: 0.0

    x: targetGeometry.x
    y: targetGeometry.y
    width: Math.max(0, targetGeometry.width)
    height: Math.max(0, targetGeometry.height)

    opacity: active ? 1.0 : 0.0
    scale: active ? 1.0 : 0.96
    visible: opacity > 0.01 && width > 0 && height > 0

    Behavior on x {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }
    Behavior on y {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }
    Behavior on width {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
    Behavior on height {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
    Behavior on opacity {
        NumberAnimation {
            duration: 150
            easing.type: Easing.OutCubic
        }
    }
    Behavior on scale {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutBack
        }
    }

    // Superfície translúcida VALO Glass com opacidade atenuada (0.35)
    ValoMaterialSurfaceV2 {
        objectName: "SnapPreviewOverlaySurfaceV2"
        geometryMode: ValoGlassItemV2.SquircleG2
        backdropSource: previewRoot.backdropSource
        cornerRadii: previewRoot.targetRadii
        blurProfile: previewRoot.blurProfile
        userHighContrast: previewRoot.userHighContrast
        userPrefersReducedMotion: previewRoot.userPrefersReducedMotion
        id: previewGlass
        anchors.fill: parent
        surfaceOpacity: 0.35
        volumetricTint: "#00bccc"
        specularStrokeWidth: 1.8
        particleIntensity: 0
        Behavior on cornerRadii {
            PropertyAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

}

