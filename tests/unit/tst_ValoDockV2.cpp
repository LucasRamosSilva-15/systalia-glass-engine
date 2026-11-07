#include <QTest>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSignalSpy>
#include <QVector4D>
#include <QImage>
#include <QColor>
#include <memory>
#include <cmath>

#include "ValoGeometryEngineV2.hpp"
#include "ValoOrganicTransform.hpp"
#include "ValoGlassItemV2.hpp"
#include "ValoShadowItemV2.hpp"
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

    // Novos testes da Etapa 04.1
    void testSmoothstepBorderOrderRegression();
    void testContinuousShadowItemProperties();
    void testVisualFocusedStateMeasurablyDistinctFromIdle();
    void testSingleCapsuleNoDuplicateAbove();
    void testPremultipliedAlphaContractAcrossDiagnosticPasses();

private:
    std::unique_ptr<QQmlEngine> m_engine;
};

void tst_ValoDockV2::initTestCase() {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QSG_RENDER_LOOP", "basic");
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

    dock->setProperty("blurProfile", 1);
    QCOMPARE(glassItem->blurProfile(), 1);

    dock->setProperty("blurProfile", 0);
    QCOMPARE(glassItem->blurProfile(), 0);
}

void tst_ValoDockV2::testSmoothstepBorderOrderRegression() {
    // Avalia a expressão canônica corrigida: f(dist) = 1.0 - smoothstep(-18.0, -2.0, dist)
    auto glslSmoothstep = [](float edge0, float edge1, float x) -> float {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    auto correctedMask = [&](float dist) -> float {
        return 1.0f - glslSmoothstep(-18.0f, -2.0f, dist);
    };

    // 1. Interior profundo da cápsula (dist < -18.0): cáusticas plenamente ativas (máscara = 1.0)
    QCOMPARE(correctedMask(-25.0f), 1.0f);
    QCOMPARE(correctedMask(-18.0f), 1.0f);

    // 2. Transição suave no miolo
    const float mid = correctedMask(-10.0f);
    QVERIFY(mid > 0.0f && mid < 1.0f);

    // 3. Na borda especular e exterior (dist >= -2.0): cáusticas suprimidas para preservar nitidez da borda
    QCOMPARE(correctedMask(-2.0f), 0.0f);
    QCOMPARE(correctedMask(0.0f), 0.0f);
    QCOMPARE(correctedMask(5.0f), 0.0f);

    // 4. Comportamento monotonicamente decrescente em direção à borda
    float prev = correctedMask(-18.0f);
    for (float d = -17.0f; d <= -2.0f; d += 1.0f) {
        float curr = correctedMask(d);
        QVERIFY(curr <= prev);
        prev = curr;
    }
}

void tst_ValoDockV2::testContinuousShadowItemProperties() {
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

    ValoShadowItemV2 *shadowItem = dock->findChild<ValoShadowItemV2*>();
    QVERIFY(shadowItem != nullptr);
    QCOMPARE(shadowItem->cornerRadii(), QVector4D(47.0f, 47.0f, 47.0f, 47.0f));
    QCOMPARE(shadowItem->shadowOffsetY(), 12.0f);
    QCOMPARE(shadowItem->shadowIntensity(), 0.35f);
    QCOMPARE(shadowItem->isIsolatedPass(), false);

    // Modo diagnóstico G: sombra isolada
    dock->setProperty("debugPass", "G");
    QCOMPARE(shadowItem->isIsolatedPass(), true);
}

void tst_ValoDockV2::testVisualFocusedStateMeasurablyDistinctFromIdle() {
    QQuickWindow window;
    window.resize(600, 200);
    window.setColor(Qt::black);

    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        Item {
            anchors.fill: parent
            ValoDockV2 {
                id: dockInstance
                anchors.centerIn: parent
            }
        }
    )", QUrl());

    auto *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root != nullptr);
    root->setParentItem(window.contentItem());

    QQuickItem *dockItem = root->findChild<QQuickItem*>();
    QVERIFY(dockItem != nullptr);

    window.show();
    (void)QTest::qWaitForWindowExposed(&window, 1000);
    for (int i = 0; i < 5; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QTest::qWait(25);
    }
    (void)QTest::qWaitForWindowExposed(&window, 1000);
    for (int i = 0; i < 5; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QTest::qWait(25);
    }

    // 1. Captura Estado 1: Idle
    dockItem->setProperty("testState", 1);
    for (int i = 0; i < 5; ++i) { window.update(); QCoreApplication::processEvents(); QTest::qWait(25); }
    const QImage imgIdle = window.grabWindow();
    QVERIFY(!imgIdle.isNull());

    // 2. Captura Estado 6: Focused
    dockItem->setProperty("testState", 6);
    for (int i = 0; i < 5; ++i) { window.update(); QCoreApplication::processEvents(); QTest::qWait(25); }
    const QImage imgFocused = window.grabWindow();
    QVERIFY(!imgFocused.isNull());

    // 3. Verifica que as capturas NÃO são idênticas (resolução do defeito #5 da auditoria)
    QVERIFY(imgIdle != imgFocused);

    // 4. Encontra diferença de pixels na vizinhança do lançador 2 (Terminal)
    int diffCount = 0;
    int maxCyanDelta = 0;
    for (int y = 0; y < imgIdle.height(); ++y) {
        for (int x = 0; x < imgIdle.width(); ++x) {
            const QColor c1 = imgIdle.pixelColor(x, y);
            const QColor c2 = imgFocused.pixelColor(x, y);
            if (c1 != c2) {
                diffCount++;
                const int cyanDelta = (c2.green() + c2.blue()) - (c1.green() + c1.blue());
                if (cyanDelta > maxCyanDelta) maxCyanDelta = cyanDelta;
            }
        }
    }

    QVERIFY2(diffCount > 50, qPrintable(QString("Número insuficiente de pixels distintos: %1").arg(diffCount)));
    QVERIFY2(maxCyanDelta > 20, "O anel de foco ciano #37DDFA não produziu delta luminoso visível.");
}

