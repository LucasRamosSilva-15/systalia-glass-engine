#include "ValoOrganicTransform.hpp"
#include <algorithm>

namespace Systalia::Gfx::V2 {

ValoOrganicTransform::ValoOrganicTransform() noexcept = default;

bool ValoOrganicTransform::isResting() const noexcept {
    constexpr float epsDisp = 0.005f;
    constexpr float epsSlant = 0.0001f;
    constexpr float epsScale = 0.001f;

    return (std::abs(m_displacement.x()) < epsDisp &&
            std::abs(m_displacement.y()) < epsDisp &&
            std::abs(m_velocity.x()) < epsDisp &&
            std::abs(m_velocity.y()) < epsDisp &&
            std::abs(m_slant) < epsSlant &&
            std::abs(m_slantVelocity) < epsSlant &&
            std::abs(m_scale.x() - 1.0f) < epsScale &&
            std::abs(m_scale.y() - 1.0f) < epsScale);
}

void ValoOrganicTransform::setPrefersReducedMotion(bool enable) noexcept {
    m_prefersReducedMotion = enable;
    if (m_prefersReducedMotion) {
        resetToRest();
    }
}

void ValoOrganicTransform::setDisplacement(QVector2D disp) noexcept {
    if (m_prefersReducedMotion) {
        m_displacement = {0.0f, 0.0f};
        m_targetDisplacement = {0.0f, 0.0f};
        return;
    }

    const QVector2D clamped(
        std::clamp(disp.x(), -MaxDisplacementPx, MaxDisplacementPx),
        std::clamp(disp.y(), -MaxDisplacementPx, MaxDisplacementPx)
    );
    m_displacement = clamped;
    m_targetDisplacement = clamped;
}

void ValoOrganicTransform::setSlant(float slantRad) noexcept {
    if (m_prefersReducedMotion) {
        m_slant = 0.0f;
        m_targetSlant = 0.0f;
        return;
    }

    const float clamped = std::clamp(slantRad, -MaxSlantAngleRad, MaxSlantAngleRad);
    m_slant = clamped;
    m_targetSlant = clamped;
}

void ValoOrganicTransform::setScale(QVector2D scale) noexcept {
    if (m_prefersReducedMotion) {
        m_scale = {1.0f, 1.0f};
        m_targetScale = {1.0f, 1.0f};
        return;
    }

    const QVector2D clamped(
        std::clamp(scale.x(), MinScale, MaxScale),
        std::clamp(scale.y(), MinScale, MaxScale)
    );
    m_scale = clamped;
    m_targetScale = clamped;
}

void ValoOrganicTransform::applyDragVelocity(float velocityX) noexcept {
    if (m_prefersReducedMotion) {
        return;
    }

    // Normaliza velocidade de arraste (ex: 1500 px/s atinge a flexão máxima de ±1.5°)
    constexpr float refVelocity = 1500.0f;
    const float normalizedV = std::clamp(velocityX / refVelocity, -1.0f, 1.0f);

    // Inércia: inclina a casca contra o movimento e desloca levemente
    m_slant = -normalizedV * MaxSlantAngleRad;
    m_targetSlant = 0.0f; // A casca oscila para retornar ao repouso vertical

    m_displacement = QVector2D(-normalizedV * 15.0f, 0.0f);
    m_targetDisplacement = QVector2D(0.0f, 0.0f); // Retorna ao repouso
}

void ValoOrganicTransform::resetToRest() noexcept {
    m_displacement = {0.0f, 0.0f};
    m_targetDisplacement = {0.0f, 0.0f};
    m_velocity = {0.0f, 0.0f};

    m_slant = 0.0f;
    m_targetSlant = 0.0f;
    m_slantVelocity = 0.0f;

    m_scale = {1.0f, 1.0f};
    m_targetScale = {1.0f, 1.0f};
}

void ValoOrganicTransform::update(float dt) noexcept {
    if (m_prefersReducedMotion) {
        resetToRest();
        return;
    }

    // Limita dt para garantir estabilidade numérica
    const float clampedDt = std::clamp(dt, 0.001f, 0.050f);

    // Integração semi-implícita de Euler para o deslocamento inercial
    const QVector2D dispDelta = m_displacement - m_targetDisplacement;
    const QVector2D forceDisp = -m_stiffness * dispDelta - m_damping * m_velocity;
    const QVector2D accelDisp = forceDisp / m_mass;

    m_velocity += accelDisp * clampedDt;
    m_displacement += m_velocity * clampedDt;

    // Critério de parada do deslocamento
    if (dispDelta.length() < 0.01f && m_velocity.length() < 0.01f) {
        m_displacement = m_targetDisplacement;
        m_velocity = {0.0f, 0.0f};
    }

    // Integração para o slant angular
    const float slantDelta = m_slant - m_targetSlant;
    const float forceSlant = -m_stiffness * slantDelta - m_damping * m_slantVelocity;
    const float accelSlant = forceSlant / m_mass;

    m_slantVelocity += accelSlant * clampedDt;
    m_slant += m_slantVelocity * clampedDt;

    if (std::abs(slantDelta) < 0.0001f && std::abs(m_slantVelocity) < 0.0001f) {
        m_slant = m_targetSlant;
        m_slantVelocity = 0.0f;
    }

    // Interpolação suave de escala
    m_scale += (m_targetScale - m_scale) * std::min(1.0f, clampedDt * 12.0f);
    if ((m_scale - m_targetScale).length() < 0.001f) {
        m_scale = m_targetScale;
    }
}

QMatrix4x4 ValoOrganicTransform::visualTransformMatrix(QPointF origin) const noexcept {
    QMatrix4x4 m;
    if (isResting()) {
        return m;
    }

    // 1. Translação para a origem de rotação/escala
    m.translate(origin.x(), origin.y());

    // 2. Translação do deslocamento inercial
    m.translate(m_displacement.x(), m_displacement.y());

    // 3. Cisalhamento (shear) derivado do slant angular
    if (std::abs(m_slant) > 1e-6f) {
        QMatrix4x4 shear;
        // x' = x + tan(slant) * y
        shear(0, 1) = std::tan(m_slant);
        m *= shear;
    }

    // 4. Escala elástica
    m.scale(m_scale.x(), m_scale.y());

    // 5. Retorno da origem
    m.translate(-origin.x(), -origin.y());

    return m;
}

QMatrix4x4 ValoOrganicTransform::clientContentMatrix() const noexcept {
    // GARANTIA INEGOCIÁVEL: Conteúdo de clientes Wayland NUNCA é deformado.
    return QMatrix4x4();
}

QRectF ValoOrganicTransform::hitTestBounds(const QRectF &logicalBounds) const noexcept {
    // GARANTIA INEGOCIÁVEL: A área de clique permanece idêntica à geometria lógica.
    return logicalBounds;
}

} // namespace Systalia::Gfx::V2

