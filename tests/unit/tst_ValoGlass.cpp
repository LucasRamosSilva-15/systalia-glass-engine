#include <QTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVector4D>
#include <QImage>
#include <QDir>

#include "graphics/v2/materials/ValoGlassItemV2.hpp"

void qml_register_types_Systalia_Engine();

class TestValoGlass final : public QObject {
    Q_OBJECT

private:
    QQmlEngine m_engine;

private slots:
    void initTestCase();
    void defaultPropertiesMatchValoV2Defaults();
    void performanceProfilesAdaptOpticalParameters();
    void contentNestingInContentContainer();
    void activeStateModulatesIllumination();
    void renderThreeProfilesAndCaptureComparison();
};

void TestValoGlass::initTestCase() {
    qml_register_types_Systalia_Engine();
}

void TestValoGlass::defaultPropertiesMatchValoV2Defaults() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "ValoMaterialSurfaceV2 {\n"
        "    id: glass\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    auto *glass = qobject_cast<QQuickItem *>(obj.data());
    QVERIFY(glass);

    QCOMPARE(glass->property("surfaceOpacity").toReal(), 0.34);
    QCOMPARE(glass->property("cornerRadius").toReal(), 20.0);
    QCOMPARE(glass->property("cornerRadii").value<QVector4D>(), QVector4D(20.0f, 20.0f, 20.0f, 20.0f));
    QCOMPARE(glass->property("specularStrokeWidth").toReal(), 1.25);
    QCOMPARE(glass->property("blurProfile").toInt(), 2);
    QCOMPARE(glass->property("ior").toReal(), 1.25);
    QCOMPARE(glass->property("chromaticDispersion").toReal(), 0.040);
    QCOMPARE(glass->property("causticIntensity").toReal(), 0.18);
    QCOMPARE(glass->property("active").toBool(), true);
}

void TestValoGlass::performanceProfilesAdaptOpticalParameters() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "Item {\n"
        "    ValoMaterialSurfaceV2 {\n"
        "        id: glassQuality\n"
        "        objectName: 'glassQuality'\n"
        "        blurProfile: 2\n"
        "    }\n"
        "    ValoMaterialSurfaceV2 {\n"
        "        id: glassBalanced\n"
        "        objectName: 'glassBalanced'\n"
        "        blurProfile: 1\n"
        "    }\n"
        "    ValoMaterialSurfaceV2 {\n"
        "        id: glassPerformance\n"
        "        objectName: 'glassPerformance'\n"
        "        blurProfile: 0\n"
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

    // 1. Perfil Quality (2): máximo detalhamento óptico
    QCOMPARE(gQuality->property("ior").toReal(), 1.25);
    QCOMPARE(gQuality->property("chromaticDispersion").toReal(), 0.040);
    QCOMPARE(gQuality->property("causticIntensity").toReal(), 0.18);

    // 2. Perfil Balanced (1): equilíbrio entre fidelidade e custo
    QCOMPARE(gBalanced->property("ior").toReal(), 1.15);
    QCOMPARE(gBalanced->property("chromaticDispersion").toReal(), 0.020);
    QCOMPARE(gBalanced->property("causticIntensity").toReal(), 0.05);

    // 3. Perfil Performance (0): partículas desativadas e menor custo
    QCOMPARE(gPerformance->property("blurProfile").toInt(), 0);
    QCOMPARE(gPerformance->property("ior").toReal(), 1.15);
}

void TestValoGlass::contentNestingInContentContainer() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "ValoMaterialSurfaceV2 {\n"
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
        "ValoMaterialSurfaceV2 {\n"
        "    id: glassInteractive\n"
        "    active: true\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    auto *glass = qobject_cast<QQuickItem *>(obj.data());
    QVERIFY(glass);

    auto *glassItem = glass->findChild<Systalia::Gfx::V2::ValoGlassItemV2 *>();
    QVERIFY(glassItem);
    QVERIFY(glassItem->isActive());

    glass->setProperty("active", false);
    QVERIFY(!glassItem->isActive());
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
        "        ValoMaterialSurfaceV2 {\n"
        "            width: 360\n"
        "            height: 480\n"
        "            blurProfile: 1\n"
        "            active: true\n"
        "            Text {\n"
        "                anchors.centerIn: parent\n"
        "                text: '1. Padrão (Balanced)'\n"
        "                color: '#DDF7FF'\n"
        "                font.family: 'Inter'\n"
        "                font.pixelSize: 16\n"
        "            }\n"
        "        }\n"
        "        ValoMaterialSurfaceV2 {\n"
        "            width: 360\n"
        "            height: 480\n"
        "            blurProfile: 2\n"
        "            active: true\n"
        "            Text {\n"
        "                anchors.centerIn: parent\n"
        "                text: '2. Prismático Ativo (Quality)'\n"
        "                color: '#49F3FC'\n"
        "                font.family: 'Inter'\n"
        "                font.bold: true\n"
        "                font.pixelSize: 16\n"
        "            }\n"
        "        }\n"
        "        ValoMaterialSurfaceV2 {\n"
        "            width: 360\n"
        "            height: 480\n"
        "            blurProfile: 0\n"
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
    QVERIFY(window.isVisible());

    const QString keepDir = qEnvironmentVariable("SGE_KEEP_EVIDENCE_DIR");
    if (!keepDir.isEmpty()) {
        QImage screenshot = window.grabWindow();
        if (!screenshot.isNull()) {
            QDir().mkpath(keepDir);
            screenshot.save(keepDir + QStringLiteral("/valo_glass_v2_comparison.png"));
        }
    }
}

QTEST_MAIN(TestValoGlass)
#include "tst_ValoGlass.moc"