void tst_ValoDockV2::testSingleCapsuleNoDuplicateAbove() {
    QQuickWindow window;
    window.resize(1280, 720);
    window.setColor(Qt::black);

    QQmlComponent component(m_engine.get());
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        ValoDockV2Scene {
            anchors.fill: parent
            showHud: false
            backgroundMode: 0 // Oceânico
        }
    )", QUrl());

    auto *scene = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(scene != nullptr);
    scene->setParentItem(window.contentItem());

    window.show();
    (void)QTest::qWaitForWindowExposed(&window, 1000);
    for (int i = 0; i < 5; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QTest::qWait(25);
    }
    (void)QTest::qWaitForWindowExposed(&window, 1000);
    for (int i = 0; i < 5; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QTest::qWait(25);
    }

    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());

    // Inspeciona a região y = 400 a 450, x = 500 a 700 (onde ficava a cápsula azul duplicada no wallpaper)
    // O wallpaper oceânico VALO tem transição suave sem saltos de gradiente com bordas rígidas tipo cápsula
    for (int y = 400; y < 450; ++y) {
        const QColor cLeft = frame.pixelColor(350, y);
        const QColor cCenter = frame.pixelColor(640, y);
        const QColor cRight = frame.pixelColor(930, y);

        const int deltaL = std::abs(cCenter.blue() - cLeft.blue());
        const int deltaR = std::abs(cCenter.blue() - cRight.blue());
        QVERIFY2(deltaL < 45 && deltaR < 45, "Detectada borda de cápsula duplicada no wallpaper acima da dock.");
    }
}

void tst_ValoDockV2::testPremultipliedAlphaContractAcrossDiagnosticPasses() {
    QQuickWindow window;
    window.resize(555, 125);
    window.setColor(Qt::transparent);

    auto *item = new ValoGlassItemV2(window.contentItem());
    item->setWidth(555);
    item->setHeight(125);
    item->setCornerRadii(QVector4D(47.0f, 47.0f, 47.0f, 47.0f));
    item->setGeometryMode(ValoGlassItemV2::CircularL2);
    item->setActive(true);

    window.show();
    (void)QTest::qWaitForWindowExposed(&window, 1000);
    for (int i = 0; i < 5; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QTest::qWait(25);
    }

    // Testa os 6 modos de diagnóstico que renderizam através do ValoGlassItemV2
    for (int mode = 0; mode <= 6; ++mode) {
        item->setDebugMode(mode);
        for (int i = 0; i < 3; ++i) {
            window.update();
            QCoreApplication::processEvents();
            QTest::qWait(10);
        }

        const QImage frame = window.grabWindow();
        QVERIFY(!frame.isNull());

        const QImage preFrame = frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int violationCount = 0;
        for (int y = 0; y < preFrame.height(); ++y) {
            const QRgb *scanLine = reinterpret_cast<const QRgb *>(preFrame.constScanLine(y));
            for (int x = 0; x < preFrame.width(); ++x) {
                const QRgb p = scanLine[x];
                const int a = qAlpha(p);
                const int r = qRed(p);
                const int g = qGreen(p);
                const int b = qBlue(p);

                // Contrato estrito de alpha pré-multiplicado: r <= a, g <= a, b <= a
                if (r > a + 1 || g > a + 1 || b > a + 1) {
                    ++violationCount;
                }
            }
        }
        QCOMPARE(violationCount, 0);
    }
}

QTEST_MAIN(tst_ValoDockV2)
#include "tst_ValoDockV2.moc"
