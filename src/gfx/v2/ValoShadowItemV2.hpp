#pragma once

#include <QColor>
#include <QQuickItem>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QVector4D>
#include <QtQml/qqmlregistration.h>

#include "SystaliaSurfacePropsV2.hpp"

namespace Systalia::Gfx::V2 {

class ValoShadowMaterialShaderV2 : public QSGMaterialShader {
public:
    ValoShadowMaterialShaderV2();
    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                           QSGMaterial *oldMaterial) override;
};

class ValoShadowMaterialV2 : public QSGMaterial {
public:
    ValoShadowMaterialV2();
    ~ValoShadowMaterialV2() override = default;

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
    int compare(const QSGMaterial *other) const override;

    void setProps(const SystaliaSurfacePropsV2 &props) noexcept { m_props = props; }
    const SystaliaSurfacePropsV2 &props() const noexcept { return m_props; }

private:
    SystaliaSurfacePropsV2 m_props;
};

/**
 * @brief Item QML para renderização da Sombra Externa Analítica VALO L2 (Etapa 04.1).
 *
 * Utiliza atenuação espacial contínua F(d_shadow, sigma) baseada em SDF e
 * oclusão analítica sob a cápsula de vidro para garantir ausência de bandas
 * retangulares sobrepostas e preservação de peça única no conceito Figma.
 */
class ValoShadowItemV2 : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVector4D cornerRadii READ cornerRadii WRITE setCornerRadii NOTIFY cornerRadiiChanged)
    Q_PROPERTY(float shadowOffsetY READ shadowOffsetY WRITE setShadowOffsetY NOTIFY shadowOffsetYChanged)
    Q_PROPERTY(float shadowIntensity READ shadowIntensity WRITE setShadowIntensity NOTIFY shadowIntensityChanged)
    Q_PROPERTY(bool isolatedPass READ isIsolatedPass WRITE setIsolatedPass NOTIFY isolatedPassChanged)

public:
    explicit ValoShadowItemV2(QQuickItem *parent = nullptr);
    ~ValoShadowItemV2() override = default;

    QVector4D cornerRadii() const noexcept { return m_cornerRadii; }
    void setCornerRadii(const QVector4D &radii);

    float shadowOffsetY() const noexcept { return m_shadowOffsetY; }
    void setShadowOffsetY(float offset);

    float shadowIntensity() const noexcept { return m_shadowIntensity; }
    void setShadowIntensity(float intensity);

    bool isIsolatedPass() const noexcept { return m_isolatedPass; }
    void setIsolatedPass(bool isolated);

signals:
    void cornerRadiiChanged();
    void shadowOffsetYChanged();
    void shadowIntensityChanged();
    void isolatedPassChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    QVector4D m_cornerRadii = {47.0f, 47.0f, 47.0f, 47.0f};
    float m_shadowOffsetY = 12.0f;
    float m_shadowIntensity = 0.35f;
    bool m_isolatedPass = false;
};

} // namespace Systalia::Gfx::V2

