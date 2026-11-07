#include <QTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

#include "tokens/ValoTokens.hpp"

class TestValoTheme final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void primitivesMatchJsonSpecification();
    void semanticColorsMatchSpecification();
    void metricsMatchSpecification();
    void materialTokensMatchSpecification();
    void compositeStylesMatchSpecification();
    void qmlValoThemeSingletonExposesAllTokens();
    void qmlSystaliaThemeBackwardsCompatibility();
};

void TestValoTheme::initTestCase() {
    // Basic verification of QML initialization
}

void TestValoTheme::primitivesMatchJsonSpecification() {
    using namespace Systalia::ValoTokens;

    // 22 Primitives
    QCOMPARE(primitiveNavy950, QColor("#061426"));
    QCOMPARE(primitiveNavy900, QColor("#071628"));
    QCOMPARE(primitiveNavy850, QColor("#081A2D"));
    QCOMPARE(primitiveNavy800, QColor("#0A1930"));
    QCOMPARE(primitiveNavy750, QColor("#0A253F"));
    QCOMPARE(primitiveNavy700, QColor("#102B46"));
    QCOMPARE(primitiveOcean700, QColor("#134966"));
    QCOMPARE(primitiveOcean600, QColor("#194F76"));
    QCOMPARE(primitiveOcean500, QColor("#206D91"));
    QCOMPARE(primitiveOcean400, QColor("#4BAFC7"));
    QCOMPARE(primitiveCyan300, QColor("#BDF5FF"));
    QCOMPARE(primitiveCyan200, QColor("#DDF7FF"));
    QCOMPARE(primitiveCyan400, QColor("#8AF5FF"));
    QCOMPARE(primitiveCyan500, QColor("#49F3FC"));
    QCOMPARE(primitiveCyan600, QColor("#16D9ED"));
    QCOMPARE(primitiveBlue500, QColor("#007DFC"));
    QCOMPARE(primitiveViolet500, QColor("#9B51E0"));
    QCOMPARE(primitiveMagenta500, QColor("#FF2D75"));
    QCOMPARE(primitiveAmber300, QColor("#FFCC7A"));
    QCOMPARE(primitiveWhite, QColor("#FFFFFF"));
    QCOMPARE(primitiveBlack, QColor("#000000"));
    QCOMPARE(primitiveTextMuted, QColor("#9CBAD3"));
}

void TestValoTheme::semanticColorsMatchSpecification() {
    using namespace Systalia::ValoTokens;

    // 23 Semantic Colors
    QCOMPARE(backgroundDesktop, primitiveNavy950);
    QCOMPARE(backgroundTopbar, primitiveNavy900);
    QCOMPARE(backgroundSidebar, primitiveNavy850);
    QCOMPARE(backgroundControlPanel, primitiveNavy800);

    QCOMPARE(surfaceWindow, primitiveOcean600);
    QCOMPARE(surfaceDock, primitiveNavy700);
    QCOMPARE(surfaceButton, primitiveOcean700);
    QCOMPARE(surfaceInput, primitiveNavy750);

    QCOMPARE(textPrimary, primitiveCyan200);
    QCOMPARE(textSecondary, primitiveTextMuted);
    QCOMPARE(textBright, primitiveWhite);
    QCOMPARE(textAccent, primitiveCyan500);

    QCOMPARE(accentPrimary, primitiveCyan600);
    QCOMPARE(accentBright, primitiveCyan500);
    QCOMPARE(accentBlue, primitiveBlue500);
    QCOMPARE(accentMagenta, primitiveMagenta500);
    QCOMPARE(accentViolet, primitiveViolet500);
    QCOMPARE(accentAmber, primitiveAmber300);

    QCOMPARE(borderHighlight, primitiveCyan300);
    QCOMPARE(borderSubtle, primitiveOcean400);
    QCOMPARE(borderFocus, primitiveCyan500);

    QCOMPARE(iconDefault, primitiveCyan300);
    QCOMPARE(iconActive, primitiveCyan500);
}

void TestValoTheme::metricsMatchSpecification() {
    using namespace Systalia::ValoTokens;

    // 28 Metrics
    QCOMPARE(spacing2, 2.0);
    QCOMPARE(spacing4, 4.0);
    QCOMPARE(spacing8, 8.0);
    QCOMPARE(spacing12, 12.0);
    QCOMPARE(spacing16, 16.0);
    QCOMPARE(spacing20, 20.0);
    QCOMPARE(spacing24, 24.0);
    QCOMPARE(spacing32, 32.0);

    QCOMPARE(radiusIcon, 9.0);
    QCOMPARE(radiusButton, 15.0);
    QCOMPARE(radiusPanel, 13.0);
    QCOMPARE(radiusSidebar, 14.0);
    QCOMPARE(radiusWindow, 20.0);
    QCOMPARE(radiusTopbar, 22.0);
    QCOMPARE(radiusDock, 47.0);

    QCOMPARE(sizeWindowControl, 28.0);
    QCOMPARE(sizeToolbarIcon, 24.0);
    QCOMPARE(sizeSliderHeight, 12.0);
    QCOMPARE(sizeSliderThumb, 20.0);
    QCOMPARE(sizeDockIcon, 78.0);

    QCOMPARE(strokeFine, 1.0);
    QCOMPARE(strokeWindow, 1.5);

    QCOMPARE(fontBody, 16.0);
    QCOMPARE(fontCaption, 14.0);
    QCOMPARE(fontHeading, 21.0);
    QCOMPARE(QString(fontFamily), QStringLiteral("Inter"));

    QCOMPARE(motionDurationFastMs, 120.0);
    QCOMPARE(motionDurationNormalMs, 220.0);
    QCOMPARE(motionDurationSpringMs, 420.0);
}

