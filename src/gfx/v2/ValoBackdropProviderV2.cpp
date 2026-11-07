#include "ValoBackdropProviderV2.hpp"
#include <QPainter>
#include <QLinearGradient>
#include <utility>

namespace Systalia::Gfx::V2 {

ValoBackdropProviderV2::ValoBackdropProviderV2(QObject *parent)
    : QObject(parent)
{
}

ValoBackdropProviderV2::~ValoBackdropProviderV2() {
    releaseResources();
}

void ValoBackdropProviderV2::setSource(QQuickItem *source) {
    // Blindagem contra recursão: um item nunca pode ser seu próprio provedor de backdrop
    if (source && source == m_consumerItem) {
        qWarning("[SGE-BACKDROP-V2] Tentativa de feedback recursivo bloqueada (source == consumer).");
        return;
    }

    if (m_source != source) {
        if (m_connectedProvider) {
            disconnect(m_connectedProvider, &QSGTextureProvider::textureChanged,
                       this, &ValoBackdropProviderV2::onSourceTextureChanged);
            m_connectedProvider = nullptr;
        }

        m_source = source;
        setDirty(true);
        emit sourceChanged();
        emit textureChanged();
    }
}

void ValoBackdropProviderV2::setDirty(bool dirty) noexcept {
    if (m_isDirty != dirty) {
        m_isDirty = dirty;
        emit dirtyChanged();
    }
}

bool ValoBackdropProviderV2::hasTexture() const noexcept {
    if (m_source && m_source->isTextureProvider()) {
        return true;
    }
    return (m_fallbackTexture != nullptr);
}

void ValoBackdropProviderV2::onSourceTextureChanged() {
    setDirty(true);
    emit textureChanged();
}

QSGTexture *ValoBackdropProviderV2::texture(QQuickWindow *window) {
    if (!window) {
        return nullptr;
    }
    m_resourceWindow = window;

    // 1. Amostragem de item upstream (ex: Wallpaper / ShaderEffectSource)
    if (m_source && m_source->isTextureProvider()) {
        QSGTextureProvider *provider = m_source->textureProvider();
        if (provider) {
            if (m_connectedProvider != provider) {
                if (m_connectedProvider) {
                    disconnect(m_connectedProvider, &QSGTextureProvider::textureChanged,
                               this, &ValoBackdropProviderV2::onSourceTextureChanged);
                }
                m_connectedProvider = provider;
                connect(provider, &QSGTextureProvider::textureChanged,
                        this, &ValoBackdropProviderV2::onSourceTextureChanged, Qt::UniqueConnection);
            }
            QSGTexture *tex = provider->texture();
            if (tex) {
                return tex;
            }
        }
    }

    // 2. Fallback / Teste Isolado: Textura sintética calibrada
    if (!m_fallbackTexture || m_isDirty) {
        delete std::exchange(m_fallbackTexture, nullptr);

        const QSize size(std::max(window->width(), 100), std::max(window->height(), 100));
        const QImage synthetic = createSyntheticBackdrop(size);
        m_fallbackTexture = window->createTextureFromImage(synthetic, QQuickWindow::TextureIsOpaque);
        if (m_fallbackTexture) {
            m_fallbackTexture->setFiltering(QSGTexture::Linear);
            m_fallbackTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            m_fallbackTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);
        }
        setDirty(false);
    }

    return m_fallbackTexture;
}

QImage ValoBackdropProviderV2::createSyntheticBackdrop(const QSize &size) {
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    // Gradiente oceânico profundo VALO (#010C1A -> #052445)
    QLinearGradient bg(0, 0, size.width(), size.height());
    bg.setColorAt(0.0, QColor(1, 12, 26));
    bg.setColorAt(0.5, QColor(3, 24, 48));
    bg.setColorAt(1.0, QColor(5, 36, 69));
    p.fillRect(QRect(QPoint(0, 0), size), bg);

    // Grade de linhas e alvos geométricos para medição de refração óptica
    p.setPen(QPen(QColor(0, 239, 255, 60), 1.0));
    constexpr int step = 40;
    for (int x = 0; x < size.width(); x += step) {
        p.drawLine(x, 0, x, size.height());
    }
    for (int y = 0; y < size.height(); y += step) {
        p.drawLine(0, y, size.width(), y);
    }

    // Círculos concêntricos de teste no centro para validação visual de curvatura refrativa
    const QPointF center(size.width() * 0.5, size.height() * 0.5);
    p.setPen(QPen(QColor(248, 196, 123, 90), 1.5));
    p.drawEllipse(center, 60, 60);
    p.setPen(QPen(QColor(158, 119, 255, 90), 1.5));
    p.drawEllipse(center, 120, 120);

    p.end();
    return img;
}

void ValoBackdropProviderV2::releaseResources() {
    if (m_connectedProvider) {
        disconnect(m_connectedProvider, &QSGTextureProvider::textureChanged,
                   this, &ValoBackdropProviderV2::onSourceTextureChanged);
        m_connectedProvider = nullptr;
    }
    delete std::exchange(m_fallbackTexture, nullptr);
}

} // namespace Systalia::Gfx::V2
