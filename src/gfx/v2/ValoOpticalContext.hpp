#pragma once

#include <QObject>
#include <algorithm>

namespace Systalia::Gfx::V2 {

/**
 * @brief Contrato de contexto óptico e adaptação ambiental (Pilares Adaptive e Lucid).
 *
 * Fornece a interface canônica para reatividade da casca VALO contra variações
 * de luminância de fundo, políticas de acessibilidade e conservação energética.
 *
 * Desacoplado do compositor, de leituras pesadas de GPU e de pipelines de shaders.
 */
class ValoOpticalContext : public QObject {
    Q_OBJECT

    Q_PROPERTY(float ambientLuminance READ ambientLuminance WRITE setAmbientLuminance NOTIFY ambientLuminanceChanged)
    Q_PROPERTY(float adaptiveOpacityOffset READ adaptiveOpacityOffset WRITE setAdaptiveOpacityOffset NOTIFY adaptiveOpacityOffsetChanged)
    Q_PROPERTY(bool prefersReducedMotion READ prefersReducedMotion WRITE setPrefersReducedMotion NOTIFY prefersReducedMotionChanged)
    Q_PROPERTY(bool highContrastEnabled READ highContrastEnabled WRITE setHighContrastEnabled NOTIFY highContrastEnabledChanged)
    Q_PROPERTY(float energySavingFactor READ energySavingFactor WRITE setEnergySavingFactor NOTIFY energySavingFactorChanged)

public:
    explicit ValoOpticalContext(QObject *parent = nullptr);
    ~ValoOpticalContext() override = default;

    // --- Getters ---
    float ambientLuminance() const noexcept { return m_ambientLuminance; }
    float adaptiveOpacityOffset() const noexcept { return m_adaptiveOpacityOffset; }
    bool prefersReducedMotion() const noexcept { return m_prefersReducedMotion; }
    bool highContrastEnabled() const noexcept { return m_highContrastEnabled; }
    float energySavingFactor() const noexcept { return m_energySavingFactor; }

    // --- Setters com Validação e Clamping ---
    void setAmbientLuminance(float luminance) noexcept;
    void setAdaptiveOpacityOffset(float offset) noexcept;
    void setPrefersReducedMotion(bool enable) noexcept;
    void setHighContrastEnabled(bool enable) noexcept;
    void setEnergySavingFactor(float factor) noexcept;

    // --- Métodos Auxiliares Derivados ---

    /**
     * @brief Calcula a opacidade final efetiva considerando offset de luminância e acessibilidade.
     * @param baseOpacity Opacidade nominal definida pelo token do componente.
     */
    float effectiveOpacity(float baseOpacity) const noexcept;

    /**
     * @brief Retorna true se a animação de cáusticas estiver autorizada pelo contexto de energia.
     */
    bool shouldAnimateCaustics() const noexcept;

signals:
    void ambientLuminanceChanged();
    void adaptiveOpacityOffsetChanged();
    void prefersReducedMotionChanged();
    void highContrastEnabledChanged();
    void energySavingFactorChanged();

private:
    float m_ambientLuminance = 0.15f;       ///< Luminância média relativa [0.0, 1.0]
    float m_adaptiveOpacityOffset = 0.0f;   ///< Offset aditivo [-0.20, 0.35]
    bool m_prefersReducedMotion = false;    ///< Redução de movimento
    bool m_highContrastEnabled = false;     ///< Modo de alto contraste
    float m_energySavingFactor = 1.0f;      ///< 1.0 = fidelity total, 0.0 = economia máxima
};

} // namespace Systalia::Gfx::V2

