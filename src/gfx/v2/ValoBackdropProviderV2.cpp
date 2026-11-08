#include "ValoBackdropProviderV2.hpp"
#include "ValoGpuBlurPipelineV2.hpp"
#include <QPainter>
#include <QLinearGradient>
#include <rhi/qrhi.h>
#include <utility>

namespace Systalia::Gfx::V2 {

namespace {
class ValoRhiTextureWrapper : public QSGTexture {
public:
    explicit ValoRhiTextureWrapper(QRhiTexture *tex) : m_tex(tex) {}
    ~ValoRhiTextureWrapper() override = default;

    QRhiTexture *rhiTexture() const override { return m_tex; }
    QSize textureSize() const override { return m_tex ? m_tex->pixelSize() : QSize(); }
    bool hasAlphaChannel() const override { return true; }
    bool hasMipmaps() const override { return false; }
    qint64 comparisonKey() const override { return reinterpret_cast<qint64>(m_tex); }

private:
    QRhiTexture *m_tex = nullptr;
};
} // namespace

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

void ValoBackdropProviderV2::attachToWindow(QQuickWindow *window) {
    if (!window || m_resourceWindow == window) {
        return;
    }
    if (m_resourceWindow) {
        disconnect(m_resourceWindow, &QQuickWindow::beforeRendering,
                   this, &ValoBackdropProviderV2::onBeforeRendering);
        disconnect(m_resourceWindow, &QQuickWindow::sceneGraphInvalidated,
                   this, &ValoBackdropProviderV2::releaseResources);
        disconnect(m_resourceWindow, &QObject::destroyed, this, nullptr);
    }
    m_resourceWindow = window;
    if (m_resourceWindow) {
        connect(m_resourceWindow, &QQuickWindow::beforeRendering,
                this, &ValoBackdropProviderV2::onBeforeRendering,
                Qt::DirectConnection);
        connect(m_resourceWindow, &QQuickWindow::sceneGraphInvalidated,
                this, &ValoBackdropProviderV2::releaseResources,
                Qt::DirectConnection);
        connect(m_resourceWindow, &QObject::destroyed,
                this, [this]() {
                    m_resourceWindow = nullptr;
                });
    }
}

QSGTexture *ValoBackdropProviderV2::texture(QQuickWindow *window) {
    if (!window) {
        return nullptr;
    }
    attachToWindow(window);

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
                static bool s_loggedReal = false;
                if (!s_loggedReal) {
                    s_loggedReal = true;
                    qWarning("[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=%dx%d, rhiTex=%p, rhi=%p",
                             tex->textureSize().width(), tex->textureSize().height(),
                             static_cast<void*>(tex->rhiTexture()),
                             static_cast<void*>(window ? window->rhi() : nullptr));
                }
                if (m_fallbackTexture) {
                    qWarning("[SGE-BACKDROP-V2] Transição: textura real do provedor obtida (%dx%d), descartando fallback sintético.",
                             tex->textureSize().width(), tex->textureSize().height());
                    delete std::exchange(m_fallbackTexture, nullptr);
                    delete std::exchange(m_qsgBlurredTexture, nullptr);
                    setDirty(true);
                }
                return tex;
            } else {
                qWarning("[SGE-BACKDROP-V2] Provedor de textura conectado mas provider->texture() é nulo.");
            }
        }
    }

    // 2. Fallback / Teste Isolado: Textura sintética calibrada
    qWarning("[SGE-BACKDROP-V2] ATENÇÃO: Recorrendo ao fallback sintético createSyntheticBackdrop (source=%p)", static_cast<void*>(m_source.data()));
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

void ValoBackdropProviderV2::setBlurProfile(BlurProfile profile) {
    if (m_blurProfile != profile) {
        m_blurProfile = profile;
        setDirty(true);
        emit blurProfileChanged();
        emit textureChanged();
        if (m_resourceWindow) {
            m_resourceWindow->update();
        }
    }
}

