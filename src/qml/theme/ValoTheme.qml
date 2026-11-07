pragma Singleton

import QtQuick

QtObject {
    id: root

    // =========================================================================
    // 1. PRIMITIVES (22 tokens)
    // =========================================================================
    readonly property color primitiveNavy950: "#061426"
    readonly property color primitiveNavy900: "#071628"
    readonly property color primitiveNavy850: "#081A2D"
    readonly property color primitiveNavy800: "#0A1930"
    readonly property color primitiveNavy750: "#0A253F"
    readonly property color primitiveNavy700: "#102B46"
    readonly property color primitiveOcean700: "#134966"
    readonly property color primitiveOcean600: "#194F76"
    readonly property color primitiveOcean500: "#206D91"
    readonly property color primitiveOcean400: "#4BAFC7"
    readonly property color primitiveCyan300: "#BDF5FF"
    readonly property color primitiveCyan200: "#DDF7FF"
    readonly property color primitiveCyan400: "#8AF5FF"
    readonly property color primitiveCyan500: "#49F3FC"
    readonly property color primitiveCyan600: "#16D9ED"
    readonly property color primitiveBlue500: "#007DFC"
    readonly property color primitiveViolet500: "#9B51E0"
    readonly property color primitiveMagenta500: "#FF2D75"
    readonly property color primitiveAmber300: "#FFCC7A"
    readonly property color primitiveWhite: "#FFFFFF"
    readonly property color primitiveBlack: "#000000"
    readonly property color primitiveTextMuted: "#9CBAD3"

    // =========================================================================
    // 2. SEMANTIC COLORS (23 tokens)
    // =========================================================================
    // Background roles
    readonly property color backgroundDesktop: primitiveNavy950
    readonly property color backgroundTopbar: primitiveNavy900
    readonly property color backgroundSidebar: primitiveNavy850
    readonly property color backgroundControlPanel: primitiveNavy800

    // Surface roles
    readonly property color surfaceWindow: primitiveOcean600
    readonly property color surfaceDock: primitiveNavy700
    readonly property color surfaceButton: primitiveOcean700
    readonly property color surfaceInput: primitiveNavy750

    // Text roles
    readonly property color textPrimary: primitiveCyan200
    readonly property color textSecondary: primitiveTextMuted
    readonly property color textBright: primitiveWhite
    readonly property color textAccent: primitiveCyan500

    // Accent roles
    readonly property color accentPrimary: primitiveCyan600
    readonly property color accentBright: primitiveCyan500
    readonly property color accentBlue: primitiveBlue500
    readonly property color accentMagenta: primitiveMagenta500
    readonly property color accentViolet: primitiveViolet500
    readonly property color accentAmber: primitiveAmber300

    // Border roles
    readonly property color borderHighlight: primitiveCyan300
    readonly property color borderSubtle: primitiveOcean400
    readonly property color borderFocus: primitiveCyan500

    // Icon roles
    readonly property color iconDefault: primitiveCyan300
    readonly property color iconActive: primitiveCyan500

    // =========================================================================
    // 3. METRICS (28 tokens)
    // =========================================================================
    // Spacings
    readonly property real spacing2: 2
    readonly property real spacing4: 4
    readonly property real spacing8: 8
    readonly property real spacing12: 12
    readonly property real spacing16: 16
    readonly property real spacing20: 20
    readonly property real spacing24: 24
    readonly property real spacing32: 32

    // Radii
    readonly property real radiusIcon: 9
    readonly property real radiusButton: 15
    readonly property real radiusPanel: 13
    readonly property real radiusSidebar: 14
    readonly property real radiusWindow: 20
    readonly property real radiusTopbar: 22
    readonly property real radiusDock: 47

    // Sizes
    readonly property real sizeWindowControl: 28
    readonly property real sizeToolbarIcon: 24
    readonly property real sizeSliderHeight: 12
    readonly property real sizeSliderThumb: 20
    readonly property real sizeDockIcon: 78

    // Strokes
    readonly property real strokeFine: 1
    readonly property real strokeWindow: 1.5

    // Typography
    readonly property real fontBody: 16
    readonly property real fontCaption: 14
    readonly property real fontHeading: 21
    readonly property string fontFamily: "Inter"

    // Motion durations (ms)
    readonly property int motionDurationFastMs: 120
    readonly property int motionDurationNormalMs: 220
    readonly property int motionDurationSpringMs: 420

    // =========================================================================
    // 4. MATERIAL & OPTICAL (18 tokens)
    // =========================================================================
    // Opacities
    readonly property real opacityWindowMin: 0.25
    readonly property real opacityWindowMax: 0.41
    readonly property real opacityGlassSpectral: 0.12
    readonly property real opacityDock: 0.34
    readonly property real opacitySidebar: 0.88
    readonly property real opacityTopbar: 0.86

    // Blurs (px)
    readonly property real blurBackdrop: 36
    readonly property real blurCaustic: 44
    readonly property real blurDocumentHighlight: 7
    readonly property real blurVolumetricRay: 50
    readonly property real blurHalo: 35

    // Shadows
    readonly property real shadowWindowAnchorY: 8
    readonly property real shadowWindowAnchorBlur: 12
    readonly property real shadowWindowGlowBlur: 40
    readonly property real shadowDockBlur: 20

    // Specular / Prism / Filters
    readonly property real specularStrokeWidth: 1.5
    readonly property real glassPrismAngleDeg: -31
    readonly property real glassBackdropFilterEnabled: 1.0
    readonly property bool backdropFilterEnabled: true

    // =========================================================================
    // 5. COMPOSITE STYLES (windowFillStops, spectralOverlayStops, windowShadows)
    // =========================================================================
    readonly property var windowFillStops: [
        { position: 0.0,  color: "#194F76", opacity: 0.39 },
        { position: 0.2,  color: "#4BAFC7", opacity: 0.27 },
        { position: 0.53, color: "#133A5D", opacity: 0.27 },
        { position: 1.0,  color: "#09233F", opacity: 0.41 }
    ]

    readonly property var spectralOverlayStops: [
        { position: 0.0,  color: "#00EFFF", opacity: 0.10 },
        { position: 0.29, color: "#F5FFFF", opacity: 0.12 },
        { position: 0.60, color: "#F8C47B", opacity: 0.07 },
        { position: 1.0,  color: "#9E77FF", opacity: 0.08 }
    ]

    readonly property real backdropBlurRadius: 36

    readonly property var windowShadows: [
        { type: "inner", x: 0, y:  1, blur:  0, color: "#FFFFFF", opacity: 0.26 },
        { type: "inner", x: 0, y: -2, blur:  4, color: "#000A1B", opacity: 0.40 },
        { type: "drop",  x: 0, y:  8, blur: 12, color: "#000000", opacity: 0.45 },
        { type: "drop",  x: 0, y: 20, blur: 40, color: "#00E5FF", opacity: 0.14 }
    ]

    // Helper functions
    function withAlpha(baseColor, alpha) {
        return Qt.rgba(baseColor.r, baseColor.g, baseColor.b, alpha)
    }
}

