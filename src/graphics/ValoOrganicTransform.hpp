#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QVector2D>
#include <cmath>

namespace Systalia::Gfx::V2 {

/**
 * @brief Contrato de transformação visual orgânica desacoplada (Pilar Organic).
 *
 * Governa a física viscoelástica, molas inerciais e deformações dinâmicas
 * aplicadas estritamente à casca externa de vidro decorativo.
 *
 * Princípio Mestre de Imunidade:
 * - O conteúdo cliente Wayland NUNCA é deformado ou esticado.
 * - A região lógica de hit-testing (cliques de mouse) permanece inalterada.
 * - Sob prefersReducedMotion, qualquer deformação é instantaneamente anulada.
 */
class ValoOrganicTransform {
public:
    // Limites Físicos e Angulares Nominais
    static constexpr float MaxSlantAngleRad = 0.0261799f; // 1.5 graus em radianos
    static constexpr float MaxDisplacementPx = 60.0f;     // Deslocamento inercial máximo em pixels
    static constexpr float MinScale = 0.80f;             // Escala de compressão máxima no clique
    static constexpr float MaxScale = 1.25f;             // Escala de ampliação máxima no hover

    explicit ValoOrganicTransform() noexcept;

    // --- Getters de Estado ---
    QVector2D displacement() const noexcept { return m_displacement; }
    float slant() const noexcept { return m_slant; }
    QVector2D scale() const noexcept { return m_scale; }
    bool prefersReducedMotion() const noexcept { return m_prefersReducedMotion; }

    /**
     * @brief Retorna true se a casca estiver em repouso estático absoluto.
     */
    bool isResting() const noexcept;

    /**
     * @brief Retorna true se houver transformação ou oscilação física ativa.
     */
    bool isActive() const noexcept { return !isResting(); }

    // --- Modificadores de Estado Físico ---

    /**
     * @brief Define a preferência de redução de movimento do sistema.
     * Quando ativada, zera instantaneamente qualquer oscilação e retorna ao repouso.
     */
    void setPrefersReducedMotion(bool enable) noexcept;

    /**
     * @brief Define o deslocamento inercial da casca (clamp a MaxDisplacementPx).
     */
    void setDisplacement(QVector2D disp) noexcept;

    /**
     * @brief Define a inclinação visual 3D (clamp a ±MaxSlantAngleRad).
     */
    void setSlant(float slantRad) noexcept;

    /**
     * @brief Define a escala elástica da casca (clamp a [MinScale, MaxScale]).
     */
    void setScale(QVector2D scale) noexcept;

    /**
     * @brief Aplica inclinação angular derivada da velocidade vetorial de arraste.
     * @param velocityX Velocidade horizontal do ponteiro em pixels/segundo.
     */
    void applyDragVelocity(float velocityX) noexcept;

    /**
     * @brief Força o retorno imediato ao repouso (zerando velocidades).
     */
    void resetToRest() noexcept;

    /**
     * @brief Avança a simulação física de mola amortecida por um intervalo dt.
     * @param dt Intervalo de tempo em segundos (estabilizado numericamente).
     */
    void update(float dt) noexcept;

    // --- Contratos de Fronteira Orgânica (Organic Boundaries) ---

    /**
     * @brief Matriz 4x4 aplicada exclusivamente ao chassi decorativo externo.
     */
    QMatrix4x4 visualTransformMatrix(QPointF origin = QPointF(0.0, 0.0)) const noexcept;

    /**
     * @brief Matriz 4x4 do conteúdo de clientes Wayland.
     * GARANTIA INEGOCIÁVEL: Sempre retorna a matriz Identidade.
     */
    QMatrix4x4 clientContentMatrix() const noexcept;

    /**
     * @brief Região interativa de entrada e hit-testing do compositor.
     * GARANTIA INEGOCIÁVEL: Sempre retorna a caixa lógica sem deformação.
     */
    QRectF hitTestBounds(const QRectF &logicalBounds) const noexcept;

private:
    QVector2D m_displacement = {0.0f, 0.0f};
    QVector2D m_targetDisplacement = {0.0f, 0.0f};
    QVector2D m_velocity = {0.0f, 0.0f};

    float m_slant = 0.0f;
    float m_targetSlant = 0.0f;
    float m_slantVelocity = 0.0f;

    QVector2D m_scale = {1.0f, 1.0f};
    QVector2D m_targetScale = {1.0f, 1.0f};

    bool m_prefersReducedMotion = false;

    // Parâmetros do Oscilador Harmônico Amortecido
    float m_stiffness = 300.0f;  // Constante de mola (k) em N/m
    float m_damping = 25.0f;     // Coeficiente de amortecimento (c) para zeta ~ 0.82
    float m_mass = 1.0f;         // Massa nominal (m)
};

} // namespace Systalia::Gfx::V2

