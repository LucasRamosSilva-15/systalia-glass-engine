#include "ValoGlassItemV2.hpp"
#include "ValoGlassMaterialV2.hpp"
#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>

namespace Systalia::Gfx::V2 {

ValoGlassItemV2::ValoGlassItemV2(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
}

void ValoGlassItemV2::setCornerRadii(const QVector4D &radii) {
  if (m_cornerRadii != radii) {
    m_cornerRadii = radii;
    emit cornerRadiiChanged();
    update();
  }
}

void ValoGlassItemV2::setGeometryMode(int mode) {
  if (m_geometryMode != mode) {
    m_geometryMode = mode;
    emit geometryModeChanged();
    update();
  }
}

void ValoGlassItemV2::setSurfaceOpacity(float opacity) {
  const float clamped = std::clamp(opacity, 0.0f, 1.0f);
  if (!qFuzzyCompare(m_surfaceOpacity, clamped)) {
    m_surfaceOpacity = clamped;
    emit surfaceOpacityChanged();
    update();
  }
}

void ValoGlassItemV2::setSpecularStrokeWidth(float width) {
  const float clamped = std::max(width, 0.0f);
  if (!qFuzzyCompare(m_specularStrokeWidth, clamped)) {
    m_specularStrokeWidth = clamped;
    emit specularStrokeWidthChanged();
    update();
  }
}

void ValoGlassItemV2::setVolumetricTint(const QColor &tint) {
  if (m_volumetricTint != tint) {
    m_volumetricTint = tint;
    emit volumetricTintChanged();
    update();
  }
}

void ValoGlassItemV2::setActive(bool active) {
  if (m_active != active) {
    m_active = active;
    emit activeChanged();
    update();
  }
}

void ValoGlassItemV2::setDebugMode(int mode) {
  if (m_debugMode != mode) {
    m_debugMode = mode;
    emit debugModeChanged();
    update();
  }
}

void ValoGlassItemV2::setSlant(float slant) {
  if (!qFuzzyCompare(m_slant, slant)) {
    m_slant = slant;
    emit slantChanged();
    update();
  }
}

void ValoGlassItemV2::setOpticalContext(ValoOpticalContext *context) {
  if (m_opticalContext != context) {
    if (m_opticalContext) {
      disconnect(m_opticalContext, nullptr, this, nullptr);
    }
    m_opticalContext = context;
    if (m_opticalContext) {
      connect(m_opticalContext,
              &ValoOpticalContext::adaptiveOpacityOffsetChanged, this,
              &QQuickItem::update);
      connect(m_opticalContext, &ValoOpticalContext::highContrastEnabledChanged,
              this, &QQuickItem::update);
    }
    emit opticalContextChanged();
    update();
  }
}

QSGNode *ValoGlassItemV2::updatePaintNode(QSGNode *oldNode,
                                          UpdatePaintNodeData *) {
  const qreal w = width();
  const qreal h = height();

  if (w <= 0.0 || h <= 0.0 || !m_active) {
    delete oldNode;
    return nullptr;
  }

  auto *node = static_cast<QSGGeometryNode *>(oldNode);
  ValoGlassMaterialV2 *material = nullptr;

  if (!node) {
    node = new QSGGeometryNode();
    auto *geometry =
        new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry, true);

    material = new ValoGlassMaterialV2();
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial, true);
  } else {
    material = static_cast<ValoGlassMaterialV2 *>(node->material());
  }

  // Geometria cobrindo a área retangular do item
  QSGGeometry::updateTexturedRectGeometry(
      node->geometry(), QRectF(0.0, 0.0, w, h), QRectF(0.0, 0.0, 1.0, 1.0));
  node->markDirty(QSGNode::DirtyGeometry);

  // Alimentação do bloco uniforme std140 de 160 bytes
  SystaliaSurfacePropsV2 props;
  props.windowSize = QVector2D(static_cast<float>(w), static_cast<float>(h));

  if (window()) {
    const qreal dpr = window()->devicePixelRatio();
    props.screenResolution =
        QVector2D(static_cast<float>(window()->width() * dpr),
                  static_cast<float>(window()->height() * dpr));
  } else {
    props.screenResolution = props.windowSize;
  }

  props.radii = m_cornerRadii;
  props.geometryMode = static_cast<float>(m_geometryMode);
  props.specularStrokeWidth = m_specularStrokeWidth;
  props.slant = m_slant;

  // Cálculo da opacidade com suporte ao contexto adaptativo
  const float baseOpacity = m_surfaceOpacity;
  props.surfaceOpacity = m_opticalContext
                             ? m_opticalContext->effectiveOpacity(baseOpacity)
                             : baseOpacity;

  props.volumetricTint =
      QVector4D(static_cast<float>(m_volumetricTint.redF()),
                static_cast<float>(m_volumetricTint.greenF()),
                static_cast<float>(m_volumetricTint.blueF()),
                static_cast<float>(m_volumetricTint.alphaF()));

  // Codificação do modo de diagnóstico em shadowOffsetY (negativo para debug)
  if (m_debugMode > 0) {
    props.shadowOffsetY = -static_cast<float>(m_debugMode);
  } else {
    props.shadowOffsetY = 12.0f;
  }

  material->setProps(props);
  node->markDirty(QSGNode::DirtyMaterial);

  return node;
}

} // namespace Systalia::Gfx::V2
