#pragma once

#include "ValoBlurPipelineV2.hpp"

#include <QMatrix4x4>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <memory>
#include <vector>

// Forward declarations de tipos QRhi privados do Qt6
class QRhi;
class QRhiTexture;
class QRhiSampler;
class QRhiBuffer;
class QRhiTextureRenderTarget;
class QRhiRenderPassDescriptor;
class QRhiShaderResourceBindings;
class QRhiGraphicsPipeline;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

namespace Systalia::Gfx::V2 {

/**
 * @brief Orquestrador de render targets GPU multipass para o desfoque Dual-Kawase (VALO Graphics 2.0).
 *
 * Implementa filtragem Dual-Kawase inteiramente na GPU através de pirâmide de texturas com QRhi,
 * eliminando transferências GPU->CPU e fornecendo a textura resultante diretamente para
 * ValoGlassMaterialV2 e o Scene Graph.
 */
class ValoGpuBlurPipelineV2 : public QObject {
    Q_OBJECT

public:
    struct alignas(16) KawaseUniforms {
        float matrix[16];   // mat4 (64 bytes)
        float halfPixel[2]; // vec2 (8 bytes)
        float offset[2];    // vec2 (8 bytes)
    };
    static_assert(sizeof(KawaseUniforms) == 80, "KawaseUniforms must be exactly 80 bytes std140");

    explicit ValoGpuBlurPipelineV2(QObject *parent = nullptr);
    ~ValoGpuBlurPipelineV2() override;

    /**
     * @brief Prepara a pirâmide de render targets e pipelines para a resolução e perfil desejados.
     * Reutiliza recursos existentes sem realocações quando os parâmetros não mudam.
     */
    bool prepare(QRhi *rhi, const QSize &sourceSize, BlurProfile profile);

    /**
     * @brief Grava as passagens multipass de downsampling e upsampling no command buffer ativo.
     * @param cb Command buffer do QRhi no qual os passes serão gravados.
     * @param sourceTexture Textura QRhi de entrada.
     * @param profile Perfil de qualidade (Performance, Balanced, Quality).
     * @param initialBatch Batch opcional de atualizações pendentes (anexado ao primeiro passe).
     */
    void execute(QRhiCommandBuffer *cb, QRhiTexture *sourceTexture, BlurProfile profile,
                 QRhiResourceUpdateBatch *initialBatch = nullptr);

    /**
     * @brief Textura QRhi resultante da filtragem multipass.
     */
    QRhiTexture *blurredTexture() const noexcept { return m_outputTexture; }

    /**
     * @brief Tamanho da textura de saída filtrada.
     */
    QSize blurredSize() const noexcept { return m_allocatedSize; }

    /**
     * @brief Indica se o pipeline está preparado e pronto para execução.
     */
    bool isPrepared() const noexcept { return m_isPrepared; }

    /**
     * @brief Libera todas as texturas, render targets e pipelines alocados na GPU.
     */
    void releaseResources();

    // Utilitários de Diagnóstico, Testes e Benchmarks
    static QRhiTexture *uploadTexture(QRhi *rhi, const QImage &image);
    static QImage readbackTexture(QRhi *rhi, QRhiTexture *texture);
    QImage filterOfflineGpu(QRhi *rhi, const QImage &source, BlurProfile profile);

private:
    struct PassResources {
        QSize size;
        QRhiTexture *texture = nullptr;
        QRhiTextureRenderTarget *renderTarget = nullptr;
        QRhiBuffer *uniformBuffer = nullptr;
        QRhiShaderResourceBindings *srb = nullptr;
        QRhiGraphicsPipeline *pipeline = nullptr;
    };

    bool initPipelinesAndGeometry(QRhi *rhi);
    void destroyPassResources(std::vector<PassResources> &passes);

    QRhi *m_rhi = nullptr;
    QSize m_allocatedSize;
    BlurProfile m_allocatedProfile = BlurProfile::Performance;
    bool m_isPrepared = false;

    // Recursos compartilhados
    QRhiSampler *m_sampler = nullptr;
    QRhiBuffer *m_vertexBuffer = nullptr;
    bool m_vertexBufferUploadPending = true;
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;

    // Pipelines gráficos
    QRhiGraphicsPipeline *m_downsamplePipeline = nullptr;
    QRhiGraphicsPipeline *m_upsamplePipeline = nullptr;

    // Níveis da pirâmide
    std::vector<PassResources> m_downPasses;
    std::vector<PassResources> m_upPasses;

    // Textura final ativa e rastreamento de entrada
    QRhiTexture *m_currentSourceTexture = nullptr;
    QRhiTexture *m_outputTexture = nullptr;
};

} // namespace Systalia::Gfx::V2

