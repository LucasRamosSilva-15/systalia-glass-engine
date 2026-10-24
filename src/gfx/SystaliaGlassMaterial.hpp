#pragma once

#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include "SystaliaSurfaceProps.hpp"

class SystaliaGlassMaterialShader : public QSGMaterialShader {
public:
    SystaliaGlassMaterialShader();

    bool updateUniformData(RenderState &state,
                           QSGMaterial *newMaterial,
                           QSGMaterial *oldMaterial) override;

    void updateSampledImage(RenderState &state,
                            int binding,
                            QSGTexture **texture,
                            QSGMaterial *newMaterial,
                            QSGMaterial *oldMaterial) override;

    bool updateGraphicsPipelineState(RenderState &state,
                                     GraphicsPipelineState *ps,
                                     QSGMaterial *newMaterial,
                                     QSGMaterial *oldMaterial) override;
};

class SystaliaGlassMaterial : public QSGMaterial {
public:
    SystaliaGlassMaterial();
    ~SystaliaGlassMaterial() override = default;

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
    int compare(const QSGMaterial *other) const override;

    void setProps(const SystaliaSurfaceProps &props) { m_props = props; }
    const SystaliaSurfaceProps &props() const { return m_props; }
    SystaliaSurfaceProps &props() { return m_props; }

    void setTexture(QSGTexture *texture) { m_texture = texture; }
    QSGTexture *texture() const { return m_texture; }

private:
    SystaliaSurfaceProps m_props;
    QSGTexture *m_texture = nullptr;
};
