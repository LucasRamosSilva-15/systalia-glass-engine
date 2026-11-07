#include <QTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVector4D>
#include <QVector2D>
#include <QImage>
#include <QDir>

#include "gfx/SystaliaGlassItem.hpp"
#include "tokens/ValoTokens.hpp"

class TestValoGlass final : public QObject {
    Q_OBJECT

private:
    QQmlEngine m_engine;

private slots:
    void initTestCase();
    void defaultPropertiesMatchValoTheme();
    void performanceProfilesAdaptOpticalParameters();
    void contentNestingInContentContainer();
    void activeStateModulatesIllumination();
    void renderThreeProfilesAndCaptureComparison();
};

void TestValoGlass::initTestCase() {
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
}

void TestValoGlass::defaultPropertiesMatchValoTheme() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "ValoGlass {\n"
        "    id: glass\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    auto *glass = qobject_cast<QQuickItem *>(obj.data());
    QVERIFY(glass);

    // Verificação das 10 propriedades principais com padrões do ValoTheme
    QCOMPARE(glass->property("glassOpacity").toReal(), Systalia::ValoTokens::opacityWindowMax);
    QCOMPARE(glass->property("blurRadius").toReal(), Systalia::ValoTokens::blurBackdrop);
    QCOMPARE(glass->property("cornerRadius").toReal(), Systalia::ValoTokens::radiusWindow);
    QCOMPARE(glass->property("cornerRadii").value<QVector4D>(),
             QVector4D(Systalia::ValoTokens::radiusWindow, Systalia::ValoTokens::radiusWindow,
                       Systalia::ValoTokens::radiusWindow, Systalia::ValoTokens::radiusWindow));
    QCOMPARE(glass->property("borderIntensity").toReal(), 1.0);
    QCOMPARE(glass->property("iridescenceStrength").toReal(), 1.0);
    QCOMPARE(glass->property("refractionStrength").toReal(), 1.0);
    QCOMPARE(glass->property("chromaticDispersion").toReal(), 0.040);
    QCOMPARE(glass->property("glowIntensity").toReal(), 1.0);
    QCOMPARE(glass->property("active").toBool(), true);
    QCOMPARE(glass->property("performanceProfile").toString(), QStringLiteral("balanced"));
}

void TestValoGlass::performanceProfilesAdaptOpticalParameters() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "Item {\n"
        "    ValoGlass {\n"
        "        id: glassQuality\n"
        "        objectName: 'glassQuality'\n"
        "        performanceProfile: 'quality'\n"
        "        refractionStrength: 1.0\n"
        "        chromaticDispersion: 0.040\n"
        "    }\n"
        "    ValoGlass {\n"
        "        id: glassBalanced\n"
        "        objectName: 'glassBalanced'\n"
        "        performanceProfile: 'balanced'\n"
        "        refractionStrength: 1.0\n"
        "        chromaticDispersion: 0.040\n"
        "    }\n"
        "    ValoGlass {\n"
        "        id: glassPerformance\n"
        "        objectName: 'glassPerformance'\n"
        "        performanceProfile: 'performance'\n"
        "        refractionStrength: 1.0\n"
        "        chromaticDispersion: 0.040\n"
        "    }\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> root(component.create());
    QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

    auto *gQuality = root->findChild<QQuickItem *>("glassQuality");
    auto *gBalanced = root->findChild<QQuickItem *>("glassBalanced");
    auto *gPerformance = root->findChild<QQuickItem *>("glassPerformance");

    QVERIFY(gQuality && gBalanced && gPerformance);

    // 1. Perfil Quality: máximo detalhamento óptico
    QCOMPARE(gQuality->property("effectiveIor").toReal(), 1.25);
    QCOMPARE(gQuality->property("effectiveDispersion").toReal(), 0.040);
    QCOMPARE(gQuality->property("effectiveCaustics").toReal(), 0.85);

    // 2. Perfil Balanced: equilíbrio entre fidelidade e custo
    QCOMPARE(gBalanced->property("effectiveIor").toReal(), 1.15);
    QVERIFY(qAbs(gBalanced->property("effectiveDispersion").toReal() - 0.026) < 0.001);
    QCOMPARE(gBalanced->property("effectiveCaustics").toReal(), 0.50);

    // 3. Perfil Performance: supressão de efeitos custosos
    QCOMPARE(gPerformance->property("effectiveIor").toReal(), 1.0);
    QCOMPARE(gPerformance->property("effectiveDispersion").toReal(), 0.0);
    QCOMPARE(gPerformance->property("effectiveCaustics").toReal(), 0.0);
}

