#pragma once

#include <QObject>
#include <QQuickItem>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <QImage>
#include <QPointer>
#include <QSize>

namespace Systalia::Gfx::V2 {

/**
 * @brief Provedor de textura de backdrop para o motor VALO Graphics 2.0 (Etapa 03).
 *
 * Responsável por disponibilizar a textura de fundo ao renderer óptico sem feedback
 * recursivo. Suporta vinculação de itens de cena upstream (ShaderEffectSource / Wallpaper)
 * e geração autônoma de backdrop sintético para testes isolados e execução headless.
 */
class ValoBackdropProviderV2 : public QObject {
    Q_OBJECT

    Q_PROPERTY(QQuickItem* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool hasTexture READ hasTexture NOTIFY textureChanged)
    Q_PROPERTY(bool isDirty READ isDirty WRITE setDirty NOTIFY dirtyChanged)

public:
    explicit ValoBackdropProviderV2(QObject *parent = nullptr);
    ~ValoBackdropProviderV2() override;

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
     * @brief Gera imagem de backdrop sintético com padrão calibrado de teste.
     */
    static QImage createSyntheticBackdrop(const QSize &size);

    void releaseResources();

signals:
    void sourceChanged();
    void textureChanged();
    void dirtyChanged();

private slots:
    void onSourceTextureChanged();

private:
    QPointer<QQuickItem> m_source;
    QPointer<QQuickItem> m_consumerItem;
    QPointer<QSGTextureProvider> m_connectedProvider;
    QSGTexture *m_fallbackTexture = nullptr;
    QPointer<QQuickWindow> m_resourceWindow;
    bool m_isDirty = true;
};

} // namespace Systalia::Gfx::V2

