#include "SystaliaGlassItem.hpp"
#include "SystaliaGlassMaterial.hpp"
#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QQuickWindow>
#include <QImage>
#include <QColor>

SystaliaGlassItem::SystaliaGlassItem(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

SystaliaGlassItem::~SystaliaGlassItem() {
    releaseResources();
}

void SystaliaGlassItem::setRadii(const QVector4D &radii) {
    if (m_radii != radii) {
        m_radii = radii;
        emit radiiChanged();
        update();
    }
}

void SystaliaGlassItem::setSlant(float slant) {
    if (!qFuzzyCompare(m_slant, slant)) {
        m_slant = slant;
        emit slantChanged();
        update();
    }
}

void SystaliaGlassItem::setIor(float ior) {
    if (!qFuzzyCompare(m_ior, ior)) {
        m_ior = ior;
        emit iorChanged();
        update();
    }
}

void SystaliaGlassItem::setChromaticDispersion(float dispersion) {
    if (!qFuzzyCompare(m_chromaticDispersion, dispersion)) {
        m_chromaticDispersion = dispersion;
        emit chromaticDispersionChanged();
        update();
    }
}

void SystaliaGlassItem::setCausticIntensity(float intensity) {
    if (!qFuzzyCompare(m_causticIntensity, intensity)) {
        m_causticIntensity = intensity;
        emit causticIntensityChanged();
        update();
    }
}

void SystaliaGlassItem::setSurfaceOpacity(float opacity) {
    if (!qFuzzyCompare(m_surfaceOpacity, opacity)) {
        m_surfaceOpacity = opacity;
        emit surfaceOpacityChanged();
        update();
    }
}

void SystaliaGlassItem::setTime(float t) {
    if (!qFuzzyCompare(m_time, t)) {
        m_time = t;
        emit timeChanged();
        update();
    }
}

void SystaliaGlassItem::setBackdropSource(QQuickItem *source) {
    if (m_backdropSource != source) {
        if (m_connectedProvider) {
            disconnect(m_connectedProvider, &QSGTextureProvider::textureChanged,
                       this, &QQuickItem::update);
            m_connectedProvider = nullptr;
        }
        m_backdropSource = source;
        emit backdropSourceChanged();
        update();
    }
}

void SystaliaGlassItem::setScreenResolution(const QVector2D &res) {
    if (m_screenResolution != res) {
        m_screenResolution = res;
        emit screenResolutionChanged();
        update();
    }
}

QSGTexture *SystaliaGlassItem::ensureFallbackTexture() {
    if (!m_fallbackTexture && window()) {
        QImage fallbackImg(4, 4, QImage::Format_RGBA8888);
        fallbackImg.fill(QColor(12, 28, 48, 255));
        m_fallbackTexture = window()->createTextureFromImage(fallbackImg, QQuickWindow::TextureIsOpaque);
    }
    return m_fallbackTexture;
}

void SystaliaGlassItem::releaseResources() {
    if (m_connectedProvider) {
        disconnect(m_connectedProvider, &QSGTextureProvider::textureChanged,
                   this, &QQuickItem::update);
        m_connectedProvider = nullptr;
    }
    if (m_fallbackTexture) {
        delete m_fallbackTexture;
        m_fallbackTexture = nullptr;
    }
}

QSGNode *SystaliaGlassItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    const qreal w = width();
    const qreal h = height();

    if (w <= 0.0 || h <= 0.0) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    SystaliaGlassMaterial *material = nullptr;

    if (!node) {
        node = new QSGGeometryNode();
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry, true);

        material = new SystaliaGlassMaterial();
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial, true);
    } else {
        material = static_cast<SystaliaGlassMaterial *>(node->material());
    }

    // Geometria estritamente ancorada na origem local (0.0, 0.0)
    QSGGeometry::updateTexturedRectGeometry(node->geometry(),
                                           QRectF(0.0, 0.0, w, h),
                                           QRectF(0.0, 0.0, 1.0, 1.0));
    node->markDirty(QSGNode::DirtyGeometry);

    // Extração dinâmica de QSGTexture via QSGTextureProvider do ShaderEffectSource
    QSGTexture *tex = nullptr;
    if (m_backdropSource && m_backdropSource->isTextureProvider()) {
        QSGTextureProvider *provider = m_backdropSource->textureProvider();
        if (provider) {
            if (m_connectedProvider != provider) {
                if (m_connectedProvider) {
                    disconnect(m_connectedProvider, &QSGTextureProvider::textureChanged,
                               this, &QQuickItem::update);
                }
                m_connectedProvider = provider;
                connect(provider, &QSGTextureProvider::textureChanged,
                        this, &QQuickItem::update, Qt::UniqueConnection);
            }
            tex = provider->texture();
        }
    }

    if (!tex) {
        tex = ensureFallbackTexture();
    }

    if (tex) {
        tex->setFiltering(QSGTexture::Linear);
        tex->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        tex->setVerticalWrapMode(QSGTexture::ClampToEdge);
        material->setTexture(tex);
    }

    // Alimentar buffer uniforme std140
    SystaliaSurfaceProps props;
    props.windowSize = QVector2D(static_cast<float>(w), static_cast<float>(h));

    if (m_screenResolution.x() > 0.0f && m_screenResolution.y() > 0.0f) {
        props.screenResolution = m_screenResolution;
    } else if (window()) {
        const float dpr = static_cast<float>(window()->devicePixelRatio());
        props.screenResolution = QVector2D(static_cast<float>(window()->width()) * dpr,
                                           static_cast<float>(window()->height()) * dpr);
    } else {
        props.screenResolution = props.windowSize;
    }

    props.radii = m_radii;
    props.slant = m_slant;
    props.ior = m_ior;
    props.chromaticDispersion = m_chromaticDispersion;
    props.causticIntensity = m_causticIntensity;
    props.opacity = m_surfaceOpacity;
    props.time = m_time;

    material->setProps(props);
    node->markDirty(QSGNode::DirtyMaterial);

    return node;
}
