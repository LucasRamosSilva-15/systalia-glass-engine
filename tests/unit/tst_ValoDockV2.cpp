#include <QTest>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QSignalSpy>
#include <QVector4D>
#include <memory>

#include "ValoGeometryEngineV2.hpp"
#include "ValoOrganicTransform.hpp"
#include "ValoGlassItemV2.hpp"
#include "ValoOpticalContext.hpp"

void qml_register_types_Systalia_Engine();

using namespace Systalia::Gfx::V2;

class tst_ValoDockV2 : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testNominalGeometry();
    void testLaunchersIntegration();
    void testOrganicMicrointeractionsAndHitTesting();
    void testReducedMotionCompliance();
    void testSevenInteractiveStates();
    void testEightDiagnosticPasses();
    void testGpuMaterialAndBackdropBinding();

private:
    std::unique_ptr<QQmlEngine> m_engine;
};

void tst_ValoDockV2::initTestCase() {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qml_register_types_Systalia_Engine();
    m_engine = std::make_unique<QQmlEngine>();
}

void tst_ValoDockV2::cleanupTestCase() {
    m_engine.reset();
}

void tst_ValoDockV2::testNominalGeometry() {
    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockV2 {
            id: dock
        }
    )", QUrl());

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> dock(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(dock != nullptr);

    // Dimensões nominais oficiais do Figma
    QCOMPARE(dock->width(), 555.0);
    QCOMPARE(dock->height(), 125.0);
    QCOMPARE(dock->property("pillRadius").toReal(), 47.0);

    // Encontra o item de vidro óptico V2
    ValoGlassItemV2 *glassItem = dock->findChild<ValoGlassItemV2*>();
    QVERIFY(glassItem != nullptr);
    QCOMPARE(glassItem->cornerRadii(), QVector4D(47.0f, 47.0f, 47.0f, 47.0f));
    QCOMPARE(glassItem->geometryMode(), static_cast<int>(GeometryMode::CircularL2));
    QCOMPARE(glassItem->specularStrokeWidth(), 1.25f);
}

void tst_ValoDockV2::testLaunchersIntegration() {
    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockV2 {
            id: dock
        }
    )", QUrl());

    std::unique_ptr<QQuickItem> dock(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(dock != nullptr);

    // Verifica os 5 lançadores oficiais do conceito Figma
    const QStringList expectedActions = { "files", "audio", "terminal", "ide", "settings" };
    const QList<qreal> expectedX = { 33.75, 128.75, 222.75, 316.75, 410.75 };

    for (int i = 0; i < 5; ++i) {
        QVariant itemVar;
        QMetaObject::invokeMethod(dock.get(), "itemAt", Q_RETURN_ARG(QVariant, itemVar), Q_ARG(QVariant, i));
        QQuickItem *item = itemVar.value<QQuickItem*>();
        QVERIFY2(item != nullptr, qPrintable(QString("Lançador %1 não foi instanciado").arg(i)));

        QCOMPARE(item->property("action").toString(), expectedActions[i]);
        QCOMPARE(item->x(), expectedX[i]);
        QCOMPARE(item->y(), 23.5);
        QCOMPARE(item->property("iconSize").toReal(), 78.0);

        // Ícone vetorial SVG presente e associado
        const QString iconSrc = item->property("iconSource").toString();
        QVERIFY(iconSrc.contains(QString("valo-dock-")));
        QVERIFY(iconSrc.endsWith(".svg"));
    }
}

void tst_ValoDockV2::testOrganicMicrointeractionsAndHitTesting() {
    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockItemV2 {
            id: item
            action: "terminal"
            iconSize: 78.0
        }
    )", QUrl());

    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(item != nullptr);

    // 1. Estado Idle: escala nominal 1.0
    QCOMPARE(item->property("targetScale").toReal(), 1.0);

    // 2. Estado Hover: ampliação elástica Organic até 1.08
    item->setProperty("forceHovered", true);
    QCOMPARE(item->property("targetScale").toReal(), 1.08);

    // 3. Estado Press: compressão tátil Organic até 0.94
    item->setProperty("forcePressed", true);
    QCOMPARE(item->property("targetScale").toReal(), 0.94);

    // 4. Hit-testing invariante: a área nominal do item nunca encolhe ou estica
    QCOMPARE(item->width(), 78.0);
    QCOMPARE(item->height(), 78.0 + 22.0);
}

