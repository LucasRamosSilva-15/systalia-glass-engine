#include "ValoGpuBlurPipelineV2.hpp"

#include <QFile>
#include <QLoggingCategory>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

Q_LOGGING_CATEGORY(lcValoGpuBlur, "systalia.gfx.valo.gpu_blur")

namespace Systalia::Gfx::V2 {

namespace {

QShader loadShader(const QString &resourcePath, const QString &fallbackPath) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        file.setFileName(fallbackPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qCCritical(lcValoGpuBlur) << "Failed to open shader file:" << resourcePath << "and" << fallbackPath;
            return {};
        }
    }
    return QShader::fromSerialized(file.readAll());
}

} // namespace

ValoGpuBlurPipelineV2::ValoGpuBlurPipelineV2(QObject *parent)
    : QObject(parent) {}

ValoGpuBlurPipelineV2::~ValoGpuBlurPipelineV2() {
    releaseResources();
}

void ValoGpuBlurPipelineV2::destroyPassResources(std::vector<PassResources> &passes) {
    for (auto &pass : passes) {
        delete pass.pipeline;
        pass.pipeline = nullptr;
        delete pass.srb;
        pass.srb = nullptr;
        delete pass.uniformBuffer;
        pass.uniformBuffer = nullptr;
        delete pass.renderTarget;
        pass.renderTarget = nullptr;
        delete pass.texture;
        pass.texture = nullptr;
    }
    passes.clear();
}

void ValoGpuBlurPipelineV2::releaseResources() {
    destroyPassResources(m_downPasses);
    destroyPassResources(m_upPasses);

    delete m_downsamplePipeline;
    m_downsamplePipeline = nullptr;

    delete m_upsamplePipeline;
    m_upsamplePipeline = nullptr;

    delete m_renderPassDesc;
    m_renderPassDesc = nullptr;

    delete m_vertexBuffer;
    m_vertexBuffer = nullptr;
    m_vertexBufferUploadPending = true;

    delete m_sampler;
    m_sampler = nullptr;

    m_currentSourceTexture = nullptr;
    m_outputTexture = nullptr;
    m_rhi = nullptr;
    m_allocatedSize = QSize();
    m_isPrepared = false;
}

bool ValoGpuBlurPipelineV2::initPipelinesAndGeometry(QRhi *rhi) {
    if (!m_sampler) {
        m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                    QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!m_sampler->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create QRhiSampler";
            return false;
        }
    }

    if (!m_vertexBuffer) {
        // Quad fullscreen: TriangleStrip cobrindo [-1, 1] em NDC
        // Formato: x, y, u, v
        const float quadData[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 1.0f
        };
        m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(quadData));
        if (!m_vertexBuffer->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create fullscreen quad vertex buffer";
            return false;
        }
        m_vertexBufferUploadPending = true;
    }

    // Carregamento de shaders
    const QShader vs = loadShader(":/shaders/v2/valo_kawase_down.vert.qsb", "build/.qsb/valo_kawase_down.vert.qsb");
    const QShader fsDown = loadShader(":/shaders/v2/valo_kawase_down.frag.qsb", "build/.qsb/valo_kawase_down.frag.qsb");
    const QShader fsUp = loadShader(":/shaders/v2/valo_kawase_up.frag.qsb", "build/.qsb/valo_kawase_up.frag.qsb");

    if (!vs.isValid() || !fsDown.isValid() || !fsUp.isValid()) {
        qCCritical(lcValoGpuBlur) << "One or more shaders failed to load/validate";
        return false;
    }

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({ { 4 * sizeof(float) } });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }
    });

    // Usa um dos SRBs internos para fixar o layout de recursos dos pipelines
    QRhiShaderResourceBindings *layoutSrb = nullptr;
    if (!m_upPasses.empty() && m_upPasses[0].srb) {
        layoutSrb = m_upPasses[0].srb;
    } else if (m_downPasses.size() > 1 && m_downPasses[1].srb) {
        layoutSrb = m_downPasses[1].srb;
    }

    if (!layoutSrb) {
        qCCritical(lcValoGpuBlur) << "No valid SRB available for pipeline layout configuration";
        return false;
    }

    if (!m_downsamplePipeline) {
        m_downsamplePipeline = rhi->newGraphicsPipeline();
        m_downsamplePipeline->setRenderPassDescriptor(m_renderPassDesc);
        m_downsamplePipeline->setShaderResourceBindings(layoutSrb);
        m_downsamplePipeline->setShaderStages({
            { QRhiShaderStage::Vertex, vs },
            { QRhiShaderStage::Fragment, fsDown }
        });
        m_downsamplePipeline->setVertexInputLayout(inputLayout);
        m_downsamplePipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        if (!m_downsamplePipeline->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create downsample pipeline";
            return false;
        }
    }

    if (!m_upsamplePipeline) {
        m_upsamplePipeline = rhi->newGraphicsPipeline();
        m_upsamplePipeline->setRenderPassDescriptor(m_renderPassDesc);
        m_upsamplePipeline->setShaderResourceBindings(layoutSrb);
        m_upsamplePipeline->setShaderStages({
            { QRhiShaderStage::Vertex, vs },
            { QRhiShaderStage::Fragment, fsUp }
        });
        m_upsamplePipeline->setVertexInputLayout(inputLayout);
        m_upsamplePipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        if (!m_upsamplePipeline->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create upsample pipeline";
            return false;
        }
    }

    return true;
}

