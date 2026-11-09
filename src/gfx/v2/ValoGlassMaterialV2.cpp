#include "ValoGlassMaterialV2.hpp"
#include <cstring>
#include <rhi/qrhi.h>

namespace Systalia::Gfx::V2 {

ValoGlassMaterialShaderV2::ValoGlassMaterialShaderV2(int diagnosticMode) {
  setShaderFileName(VertexStage,
                    QStringLiteral(":/shaders/v2/valo_optical_v2.vert.qsb"));

  switch (diagnosticMode) {
  case 1:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_sdf.frag.qsb"));
    break;
  case 2:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_mask.frag.qsb"));
    break;
  case 3:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_volume.frag.qsb"));
    break;
  case 4:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_refraction.frag.qsb"));
    break;
  case 5:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_blur.frag.qsb"));
    break;
  case 6:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_specular.frag.qsb"));
    break;
  case 7:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_caustics.frag.qsb"));
    break;
  case 8:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_reflection.frag.qsb"));
    break;
  case 9:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_diag_basic.frag.qsb"));
    break;
  default:
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/shaders/v2/valo_optical_v2.frag.qsb"));
    break;
  }
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
  // Modulação da opacidade pela opacidade herdada do item na árvore do Scene Graph
  // The optical mix remains independent of scene opacity. Slot 124 is unused
  // by V2 geometry; only the GPU copy aliases it as inherited opacity.
  props.titleBarHeight = state.opacity();
  // Signed height carries framebuffer orientation without changing the ABI.
  // Qt's OpenGL layers invert the projection to make their texture top-down;
  // API orientation alone would flip the backdrop a second time in a capture.
  if (state.rhi() && state.rhi()->isYUpInFramebuffer()
      && state.projectionMatrix()(1, 1) < 0.0f)
    props.screenResolution.setY(-props.screenResolution.y());

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

void ValoGlassMaterialShaderV2::updateSampledImage(RenderState &state,
                                                   int binding,
                                                   QSGTexture **texture,
                                                   QSGMaterial *newMaterial,
                                                   QSGMaterial *oldMaterial) {
  Q_UNUSED(oldMaterial);
  auto *mat = static_cast<ValoGlassMaterialV2 *>(newMaterial);
  if (!mat || !texture) {
    return;
  }

  QSGTexture *targetTexture = nullptr;
  if (binding == 1) {
    targetTexture = mat->backdropTexture();
  } else if (binding == 2) {
    targetTexture = mat->clientTexture();
    if (!targetTexture) {
      targetTexture = mat->backdropTexture(); // Fallback seguro caso clientTexture não esteja vinculada
    }
  }

  // Fallback para evitar dereferência de ponteiro nulo/sentinela no Scene Graph
  if (!targetTexture && *texture && reinterpret_cast<uintptr_t>(*texture) != 0xfefefefefefefefeULL) {
    targetTexture = *texture;
  }

  if (targetTexture) {
    targetTexture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    *texture = targetTexture;
  }
}

ValoGlassMaterialV2::ValoGlassMaterialV2(int diagnosticMode)
    : m_diagnosticMode(diagnosticMode) {
  // Composição alpha pré-multiplicada para transparência óptica e antialiasing
  setFlag(Blending, true);
  setFlag(RequiresFullMatrix, true);
}

QSGMaterialType *ValoGlassMaterialV2::type() const {
  static QSGMaterialType s_types[16];
  const int idx = std::clamp(m_diagnosticMode, 0, 15);
  return &s_types[idx];
}

QSGMaterialShader *
ValoGlassMaterialV2::createShader(QSGRendererInterface::RenderMode) const {
  return new ValoGlassMaterialShaderV2(m_diagnosticMode);
}

int ValoGlassMaterialV2::compare(const QSGMaterial *other) const {
  const auto *o = static_cast<const ValoGlassMaterialV2 *>(other);
  if (!o)
    return 1;

  if (m_diagnosticMode != o->m_diagnosticMode) {
    return (m_diagnosticMode < o->m_diagnosticMode) ? -1 : 1;
  }

  if (m_backdropTexture != o->m_backdropTexture) {
    return (m_backdropTexture < o->m_backdropTexture) ? -1 : 1;
  }
  if (m_clientTexture != o->m_clientTexture) {
    return (m_clientTexture < o->m_clientTexture) ? -1 : 1;
  }

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