void TestValoTheme::materialTokensMatchSpecification() {
    using namespace Systalia::ValoTokens;

    // 18 Material & Optical tokens
    QCOMPARE(opacityWindowMin, 0.25);
    QCOMPARE(opacityWindowMax, 0.41);
    QCOMPARE(opacityGlassSpectral, 0.12);
    QCOMPARE(opacityDock, 0.34);
    QCOMPARE(opacitySidebar, 0.88);
    QCOMPARE(opacityTopbar, 0.86);

    QCOMPARE(blurBackdrop, 36.0);
    QCOMPARE(blurCaustic, 44.0);
    QCOMPARE(blurDocumentHighlight, 7.0);
    QCOMPARE(blurVolumetricRay, 50.0);
    QCOMPARE(blurHalo, 35.0);

    QCOMPARE(shadowWindowAnchorY, 8.0);
    QCOMPARE(shadowWindowAnchorBlur, 12.0);
    QCOMPARE(shadowWindowGlowBlur, 40.0);
    QCOMPARE(shadowDockBlur, 20.0);

    QCOMPARE(specularStrokeWidth, 1.5);
    QCOMPARE(glassPrismAngleDeg, -31.0);
    QCOMPARE(glassBackdropFilterEnabled, 1.0);
}

void TestValoTheme::compositeStylesMatchSpecification() {
    using namespace Systalia::ValoTokens;

    QCOMPARE(windowFillStops.size(), 4u);
    QCOMPARE(windowFillStops[0].position, 0.0);
    QCOMPARE(windowFillStops[0].color, QColor("#194F76"));
    QCOMPARE(windowFillStops[0].opacity, 0.39);
    QCOMPARE(windowFillStops[3].color, QColor("#09233F"));
    QCOMPARE(windowFillStops[3].opacity, 0.41);

    QCOMPARE(spectralOverlayStops.size(), 4u);
    QCOMPARE(spectralOverlayStops[0].color, QColor("#00EFFF"));
    QCOMPARE(spectralOverlayStops[3].color, QColor("#9E77FF"));

    QCOMPARE(backdropBlurRadius, 36.0);

    QCOMPARE(windowShadows.size(), 4u);
    QCOMPARE(windowShadows[0].type, ShadowSpec::Type::Inner);
    QCOMPARE(windowShadows[2].type, ShadowSpec::Type::Drop);
    QCOMPARE(windowShadows[2].blur, 12.0);
}

void TestValoTheme::qmlValoThemeSingletonExposesAllTokens() {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "Item {\n"
        "    readonly property color navy950: ValoTheme.primitiveNavy950\n"
        "    readonly property color deskBg: ValoTheme.backgroundDesktop\n"
        "    readonly property real rDock: ValoTheme.radiusDock\n"
        "    readonly property real bBackdrop: ValoTheme.blurBackdrop\n"
        "    readonly property color alphaSample: ValoTheme.withAlpha(ValoTheme.primitiveCyan500, 0.5)\n"
        "    readonly property int stopCount: ValoTheme.windowFillStops.length\n"
        "    readonly property int shadowCount: ValoTheme.windowShadows.length\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    QCOMPARE(obj->property("navy950").value<QColor>(), QColor("#061426"));
    QCOMPARE(obj->property("deskBg").value<QColor>(), QColor("#061426"));
    QCOMPARE(obj->property("rDock").toReal(), 47.0);
    QCOMPARE(obj->property("bBackdrop").toReal(), 36.0);
    QCOMPARE(obj->property("stopCount").toInt(), 4);
    QCOMPARE(obj->property("shadowCount").toInt(), 4);

    const QColor alphaSample = obj->property("alphaSample").value<QColor>();
    QVERIFY(qAbs(alphaSample.alphaF() - 0.5) < 0.001);
}

void TestValoTheme::qmlSystaliaThemeBackwardsCompatibility() {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "Item {\n"
        "    readonly property color ocean: SystaliaTheme.ocean\n"
        "    readonly property color textMuted: SystaliaTheme.textMuted\n"
        "    readonly property string family: SystaliaTheme.fontFamily\n"
        "    readonly property bool hasValo: SystaliaTheme.valo !== undefined\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    QCOMPARE(obj->property("ocean").value<QColor>(), QColor("#061426"));
    QCOMPARE(obj->property("textMuted").value<QColor>(), QColor("#9CBAD3"));
    QCOMPARE(obj->property("family").toString(), QStringLiteral("Inter"));
    QVERIFY(obj->property("hasValo").toBool());
}

QTEST_MAIN(TestValoTheme)
#include "tst_ValoTheme.moc"
