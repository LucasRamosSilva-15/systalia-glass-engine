#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointer>
#include <QQuickItem>
#include <QSGTexture>
#include <QTimer>
#include <QVector2D>
#include <QVector4D>
#include <QtQml/qqmlregistration.h>

#include "SystaliaSurfacePropsV2.hpp"
#include "ValoBackdropProviderV2.hpp"
#include "ValoBlurPipelineV2.hpp"
#include "ValoOpticalContext.hpp"

namespace Systalia::Gfx::V2 {

/**
 * @brief Item QML do Scene Graph para o motor gráfico VALO Graphics 2.0 (Etapa 03).
 *
 * Ancara o material ValoGlassMaterialV2 com SDF analítica (L2 / G2),
 * refração cromática Cauchy, reflexos espectrais -31°, cáusticas procedurais,
 * congelamento de inatividade (idle-freeze) e perfis Dual-Kawase.
 */
class ValoGlassItemV2 : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QVector4D cornerRadii READ cornerRadii WRITE setCornerRadii NOTIFY cornerRadiiChanged)
  Q_PROPERTY(int geometryMode READ geometryMode WRITE setGeometryMode NOTIFY geometryModeChanged)
  Q_PROPERTY(float surfaceOpacity READ surfaceOpacity WRITE setSurfaceOpacity NOTIFY surfaceOpacityChanged)
  Q_PROPERTY(float specularStrokeWidth READ specularStrokeWidth WRITE setSpecularStrokeWidth NOTIFY specularStrokeWidthChanged)
  Q_PROPERTY(QColor volumetricTint READ volumetricTint WRITE setVolumetricTint NOTIFY volumetricTintChanged)
  Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
  Q_PROPERTY(int debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)
  Q_PROPERTY(float slant READ slant WRITE setSlant NOTIFY slantChanged)
  Q_PROPERTY(float ior READ ior WRITE setIor NOTIFY iorChanged)
  Q_PROPERTY(float chromaticDispersion READ chromaticDispersion WRITE setChromaticDispersion NOTIFY chromaticDispersionChanged)
  Q_PROPERTY(float causticIntensity READ causticIntensity WRITE setCausticIntensity NOTIFY causticIntensityChanged)
  Q_PROPERTY(float animationTime READ causticTime NOTIFY animationTimeChanged)
  Q_PROPERTY(float particleIntensity READ particleIntensity WRITE setParticleIntensity NOTIFY particleIntensityChanged)
  Q_PROPERTY(int blurProfile READ blurProfile WRITE setBlurProfile NOTIFY blurProfileChanged)
  Q_PROPERTY(Systalia::Gfx::V2::ValoBackdropProviderV2 *backdropProvider READ backdropProvider WRITE setBackdropProvider NOTIFY backdropProviderChanged)
  Q_PROPERTY(Systalia::Gfx::V2::ValoOpticalContext *opticalContext READ opticalContext WRITE setOpticalContext NOTIFY opticalContextChanged)

public:
  enum GeometryModeType { CircularL2 = 0, SquircleG2 = 1 };
  Q_ENUM(GeometryModeType)

  enum BlurProfileType {
    Performance = 0,
    Balanced = 1,
    Quality = 2
  };
  Q_ENUM(BlurProfileType)

  explicit ValoGlassItemV2(QQuickItem *parent = nullptr);
  ~ValoGlassItemV2() override = default;

  QVector4D cornerRadii() const noexcept { return m_cornerRadii; }
  void setCornerRadii(const QVector4D &radii);

  int geometryMode() const noexcept { return m_geometryMode; }
  void setGeometryMode(int mode);

  float surfaceOpacity() const noexcept { return m_surfaceOpacity; }
  void setSurfaceOpacity(float opacity);

  float specularStrokeWidth() const noexcept { return m_specularStrokeWidth; }
  void setSpecularStrokeWidth(float width);

  QColor volumetricTint() const noexcept { return m_volumetricTint; }
  void setVolumetricTint(const QColor &tint);

  bool isActive() const noexcept { return m_active; }
  void setActive(bool active);

  int debugMode() const noexcept { return m_debugMode; }
  void setDebugMode(int mode);

  float slant() const noexcept { return m_slant; }
  void setSlant(float slant);

  float ior() const noexcept { return m_ior; }
  void setIor(float ior);

  float chromaticDispersion() const noexcept { return m_chromaticDispersion; }
  void setChromaticDispersion(float dispersion);

  float causticIntensity() const noexcept { return m_causticIntensity; }
  void setCausticIntensity(float intensity);

  float particleIntensity() const noexcept { return m_particleIntensity; }
  void setParticleIntensity(float intensity);

  int blurProfile() const noexcept { return m_blurProfile; }
  void setBlurProfile(int profile);

  ValoBackdropProviderV2 *backdropProvider() const noexcept { return m_backdropProvider; }
  void setBackdropProvider(ValoBackdropProviderV2 *provider);

  ValoOpticalContext *opticalContext() const noexcept { return m_opticalContext; }
  void setOpticalContext(ValoOpticalContext *context);

  void resetActivity();
  bool isCausticFrozen() const noexcept { return m_causticFrozen; }
  float causticTime() const noexcept { return m_accumulatedTime; }
  int animationIntervalMs() const noexcept { return m_animTimer.interval(); }
  bool animationRunning() const noexcept { return m_animTimer.isActive(); }

signals:
  void cornerRadiiChanged();
  void geometryModeChanged();
  void surfaceOpacityChanged();
  void specularStrokeWidthChanged();
  void volumetricTintChanged();
  void activeChanged();
  void debugModeChanged();
  void slantChanged();
  void iorChanged();
  void chromaticDispersionChanged();
  void causticIntensityChanged();
  void animationTimeChanged();
  void particleIntensityChanged();
  void blurProfileChanged();
  void backdropProviderChanged();
  void opticalContextChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private slots:
  void onIdleTimeout();
  void onAnimationFrame();

private:
  void updateAnimationPolicy();
  QMetaObject::Connection m_windowVisibilityConnection;
  QVector4D m_cornerRadii = {32.0f, 32.0f, 32.0f, 32.0f};
  int m_geometryMode = CircularL2;
  float m_surfaceOpacity = 0.34f;
  float m_specularStrokeWidth = 1.25f;
  QColor m_volumetricTint = QColor(8, 46, 82, 100);
  bool m_active = true;
  int m_debugMode = 0;
  float m_slant = 0.0f;
  float m_ior = 1.12f;
  float m_chromaticDispersion = 0.65f;
  float m_causticIntensity = 0.35f;
  int m_blurProfile = Quality;

  QPointer<ValoBackdropProviderV2> m_backdropProvider;
  QPointer<ValoOpticalContext> m_opticalContext;

  float m_particleIntensity = 0.0f; // Scheduling only; never appended to the optical UBO.
  QTimer m_animTimer;
  QElapsedTimer m_elapsedTimer;
  float m_accumulatedTime = 0.0f;
  bool m_causticFrozen = false;

  QSGTexture *m_fallbackBackdropTexture = nullptr;
};

} // namespace Systalia::Gfx::V2
