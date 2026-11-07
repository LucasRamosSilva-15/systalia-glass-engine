#include "ValoGlassItemV2.hpp"
#include "ValoBackdropProviderV2.hpp"
#include "ValoGlassMaterialV2.hpp"
#include <QPainter>
#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>

namespace Systalia::Gfx::V2 {

ValoGlassItemV2::ValoGlassItemV2(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);

  m_idleTimer.setSingleShot(true);
  m_idleTimer.setInterval(5000); // 5 segundos de inatividade para congelamento de cáusticas
  connect(&m_idleTimer, &QTimer::timeout, this, &ValoGlassItemV2::onIdleTimeout);

  connect(&m_animTimer, &QTimer::timeout, this, &ValoGlassItemV2::onAnimationFrame);
  m_animTimer.setInterval(16); // ~60fps para ondulação fluida

  m_elapsedTimer.start();
  m_idleTimer.start();

  if (m_causticIntensity > 0.001f && m_active) {
    m_animTimer.start();
  }
}

void ValoGlassItemV2::resetActivity() {
  m_causticFrozen = false;
  m_idleTimer.start();
  m_elapsedTimer.restart();
  if (m_causticIntensity > 0.001f && m_active && !m_animTimer.isActive()) {
    m_animTimer.start();
  }
  update();
}

void ValoGlassItemV2::onIdleTimeout() {
  m_causticFrozen = true;
  m_animTimer.stop();
  update();
}

void ValoGlassItemV2::onAnimationFrame() {
  if (m_causticFrozen) {
    m_animTimer.stop();
    return;
  }
  const float dt = static_cast<float>(m_elapsedTimer.restart()) / 1000.0f;
  m_accumulatedTime += dt;
  update();
}

void ValoGlassItemV2::setCornerRadii(const QVector4D &radii) {
  if (m_cornerRadii != radii) {
    m_cornerRadii = radii;
    resetActivity();
    emit cornerRadiiChanged();
    update();
  }
}

void ValoGlassItemV2::setGeometryMode(int mode) {
  if (m_geometryMode != mode) {
    m_geometryMode = mode;
    resetActivity();
    emit geometryModeChanged();
    update();
  }
}

void ValoGlassItemV2::setSurfaceOpacity(float opacity) {
  const float clamped = std::clamp(opacity, 0.0f, 1.0f);
  if (!qFuzzyCompare(m_surfaceOpacity, clamped)) {
    m_surfaceOpacity = clamped;
    resetActivity();
    emit surfaceOpacityChanged();
    update();
  }
}

void ValoGlassItemV2::setSpecularStrokeWidth(float width) {
  const float clamped = std::max(width, 0.0f);
  if (!qFuzzyCompare(m_specularStrokeWidth, clamped)) {
    m_specularStrokeWidth = clamped;
    resetActivity();
    emit specularStrokeWidthChanged();
    update();
  }
}

void ValoGlassItemV2::setVolumetricTint(const QColor &tint) {
  if (m_volumetricTint != tint) {
    m_volumetricTint = tint;
    resetActivity();
    emit volumetricTintChanged();
    update();
  }
}

void ValoGlassItemV2::setActive(bool active) {
  if (m_active != active) {
    m_active = active;
    if (m_active) {
      resetActivity();
    } else {
      m_animTimer.stop();
      m_idleTimer.stop();
    }
    emit activeChanged();
    update();
  }
}

void ValoGlassItemV2::setDebugMode(int mode) {
  if (m_debugMode != mode) {
    m_debugMode = mode;
    resetActivity();
    emit debugModeChanged();
    update();
  }
}

void ValoGlassItemV2::setSlant(float slant) {
  if (!qFuzzyCompare(m_slant, slant)) {
    m_slant = slant;
    resetActivity();
    emit slantChanged();
    update();
  }
}

void ValoGlassItemV2::setIor(float ior) {
  const float clamped = std::clamp(ior, 1.0f, 2.5f);
  if (!qFuzzyCompare(m_ior, clamped)) {
    m_ior = clamped;
    resetActivity();
    emit iorChanged();
    update();
  }
}

void ValoGlassItemV2::setChromaticDispersion(float dispersion) {
  const float clamped = std::clamp(dispersion, 0.0f, 2.0f);
  if (!qFuzzyCompare(m_chromaticDispersion, clamped)) {
    m_chromaticDispersion = clamped;
    resetActivity();
    emit chromaticDispersionChanged();
    update();
  }
}

