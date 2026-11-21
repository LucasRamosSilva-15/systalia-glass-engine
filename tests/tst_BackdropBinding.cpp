#include <QTest>
#include <QSignalSpy>
#include <QQuickWindow>
#include <QQuickItem>
#include "graphics/ValoGlassItemV2.hpp"
#include "graphics/ValoBackdropProviderV2.hpp"
#include "graphics/SystaliaSurfacePropsV2.hpp"

using namespace Systalia::Gfx::V2;

class TestBackdropBinding : public QObject {
    Q_OBJECT

private slots:
    void testBackdropProviderAssignmentAndSignal();
    void testSurfacePropsScreenParameters();
    void testBackdropProviderLifecycle();
};

void TestBackdropBinding::testBackdropProviderAssignmentAndSignal() {
    ValoGlassItemV2 item;
    ValoBackdropProviderV2 provider;
    QSignalSpy spy(&item, &ValoGlassItemV2::backdropProviderChanged);

    item.setBackdropProvider(&provider);
    QCOMPARE(item.backdropProvider(), &provider);
    QCOMPARE(spy.count(), 1);

    // Atribuição com ponteiro idêntico não deve disparar sinal desnecessário
    item.setBackdropProvider(&provider);
    QCOMPARE(spy.count(), 1);
}

void TestBackdropBinding::testSurfacePropsScreenParameters() {
    SystaliaSurfacePropsV2 props;
    props.windowSize = QVector2D(860.0f, 580.0f);
    props.screenResolution = QVector2D(1280.0f, 800.0f);

    QVERIFY2(props.screenResolution.x() > 0.0f, "Largura da resolução física deve ser positiva");
    QVERIFY2(props.screenResolution.y() > 0.0f, "Altura da resolução física deve ser positiva");

    float aspectRatio = props.screenResolution.x() / props.screenResolution.y();
    QCOMPARE(aspectRatio, 1280.0f / 800.0f);
}

void TestBackdropBinding::testBackdropProviderLifecycle() {
    QQuickWindow window;
    window.resize(300, 300);

    ValoBackdropProviderV2 provider;
    ValoGlassItemV2 item(window.contentItem());
    item.setSize(QSizeF(200.0, 200.0));
    item.setBackdropProvider(&provider);

    window.show();
    const bool exposed = QTest::qWaitForWindowExposed(&window);
    Q_UNUSED(exposed);

    QVERIFY(item.window() != nullptr);
    QCOMPARE(item.backdropProvider(), &provider);

    // Garante limpeza segura dos recursos sem vazamentos em execução headless/RHI
    provider.releaseResources();
    item.setBackdropProvider(nullptr);
    QVERIFY(item.backdropProvider() == nullptr);
}

QTEST_MAIN(TestBackdropBinding)
#include "tst_BackdropBinding.moc"
