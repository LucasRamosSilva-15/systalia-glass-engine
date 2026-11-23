import QtQuick

SystaliaFilesWindowV2 {
    id: legacyFiles

    caustics: 0.85
    surfaceOpacity: 0.85
    blurProfile: 1

    Binding {
        target: legacyFiles.opticalSurface.glassItemInstance
        property: "ior"
        value: 1 + 0.6 * Math.max(0, legacyFiles.refractiveIndex - 1)
    }

    Binding {
        target: legacyFiles.opticalSurface.glassItemInstance
        property: "chromaticDispersion"
        value: legacyFiles.dispersion * 0.65
    }
}
