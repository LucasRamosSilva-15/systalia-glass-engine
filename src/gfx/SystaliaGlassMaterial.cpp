#include "SystaliaGlassMaterial.hpp"
#include <QDebug>
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

    auto *mat = static_cast<SystaliaGlassMaterial *>(newMaterial);
    if (!mat) {
        return false;
    }

    SystaliaSurfaceProps props = mat->props();
    props.matrix = state.combinedMatrix();

    // Otimização QRhi: só transferir para o UBO se os bytes tiverem sofrido alteração real
    if (buf->size() >= static_cast<qsizetype>(sizeof(SystaliaSurfaceProps))
        && std::memcmp(buf->constData(), &props, sizeof(SystaliaSurfaceProps)) == 0) {
        return false;
    }

    if (buf->size() < static_cast<qsizetype>(sizeof(SystaliaSurfaceProps))) {
        buf->resize(sizeof(SystaliaSurfaceProps));
    }

    std::memcpy(buf->data(), &props, sizeof(SystaliaSurfaceProps));

    static const bool s_perfDebug = qEnvironmentVariableIntValue("SGE_DEBUG_PERF") == 1;
    if (s_perfDebug) {
        static quint64 s_uniformUploads = 0;
        if ((++s_uniformUploads % 300) == 1) {
            qDebug().noquote() << "[SGE-PERF-UBO] Upload de buffer uniforme RHI realizado (total:" << s_uniformUploads << ")";
        }
    }

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
    if (!mat || !texture)
        return;

    QSGTexture *targetTexture = nullptr;
    if (binding == 1) {
        targetTexture = mat->texture();
    } else if (binding == 2) {
        targetTexture = mat->clientTexture();
    }

    if (targetTexture && *texture != targetTexture) {
        *texture = targetTexture;
        static const bool s_perfDebug = qEnvironmentVariableIntValue("SGE_DEBUG_PERF") == 1;
        if (s_perfDebug) {
            qDebug().noquote() << "[SGE-PERF-RHI] Vinculada nova textura RHI no binding:" << binding;
        }
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
