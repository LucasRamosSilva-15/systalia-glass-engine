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

  connect(&m_animTimer, &QTimer::timeout, this, &ValoGlassItemV2::onAnimationFrame);
  connect(this, &QQuickItem::visibleChanged, this, &ValoGlassItemV2::resetActivity);
  const auto observeWindow = [this](QQuickWindow *window) {
    disconnect(m_windowVisibilityConnection);
    if (window)
      m_windowVisibilityConnection = connect(window, &QWindow::visibilityChanged,
          this, [this] { resetActivity(); });
    resetActivity();
  };
  connect(this, &QQuickItem::windowChanged, this, observeWindow);
  observeWindow(window()); // A C++ parent may already supply a window during construction.
}

void ValoGlassItemV2::updateAnimationPolicy() {
  const bool permitted = !m_opticalContext || m_opticalContext->shouldAnimateCaustics();
  const bool visible = isVisible() && window() && window()->isVisible()
                       && window()->visibility() != QWindow::Minimized;
  const bool animate = visible && m_active && m_blurProfile != Performance
                       && (m_causticIntensity > 0.001f || m_particleIntensity > 0.001f) && permitted;
  m_causticFrozen = !animate;
  if (!animate) {
    m_animTimer.stop();
    return;
  }
  const bool economical = m_opticalContext && m_opticalContext->energySavingFactor() < 0.75f;
  const int interval = m_blurProfile == Quality ? (economical ? 33 : 16)
                                               : (economical ? 50 : 33);
  if (!m_animTimer.isActive()) {
    m_elapsedTimer.restart(); // Resume at the last phase, never integrate hidden time.
    m_animTimer.start(interval);
  } else if (m_animTimer.interval() != interval) {
    m_animTimer.setInterval(interval);
  }
}

void ValoGlassItemV2::resetActivity() {
  updateAnimationPolicy();
  update();
}

void ValoGlassItemV2::onIdleTimeout() {
  // Retained as a diagnostic hook: inactivity alone no longer freezes B/Q.
  updateAnimationPolicy();
  update();
}

void ValoGlassItemV2::onAnimationFrame() {
  updateAnimationPolicy();
  if (m_causticFrozen) return;
  const float dt = std::min(static_cast<float>(m_elapsedTimer.restart()) / 1000.0f, 0.1f);
  m_accumulatedTime += dt;
  emit animationTimeChanged();
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
    resetActivity();
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

void ValoGlassItemV2::setParticleIntensity(float intensity) {
  const float clamped = std::clamp(intensity, 0.0f, 1.0f);
  if (qFuzzyCompare(m_particleIntensity, clamped)) return;
  m_particleIntensity = clamped;
  resetActivity();
  emit particleIntensityChanged();
}

void ValoGlassItemV2::setBlurProfile(int profile) {
  if (profile < Performance || profile > Quality) return;
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
      connect(m_opticalContext, &QObject::destroyed, this, [this] { resetActivity(); });
      connect(m_opticalContext,
              &ValoOpticalContext::adaptiveOpacityOffsetChanged, this,
              &QQuickItem::update);
      connect(m_opticalContext, &ValoOpticalContext::highContrastEnabledChanged,
              this, &QQuickItem::update);
      connect(m_opticalContext, &ValoOpticalContext::prefersReducedMotionChanged,
              this, &ValoGlassItemV2::resetActivity);
      connect(m_opticalContext, &ValoOpticalContext::energySavingFactorChanged,
              this, &ValoGlassItemV2::resetActivity);
    }
    resetActivity();
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

  // Animation policy freezes time independently of illumination intensity.
  props.causticIntensity = m_causticIntensity;
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
