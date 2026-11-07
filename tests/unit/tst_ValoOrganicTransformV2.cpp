#include <QTest>
#include <QSignalSpy>
#include <cmath>

#include "ValoOrganicTransform.hpp"
#include "ValoOpticalContext.hpp"

using namespace Systalia::Gfx::V2;

class TestValoOrganicTransformV2 final : public QObject {
    Q_OBJECT

private slots:
    // --- Testes de ValoOrganicTransform ---
    void organicTransformInitialStateIsResting();
    void organicTransformSlantClamping();
    void organicTransformDisplacementClamping();
    void organicTransformScaleClamping();
    void organicTransformDragAndSpringSimulation();
    void organicTransformResetToRest();
    void organicTransformReducedMotionImmunity();
    void organicTransformClientContentImmunity();
    void organicTransformHitTestBoundsPreservation();

    // --- Testes de ValoOpticalContext ---
    void opticalContextInitialState();
    void opticalContextAmbientLuminanceAdaptation();
    void opticalContextClampingAndSignals();
    void opticalContextEffectiveOpacityAndHighContrast();
    void opticalContextCausticsAnimationGating();
};

void TestValoOrganicTransformV2::organicTransformInitialStateIsResting() {
    ValoOrganicTransform transform;
    QVERIFY(transform.isResting());
    QVERIFY(!transform.isActive());
    QCOMPARE(transform.displacement(), QVector2D(0.0f, 0.0f));
    QCOMPARE(transform.slant(), 0.0f);
    QCOMPARE(transform.scale(), QVector2D(1.0f, 1.0f));
    QVERIFY(!transform.prefersReducedMotion());
}

void TestValoOrganicTransformV2::organicTransformSlantClamping() {
    ValoOrganicTransform transform;

    // Testar inclinação excessiva positiva (ex: 45 graus / 0.785 rad)
    transform.setSlant(0.80f);
    QVERIFY(transform.slant() <= ValoOrganicTransform::MaxSlantAngleRad);
    QCOMPARE(transform.slant(), ValoOrganicTransform::MaxSlantAngleRad);

    // Testar inclinação excessiva negativa
    transform.setSlant(-0.80f);
    QVERIFY(transform.slant() >= -ValoOrganicTransform::MaxSlantAngleRad);
    QCOMPARE(transform.slant(), -ValoOrganicTransform::MaxSlantAngleRad);

    // Testar inclinação dentro da faixa permitida (0.5 graus = ~0.0087 rad)
    transform.setSlant(0.010f);
    QCOMPARE(transform.slant(), 0.010f);
}

void TestValoOrganicTransformV2::organicTransformDisplacementClamping() {
    ValoOrganicTransform transform;

    // Testar deslocamento excessivo
    transform.setDisplacement(QVector2D(200.0f, -300.0f));
    QCOMPARE(transform.displacement().x(), ValoOrganicTransform::MaxDisplacementPx);
    QCOMPARE(transform.displacement().y(), -ValoOrganicTransform::MaxDisplacementPx);

    // Testar deslocamento permitido
    transform.setDisplacement(QVector2D(15.0f, -25.0f));
    QCOMPARE(transform.displacement(), QVector2D(15.0f, -25.0f));
}

void TestValoOrganicTransformV2::organicTransformScaleClamping() {
    ValoOrganicTransform transform;

    // Testar escala abaixo do mínimo permitido (0.50 < 0.80)
    transform.setScale(QVector2D(0.50f, 0.60f));
    QCOMPARE(transform.scale().x(), ValoOrganicTransform::MinScale);
    QCOMPARE(transform.scale().y(), ValoOrganicTransform::MinScale);

    // Testar escala acima do máximo permitido (2.0 > 1.25)
    transform.setScale(QVector2D(2.0f, 1.50f));
    QCOMPARE(transform.scale().x(), ValoOrganicTransform::MaxScale);
    QCOMPARE(transform.scale().y(), ValoOrganicTransform::MaxScale);

    // Testar escala válida
    transform.setScale(QVector2D(1.05f, 0.95f));
    QCOMPARE(transform.scale(), QVector2D(1.05f, 0.95f));
}

