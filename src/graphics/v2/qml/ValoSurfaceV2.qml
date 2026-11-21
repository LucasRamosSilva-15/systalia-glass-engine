import QtQuick
import Systalia.Engine

/**
 * ValoSurfaceV2.qml - Encapsulamento QML do Renderer VALO Graphics 2.0 (Etapa 02)
 *
 * Expõe as propriedades do material analítico SDF, modos geométricos (L2 / G2),
 * contorno especular unificado e composição alpha pré-multiplicada.
 */
Item {
    id: root

    property alias cornerRadii: glassItem.cornerRadii
    property alias geometryMode: glassItem.geometryMode
    property alias surfaceOpacity: glassItem.surfaceOpacity
    property alias specularStrokeWidth: glassItem.specularStrokeWidth
    property alias volumetricTint: glassItem.volumetricTint
    property alias active: glassItem.active
    property alias debugMode: glassItem.debugMode
    property alias slant: glassItem.slant
    property alias opticalContext: glassItem.opticalContext

    implicitWidth: 300
    implicitHeight: 180

    ValoGlassItemV2 {
        id: glassItem
        anchors.fill: parent
        cornerRadii: Qt.vector4d(32, 32, 32, 32)
        geometryMode: ValoGlassItemV2.CircularL2
        surfaceOpacity: 0.34
        specularStrokeWidth: 1.25
        volumetricTint: Qt.rgba(0.03, 0.18, 0.32, 0.39)
        active: true
        debugMode: 0
    }
}

