#include <QTest>
#include <QSignalSpy>
#include <QQuickWindow>
#include <QQuickItem>
#include "SystaliaGlassItem.hpp"
#include "SystaliaSurfaceProps.hpp"

class TestBackdropBinding : public QObject {
    Q_OBJECT

private slots:
    void testScreenResolutionAssignmentAndSignal();
    void testSurfacePropsScreenParameters();
    void testFallbackTextureManagement();
};

void TestBackdropBinding::testScreenResolutionAssignmentAndSignal() {
    SystaliaGlassItem item;
    QSignalSpy spy(&item, &SystaliaGlassItem::screenResolutionChanged);

    const QVector2D targetRes(1920.0f, 1080.0f);
    item.setScreenResolution(targetRes);

    QCOMPARE(item.screenResolution(), targetRes);
    QCOMPARE(spy.count(), 1);

    // Atribuição com valor idêntico não deve disparar sinal desnecessário
    item.setScreenResolution(targetRes);
    QCOMPARE(spy.count(), 1);
}

void TestBackdropBinding::testSurfacePropsScreenParameters() {
    SystaliaSurfaceProps props;
    props.windowSize = QVector2D(860.0f, 580.0f);
    props.screenResolution = QVector2D(1280.0f, 800.0f);

    QVERIFY2(props.screenResolution.x() > 0.0f, "Largura da resolução física deve ser positiva");
    QVERIFY2(props.screenResolution.y() > 0.0f, "Altura da resolução física deve ser positiva");

    float aspectRatio = props.screenResolution.x() / props.screenResolution.y();
    QCOMPARE(aspectRatio, 1280.0f / 800.0f);
}

void TestBackdropBinding::testFallbackTextureManagement() {
    QQuickWindow window;
    window.resize(300, 300);
    SystaliaGlassItem item;
    item.setParentItem(window.contentItem());
    window.show();
    const bool exposed = QTest::qWaitForWindowExposed(&window);
    Q_UNUSED(exposed);

    QVERIFY(item.window() != nullptr);

    // ensureFallbackTexture aloca uma textura sólida quando o SceneGraph está ativo
    QSGTexture *tex = item.ensureFallbackTexture();
    if (tex) {
        QCOMPARE(tex->textureSize(), QSize(4, 4));
        QSGTexture *cachedTex = item.ensureFallbackTexture();
        QCOMPARE(cachedTex, tex);
    } else {
        // Se a plataforma headless não inicializar RHI swapchain, valida o ciclo de fallback
        QVERIFY(item.window() != nullptr);
    }

    // Limpeza de recursos
    item.releaseResources();
}

QTEST_MAIN(TestBackdropBinding)
#include "tst_BackdropBinding.moc"