void TestValoOrganicTransformV2::organicTransformDragAndSpringSimulation() {
    ValoOrganicTransform transform;

    // Simular arrasto para a direita a 800 px/s
    transform.applyDragVelocity(800.0f);
    QVERIFY(transform.isActive());
    QVERIFY(transform.slant() < 0.0f); // Inércia inclina na direção oposta ao movimento
    QVERIFY(transform.displacement().x() < 0.0f); // Inércia atrasa a casca na direção oposta ao movimento

    // Avançar simulação por vários frames (ex: 60 frames a 16ms = ~1 segundo)
    for (int frame = 0; frame < 60; ++frame) {
        transform.update(0.016f);
    }

    // Após 1 segundo com amortecimento zeta ~ 0.82, a mola deve ter convergido para o repouso
    QVERIFY(transform.isResting());
    QVERIFY(std::abs(transform.displacement().x()) < 0.001f);
    QVERIFY(std::abs(transform.slant()) < 0.0001f);
}

void TestValoOrganicTransformV2::organicTransformResetToRest() {
    ValoOrganicTransform transform;
    transform.setDisplacement(QVector2D(30.0f, 20.0f));
    transform.setSlant(0.02f);
    transform.setScale(QVector2D(1.10f, 1.10f));
    QVERIFY(transform.isActive());

    transform.resetToRest();
    QVERIFY(transform.isResting());
    QCOMPARE(transform.displacement(), QVector2D(0.0f, 0.0f));
    QCOMPARE(transform.slant(), 0.0f);
    QCOMPARE(transform.scale(), QVector2D(1.0f, 1.0f));
}

void TestValoOrganicTransformV2::organicTransformReducedMotionImmunity() {
    ValoOrganicTransform transform;

    // Ativar modo prefersReducedMotion
    transform.setPrefersReducedMotion(true);
    QVERIFY(transform.prefersReducedMotion());
    QVERIFY(transform.isResting());

    // Tentar aplicar deslocamento, inclinação, escala e arrasto
    transform.setDisplacement(QVector2D(40.0f, 30.0f));
    transform.setSlant(0.02f);
    transform.setScale(QVector2D(1.20f, 1.20f));
    transform.applyDragVelocity(1200.0f);

    // Nenhuma transformação deve surtir efeito sob reducedMotion
    QVERIFY(transform.isResting());
    QCOMPARE(transform.displacement(), QVector2D(0.0f, 0.0f));
    QCOMPARE(transform.slant(), 0.0f);
    QCOMPARE(transform.scale(), QVector2D(1.0f, 1.0f));
}

void TestValoOrganicTransformV2::organicTransformClientContentImmunity() {
    ValoOrganicTransform transform;

    // Deformar a casca externa severamente
    transform.setDisplacement(QVector2D(50.0f, -40.0f));
    transform.setSlant(ValoOrganicTransform::MaxSlantAngleRad);
    transform.setScale(QVector2D(1.20f, 0.85f));

    // A matriz do conteúdo de cliente Wayland DEVE ser estritamente identidade
    const QMatrix4x4 clientMatrix = transform.clientContentMatrix();
    QVERIFY(clientMatrix.isIdentity());

    // A matriz visual da casca externa, no entanto, deve estar deformada
    const QMatrix4x4 visualMatrix = transform.visualTransformMatrix(QPointF(200, 200));
    QVERIFY(!visualMatrix.isIdentity());
}

void TestValoOrganicTransformV2::organicTransformHitTestBoundsPreservation() {
    ValoOrganicTransform transform;
    transform.setDisplacement(QVector2D(55.0f, 45.0f));
    transform.setSlant(ValoOrganicTransform::MaxSlantAngleRad);
    transform.setScale(QVector2D(1.25f, 1.25f));

    const QRectF logicalBounds(100.0, 150.0, 600.0, 400.0);
    const QRectF hitTest = transform.hitTestBounds(logicalBounds);

    // O hit-test lógico DEVE ser exatamente o mesmo retângulo fornecido
    QCOMPARE(hitTest, logicalBounds);
}

void TestValoOrganicTransformV2::opticalContextInitialState() {
    ValoOpticalContext context;
    QCOMPARE(context.ambientLuminance(), 0.15f);
    QCOMPARE(context.adaptiveOpacityOffset(), 0.0f);
    QVERIFY(!context.prefersReducedMotion());
    QVERIFY(!context.highContrastEnabled());
    QCOMPARE(context.energySavingFactor(), 1.0f);
    QVERIFY(context.shouldAnimateCaustics());
}

void TestValoOpticalContextAmbientLuminanceAdaptation() {
    // Definida no slot abaixo
}

