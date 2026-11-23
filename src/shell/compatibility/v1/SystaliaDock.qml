import QtQuick
import Systalia.Engine

// Preserve the published QML name and resource URL.
SystaliaDockV2 {
    id: dock

    performanceProfile: "balanced"
    caustics: 0.65
    blurProfile: performanceProfile === "performance" ? 0 : performanceProfile === "quality" ? 2 : 1

    Binding {
        target: dock.valoDockInstance.glassItemInstance
        property: "surfaceOpacity"
        value: dock.materialActive ? dock.surfaceOpacity : 0
    }
    Binding {
        target: dock.valoDockInstance.glassItemInstance
        property: "ior"
        value: 1 + (dock.blurProfile === 2 ? 0.25 : dock.blurProfile === 1 ? 0.15 : 0)
               * Math.max(0, (dock.refractiveIndex - 1) / 0.25)
    }
    Binding {
        target: dock.valoDockInstance.glassItemInstance
        property: "chromaticDispersion"
        value: dock.dispersion * (dock.blurProfile === 2 ? 1 : dock.blurProfile === 1 ? 0.65 : 0)
    }
}
