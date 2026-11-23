import QtQuick

// Separate overlay/material: the 160-byte optical UBO is unchanged.
ShaderEffect {
    property real animationTime: 0
    property real intensity: 0.35
    property real quality: 2
    property real seedOffset: 0
    property vector2d surfaceSize: Qt.vector2d(width, height)
    property vector4d radii: Qt.vector4d(20, 20, 20, 20)
    property real geometryMode: 0
    property real slant: 0
    fragmentShader: "qrc:/shaders/v2/valo_particles.frag.qsb"
}
