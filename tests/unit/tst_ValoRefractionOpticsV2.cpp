#include <QTest>
#include <QSignalSpy>
#include <QQuickItem>

#include "ValoGlassItemV2.hpp"
#include "ValoOpticalContext.hpp"

using namespace Systalia::Gfx::V2;

class tst_ValoRefractionOpticsV2 : public QObject {
    Q_OBJECT

private slots:
    void testOpticalPropertiesClamping();
    void testReducedMotionCausticsSuppression();
    void testEnergySavingCausticsSuppression();
    void testIdleFreezeLogic();
    void testDiagnosticModesRange();
};

void tst_ValoRefractionOpticsV2::testOpticalPropertiesClamping() {
    ValoGlassItemV2 item;

    // Defaults VALO
    QCOMPARE(item.ior(), 1.12f);
    QCOMPARE(item.chromaticDispersion(), 0.65f);
    QCOMPARE(item.causticIntensity(), 0.35f);

    // Clamping de IOR [1.0, 2.5]
    item.setIor(0.5f);
    QCOMPARE(item.ior(), 1.0f);
    item.setIor(3.2f);
    QCOMPARE(item.ior(), 2.5f);
    item.setIor(1.15f);
    QCOMPARE(item.ior(), 1.15f);

    // Clamping de Dispersão Cromática [0.0, 2.0]
    item.setChromaticDispersion(-0.5f);
    QCOMPARE(item.chromaticDispersion(), 0.0f);
    item.setChromaticDispersion(5.0f);
    QCOMPARE(item.chromaticDispersion(), 2.0f);
    item.setChromaticDispersion(0.80f);
    QCOMPARE(item.chromaticDispersion(), 0.80f);

    // Clamping de Intensidade de Cáusticas [0.0, 1.0]
    item.setCausticIntensity(-1.0f);
    QCOMPARE(item.causticIntensity(), 0.0f);
    item.setCausticIntensity(2.0f);
    QCOMPARE(item.causticIntensity(), 1.0f);
    item.setCausticIntensity(0.40f);
    QCOMPARE(item.causticIntensity(), 0.40f);
}

void tst_ValoRefractionOpticsV2::testReducedMotionCausticsSuppression() {
    ValoGlassItemV2 item;
    ValoOpticalContext context;
    item.setOpticalContext(&context);

    QVERIFY(!context.prefersReducedMotion());
    QVERIFY(context.shouldAnimateCaustics());

    // Ativa redução de movimento por preferência de acessibilidade
    context.setPrefersReducedMotion(true);
    QVERIFY(context.prefersReducedMotion());
    // Cáusticas procedurais devem ser suprimidas sob redução de movimento
    QVERIFY(!context.shouldAnimateCaustics());
}

void tst_ValoRefractionOpticsV2::testEnergySavingCausticsSuppression() {
    ValoOpticalContext context;

    // Fator normal = 1.0 (Full Fidelity)
    context.setEnergySavingFactor(1.0f);
    QVERIFY(context.shouldAnimateCaustics());

    // Fator equilibrado = 0.5
    context.setEnergySavingFactor(0.5f);
    QVERIFY(context.shouldAnimateCaustics());

    // Economia severa de bateria (< 0.3)
    context.setEnergySavingFactor(0.2f);
    QVERIFY(!context.shouldAnimateCaustics());
}

void tst_ValoRefractionOpticsV2::testIdleFreezeLogic() {
    ValoGlassItemV2 item;
    QVERIFY(!item.isCausticFrozen());

    // Reinicia atividade
    item.resetActivity();
    QVERIFY(!item.isCausticFrozen());
}

void tst_ValoRefractionOpticsV2::testDiagnosticModesRange() {
    ValoGlassItemV2 item;

    // Modo padrão = 0 (Composição normal)
    QCOMPARE(item.debugMode(), 0);

    for (int mode = 1; mode <= 8; ++mode) {
        item.setDebugMode(mode);
        QCOMPARE(item.debugMode(), mode);
    }
}

QTEST_MAIN(tst_ValoRefractionOpticsV2)
#include "tst_ValoRefractionOpticsV2.moc"
