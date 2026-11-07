#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QVector2D>
#include <QVector4D>
#include <QtQml/qqmlregistration.h>

#include "SystaliaSurfacePropsV2.hpp"
#include "ValoOpticalContext.hpp"

namespace Systalia::Gfx::V2 {

/**
 * @brief Item QML do Scene Graph para o motor gráfico VALO Graphics 2.0 (Etapa
 * 02).
 *
 * Gera a geometria retangular e ancora o material ValoGlassMaterialV2 com
 * SDF analítica (L2 / G2), contorno especular unificado e composição alpha.
 */
class ValoGlassItemV2 : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QVector4D cornerRadii READ cornerRadii WRITE setCornerRadii NOTIFY
                 cornerRadiiChanged)
  Q_PROPERTY(int geometryMode READ geometryMode WRITE setGeometryMode NOTIFY
                 geometryModeChanged)
  Q_PROPERTY(float surfaceOpacity READ surfaceOpacity WRITE setSurfaceOpacity
                 NOTIFY surfaceOpacityChanged)
  Q_PROPERTY(float specularStrokeWidth READ specularStrokeWidth WRITE
                 setSpecularStrokeWidth NOTIFY specularStrokeWidthChanged)
  Q_PROPERTY(QColor volumetricTint READ volumetricTint WRITE setVolumetricTint
                 NOTIFY volumetricTintChanged)
  Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
  Q_PROPERTY(
      int debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)
  Q_PROPERTY(float slant READ slant WRITE setSlant NOTIFY slantChanged)
  Q_PROPERTY(
      Systalia::Gfx::V2::ValoOpticalContext *opticalContext READ opticalContext
          WRITE setOpticalContext NOTIFY opticalContextChanged)

public:
  enum GeometryModeType { CircularL2 = 0, SquircleG2 = 1 };
  Q_ENUM(GeometryModeType)

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

  ValoOpticalContext *opticalContext() const noexcept {
    return m_opticalContext;
  }
  void setOpticalContext(ValoOpticalContext *context);

signals:
  void cornerRadiiChanged();
  void geometryModeChanged();
  void surfaceOpacityChanged();
  void specularStrokeWidthChanged();
  void volumetricTintChanged();
  void activeChanged();
  void debugModeChanged();
  void slantChanged();
  void opticalContextChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *oldNode,
                           UpdatePaintNodeData *data) override;

private:
  QVector4D m_cornerRadii = {32.0f, 32.0f, 32.0f, 32.0f};
  int m_geometryMode = CircularL2;
  float m_surfaceOpacity = 0.34f;
  float m_specularStrokeWidth = 1.25f;
  QColor m_volumetricTint = QColor(8, 46, 82, 100);
  bool m_active = true;
  int m_debugMode = 0;
  float m_slant = 0.0f;
  QPointer<ValoOpticalContext> m_opticalContext;
};

} // namespace Systalia::Gfx::V2
