#include "ValoOpticalContext.hpp"

namespace Systalia::Gfx::V2 {

ValoOpticalContext::ValoOpticalContext(QObject *parent)
    : QObject(parent)
{
}

void ValoOpticalContext::setAmbientLuminance(float luminance) noexcept {
    const float clamped = std::clamp(luminance, 0.0f, 1.0f);
    if (!qFuzzyCompare(m_ambientLuminance, clamped)) {
        m_ambientLuminance = clamped;

        // Adaptação autônoma: se a luminância for alta (> 0.50), adiciona offset de opacidade
        // para garantir contraste WCAG AA imediato
        if (m_ambientLuminance > 0.50f) {
            setAdaptiveOpacityOffset((m_ambientLuminance - 0.50f) * 0.40f);
        } else {
            setAdaptiveOpacityOffset(0.0f);
        }

        emit ambientLuminanceChanged();
    }
}

void ValoOpticalContext::setAdaptiveOpacityOffset(float offset) noexcept {
    const float clamped = std::clamp(offset, -0.20f, 0.35f);
    if (!qFuzzyCompare(m_adaptiveOpacityOffset, clamped)) {
        m_adaptiveOpacityOffset = clamped;
        emit adaptiveOpacityOffsetChanged();
    }
}

void ValoOpticalContext::setPrefersReducedMotion(bool enable) noexcept {
    if (m_prefersReducedMotion != enable) {
        m_prefersReducedMotion = enable;
        emit prefersReducedMotionChanged();
    }
}

void ValoOpticalContext::setHighContrastEnabled(bool enable) noexcept {
    if (m_highContrastEnabled != enable) {
        m_highContrastEnabled = enable;
        emit highContrastEnabledChanged();
    }
}

void ValoOpticalContext::setEnergySavingFactor(float factor) noexcept {
    const float clamped = std::clamp(factor, 0.0f, 1.0f);
    if (!qFuzzyCompare(m_energySavingFactor, clamped)) {
        m_energySavingFactor = clamped;
        emit energySavingFactorChanged();
    }
}

float ValoOpticalContext::effectiveOpacity(float baseOpacity) const noexcept {
    if (m_highContrastEnabled) {
        return std::max(0.85f, baseOpacity);
    }

    const float combined = baseOpacity + m_adaptiveOpacityOffset;
    return std::clamp(combined, 0.20f, 0.95f);
}

bool ValoOpticalContext::shouldAnimateCaustics() const noexcept {
    return (!m_prefersReducedMotion && m_energySavingFactor > 0.25f);
}

} // namespace Systalia::Gfx::V2