void tst_ValoDockV2::testReducedMotionCompliance() {
    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockItemV2 {
            id: item
            prefersReducedMotion: true
        }
    )", QUrl());

    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(item != nullptr);

    QVERIFY(item->property("prefersReducedMotion").toBool());

    // Altera hover sob reduced motion: resposta instantânea sem oscilação
    item->setProperty("forceHovered", true);
    QCOMPARE(item->property("targetScale").toReal(), 1.08);
}

void tst_ValoDockV2::testSevenInteractiveStates() {
    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockV2 {
            id: dock
        }
    )", QUrl());

    std::unique_ptr<QQuickItem> dock(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(dock != nullptr);

    // Testa os 7 estados canônicos
    // 1: Idle
    dock->setProperty("testState", 1);
    QCOMPARE(dock->property("testState").toInt(), 1);

    // 2: Hover
    dock->setProperty("testState", 2);
    QVariant item2Var;
    QMetaObject::invokeMethod(dock.get(), "itemAt", Q_RETURN_ARG(QVariant, item2Var), Q_ARG(QVariant, 2));
    QQuickItem *termItem = item2Var.value<QQuickItem*>();
    QVERIFY(termItem != nullptr);
    QVERIFY(termItem->property("effectiveHovered").toBool());
    QCOMPARE(termItem->property("targetScale").toReal(), 1.08);

    // 3: Press
    dock->setProperty("testState", 3);
    QVERIFY(termItem->property("effectivePressed").toBool());
    QCOMPARE(termItem->property("targetScale").toReal(), 0.94);

    // 4: Active (Files em primeiro plano)
    dock->setProperty("testState", 4);
    QVariant item0Var;
    QMetaObject::invokeMethod(dock.get(), "itemAt", Q_RETURN_ARG(QVariant, item0Var), Q_ARG(QVariant, 0));
    QQuickItem *filesItem = item0Var.value<QQuickItem*>();
    QVERIFY(filesItem != nullptr);
    QVERIFY(filesItem->property("effectiveRunning").toBool());
    QVERIFY(filesItem->property("effectiveFocused").toBool());

    // 5: Background Running
    dock->setProperty("testState", 5);
    QVariant item1Var;
    QMetaObject::invokeMethod(dock.get(), "itemAt", Q_RETURN_ARG(QVariant, item1Var), Q_ARG(QVariant, 1));
    QQuickItem *audioItem = item1Var.value<QQuickItem*>();
    QVERIFY(audioItem != nullptr);
    QVERIFY(audioItem->property("effectiveRunning").toBool());
    QVERIFY(audioItem->property("effectiveBackground").toBool());

    // 6: Focused
    dock->setProperty("testState", 6);
    QVERIFY(termItem->property("showKeyboardFocus").toBool());

    // 7: Reduced Motion / High Contrast
    dock->setProperty("testState", 7);
    QVERIFY(dock->property("highContrast").toBool());
    QVERIFY(dock->property("prefersReducedMotion").toBool());
}

void tst_ValoDockV2::testEightDiagnosticPasses() {
    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockV2 {
            id: dock
        }
    )", QUrl());

    std::unique_ptr<QQuickItem> dock(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(dock != nullptr);

    // Verifica que cada um dos 8 passes diagnósticos é atribuído e mapeado sem erro
    const QStringList passes = { "A", "B", "C", "D", "E", "F", "G", "H" };
    for (const QString &p : passes) {
        dock->setProperty("debugPass", p);
        QCOMPARE(dock->property("debugPass").toString(), p);
        const int mode = dock->property("debugMode").toInt();
        QVERIFY(mode >= 0);
    }
}

void tst_ValoDockV2::testGpuMaterialAndBackdropBinding() {
    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockV2 {
            id: dock
            blurProfile: 2 // Quality
        }
    )", QUrl());

    std::unique_ptr<QQuickItem> dock(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(dock != nullptr);

    ValoGlassItemV2 *glassItem = dock->findChild<ValoGlassItemV2*>();
    QVERIFY(glassItem != nullptr);
    QVERIFY(glassItem->backdropProvider() != nullptr);
    QCOMPARE(glassItem->blurProfile(), 2);

    // Alternar o perfil propaga ao material
    dock->setProperty("blurProfile", 1); // Balanced
    QCOMPARE(glassItem->blurProfile(), 1);

    dock->setProperty("blurProfile", 0); // Performance
    QCOMPARE(glassItem->blurProfile(), 0);
}

QTEST_MAIN(tst_ValoDockV2)
#include "tst_ValoDockV2.moc"
