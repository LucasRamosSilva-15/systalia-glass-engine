#include "SystaliaGlassMaterial.hpp"
#include <cstring>
#include <functional>

SystaliaGlassMaterialShader::SystaliaGlassMaterialShader() {
    setShaderFileName(VertexStage, QStringLiteral(":/shaders/valo_optical.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/shaders/valo_optical.frag.qsb"));
}

bool SystaliaGlassMaterialShader::updateUniformData(RenderState &state,
                                                   QSGMaterial *newMaterial,
                                                   QSGMaterial *oldMaterial) {
    Q_UNUSED(oldMaterial);
    QByteArray *buf = state.uniformData();
    if (!buf) {
        return false;
    }

    if (buf->size() < static_cast<qsizetype>(sizeof(SystaliaSurfaceProps))) {
        buf->resize(sizeof(SystaliaSurfaceProps));
    }

    auto *mat = static_cast<SystaliaGlassMaterial *>(newMaterial);
    SystaliaSurfaceProps props = mat->props();

    props.matrix = state.combinedMatrix();

    std::memcpy(buf->data(), &props, sizeof(SystaliaSurfaceProps));
    return true;
}

void SystaliaGlassMaterialShader::updateSampledImage(RenderState &state,
                                                    int binding,
                                                    QSGTexture **texture,
                                                    QSGMaterial *newMaterial,
                                                    QSGMaterial *oldMaterial) {
    Q_UNUSED(state);
    Q_UNUSED(oldMaterial);

    auto *mat = static_cast<SystaliaGlassMaterial *>(newMaterial);
    if (!mat)
        return;
    if (binding == 1 && mat->texture()) {
        *texture = mat->texture();
    } else if (binding == 2 && mat->clientTexture()) {
        *texture = mat->clientTexture();
    }
}

bool SystaliaGlassMaterialShader::updateGraphicsPipelineState(RenderState &state,
                                                             GraphicsPipelineState *ps,
                                                             QSGMaterial *newMaterial,
                                                             QSGMaterial *oldMaterial) {
    Q_UNUSED(state);
    Q_UNUSED(newMaterial);
    Q_UNUSED(oldMaterial);

    ps->blendEnable = true;
    ps->srcColor = GraphicsPipelineState::SrcAlpha;
    ps->dstColor = GraphicsPipelineState::OneMinusSrcAlpha;
    ps->colorWrite = GraphicsPipelineState::R | GraphicsPipelineState::G | GraphicsPipelineState::B | GraphicsPipelineState::A;
    ps->cullMode = GraphicsPipelineState::CullNone;
    ps->polygonMode = GraphicsPipelineState::Fill;

    return true;
}

SystaliaGlassMaterial::SystaliaGlassMaterial() {
    setFlag(Blending, true);
    setFlag(RequiresFullMatrix, true);
}

QSGMaterialType *SystaliaGlassMaterial::type() const {
    static QSGMaterialType s_type;
    return &s_type;
}

QSGMaterialShader *SystaliaGlassMaterial::createShader(QSGRendererInterface::RenderMode) const {
    return new SystaliaGlassMaterialShader();
}

int SystaliaGlassMaterial::compare(const QSGMaterial *other) const {
    const auto *otherMaterial = static_cast<const SystaliaGlassMaterial *>(other);
    const std::less<QSGTexture *> less;
    if (m_texture != otherMaterial->m_texture)
        return less(m_texture, otherMaterial->m_texture) ? -1 : 1;
    if (m_clientTexture != otherMaterial->m_clientTexture)
        return less(m_clientTexture, otherMaterial->m_clientTexture) ? -1 : 1;
    return 0;
}