bool ValoGpuBlurPipelineV2::prepare(QRhi *rhi, const QSize &sourceSize, BlurProfile profile) {
    if (!rhi || sourceSize.isEmpty()) {
        m_isPrepared = false;
        return false;
    }

    // Se já estiver alocado com mesmas dimensões, perfil e instância RHI, reaproveita sem custo
    if (m_isPrepared && m_rhi == rhi && m_allocatedSize == sourceSize && m_allocatedProfile == profile) {
        return true;
    }

    releaseResources();
    m_rhi = rhi;
    m_allocatedSize = sourceSize;
    m_allocatedProfile = profile;

    const int passCount = ValoBlurPipelineV2::passCountForProfile(profile);
    if (passCount == 0) {
        // Perfil Performance: zero passos na GPU, bypass transparente
        m_isPrepared = true;
        return true;
    }

    if (!m_sampler) {
        m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                    QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!m_sampler->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create QRhiSampler";
            releaseResources();
            return false;
        }
    }

    // Calcula dimensões dos níveis da pirâmide de downsampling
    std::vector<QSize> downSizes;
    downSizes.reserve(passCount);
    QSize currentSize = sourceSize;
    for (int i = 0; i < passCount; ++i) {
        currentSize = QSize(std::max(1, currentSize.width() / 2), std::max(1, currentSize.height() / 2));
        downSizes.push_back(currentSize);
    }

    // Calcula dimensões dos níveis da pirâmide de upsampling
    std::vector<QSize> upSizes;
    upSizes.reserve(passCount);
    for (int i = passCount - 2; i >= 0; --i) {
        upSizes.push_back(downSizes[i]);
    }
    upSizes.push_back(sourceSize); // Passo final retorna à resolução original 1:1

    // Aloca texturas e render targets de downsampling
    m_downPasses.resize(passCount);
    for (int i = 0; i < passCount; ++i) {
        auto &pass = m_downPasses[i];
        pass.size = downSizes[i];

        pass.texture = rhi->newTexture(QRhiTexture::RGBA8, pass.size, 1,
                                       QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
        if (!pass.texture->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create downsample texture level" << i << pass.size;
            releaseResources();
            return false;
        }

        QRhiTextureRenderTargetDescription rtDesc(pass.texture);
        pass.renderTarget = rhi->newTextureRenderTarget(rtDesc);
        if (!m_renderPassDesc) {
            m_renderPassDesc = pass.renderTarget->newCompatibleRenderPassDescriptor();
        }
        pass.renderTarget->setRenderPassDescriptor(m_renderPassDesc);
        if (!pass.renderTarget->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create downsample render target level" << i;
            releaseResources();
            return false;
        }

        pass.uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(KawaseUniforms));
        if (!pass.uniformBuffer->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create downsample UBO level" << i;
            releaseResources();
            return false;
        }
    }

    // Cria SRBs dos passos internos de downsample (passo i > 0 amostra o nível anterior i-1)
    for (int i = 1; i < passCount; ++i) {
        auto &pass = m_downPasses[i];
        pass.srb = rhi->newShaderResourceBindings();
        pass.srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, pass.uniformBuffer),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_downPasses[i - 1].texture, m_sampler)
        });
        if (!pass.srb->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create downsample SRB level" << i;
            releaseResources();
            return false;
        }
    }

    // Aloca texturas e render targets de upsampling
    m_upPasses.resize(passCount);
    for (int i = 0; i < passCount; ++i) {
        auto &pass = m_upPasses[i];
        pass.size = upSizes[i];

        pass.texture = rhi->newTexture(QRhiTexture::RGBA8, pass.size, 1,
                                       QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
        if (!pass.texture->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create upsample texture level" << i << pass.size;
            releaseResources();
            return false;
        }

        QRhiTextureRenderTargetDescription rtDesc(pass.texture);
        pass.renderTarget = rhi->newTextureRenderTarget(rtDesc);
        pass.renderTarget->setRenderPassDescriptor(m_renderPassDesc);
        if (!pass.renderTarget->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create upsample render target level" << i;
            releaseResources();
            return false;
        }

        pass.uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(KawaseUniforms));
        if (!pass.uniformBuffer->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create upsample UBO level" << i;
            releaseResources();
            return false;
        }

        // Configura SRB do upsample: passo 0 amostra o menor nível de downsample, passos seguintes amostram o upsample anterior
        QRhiTexture *inputTex = (i == 0) ? m_downPasses.back().texture : m_upPasses[i - 1].texture;
        pass.srb = rhi->newShaderResourceBindings();
        pass.srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, pass.uniformBuffer),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, inputTex, m_sampler)
        });
        if (!pass.srb->create()) {
            qCCritical(lcValoGpuBlur) << "Failed to create upsample SRB level" << i;
            releaseResources();
            return false;
        }
    }

    if (!initPipelinesAndGeometry(rhi)) {
        releaseResources();
        return false;
    }

    m_isPrepared = true;
    return true;
}

