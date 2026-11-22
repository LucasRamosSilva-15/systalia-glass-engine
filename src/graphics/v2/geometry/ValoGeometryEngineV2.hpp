#pragma once

#include "SystaliaSurfacePropsV2.hpp"
#include <QPointF>
#include <QRectF>
#include <QVector2D>
#include <QVector4D>
#include <cmath>

namespace Systalia::Gfx::V2 {

/**
 * @brief Classificação topológica de um ponto em relação à fronteira da superfície.
 */
enum class PointClassification {
    Inside,  ///< d < -tolerance
    Border,  ///< -tolerance <= d <= tolerance
    Outside  ///< d > tolerance
};

/**
 * @brief Motor geométrico analítico independente do VALO Graphics 2.0.
 *
 * Fornece avaliação analítica rigorosa de funções de distância assinada (SDF),
 * vetores normais de refração, normalização de raios e caixas delimitadoras.
 *
 * Opera sem qualquer dependência de QML, Scene Graph ou buffers de vídeo GPU,
 * atuando como autoridade matemática canônica de geometria para CPU e shaders.
 */
class ValoGeometryEngineV2 {
public:
    ValoGeometryEngineV2() = delete;

    /**
     * @brief Normaliza o vetor de 4 raios contra as dimensões da superfície.
     *
     * Aplica a regra de redução proporcional de raios (especificação CSS Backgrounds 3):
     * Se r_TL + r_TR > w, ambos os raios são escalados por w / (r_TL + r_TR).
     * Garante que nenhum raio exceda min(width, height) * 0.5f e que todos sejam >= 0.
     *
     * @param size Dimensões (largura, altura).
     * @param radii Vetor com raios (TL, TR, BL, BR).
     * @return Vetor com os 4 raios normalizados e seguros contra degeneração.
     */
    static QVector4D normalizeRadii(QVector2D size, QVector4D radii) noexcept;

    /**
     * @brief Seleciona o raio correspondente ao quadrante do ponto em relação ao centro.
     *
     * @param p Ponto relativo ao centro da superfície (p.x, p.y).
     * @param radii Vetor com 4 raios normalizados (x=TL, y=TR, z=BL, w=BR).
     * @return Raio escalar do quadrante correspondente.
     */
    static float selectRadius(QVector2D p, QVector4D radii) noexcept;

    /**
     * @brief Avalia a SDF euclidiana analítica (L2) para elementos com cantos circulares.
     *
     * Convenção de sinal:
     * - dist < 0 : interior da forma.
     * - dist = 0 : borda perimétrica exata.
     * - dist > 0 : exterior da forma.
     *
     * @param p Ponto relativo ao centro da superfície.
     * @param size Dimensões (largura, altura).
     * @param radii Vetor com 4 raios nominais (TL, TR, BL, BR).
     * @param slant Inclinação visual 3D em radianos (opcional, padrão 0.0f).
     * @return Distância euclidiana euclidiana assinada exata em pixels.
     */
    static float evaluateSignedDistanceL2(QVector2D p, QVector2D size, QVector4D radii, float slant = 0.0f) noexcept;

    /**
     * @brief Avalia a SDF squircle orgânica (G2) com continuidade de curvatura C2.
     *
     * Utiliza a superelipse de ordem 4 (L4 Minkowski norm: (q.x^4 + q.y^4)^(1/4) - r)
     * nos cantos. Na junção com as bordas retas (q.x = 0 ou q.y = 0), a segunda derivada
     * d²y/dx² é identicamente 0, eliminando descontinuidades de curvatura ópticas.
     *
     * @param p Ponto relativo ao centro da superfície.
     * @param size Dimensões (largura, altura).
     * @param radii Vetor com 4 raios nominais (TL, TR, BL, BR).
     * @param slant Inclinação visual 3D em radianos (opcional, padrão 0.0f).
     * @return Distância assinada contínua G2 em pixels.
     */
    static float evaluateSignedDistanceG2(QVector2D p, QVector2D size, QVector4D radii, float slant = 0.0f) noexcept;

    /**
     * @brief Avalia a SDF no modo especificado (CircularL2 ou SquircleG2).
     */
    static float evaluateSignedDistance(QVector2D p, QVector2D size, QVector4D radii,
                                         GeometryMode mode, float slant = 0.0f) noexcept;

    /**
     * @brief Avalia o vetor normal unitário apontando para o exterior da superfície.
     *
     * Computa o gradiente analítico/numérico grad(SDF) normalizado.
     *
     * @param p Ponto relativo ao centro da superfície.
     * @param size Dimensões (largura, altura).
     * @param radii Vetor com 4 raios.
     * @param mode Modo geométrico (L2 ou G2).
     * @param slant Inclinação visual 3D.
     * @return Vetor normal bidimensional unitário (nx, ny).
     */
    static QVector2D evaluateNormal(QVector2D p, QVector2D size, QVector4D radii,
                                    GeometryMode mode, float slant = 0.0f) noexcept;

    /**
     * @brief Classifica a posição topológica de um ponto em relação à borda.
     */
    static PointClassification classifyPoint(QVector2D p, QVector2D size, QVector4D radii,
                                            GeometryMode mode, float tolerance = 0.5f,
                                            float slant = 0.0f) noexcept;

    /**
     * @brief Retorna a caixa delimitadora retangular externa.
     */
    static QRectF boundingRect(QVector2D size, float margin = 0.0f) noexcept;

    /**
     * @brief Retorna a região interior retangular segura para apresentação de clientes Wayland.
     */
    static QRectF innerContentRect(QVector2D size, float titleBarHeight, float inset = 8.0f) noexcept;
};

} // namespace Systalia::Gfx::V2

