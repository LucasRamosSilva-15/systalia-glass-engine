#pragma once

#include "SystaliaSurfacePropsV2.hpp"
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>

namespace Systalia::Gfx::V2 {

/**
 * @brief Shader do material óptico VALO Graphics 2.0.
 *
 * Gerencia o upload do Uniform Buffer Object (UBO) std140 de 160 bytes,
 * vincula os bytecodes SPIR-V compilados via Qt ShaderTools (QSB) e
 * provê o binding de samplers de textura (backdrop filtrado e cliente).
 */
class ValoGlassMaterialShaderV2 : public QSGMaterialShader {
public:
  explicit ValoGlassMaterialShaderV2(int diagnosticMode = 0);

  bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                         QSGMaterial *oldMaterial) override;

  void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                          QSGMaterial *newMaterial,
                          QSGMaterial *oldMaterial) override;
};

/**
 * @brief Material gráfico para o Scene Graph do Qt Quick (Etapa 03).
 *
 * Encapsula o pipeline de renderização analítica SDF (L2 / G2),
 * refração com dispersão cromática Cauchy, reflexos espectrais -31°,
 * cáusticas procedurais, preenchimento volumétrico oceânico e
 * composição alpha pré-multiplicada.
 */
class ValoGlassMaterialV2 : public QSGMaterial {
public:
  explicit ValoGlassMaterialV2(int diagnosticMode = 0);
  ~ValoGlassMaterialV2() override = default;

  QSGMaterialType *type() const override;
  QSGMaterialShader *
  createShader(QSGRendererInterface::RenderMode renderMode) const override;
  int compare(const QSGMaterial *other) const override;

  int diagnosticMode() const noexcept { return m_diagnosticMode; }
  void setDiagnosticMode(int mode) noexcept { m_diagnosticMode = mode; }

  void setProps(const SystaliaSurfacePropsV2 &props) noexcept {
    m_props = props;
  }
  const SystaliaSurfacePropsV2 &props() const noexcept { return m_props; }
  SystaliaSurfacePropsV2 &props() noexcept { return m_props; }

  QSGTexture *backdropTexture() const noexcept { return m_backdropTexture; }
  void setBackdropTexture(QSGTexture *tex) noexcept { m_backdropTexture = tex; }

  QSGTexture *clientTexture() const noexcept { return m_clientTexture; }
  void setClientTexture(QSGTexture *tex) noexcept { m_clientTexture = tex; }

private:
  SystaliaSurfacePropsV2 m_props;
  QSGTexture *m_backdropTexture = nullptr;
  QSGTexture *m_clientTexture = nullptr;
  int m_diagnosticMode = 0;
};

} // namespace Systalia::Gfx::V2