void ValoGpuBlurPipelineV2::execute(QRhiCommandBuffer *cb, QRhiTexture *sourceTexture, BlurProfile profile,
                                    QRhiResourceUpdateBatch *initialBatch) {
    if (!cb || !sourceTexture) {
        m_outputTexture = nullptr;
        return;
    }

    if (profile == BlurProfile::Performance || m_downPasses.empty()) {
        // Bypass transparente sem passes de renderização
        m_outputTexture = sourceTexture;
        if (initialBatch) {
            cb->resourceUpdate(initialBatch);
        }
        return;
    }

    // Se o vertex buffer ainda não foi carregado para a VRAM, carrega agora
    if (m_vertexBufferUploadPending) {
        const float quadData[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 1.0f
        };
        QRhiResourceUpdateBatch *vbufBatch = m_rhi->nextResourceUpdateBatch();
        vbufBatch->uploadStaticBuffer(m_vertexBuffer, quadData);
        cb->resourceUpdate(vbufBatch);
        m_vertexBufferUploadPending = false;
    }

    const int passCount = static_cast<int>(m_downPasses.size());
    const QMatrix4x4 corrMat = m_rhi->clipSpaceCorrMatrix();

    // Atualiza o SRB do passo 0 caso o ponteiro da textura fonte tenha mudado
    auto &firstPass = m_downPasses[0];
    if (!firstPass.srb || m_currentSourceTexture != sourceTexture) {
        delete firstPass.srb;
        firstPass.srb = m_rhi->newShaderResourceBindings();
        firstPass.srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, firstPass.uniformBuffer),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, sourceTexture, m_sampler)
        });
        firstPass.srb->create();
        m_currentSourceTexture = sourceTexture;
    }

    // 1. Configuração e execução dos passes de DOWNSAMPLING
    for (int i = 0; i < passCount; ++i) {
        auto &pass = m_downPasses[i];
        const QSize inSize = (i == 0) ? m_allocatedSize : m_downPasses[i - 1].size;

        // Atualização de UBO std140
        KawaseUniforms uniforms{};
        memcpy(uniforms.matrix, corrMat.constData(), 16 * sizeof(float));
        uniforms.halfPixel[0] = 1.0f / static_cast<float>(inSize.width());
        uniforms.halfPixel[1] = 1.0f / static_cast<float>(inSize.height());
        const float passOffset = 1.0f + static_cast<float>(i) * 0.25f;
        uniforms.offset[0] = passOffset * 0.75f;
        uniforms.offset[1] = passOffset * 0.75f;

        QRhiResourceUpdateBatch *rub = (i == 0 && initialBatch) ? initialBatch : m_rhi->nextResourceUpdateBatch();
        rub->updateDynamicBuffer(pass.uniformBuffer, 0, sizeof(uniforms), &uniforms);

        cb->beginPass(pass.renderTarget, Qt::transparent, { 1.0f, 0 }, rub);
        cb->setGraphicsPipeline(m_downsamplePipeline);
        cb->setViewport({ 0.0f, 0.0f, static_cast<float>(pass.size.width()), static_cast<float>(pass.size.height()) });
        cb->setShaderResources(pass.srb);
        const QRhiCommandBuffer::VertexInput vbufBinding(m_vertexBuffer, 0);
        cb->setVertexInput(0, 1, &vbufBinding);
        cb->draw(4);
        cb->endPass();
    }

    // 2. Configuração e execução dos passes de UPSAMPLING
    for (int i = 0; i < passCount; ++i) {
        auto &pass = m_upPasses[i];
        const QSize inSize = (i == 0) ? m_downPasses.back().size : m_upPasses[i - 1].size;

        // Atualização de UBO std140
        KawaseUniforms uniforms{};
        memcpy(uniforms.matrix, corrMat.constData(), 16 * sizeof(float));
        uniforms.halfPixel[0] = 1.0f / static_cast<float>(inSize.width());
        uniforms.halfPixel[1] = 1.0f / static_cast<float>(inSize.height());
        const float passOffset = 1.0f + static_cast<float>(passCount - 1 - i) * 0.25f;
        uniforms.offset[0] = passOffset * 0.75f;
        uniforms.offset[1] = passOffset * 0.75f;

        QRhiResourceUpdateBatch *rub = m_rhi->nextResourceUpdateBatch();
        rub->updateDynamicBuffer(pass.uniformBuffer, 0, sizeof(uniforms), &uniforms);

        cb->beginPass(pass.renderTarget, Qt::transparent, { 1.0f, 0 }, rub);
        cb->setGraphicsPipeline(m_upsamplePipeline);
        cb->setViewport({ 0.0f, 0.0f, static_cast<float>(pass.size.width()), static_cast<float>(pass.size.height()) });
        cb->setShaderResources(pass.srb);
        const QRhiCommandBuffer::VertexInput vbufBinding(m_vertexBuffer, 0);
        cb->setVertexInput(0, 1, &vbufBinding);
        cb->draw(4);
        cb->endPass();
    }

    m_outputTexture = m_upPasses.back().texture;
}

