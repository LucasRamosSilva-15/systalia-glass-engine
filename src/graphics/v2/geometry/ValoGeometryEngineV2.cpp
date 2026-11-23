#include "ValoGeometryEngineV2.hpp"
#include <algorithm>
#include <limits>

namespace Systalia::Gfx::V2 {

QVector4D ValoGeometryEngineV2::normalizeRadii(QVector2D size, QVector4D radii) noexcept {
    const float w = std::max(0.0f, size.x());
    const float h = std::max(0.0f, size.y());

    if (w <= 0.0f || h <= 0.0f) {
        return QVector4D(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Garante que nenhum raio seja negativo
    float rTL = std::max(0.0f, radii.x());
    float rTR = std::max(0.0f, radii.y());
    float rBL = std::max(0.0f, radii.z());
    float rBR = std::max(0.0f, radii.w());

    // Regra de redução proporcional do CSS Backgrounds Level 3:
    // f = min(1.0, w / (rTL + rTR), w / (rBL + rBR), h / (rTL + rBL), h / (rTR + rBR))
    float factor = 1.0f;

    const float sumTop = rTL + rTR;
    if (sumTop > w && sumTop > 0.0f) {
        factor = std::min(factor, w / sumTop);
    }

    const float sumBottom = rBL + rBR;
    if (sumBottom > w && sumBottom > 0.0f) {
        factor = std::min(factor, w / sumBottom);
    }

    const float sumLeft = rTL + rBL;
    if (sumLeft > h && sumLeft > 0.0f) {
        factor = std::min(factor, h / sumLeft);
    }

    const float sumRight = rTR + rBR;
    if (sumRight > h && sumRight > 0.0f) {
        factor = std::min(factor, h / sumRight);
    }

    rTL *= factor;
    rTR *= factor;
    rBL *= factor;
    rBR *= factor;

    // Limite superior absoluto: metade da menor dimensão
    const float maxR = std::min(w, h) * 0.5f;
    return QVector4D(std::min(rTL, maxR),
                     std::min(rTR, maxR),
                     std::min(rBL, maxR),
                     std::min(rBR, maxR));
}

float ValoGeometryEngineV2::selectRadius(QVector2D p, QVector4D radii) noexcept {
    if (p.x() < 0.0f) {
        return (p.y() < 0.0f) ? radii.x() : radii.z();
    } else {
        return (p.y() < 0.0f) ? radii.y() : radii.w();
    }
}

float ValoGeometryEngineV2::evaluateSignedDistanceL2(QVector2D p, QVector2D size,
                                                     QVector4D radii, float slant) noexcept {
    const float w = size.x();
    const float h = size.y();

    if (w <= 0.0f || h <= 0.0f) {
        return std::numeric_limits<float>::infinity();
    }

    const QVector2D halfSize(w * 0.5f, h * 0.5f);
    const QVector4D normRadii = normalizeRadii(size, radii);

    // Aplicação da inclinação 3D se slant != 0
    QVector2D pMod = p;
    float slantFactor = 1.0f;
    if (std::abs(slant) > 1e-6f) {
        slantFactor = 1.0f - (p.y() / std::max(halfSize.y(), 1.0f)) * slant * 0.25f;
        pMod.setX(p.x() / std::max(slantFactor, 0.05f));
    }

    const float rad = selectRadius(pMod, normRadii);
    const QVector2D q(std::abs(pMod.x()) - halfSize.x() + rad,
                      std::abs(pMod.y()) - halfSize.y() + rad);

    const float outsideX = std::max(q.x(), 0.0f);
    const float outsideY = std::max(q.y(), 0.0f);
    const float outsideDist = std::sqrt(outsideX * outsideX + outsideY * outsideY) - rad;
    const float insideDist = std::min(std::max(q.x(), q.y()), 0.0f);

    return (outsideDist + insideDist) * slantFactor;
}

float ValoGeometryEngineV2::evaluateSignedDistanceG2(QVector2D p, QVector2D size,
                                                     QVector4D radii, float slant) noexcept {
    const float w = size.x();
    const float h = size.y();

    if (w <= 0.0f || h <= 0.0f) {
        return std::numeric_limits<float>::infinity();
    }

    const QVector2D halfSize(w * 0.5f, h * 0.5f);
    const QVector4D normRadii = normalizeRadii(size, radii);

    QVector2D pMod = p;
    float slantFactor = 1.0f;
    if (std::abs(slant) > 1e-6f) {
        slantFactor = 1.0f - (p.y() / std::max(halfSize.y(), 1.0f)) * slant * 0.25f;
        pMod.setX(p.x() / std::max(slantFactor, 0.05f));
    }

    const float rad = selectRadius(pMod, normRadii);
    const QVector2D q(std::abs(pMod.x()) - halfSize.x() + rad,
                      std::abs(pMod.y()) - halfSize.y() + rad);

    float rawDist = 0.0f;
    if (q.x() > 0.0f && q.y() > 0.0f) {
        // Superelipse de ordem 4: (qx^4 + qy^4)^(1/4) - rad
        const float qx2 = q.x() * q.x();
        const float qy2 = q.y() * q.y();
        const float l4 = qx2 * qx2 + qy2 * qy2;
        rawDist = (l4 > 1e-12f) ? (std::sqrt(std::sqrt(l4)) - rad) : -rad;
    } else {
        // Regiões lineares onde qx <= 0 ou qy <= 0
        rawDist = std::max(q.x(), q.y()) - rad;
    }

    return rawDist * slantFactor;
}

float ValoGeometryEngineV2::evaluateSignedDistance(QVector2D p, QVector2D size, QVector4D radii,
                                                   GeometryMode mode, float slant) noexcept {
    if (mode == GeometryMode::CircularL2) {
        return evaluateSignedDistanceL2(p, size, radii, slant);
    } else {
        return evaluateSignedDistanceG2(p, size, radii, slant);
    }
}

QVector2D ValoGeometryEngineV2::evaluateNormal(QVector2D p, QVector2D size, QVector4D radii,
                                              GeometryMode mode, float slant) noexcept {
    constexpr float eps = 0.05f;
    constexpr float invTwoEps = 1.0f / (2.0f * eps);

    const float dx = evaluateSignedDistance(p + QVector2D(eps, 0.0f), size, radii, mode, slant)
                   - evaluateSignedDistance(p - QVector2D(eps, 0.0f), size, radii, mode, slant);
    const float dy = evaluateSignedDistance(p + QVector2D(0.0f, eps), size, radii, mode, slant)
                   - evaluateSignedDistance(p - QVector2D(0.0f, eps), size, radii, mode, slant);

    const float nx = dx * invTwoEps;
    const float ny = dy * invTwoEps;
    const float len = std::sqrt(nx * nx + ny * ny);

    if (len > 1e-5f) {
        return QVector2D(nx / len, ny / len);
    }

    // Fallback zenital superior se o gradiente for nulo (ex: exatamente no centro)
    return QVector2D(0.0f, -1.0f);
}

PointClassification ValoGeometryEngineV2::classifyPoint(QVector2D p, QVector2D size,
                                                      QVector4D radii, GeometryMode mode,
                                                      float tolerance, float slant) noexcept {
    const float dist = evaluateSignedDistance(p, size, radii, mode, slant);
    if (dist < -tolerance) {
        return PointClassification::Inside;
    } else if (dist > tolerance) {
        return PointClassification::Outside;
    } else {
        return PointClassification::Border;
    }
}

QRectF ValoGeometryEngineV2::boundingRect(QVector2D size, float margin) noexcept {
    const float w = std::max(0.0f, size.x()) + 2.0f * margin;
    const float h = std::max(0.0f, size.y()) + 2.0f * margin;
    return QRectF(-margin, -margin, w, h);
}

QRectF ValoGeometryEngineV2::innerContentRect(QVector2D size, float titleBarHeight, float inset) noexcept {
    const float w = std::max(0.0f, size.x() - 2.0f * inset);
    const float h = std::max(0.0f, size.y() - titleBarHeight - 2.0f * inset);
    return QRectF(inset, titleBarHeight + inset, w, h);
}

} // namespace Systalia::Gfx::V2

