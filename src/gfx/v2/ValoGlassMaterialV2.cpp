#include "ValoGlassMaterialV2.hpp"
#include <cstring>

namespace Systalia::Gfx::V2 {

ValoGlassMaterialShaderV2::ValoGlassMaterialShaderV2() {
  setShaderFileName(VertexStage,
                    QStringLiteral(":/shaders/v2/valo_optical_v2.vert.qsb"));
  setShaderFileName(FragmentStage,
                    QStringLiteral(":/shaders/v2/valo_optical_v2.frag.qsb"));
}

bool ValoGlassMaterialShaderV2::updateUniformData(RenderState &state,
                                                  QSGMaterial *newMaterial,
                                                  QSGMaterial *oldMaterial) {
  Q_UNUSED(oldMaterial);
  QByteArray *buf = state.uniformData();
  if (!buf) {
    return false;
  }

  auto *mat = static_cast<ValoGlassMaterialV2 *>(newMaterial);
  if (!mat) {
    return false;
  }

  SystaliaSurfacePropsV2 props = mat->props();
  // Projeção MVP normalizada do Scene Graph do Qt
  props.matrix = state.combinedMatrix();
  // Modulação da opacidade pela opacidade herdada do item na árvore do Scene
  // Graph
  props.surfaceOpacity = mat->props().surfaceOpacity * state.opacity();

  constexpr qsizetype uboSize =
      static_cast<qsizetype>(sizeof(SystaliaSurfacePropsV2));
  static_assert(uboSize == 160, "UBO V2 deve ter exatamente 160 bytes.");

  // Otimização QRhi: dirty tracking binário para prevenir uploads redundantes
  if (buf->size() >= uboSize &&
      std::memcmp(buf->constData(), &props, uboSize) == 0) {
    return false;
  }

  if (buf->size() < uboSize) {
    buf->resize(uboSize);
  }

  std::memcpy(buf->data(), &props, uboSize);
  return true;
}

ValoGlassMaterialV2::ValoGlassMaterialV2() {
  // Composição alpha pré-multiplicada para transparência óptica e antialiasing
  setFlag(Blending, true);
  setFlag(RequiresFullMatrix, true);
}

QSGMaterialType *ValoGlassMaterialV2::type() const {
  static QSGMaterialType s_type;
  return &s_type;
}

QSGMaterialShader *
ValoGlassMaterialV2::createShader(QSGRendererInterface::RenderMode) const {
  return new ValoGlassMaterialShaderV2();
}

int ValoGlassMaterialV2::compare(const QSGMaterial *other) const {
  const auto *o = static_cast<const ValoGlassMaterialV2 *>(other);
  if (!o)
    return 1;

  // Agrupamento de estado: compara modo geométrico e propriedades estruturais
  if (m_props.geometryMode != o->m_props.geometryMode) {
    return (m_props.geometryMode < o->m_props.geometryMode) ? -1 : 1;
  }
  if (m_props.specularStrokeWidth != o->m_props.specularStrokeWidth) {
    return (m_props.specularStrokeWidth < o->m_props.specularStrokeWidth) ? -1
                                                                          : 1;
  }
  return 0;
}

} // namespace Systalia::Gfx::V2
