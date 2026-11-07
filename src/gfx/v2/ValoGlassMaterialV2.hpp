#pragma once

#include "SystaliaSurfacePropsV2.hpp"
#include <QSGMaterial>
#include <QSGMaterialShader>

namespace Systalia::Gfx::V2 {

/**
 * @brief Shader do material óptico VALO Graphics 2.0.
 *
 * Gerencia o upload do Uniform Buffer Object (UBO) std140 de 160 bytes
 * e vincula os bytecodes SPIR-V compilados via Qt ShaderTools (QSB).
 */
class ValoGlassMaterialShaderV2 : public QSGMaterialShader {
public:
  ValoGlassMaterialShaderV2();

  bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                         QSGMaterial *oldMaterial) override;
};

/**
 * @brief Material gráfico para o Scene Graph do Qt Quick (Etapa 02).
 *
 * Encapsula o pipeline de renderização analítica SDF (L2 / G2),
 * preenchimento volumétrico oceânico, contorno especular unificado
 * e composição alpha pré-multiplicada.
 */
class ValoGlassMaterialV2 : public QSGMaterial {
public:
  ValoGlassMaterialV2();
  ~ValoGlassMaterialV2() override = default;

  QSGMaterialType *type() const override;
  QSGMaterialShader *
  createShader(QSGRendererInterface::RenderMode renderMode) const override;
  int compare(const QSGMaterial *other) const override;

  void setProps(const SystaliaSurfacePropsV2 &props) noexcept {
    m_props = props;
  }
  const SystaliaSurfacePropsV2 &props() const noexcept { return m_props; }
  SystaliaSurfacePropsV2 &props() noexcept { return m_props; }

private:
  SystaliaSurfacePropsV2 m_props;
};

} // namespace Systalia::Gfx::V2