void ValoBackdropProviderV2::onBeforeRendering() {
    if (!m_resourceWindow) return;
    if (m_blurProfile == BlurProfile::Performance) {
        return;
    }

    QRhi *rhi = m_resourceWindow->rhi();
    if (!rhi) return;

    if (m_qsgBlurredTexture && !m_isDirty) {
        return;
    }

    QSGTexture *srcTex = texture(m_resourceWindow);
    if (!srcTex) return;

    if (auto *dynTex = qobject_cast<QSGDynamicTexture *>(srcTex)) {
        dynTex->updateTexture();
    }

    const QSize srcSize = srcTex->textureSize();
    if (!srcSize.isValid() || srcSize.isEmpty()) return;

    QRhiTexture *rhiSrc = srcTex->rhiTexture();
    static int s_beforeRenderingCount = 0;
    if (s_beforeRenderingCount < 5) {
        s_beforeRenderingCount++;
        qWarning("[SGE-BACKDROP-V2] onBeforeRendering frame=%d: srcTex=%p, rhiSrc=%p, srcSize=%dx%d",
                 s_beforeRenderingCount, static_cast<void*>(srcTex), static_cast<void*>(rhiSrc),
                 srcSize.width(), srcSize.height());
    }
    if (!rhiSrc) return;

    if (!m_gpuBlurPipeline) {
        m_gpuBlurPipeline = std::make_unique<ValoGpuBlurPipelineV2>();
    }

    if (!m_gpuBlurPipeline->prepare(rhi, srcSize, m_blurProfile)) {
        return;
    }

    QRhiCommandBuffer *cb = nullptr;
    QRhiSwapChain *swapchain = m_resourceWindow->swapChain();
    if (swapchain) {
        cb = swapchain->currentFrameCommandBuffer();
    }
    if (!cb) {
        cb = static_cast<QRhiCommandBuffer *>(
            m_resourceWindow->rendererInterface()->getResource(m_resourceWindow, QSGRendererInterface::RhiRedirectCommandBuffer));
    }
    if (!cb) return;

    // Executa os passes Dual-Kawase multipass com total segurança no estágio beforeRendering
    // (após beginFrame(), mas antes do beginPass() da cena principal do Scene Graph)
    m_gpuBlurPipeline->execute(cb, rhiSrc, m_blurProfile);

    QRhiTexture *outRhi = m_gpuBlurPipeline->blurredTexture();
    static int s_blurLogCount = 0;
    if (s_blurLogCount < 5) {
        s_blurLogCount++;
        qWarning("[SGE-BACKDROP-V2] Blur GPU executado com sucesso! outRhi=%p, format=%d, size=%dx%d",
                 static_cast<void*>(outRhi),
                 outRhi ? outRhi->format() : 0,
                 outRhi ? outRhi->pixelSize().width() : 0,
                 outRhi ? outRhi->pixelSize().height() : 0);
    }
    if (outRhi) {
        if (!m_qsgBlurredTexture || m_qsgBlurredTexture->rhiTexture() != outRhi) {
            delete std::exchange(m_qsgBlurredTexture, nullptr);
            auto *wrapper = new ValoRhiTextureWrapper(outRhi);
            wrapper->setFiltering(QSGTexture::Linear);
            wrapper->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            wrapper->setVerticalWrapMode(QSGTexture::ClampToEdge);
            m_qsgBlurredTexture = wrapper;
            m_resourceWindow->update();
            m_resourceWindow->update();
        }
    }
    m_isDirty = false;
}

QSGTexture *ValoBackdropProviderV2::blurredTexture(QQuickWindow *window) {
    if (!window) return nullptr;
    attachToWindow(window);

    QSGTexture *srcTex = texture(window);
    if (!srcTex || m_blurProfile == BlurProfile::Performance) {
        return srcTex;
    }

    // Durante syncSceneGraph (updatePaintNode), apenas retorna a textura processada
    // NENHUM comando GPU é emitido nesta fase para preservar o pipeline Vulkan/OpenGL.
    return m_qsgBlurredTexture ? m_qsgBlurredTexture : srcTex;
}

void ValoBackdropProviderV2::releaseResources() {
    if (m_connectedProvider) {
        disconnect(m_connectedProvider, &QSGTextureProvider::textureChanged,
                   this, &ValoBackdropProviderV2::onSourceTextureChanged);
        m_connectedProvider = nullptr;
    }
    if (m_resourceWindow) {
        disconnect(m_resourceWindow, &QQuickWindow::beforeRendering,
                   this, &ValoBackdropProviderV2::onBeforeRendering);
        disconnect(m_resourceWindow, &QQuickWindow::sceneGraphInvalidated,
                   this, &ValoBackdropProviderV2::releaseResources);
        disconnect(m_resourceWindow, &QObject::destroyed, this, nullptr);
        m_resourceWindow = nullptr;
    }
    delete std::exchange(m_qsgBlurredTexture, nullptr);
    delete std::exchange(m_fallbackTexture, nullptr);
    if (m_gpuBlurPipeline) {
        m_gpuBlurPipeline->releaseResources();
        m_gpuBlurPipeline.reset();
    }
}

} // namespace Systalia::Gfx::V2