void TestValoGlass::contentNestingInContentContainer() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "ValoGlass {\n"
        "    id: glassHost\n"
        "    width: 400\n"
        "    height: 300\n"
        "    Item {\n"
        "        id: nestedChild\n"
        "        objectName: 'nestedChild'\n"
        "        width: 100\n"
        "        height: 50\n"
        "    }\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    auto *glassHost = qobject_cast<QQuickItem *>(obj.data());
    QVERIFY(glassHost);

    auto *child = glassHost->findChild<QQuickItem *>("nestedChild");
    QVERIFY(child);

    // Verifica que o filho foi alocado dentro do contentItem e não no nível de fundo do shader
    QQuickItem *contentItem = glassHost->property("contentItem").value<QQuickItem *>();
    QVERIFY(contentItem);
    QCOMPARE(child->parentItem(), contentItem);
    QCOMPARE(contentItem->z(), 10.0);
}

void TestValoGlass::activeStateModulatesIllumination() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "ValoGlass {\n"
        "    id: glassInteractive\n"
        "    active: true\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    auto *glass = qobject_cast<QQuickItem *>(obj.data());
    QVERIFY(glass);

    const qreal activeCaustics = glass->property("effectiveCaustics").toReal();
    const qreal activeSurfaceOpacity = glass->property("effectiveSurfaceOpacity").toReal();

    glass->setProperty("active", false);

    const qreal inactiveCaustics = glass->property("effectiveCaustics").toReal();
    const qreal inactiveSurfaceOpacity = glass->property("effectiveSurfaceOpacity").toReal();

    QVERIFY(inactiveCaustics < activeCaustics);
    QVERIFY(inactiveSurfaceOpacity < activeSurfaceOpacity);
}

void TestValoGlass::renderThreeProfilesAndCaptureComparison() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "Rectangle {\n"
        "    id: canvas\n"
        "    width: 1280\n"
        "    height: 600\n"
        "    color: '#071628'\n"
        "    Row {\n"
        "        anchors.centerIn: parent\n"
        "        spacing: 32\n"
        "        // 1. Vidro Padrão (Balanced)\n"
        "        ValoGlass {\n"
        "            width: 360\n"
        "            height: 480\n"
        "            performanceProfile: 'balanced'\n"
        "            active: true\n"
        "            Text {\n"
        "                anchors.centerIn: parent\n"
        "                text: '1. Padrão (Balanced)'\n"
        "                color: '#DDF7FF'\n"
        "                font.family: 'Inter'\n"
        "                font.pixelSize: 16\n"
        "            }\n"
        "        }\n"
        "        // 2. Vidro Ativo com Efeitos Prismáticos (Quality)\n"
        "        ValoGlass {\n"
        "            width: 360\n"
        "            height: 480\n"
        "            performanceProfile: 'quality'\n"
        "            active: true\n"
        "            iridescenceStrength: 1.5\n"
        "            borderIntensity: 1.2\n"
        "            Text {\n"
        "                anchors.centerIn: parent\n"
        "                text: '2. Prismático Ativo (Quality)'\n"
        "                color: '#49F3FC'\n"
        "                font.family: 'Inter'\n"
        "                font.bold: true\n"
        "                font.pixelSize: 16\n"
        "            }\n"
        "        }\n"
        "        // 3. Vidro Simplificado (Performance)\n"
        "        ValoGlass {\n"
        "            width: 360\n"
        "            height: 480\n"
        "            performanceProfile: 'performance'\n"
        "            active: false\n"
        "            Text {\n"
        "                anchors.centerIn: parent\n"
        "                text: '3. Simplificado (Performance)'\n"
        "                color: '#9CBAD3'\n"
        "                font.family: 'Inter'\n"
        "                font.pixelSize: 16\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> root(component.create());
    QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

    auto *item = qobject_cast<QQuickItem *>(root.data());
    QVERIFY(item);

    QQuickWindow window;
    window.setGeometry(0, 0, 1280, 600);
    item->setParentItem(window.contentItem());
    window.show();

    QImage screenshot = window.grabWindow();
    if (!screenshot.isNull()) {
        const QString artifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
        QDir().mkpath(artifactDir);
        screenshot.save(artifactDir + QStringLiteral("/valo_glass_comparison.png"));
    }
}

QTEST_MAIN(TestValoGlass)
#include "tst_ValoGlass.moc"
