#pragma once

#include <QObject>
#include <QQuickItem>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <QImage>
#include <QPointer>
#include <QSize>
#include <memory>
#include <QtQml/qqmlregistration.h>

#include "ValoBlurPipelineV2.hpp"

namespace Systalia::Gfx::V2 {

class ValoGpuBlurPipelineV2;

/**
 * @brief Provedor de textura de backdrop para o motor VALO Graphics 2.0 (Etapa 03).
 *
 * Responsável por disponibilizar a textura de fundo ao renderer óptico sem feedback
 * recursivo. Suporta vinculação de itens de cena upstream (ShaderEffectSource / Wallpaper)
 * e geração autônoma de backdrop sintético para testes isolados e execução headless.
 */
class ValoBackdropProviderV2 : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickItem* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool hasTexture READ hasTexture NOTIFY textureChanged)
    Q_PROPERTY(bool isDirty READ isDirty WRITE setDirty NOTIFY dirtyChanged)
    Q_PROPERTY(int blurProfile READ blurProfileInt WRITE setBlurProfileInt NOTIFY blurProfileChanged)

public:
    explicit ValoBackdropProviderV2(QObject *parent = nullptr);
    ~ValoBackdropProviderV2() override;

    BlurProfile blurProfile() const noexcept { return m_blurProfile; }
    int blurProfileInt() const noexcept { return static_cast<int>(m_blurProfile); }
    void setBlurProfile(BlurProfile profile);
    void setBlurProfileInt(int profile) { setBlurProfile(static_cast<BlurProfile>(profile)); }

    QQuickItem *source() const noexcept { return m_source; }
    void setSource(QQuickItem *source);

    /**
     * @brief Define o item consumidor para proteção estrita contra feedback recursivo.
     */
    void setConsumerItem(QQuickItem *consumer) noexcept { m_consumerItem = consumer; }

    bool hasTexture() const noexcept;
    bool isDirty() const noexcept { return m_isDirty; }
    void setDirty(bool dirty) noexcept;

    /**
     * @brief Obtém a textura de backdrop pronta para amostragem no Scene Graph.
     * @param window Janela dona dos recursos gráficos RHI.
     */
    QSGTexture *texture(QQuickWindow *window);

    /**
     * @brief Obtém a textura de backdrop com desfoque Dual-Kawase multipass aplicado na GPU.
     */
    QSGTexture *blurredTexture(QQuickWindow *window);

    /**
     * @brief Gera imagem de backdrop sintético com padrão calibrado de teste.
     */
    static QImage createSyntheticBackdrop(const QSize &size);

    void releaseResources();
    void attachToWindow(QQuickWindow *window);

signals:
    void sourceChanged();
    void textureChanged();
    void dirtyChanged();
    void blurProfileChanged();

private slots:
    void onSourceTextureChanged();
    void onBeforeRendering();

private:
    QPointer<QQuickItem> m_source;
    QPointer<QQuickItem> m_consumerItem;
    QPointer<QSGTextureProvider> m_connectedProvider;
    QSGTexture *m_fallbackTexture = nullptr;
    QSGTexture *m_qsgBlurredTexture = nullptr;
    std::unique_ptr<ValoGpuBlurPipelineV2> m_gpuBlurPipeline;
    QPointer<QQuickWindow> m_resourceWindow;
    BlurProfile m_blurProfile = BlurProfile::Performance;
    bool m_isDirty = true;
};

} // namespace Systalia::Gfx::V2

