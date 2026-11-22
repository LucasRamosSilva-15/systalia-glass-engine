pragma Singleton

import QtQuick

QtObject {
    id: root

    // =========================================================================
    // Camada de Compatibilidade Retroativa (SystaliaTheme -> ValoTheme)
    // =========================================================================
    readonly property color ocean: ValoTheme.primitiveNavy950
    readonly property color deepOcean: "#0a2542"
    readonly property color surface: "#db071628"
    readonly property color surfaceSoft: "#6b0b2036"
    readonly property color border: "#c978b9d0"
    readonly property color borderSoft: "#4676b5d0"
    readonly property color cyan: ValoTheme.accentBright
    readonly property color aqua: "#17f5df"
    readonly property color text: "#f4fcff"
    readonly property color textSecondary: "#d5e2ee"
    readonly property color textMuted: ValoTheme.primitiveTextMuted
    readonly property string fontFamily: ValoTheme.fontFamily

    // Acesso direto ao singleton VALO para migração progressiva
    readonly property var valo: ValoTheme
}