void TestValoOrganicTransformV2::opticalContextAmbientLuminanceAdaptation() {
    ValoOpticalContext context;
    QSignalSpy luminanceSpy(&context, &ValoOpticalContext::ambientLuminanceChanged);
    QSignalSpy offsetSpy(&context, &ValoOpticalContext::adaptiveOpacityOffsetChanged);

    // Luminância escura (<= 0.50): sem offset aditivo
    context.setAmbientLuminance(0.20f);
    QCOMPARE(luminanceSpy.count(), 1);
    QCOMPARE(context.adaptiveOpacityOffset(), 0.0f);

    // Luminância clara (> 0.50): adaptação autônoma (0.80 - 0.50) * 0.40 = 0.12
    context.setAmbientLuminance(0.80f);
    QCOMPARE(luminanceSpy.count(), 2);
    QCOMPARE(offsetSpy.count(), 1);
    QVERIFY(std::abs(context.adaptiveOpacityOffset() - 0.12f) < 0.001f);
}

void TestValoOrganicTransformV2::opticalContextClampingAndSignals() {
    ValoOpticalContext context;

    QSignalSpy motionSpy(&context, &ValoOpticalContext::prefersReducedMotionChanged);
    QSignalSpy contrastSpy(&context, &ValoOpticalContext::highContrastEnabledChanged);
    QSignalSpy energySpy(&context, &ValoOpticalContext::energySavingFactorChanged);

    // Clamping de luminância
    context.setAmbientLuminance(1.5f);
    QCOMPARE(context.ambientLuminance(), 1.0f);
    context.setAmbientLuminance(-0.5f);
    QCOMPARE(context.ambientLuminance(), 0.0f);

    // Clamping de offset
    context.setAdaptiveOpacityOffset(0.99f);
    QCOMPARE(context.adaptiveOpacityOffset(), 0.35f);
    context.setAdaptiveOpacityOffset(-0.99f);
    QCOMPARE(context.adaptiveOpacityOffset(), -0.20f);

    // Preferências e sinais
    context.setPrefersReducedMotion(true);
    QCOMPARE(motionSpy.count(), 1);
    QVERIFY(context.prefersReducedMotion());

    context.setHighContrastEnabled(true);
    QCOMPARE(contrastSpy.count(), 1);
    QVERIFY(context.highContrastEnabled());

    context.setEnergySavingFactor(0.50f);
    QCOMPARE(context.energySavingFactor(), 0.50f);
    QCOMPARE(energySpy.count(), 1);

    context.setEnergySavingFactor(2.5f);
    QCOMPARE(context.energySavingFactor(), 1.0f);
    QCOMPARE(energySpy.count(), 2);

    context.setEnergySavingFactor(-1.0f);
    QCOMPARE(context.energySavingFactor(), 0.0f);
    QCOMPARE(energySpy.count(), 3);
}

void TestValoOrganicTransformV2::opticalContextEffectiveOpacityAndHighContrast() {
    ValoOpticalContext context;

    // Caso base: base = 0.34, offset = 0.0 -> 0.34
    QCOMPARE(context.effectiveOpacity(0.34f), 0.34f);

    // Caso com adaptação: offset = 0.10 -> 0.44
    context.setAdaptiveOpacityOffset(0.10f);
    QCOMPARE(context.effectiveOpacity(0.34f), 0.44f);

    // Caso alto contraste ativado: força mínimo de 0.85
    context.setHighContrastEnabled(true);
    QCOMPARE(context.effectiveOpacity(0.34f), 0.85f);
    // Se base for maior que 0.85 (ex: 0.90), preserva o maior
    QCOMPARE(context.effectiveOpacity(0.90f), 0.90f);
}

void TestValoOrganicTransformV2::opticalContextCausticsAnimationGating() {
    ValoOpticalContext context;
    QVERIFY(context.shouldAnimateCaustics());

    // Desabilitado por prefersReducedMotion
    context.setPrefersReducedMotion(true);
    QVERIFY(!context.shouldAnimateCaustics());

    // Reabilitar movimento, mas reduzir energia abaixo do limiar (0.10)
    context.setPrefersReducedMotion(false);
    context.setEnergySavingFactor(0.05f);
    QVERIFY(!context.shouldAnimateCaustics());

    // Acima do limiar
    context.setEnergySavingFactor(0.50f);
    QVERIFY(context.shouldAnimateCaustics());
}

QTEST_GUILESS_MAIN(TestValoOrganicTransformV2)
#include "tst_ValoOrganicTransformV2.moc"

