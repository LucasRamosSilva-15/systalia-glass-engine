#pragma once
#include <QColor>
#include <QtGlobal>
#include <array>

namespace Systalia::ValoTokens {

// =========================================================================
// 1. PRIMITIVES (22 tokens)
// =========================================================================
inline const QColor primitiveNavy950 = QColor("#061426");
inline const QColor primitiveNavy900 = QColor("#071628");
inline const QColor primitiveNavy850 = QColor("#081A2D");
inline const QColor primitiveNavy800 = QColor("#0A1930");
inline const QColor primitiveNavy750 = QColor("#0A253F");
inline const QColor primitiveNavy700 = QColor("#102B46");
inline const QColor primitiveOcean700 = QColor("#134966");
inline const QColor primitiveOcean600 = QColor("#194F76");
inline const QColor primitiveOcean500 = QColor("#206D91");
inline const QColor primitiveOcean400 = QColor("#4BAFC7");
inline const QColor primitiveCyan300 = QColor("#BDF5FF");
inline const QColor primitiveCyan200 = QColor("#DDF7FF");
inline const QColor primitiveCyan400 = QColor("#8AF5FF");
inline const QColor primitiveCyan500 = QColor("#49F3FC");
inline const QColor primitiveCyan600 = QColor("#16D9ED");
inline const QColor primitiveBlue500 = QColor("#007DFC");
inline const QColor primitiveViolet500 = QColor("#9B51E0");
inline const QColor primitiveMagenta500 = QColor("#FF2D75");
inline const QColor primitiveAmber300 = QColor("#FFCC7A");
inline const QColor primitiveWhite = QColor("#FFFFFF");
inline const QColor primitiveBlack = QColor("#000000");
inline const QColor primitiveTextMuted = QColor("#9CBAD3");

// VALO V3 — 11 additive acrylic icon primitives (Figma 2026-11-07)
inline const QColor primitiveIce100 = QColor("#F6FFFF");
inline const QColor primitiveIce200 = QColor("#CFFBFF");
inline const QColor primitiveIce300 = QColor("#A4F6FF");
inline const QColor primitiveAqua400 = QColor("#79EFFF");
inline const QColor primitiveAquaEdge = QColor("#C1FAFF");
inline const QColor primitiveAquaGlass = QColor("#6ADEED");
inline const QColor primitiveBlueFolder400 = QColor("#2BCCF5");
inline const QColor primitiveBlueFolder700 = QColor("#235BA9");
inline const QColor primitiveOceanAcrylic600 = QColor("#285D86");
inline const QColor primitiveNavyAcrylic800 = QColor("#152C65");
inline const QColor primitiveVioletEdge = QColor("#738ADD");

// =========================================================================
// 2. SEMANTIC COLORS (23 tokens)
// =========================================================================
inline const QColor backgroundDesktop = QColor("#061426");
inline const QColor backgroundTopbar = QColor("#071628");
inline const QColor backgroundSidebar = QColor("#081A2D");
inline const QColor backgroundControlPanel = QColor("#0A1930");
inline const QColor surfaceWindow = QColor("#194F76");
inline const QColor surfaceDock = QColor("#102B46");
inline const QColor surfaceButton = QColor("#134966");
inline const QColor surfaceInput = QColor("#0A253F");
inline const QColor textPrimary = QColor("#DDF7FF");
inline const QColor textSecondary = QColor("#9CBAD3");
inline const QColor textBright = QColor("#FFFFFF");
inline const QColor textAccent = QColor("#49F3FC");
inline const QColor accentPrimary = QColor("#16D9ED");
inline const QColor accentBright = QColor("#49F3FC");
inline const QColor accentBlue = QColor("#007DFC");
inline const QColor accentMagenta = QColor("#FF2D75");
inline const QColor accentViolet = QColor("#9B51E0");
inline const QColor accentAmber = QColor("#FFCC7A");
inline const QColor borderHighlight = QColor("#BDF5FF");
inline const QColor borderSubtle = QColor("#4BAFC7");
inline const QColor borderFocus = QColor("#49F3FC");
inline const QColor iconDefault = QColor("#BDF5FF");
inline const QColor iconActive = QColor("#49F3FC");

// VALO V3 — 12 semantic icon roles; primitive values are retained for compatibility
inline const QColor iconSidebarDefault = primitiveAqua400;
inline const QColor iconSidebarSelected = primitiveIce300;
inline const QColor iconFileEdge = primitiveAquaEdge;
inline const QColor iconFileHighlight = primitiveIce100;
inline const QColor iconFileGlassLight = primitiveIce200;
inline const QColor iconFileGlassMid = primitiveAquaGlass;
inline const QColor iconFileGlassDeep = primitiveOceanAcrylic600;
inline const QColor iconFileFold = primitiveIce300;
inline const QColor iconFolderFront = primitiveBlueFolder400;
inline const QColor iconFolderBack = primitiveBlueFolder700;
inline const QColor iconFileSymbol = primitiveAqua400;
inline const QColor iconFileEdgeShadow = primitiveVioletEdge;

// =========================================================================
// 3. METRICS (28 tokens)
// =========================================================================
inline constexpr qreal spacing2 = 2;
inline constexpr qreal spacing4 = 4;
inline constexpr qreal spacing8 = 8;
inline constexpr qreal spacing12 = 12;
inline constexpr qreal spacing16 = 16;
inline constexpr qreal spacing20 = 20;
inline constexpr qreal spacing24 = 24;
inline constexpr qreal spacing32 = 32;
inline constexpr qreal radiusIcon = 9;
inline constexpr qreal radiusButton = 15;
inline constexpr qreal radiusPanel = 13;
inline constexpr qreal radiusSidebar = 14;
inline constexpr qreal radiusWindow = 20;
inline constexpr qreal radiusTopbar = 22;
inline constexpr qreal radiusDock = 47;
inline constexpr qreal sizeWindowControl = 28;
inline constexpr qreal sizeToolbarIcon = 24;
inline constexpr qreal sizeSliderHeight = 12;
inline constexpr qreal sizeSliderThumb = 20;
inline constexpr qreal sizeDockIcon = 78;
inline constexpr qreal strokeFine = 1;
inline constexpr qreal strokeWindow = 1.5;
inline constexpr qreal fontBody = 16;
inline constexpr qreal fontCaption = 14;
inline constexpr qreal fontHeading = 21;
inline constexpr auto fontFamily = "Inter";
inline constexpr qreal motionDurationFastMs = 120;
inline constexpr qreal motionDurationNormalMs = 220;
inline constexpr qreal motionDurationSpringMs = 420;

// VALO V3 — 6 acrylic icon dimensions and strokes (logical pixels)
inline constexpr qreal sizeFileIconWidth = 55;
inline constexpr qreal sizeFileIconHeight = 67;
inline constexpr qreal sizeFolderIconWidth = 74;
inline constexpr qreal sizeSidebarGlyph = 23;
inline constexpr qreal strokeSidebarIcon = 1.65;
inline constexpr qreal strokeFileOutline = 1.35;

// =========================================================================
// 4. MATERIAL & OPTICAL (18 tokens)
// =========================================================================
inline constexpr qreal opacityWindowMin = 0.25;
inline constexpr qreal opacityWindowMax = 0.41;
inline constexpr qreal opacityGlassSpectral = 0.12;
inline constexpr qreal opacityDock = 0.34;
inline constexpr qreal opacitySidebar = 0.88;
inline constexpr qreal opacityTopbar = 0.86;
inline constexpr qreal blurBackdrop = 36;
inline constexpr qreal blurCaustic = 44;
inline constexpr qreal blurDocumentHighlight = 7;
inline constexpr qreal blurVolumetricRay = 50;
inline constexpr qreal blurHalo = 35;
inline constexpr qreal shadowWindowAnchorY = 8;
inline constexpr qreal shadowWindowAnchorBlur = 12;
inline constexpr qreal shadowWindowGlowBlur = 40;
inline constexpr qreal shadowDockBlur = 20;
inline constexpr qreal specularStrokeWidth = 1.5;
inline constexpr qreal glassPrismAngleDeg = -31;
inline constexpr qreal glassBackdropFilterEnabled = 1;

// VALO V3 — 4 acrylic file icon material parameters
inline constexpr qreal shadowFileY = 5;
inline constexpr qreal shadowFileBlur = 7;
inline constexpr qreal opacityFileHighlight = 0.36;
inline constexpr qreal opacityFileGlassEdge = 0.88;

// =========================================================================
// 5. COMPOSITE STYLES
// =========================================================================
struct GradientStop {
    qreal position;
    QColor color;
    qreal opacity;
};

struct ShadowSpec {
    enum class Type { Inner, Drop };
    Type type;
    qreal x;
    qreal y;
    qreal blur;
    QColor color;
    qreal opacity;
};

inline const std::array<GradientStop, 4> windowFillStops = {{
    { 0.0,  QColor("#194F76"), 0.39 },
    { 0.2,  QColor("#4BAFC7"), 0.27 },
    { 0.53, QColor("#133A5D"), 0.27 },
    { 1.0,  QColor("#09233F"), 0.41 }
}};

inline const std::array<GradientStop, 4> spectralOverlayStops = {{
    { 0.0,  QColor("#00EFFF"), 0.10 },
    { 0.29, QColor("#F5FFFF"), 0.12 },
    { 0.60, QColor("#F8C47B"), 0.07 },
    { 1.0,  QColor("#9E77FF"), 0.08 }
}};

inline constexpr qreal backdropBlurRadius = 36.0;

inline const std::array<ShadowSpec, 4> windowShadows = {{
    { ShadowSpec::Type::Inner, 0.0,  1.0,  0.0, QColor("#FFFFFF"), 0.26 },
    { ShadowSpec::Type::Inner, 0.0, -2.0,  4.0, QColor("#000A1B"), 0.40 },
    { ShadowSpec::Type::Drop,  0.0,  8.0, 12.0, QColor("#000000"), 0.45 },
    { ShadowSpec::Type::Drop,  0.0, 20.0, 40.0, QColor("#00E5FF"), 0.14 }
}};

} // namespace Systalia::ValoTokens