void ValoGlassItemV2::setCausticIntensity(float intensity) {
  const float clamped = std::clamp(intensity, 0.0f, 1.0f);
  if (!qFuzzyCompare(m_causticIntensity, clamped)) {
    m_causticIntensity = clamped;
    resetActivity();
    emit causticIntensityChanged();
    update();
  }
}

void ValoGlassItemV2::setBlurProfile(int profile) {
  if (m_blurProfile != profile) {
    m_blurProfile = profile;
    if (m_backdropProvider) {
      m_backdropProvider->setBlurProfile(static_cast<BlurProfile>(profile));
    }
    resetActivity();
    emit blurProfileChanged();
    update();
  }
}

void ValoGlassItemV2::setBackdropProvider(ValoBackdropProviderV2 *provider) {
  if (m_backdropProvider != provider) {
    if (m_backdropProvider) {
      disconnect(m_backdropProvider, nullptr, this, nullptr);
    }
    m_backdropProvider = provider;
    if (m_backdropProvider) {
      m_backdropProvider->setBlurProfile(static_cast<BlurProfile>(m_blurProfile));
      connect(m_backdropProvider, &ValoBackdropProviderV2::textureChanged,
              this, &QQuickItem::update);
    }
    emit backdropProviderChanged();
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
      connect(m_opticalContext, &ValoOpticalContext::prefersReducedMotionChanged,
              this, &QQuickItem::update);
      connect(m_opticalContext, &ValoOpticalContext::energySavingFactorChanged,
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

    material = new ValoGlassMaterialV2(m_debugMode);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial, true);
  } else {
    material = static_cast<ValoGlassMaterialV2 *>(node->material());
    if (!material || material->diagnosticMode() != m_debugMode) {
      material = new ValoGlassMaterialV2(m_debugMode);
      node->setMaterial(material);
    }
  }

  // Geometria cobrindo a área retangular do item
  QSGGeometry::updateTexturedRectGeometry(
      node->geometry(), QRectF(0.0, 0.0, w, h), QRectF(0.0, 0.0, 1.0, 1.0));
  node->markDirty(QSGNode::DirtyGeometry);

  // Vincula a textura do backdrop provider com desfoque GPU real se conectado
  QSGTexture *backdropTex = nullptr;
  if (m_backdropProvider) {
    backdropTex = m_backdropProvider->blurredTexture(window());
  }

  if (!backdropTex) {
    if (window() && !m_fallbackBackdropTexture) {
      const QImage synthBackdrop =
          ValoBackdropProviderV2::createSyntheticBackdrop(QSize(256, 256));
      m_fallbackBackdropTexture =
          window()->createTextureFromImage(synthBackdrop, QQuickWindow::TextureHasAlphaChannel);
    }
    backdropTex = m_fallbackBackdropTexture;
  }

  if (backdropTex) {
    material->setBackdropTexture(backdropTex);
  }

  // Bloco uniforme std140 de 160 bytes
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
  props.ior = m_ior;
  props.chromaticDispersion = m_chromaticDispersion;

  // Cáusticas: desligadas se houver restrição de movimento ou economia de energia
  float effectiveCaustics = m_causticIntensity;
  if (m_opticalContext && (m_opticalContext->prefersReducedMotion() ||
                           !m_opticalContext->shouldAnimateCaustics())) {
    effectiveCaustics = 0.0f;
  }
  if (m_causticFrozen) {
    effectiveCaustics = 0.0f;
  }
  props.causticIntensity = effectiveCaustics;
  props.time = m_accumulatedTime;

  // Opacidade com suporte ao contexto adaptativo
  const float baseOpacity = m_surfaceOpacity;
  props.surfaceOpacity = m_opticalContext
                             ? m_opticalContext->effectiveOpacity(baseOpacity)
                             : baseOpacity;

  props.volumetricTint =
      QVector4D(static_cast<float>(m_volumetricTint.redF()),
                static_cast<float>(m_volumetricTint.greenF()),
                static_cast<float>(m_volumetricTint.blueF()),
                static_cast<float>(m_volumetricTint.alphaF()));

  // Configuração desacoplada do modo de diagnóstico (mantém propriedades físicas intactas)
  material->setDiagnosticMode(m_debugMode);
  props.shadowOffsetY = 12.0f;
  props.shadowIntensity = 0.35f;

  material->setProps(props);
  node->markDirty(QSGNode::DirtyMaterial);

  return node;
}

} // namespace Systalia::Gfx::V2
