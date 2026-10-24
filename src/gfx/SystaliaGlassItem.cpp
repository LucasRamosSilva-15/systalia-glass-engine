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

void SystaliaGlassItem::releaseResources() {
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

    QSGGeometry::updateTexturedRectGeometry(node->geometry(),
                                           QRectF(0, 0, w, h),
                                           QRectF(0, 0, 1, 1));

    // Alocar textura sólida de fallback para blindar sampler binding 1 no Vulkan
    if (!m_fallbackTexture && window()) {
        QImage fallbackImg(4, 4, QImage::Format_RGBA8888);
        fallbackImg.fill(QColor(12, 28, 48, 255));
        m_fallbackTexture = window()->createTextureFromImage(fallbackImg, QQuickWindow::TextureIsOpaque);
    }
    material->setTexture(m_fallbackTexture);

    // Alimentar buffer uniforme std140
    SystaliaSurfaceProps props;
    props.windowSize = QVector2D(static_cast<float>(w), static_cast<float>(h));

    if (window()) {
        const qreal dpr = window()->devicePixelRatio();
        props.screenResolution = QVector2D(static_cast<float>(window()->width() * dpr),
                                           static_cast<float>(window()->height() * dpr));
    } else {
        props.screenResolution = props.windowSize;
    }

    props.radii = m_radii;
    props.slant = m_slant;
    props.ior = m_ior;
    props.chromaticDispersion = m_chromaticDispersion;
    props.causticIntensity = m_causticIntensity;
    props.opacity = m_surfaceOpacity;

    material->setProps(props);

    node->markDirty(QSGNode::DirtyGeometry);
    node->markDirty(QSGNode::DirtyMaterial);

    return node;
}