QRhiTexture *ValoGpuBlurPipelineV2::uploadTexture(QRhi *rhi, const QImage &image) {
    if (!rhi || image.isNull()) return nullptr;

    const QImage formatted = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    QRhiTexture *tex = rhi->newTexture(QRhiTexture::RGBA8, formatted.size(), 1, QRhiTexture::UsedAsTransferSource);
    if (!tex->create()) {
        delete tex;
        return nullptr;
    }

    QRhiResourceUpdateBatch *rub = rhi->nextResourceUpdateBatch();
    rub->uploadTexture(tex, formatted);

    QRhiCommandBuffer *cb = nullptr;
    if (rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess) {
        cb->resourceUpdate(rub);
        rhi->endOffscreenFrame();
        rhi->finish();
    }

    return tex;
}

QImage ValoGpuBlurPipelineV2::readbackTexture(QRhi *rhi, QRhiTexture *texture) {
    if (!rhi || !texture) return {};

    QRhiReadbackResult result;
    QRhiResourceUpdateBatch *rub = rhi->nextResourceUpdateBatch();
    rub->readBackTexture(QRhiReadbackDescription(texture), &result);

    QRhiCommandBuffer *cb = nullptr;
    if (rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess) {
        cb->resourceUpdate(rub);
        rhi->endOffscreenFrame();
        rhi->finish();
    }

    if (result.data.isEmpty()) return {};

    return QImage(reinterpret_cast<const uchar*>(result.data.constData()),
                  result.pixelSize.width(),
                  result.pixelSize.height(),
                  QImage::Format_RGBA8888_Premultiplied).copy();
}

QImage ValoGpuBlurPipelineV2::filterOfflineGpu(QRhi *rhi, const QImage &source, BlurProfile profile) {
    if (!rhi || source.isNull()) return {};

    const QImage formatted = source.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    QRhiTexture *srcTex = rhi->newTexture(QRhiTexture::RGBA8, formatted.size(), 1, QRhiTexture::UsedAsTransferSource);
    if (!srcTex->create()) {
        delete srcTex;
        return {};
    }

    if (!prepare(rhi, formatted.size(), profile)) {
        delete srcTex;
        return {};
    }

    QRhiResourceUpdateBatch *uploadBatch = rhi->nextResourceUpdateBatch();
    uploadBatch->uploadTexture(srcTex, formatted);

    QRhiCommandBuffer *cb = nullptr;
    if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess) {
        delete srcTex;
        return {};
    }

    execute(cb, srcTex, profile, uploadBatch);

    QRhiReadbackResult result;
    QRhiResourceUpdateBatch *readbackBatch = rhi->nextResourceUpdateBatch();
    readbackBatch->readBackTexture(QRhiReadbackDescription(blurredTexture()), &result);
    cb->resourceUpdate(readbackBatch);

    rhi->endOffscreenFrame();
    rhi->finish();

    delete srcTex;

    if (result.data.isEmpty()) return {};

    return QImage(reinterpret_cast<const uchar*>(result.data.constData()),
                  result.pixelSize.width(),
                  result.pixelSize.height(),
                  QImage::Format_RGBA8888_Premultiplied).copy();
}

} // namespace Systalia::Gfx::V2
