#include "ValoShadowItemV2.hpp"
#include <QSGGeometry>
#include <cstring>
#include <algorithm>

namespace Systalia::Gfx::V2 {

ValoShadowMaterialShaderV2::ValoShadowMaterialShaderV2() {
    setShaderFileName(VertexStage, QStringLiteral(":/shaders/v2/valo_shadow_v2.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/shaders/v2/valo_shadow_v2.frag.qsb"));
}

bool ValoShadowMaterialShaderV2::updateUniformData(RenderState &state,
                                                   QSGMaterial *newMaterial,
                                                   QSGMaterial *oldMaterial) {
    Q_UNUSED(oldMaterial);
    QByteArray *buf = state.uniformData();
    if (!buf) return false;

    auto *mat = static_cast<ValoShadowMaterialV2 *>(newMaterial);
    if (!mat) return false;

    SystaliaSurfacePropsV2 props = mat->props();
    props.matrix = state.combinedMatrix();
    props.shadowIntensity = mat->props().shadowIntensity * state.opacity();

    constexpr qsizetype uboSize = static_cast<qsizetype>(sizeof(SystaliaSurfacePropsV2));
    static_assert(uboSize == 160, "UBO V2 deve ter exatamente 160 bytes.");

    if (buf->size() >= uboSize && std::memcmp(buf->constData(), &props, uboSize) == 0) {
        return false;
    }

    if (buf->size() < uboSize) {
        buf->resize(uboSize);
    }

    std::memcpy(buf->data(), &props, uboSize);
    return true;
}

ValoShadowMaterialV2::ValoShadowMaterialV2() {
    setFlag(Blending, true);
    setFlag(RequiresFullMatrix, true);
}

QSGMaterialType *ValoShadowMaterialV2::type() const {
    static QSGMaterialType s_type;
    return &s_type;
}

QSGMaterialShader *ValoShadowMaterialV2::createShader(QSGRendererInterface::RenderMode) const {
    return new ValoShadowMaterialShaderV2();
}

int ValoShadowMaterialV2::compare(const QSGMaterial *other) const {
    const auto *o = static_cast<const ValoShadowMaterialV2 *>(other);
    if (!o) return 1;
    if (m_props.shadowOffsetY != o->m_props.shadowOffsetY) {
        return (m_props.shadowOffsetY < o->m_props.shadowOffsetY) ? -1 : 1;
    }
    return 0;
}

ValoShadowItemV2::ValoShadowItemV2(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void ValoShadowItemV2::setCornerRadii(const QVector4D &radii) {
    if (m_cornerRadii != radii) {
        m_cornerRadii = radii;
        emit cornerRadiiChanged();
        update();
    }
}

void ValoShadowItemV2::setShadowOffsetY(float offset) {
    if (!qFuzzyCompare(m_shadowOffsetY, offset)) {
        m_shadowOffsetY = offset;
        emit shadowOffsetYChanged();
        update();
    }
}

void ValoShadowItemV2::setShadowIntensity(float intensity) {
    const float clamped = std::clamp(intensity, 0.0f, 1.0f);
    if (!qFuzzyCompare(m_shadowIntensity, clamped)) {
        m_shadowIntensity = clamped;
        emit shadowIntensityChanged();
        update();
    }
}

void ValoShadowItemV2::setIsolatedPass(bool isolated) {
    if (m_isolatedPass != isolated) {
        m_isolatedPass = isolated;
        emit isolatedPassChanged();
        update();
    }
}

QSGNode *ValoShadowItemV2::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    const qreal w = width();
    const qreal h = height();
    if (w <= 0.0 || h <= 0.0) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    ValoShadowMaterialV2 *material = nullptr;

    if (!node) {
        node = new QSGGeometryNode();
        auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geom);
        node->setFlag(QSGNode::OwnsGeometry, true);

        material = new ValoShadowMaterialV2();
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial, true);
    } else {
        material = static_cast<ValoShadowMaterialV2 *>(node->material());
    }

    // Área expandida para acomodar o decaimento suave contínuo da sombra (sigma = 20px)
    constexpr qreal pad = 40.0;
    const qreal totalOffsetY = std::max<qreal>(m_shadowOffsetY, 0.0);
    const QRectF rect(-pad, -pad, w + 2.0 * pad, h + 2.0 * pad + totalOffsetY);

    QSGGeometry::updateTexturedRectGeometry(node->geometry(), rect, QRectF(0.0, 0.0, 1.0, 1.0));
    node->markDirty(QSGNode::DirtyGeometry);

    SystaliaSurfacePropsV2 props;
    props.windowSize = QVector2D(static_cast<float>(w), static_cast<float>(h));
    props.radii = m_cornerRadii;
    props.geometryMode = 0.0f; // CircularL2
    props.shadowOffsetY = m_shadowOffsetY;
    props.shadowIntensity = m_shadowIntensity;
    props.slant = m_isolatedPass ? 1.0f : 0.0f; // 1.0 = Não oclui sob o vidro (Passe G isolado)

    material->setProps(props);
    node->markDirty(QSGNode::DirtyMaterial);

    return node;
}

} // namespace Systalia::Gfx::V2

