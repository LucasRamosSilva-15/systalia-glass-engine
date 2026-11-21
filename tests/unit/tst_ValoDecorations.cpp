#include <QTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVector4D>
#include <QVector2D>
#include <QImage>
#include <QDir>
#include <QSignalSpy>

#include "ValoGlassItemV2.hpp"
#include "tokens/ValoTokens.hpp"

void qml_register_types_Systalia_Engine();

class TestValoDecorations final : public QObject {
    Q_OBJECT

private:
    QQmlEngine m_engine;

private slots:
    void initTestCase();
    void windowContainerIntegratesValoGlass();
    void activeStateModulatesWindowDecoration();
    void windowControlsEmitSignalsAndHaveVectorGlyphs();
    void renderWallpaperAndDecorationsComparison();
};

void TestValoDecorations::initTestCase() {
    qml_register_types_Systalia_Engine();
}

void TestValoDecorations::windowContainerIntegratesValoGlass() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "WaylandWindowContainer {\n"
        "    id: container\n"
        "    width: 680\n"
        "    height: 480\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    auto *container = qobject_cast<QQuickItem *>(obj.data());
    QVERIFY(container);

    // Verifica que a decoração oficial V2 está presente diretamente, sem captura redundante de cliente
    auto *glass = container->findChild<Systalia::Gfx::V2::ValoGlassItemV2 *>();
    QVERIFY(glass);
    QVERIFY(!container->findChild<QQuickItem *>("clientDecorationV1"));
    QVERIFY(container->findChild<QQuickItem *>("clientDecorationV2"));
    QVERIFY(!container->findChild<QQuickItem *>("clientTextureCapture"));
    QVERIFY(container->property("useGraphicsV2").toBool());
    QCOMPARE(glass->geometryMode(), int(Systalia::Gfx::V2::ValoGlassItemV2::SquircleG2));
    for (int i = 0; i < 30; ++i) {
        container->setProperty("blurProfile", i % 3);
        QCOMPARE(glass->blurProfile(), i % 3);
    }

    auto *viewport = container->findChild<QQuickItem *>("clientViewport");
    QVERIFY(viewport);
    QCOMPARE(viewport->z(), 1.0);
    QVERIFY(viewport->clip());

    auto *header = container->findChild<QQuickItem *>("sgeTitleBar");
    QVERIFY(header);
}

void TestValoDecorations::activeStateModulatesWindowDecoration() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "Item {\n"
        "    ValoMaterialSurfaceV2 {\n"
        "        id: winActive\n"
        "        objectName: 'winActive'\n"
        "        active: true\n"
        "        surfaceOpacity: ValoTheme.opacityWindowMax\n"
        "    }\n"
        "    ValoMaterialSurfaceV2 {\n"
        "        id: winInactive\n"
        "        objectName: 'winInactive'\n"
        "        active: false\n"
        "        surfaceOpacity: ValoTheme.opacityWindowMin\n"
        "    }\n"
        "}\n",
        QUrl()
    );

    QScopedPointer<QObject> root(component.create());
    QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

    auto *activeGlass = root->findChild<QQuickItem *>("winActive");
    auto *inactiveGlass = root->findChild<QQuickItem *>("winInactive");
    QVERIFY(activeGlass && inactiveGlass);

    QCOMPARE(activeGlass->property("surfaceOpacity").toReal(), Systalia::ValoTokens::opacityWindowMax);
    QCOMPARE(inactiveGlass->property("surfaceOpacity").toReal(), Systalia::ValoTokens::opacityWindowMin);

    auto *activeItem = activeGlass->findChild<Systalia::Gfx::V2::ValoGlassItemV2 *>();
    auto *inactiveItem = inactiveGlass->findChild<Systalia::Gfx::V2::ValoGlassItemV2 *>();
    QVERIFY(activeItem && inactiveItem);
    QVERIFY(activeItem->isActive());
    QVERIFY(!inactiveItem->isActive());
}

void TestValoDecorations::windowControlsEmitSignalsAndHaveVectorGlyphs() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import Systalia.Engine\n"
        "import 'shell' as Shell\n"
        "Shell.SystaliaWindowHeader {\n"
        "    id: testHeader\n"
        "    width: 600\n"
        "    height: 36\n"
        "    title: 'Janela de Teste'\n"
        "}\n",
        QUrl("qrc:/qt/qml/Systalia/Engine/src/qml/")
    );

    QScopedPointer<QObject> obj(component.create());
    QVERIFY2(!obj.isNull(), qPrintable(component.errorString()));

    auto *header = qobject_cast<QQuickItem *>(obj.data());
    QVERIFY(header);

    auto *minBtn = header->findChild<QQuickItem *>("minimizeToplevelButton");
    auto *maxBtn = header->findChild<QQuickItem *>("maximizeToplevelButton");
    auto *closeBtn = header->findChild<QQuickItem *>("closeToplevelButton");

    QVERIFY(minBtn && maxBtn && closeBtn);

    // Dimensões de controlo conforme Figma (28x28)
    QCOMPARE(minBtn->width(), 28.0);
    QCOMPARE(minBtn->height(), 28.0);
    QCOMPARE(maxBtn->width(), 28.0);
    QCOMPARE(maxBtn->height(), 28.0);
    QCOMPARE(closeBtn->width(), 28.0);
    QCOMPARE(closeBtn->height(), 28.0);

    // Teste dos sinais de ação
    QSignalSpy minSpy(header, SIGNAL(minimizeRequested()));
    QSignalSpy maxSpy(header, SIGNAL(maximizeRequested()));
    QSignalSpy closeSpy(header, SIGNAL(closeRequested()));

    QMetaObject::invokeMethod(minBtn, "clicked");
    QCOMPARE(minSpy.count(), 1);

    QMetaObject::invokeMethod(maxBtn, "clicked");
    QCOMPARE(maxSpy.count(), 1);

    QMetaObject::invokeMethod(closeBtn, "clicked");
    QCOMPARE(closeSpy.count(), 1);
}

void TestValoDecorations::renderWallpaperAndDecorationsComparison() {
    QQmlComponent component(&m_engine);
    component.setData(
        "import QtQuick\n"
        "import QtQuick.Shapes\n"
        "import Systalia.Engine\n"
        "import 'shell' as Shell\n"
        "Item {\n"
        "    id: desktopScene\n"
        "    width: 1662\n"
        "    height: 975\n"
        "    // 1. Fundo Oceânico Real com Wallpaper\n"
        "    Rectangle {\n"
        "        id: bgRoot\n"
        "        anchors.fill: parent\n"
        "        color: '#061426'\n"
        "        Image {\n"
        "            anchors.fill: parent\n"
        "            source: 'qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-ocean-atmosphere.png'\n"
        "            fillMode: Image.PreserveAspectCrop\n"
        "        }\n"
        "        // Elementos visuais atrás das janelas para comprovar a refração óptica real\n"
        "        Shape {\n"
        "            anchors.fill: parent\n"
        "            ShapePath {\n"
        "                strokeColor: '#3049F3FC'\n"
        "                strokeWidth: 4\n"
        "                startX: 100; startY: 100\n"
        "                PathLine { x: 1500; y: 900 }\n"
        "            }\n"
        "            ShapePath {\n"
        "                strokeColor: '#30FF2D75'\n"
        "                strokeWidth: 4\n"
        "                startX: 100; startY: 900\n"
        "                PathLine { x: 1500; y: 100 }\n"
        "            }\n"
        "        }\n"
        "        Rectangle {\n"
        "            x: 500; y: 250; width: 220; height: 220; radius: 110\n"
        "            color: '#2500E5FF'\n"
        "        }\n"
        "        Rectangle {\n"
        "            x: 950; y: 400; width: 180; height: 180; radius: 90\n"
        "            color: '#259B51E0'\n"
        "        }\n"
        "    }\n"
        "    // ShaderEffectSource para alimentar backdropSource\n"
        "    ShaderEffectSource {\n"
        "        id: testBackdrop\n"
        "        anchors.fill: bgRoot\n"
        "        sourceItem: bgRoot\n"
        "        live: true\n"
        "        visible: false\n"
        "    }\n"
        "    // 2. Janela 1: Systalia Files (Ativa, Perfil Quality)\n"
        "    ValoMaterialSurfaceV2 {\n"
        "        x: 100; y: 180\n"
        "        width: 680; height: 500\n"
        "        geometryMode: ValoGlassItemV2.SquircleG2\n"
        "        active: true\n"
        "        blurProfile: 2\n"
        "        backdropSource: testBackdrop\n"
        "        Shell.SystaliaWindowHeader {\n"
        "            width: parent.width; height: 36\n"
        "            title: 'Systalia Files'\n"
        "            badgeText: 'ABYSSUS VES'\n"
        "            glassSurface: false\n"
        "        }\n"
        "        Item {\n"
        "            x: 8; y: 44; width: parent.width - 16; height: parent.height - 52\n"
        "            Text {\n"
        "                anchors.centerIn: parent\n"
        "                text: 'Janela Ativa — ValoMaterialSurfaceV2 (Quality)'\n"
        "                color: '#DDF7FF'\n"
        "                font.family: 'Inter'\n"
        "                font.pixelSize: 18\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    // 3. Janela 2: Systalia Control Center (Inativa, Perfil Balanced)\n"
        "    ValoMaterialSurfaceV2 {\n"
        "        x: 860; y: 220\n"
        "        width: 680; height: 500\n"
        "        geometryMode: ValoGlassItemV2.SquircleG2\n"
        "        active: false\n"
        "        blurProfile: 1\n"
        "        backdropSource: testBackdrop\n"
        "        Shell.SystaliaWindowHeader {\n"
        "            width: parent.width; height: 36\n"
        "            title: 'Systalia Control Center'\n"
        "            badgeText: 'VALO & SOAR Engine'\n"
        "            glassSurface: false\n"
        "        }\n"
        "        Item {\n"
        "            x: 8; y: 44; width: parent.width - 16; height: parent.height - 52\n"
        "            Text {\n"
        "                anchors.centerIn: parent\n"
        "                text: 'Janela Inativa — ValoMaterialSurfaceV2 (Balanced)'\n"
        "                color: '#9CBAD3'\n"
        "                font.family: 'Inter'\n"
        "                font.pixelSize: 18\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n",
        QUrl("qrc:/qt/qml/Systalia/Engine/src/qml/")
    );

    QScopedPointer<QObject> root(component.create());
    QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

    auto *item = qobject_cast<QQuickItem *>(root.data());
    QVERIFY(item);

    QQuickWindow window;
    window.setGeometry(0, 0, 1662, 975);
    item->setParentItem(window.contentItem());
    window.show();
    QVERIFY(window.isVisible());

    const QString keepDir = qEnvironmentVariable("SGE_KEEP_EVIDENCE_DIR");
    if (!keepDir.isEmpty()) {
        QImage screenshot = window.grabWindow();
        if (!screenshot.isNull()) {
            QDir().mkpath(keepDir);
            screenshot.save(keepDir + QStringLiteral("/valo_decorations_v2_scene.png"));
        }
    }
}

QTEST_MAIN(TestValoDecorations)
#include "tst_ValoDecorations.moc"

