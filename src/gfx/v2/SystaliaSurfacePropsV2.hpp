#pragma once

#include "SystaliaSurfaceProps.hpp"
#include <QVector4D>
#include <QVector2D>
#include <cstddef>
#include <cstdint>

namespace Systalia::Gfx::V2 {

/**
 * @brief Modo geométrico para avaliação de contorno e SDF.
 * 0 = CircularL2: cantos circulares euclidianos analíticos (TopBar, Dock).
 * 1 = SquircleG2: superelipse L4 com continuidade de curvatura C2 (Janelas).
 */
enum class GeometryMode : uint32_t {
    CircularL2 = 0,
    SquircleG2 = 1
};

/**
 * @brief Bloco de dados uniformes da arquitetura VALO Graphics 2.0.
 *
 * Layout em conformidade rigorosa com std140 (Vulkan / SPIR-V):
 * - Tamanho total: exatamente 160 bytes (10 blocos de 16 bytes).
 * - Alinhamento: 16 bytes.
 * - Todos os membros alinhados aos múltiplos dos seus tamanhos base std140.
 *
 * Contrato legado V1 (SystaliaSurfaceProps, 128 bytes) é preservado em paralelo.
 */
struct alignas(16) SystaliaSurfacePropsV2 {
    Matrix4x4_std140 matrix;               // offset 0,   tamanho 64 (Matriz MVP normalizada)
    QVector4D radii;                       // offset 64,  tamanho 16 (TL, TR, BL, BR em pixels)
    QVector2D windowSize;                  // offset 80,  tamanho 8  (Largura, Altura lógicas)
    QVector2D screenResolution;            // offset 88, tamanho 8 (CPU: viewport físico; upload: sinal Y indica framebuffer Y-up)
    float slant = 0.0f;                    // offset 96,  tamanho 4  (Inclinação visual 3D em radianos)
    float ior = 1.25f;                     // offset 100, tamanho 4  (Índice de refração: 1.00 a 1.25)
    float chromaticDispersion = 0.040f;    // offset 104, tamanho 4  (Dispersão de Cauchy)
    float causticIntensity = 0.85f;        // offset 108, tamanho 4  (Intensidade das cáusticas)
    float surfaceOpacity = 0.34f;          // offset 112, tamanho 4  (Opacidade de base: 0.25 a 0.55)
    float time = 0.0f;                     // offset 116, tamanho 4  (Tempo decorrido em segundos)
    float hasClientTexture = 0.0f;         // offset 120, tamanho 4  (Flag 0.0f ou 1.0f)
    float titleBarHeight = 36.0f;          // offset 124, tamanho 4 (CPU legado; upload do material V2: opacidade herdada)
    // --- Campos de Expansão V2 ---
    float specularStrokeWidth = 1.25f;     // offset 128, tamanho 4  (Espessura da borda especular em pixels)
    float geometryMode = 0.0f;             // offset 132, tamanho 4  (0.0f = CircularL2, 1.0f = SquircleG2)
    float shadowIntensity = 0.35f;         // offset 136, tamanho 4  (Opacidade da sombra externa: 0.0 a 1.0)
    float shadowOffsetY = 12.0f;           // offset 140, tamanho 4  (Deslocamento vertical da sombra em pixels)
    QVector4D volumetricTint = {0.03f, 0.18f, 0.32f, 0.39f}; // offset 144, tamanho 16 (Tonalização RGBA)
};

// =========================================================================
// Validação em Tempo de Compilação da ABI std140 (160 Bytes)
// =========================================================================

static_assert(sizeof(SystaliaSurfacePropsV2) == 160,
              "SystaliaSurfacePropsV2 deve ter exatamente 160 bytes.");
static_assert(sizeof(SystaliaSurfacePropsV2) % 16 == 0,
              "Tamanho de SystaliaSurfacePropsV2 deve ser multiplo de 16 bytes.");
static_assert(alignof(SystaliaSurfacePropsV2) == 16,
              "SystaliaSurfacePropsV2 deve ter alinhamento de 16 bytes.");

static_assert(offsetof(SystaliaSurfacePropsV2, matrix) == 0,
              "Offset de matrix deve ser 0.");
static_assert(offsetof(SystaliaSurfacePropsV2, radii) == 64,
              "Offset de radii deve ser 64.");
static_assert(offsetof(SystaliaSurfacePropsV2, windowSize) == 80,
              "Offset de windowSize deve ser 80.");
static_assert(offsetof(SystaliaSurfacePropsV2, screenResolution) == 88,
              "Offset de screenResolution deve ser 88.");
static_assert(offsetof(SystaliaSurfacePropsV2, slant) == 96,
              "Offset de slant deve ser 96.");
static_assert(offsetof(SystaliaSurfacePropsV2, ior) == 100,
              "Offset de ior deve ser 100.");
static_assert(offsetof(SystaliaSurfacePropsV2, chromaticDispersion) == 104,
              "Offset de chromaticDispersion deve ser 104.");
static_assert(offsetof(SystaliaSurfacePropsV2, causticIntensity) == 108,
              "Offset de causticIntensity deve ser 108.");
static_assert(offsetof(SystaliaSurfacePropsV2, surfaceOpacity) == 112,
              "Offset de surfaceOpacity deve ser 112.");
static_assert(offsetof(SystaliaSurfacePropsV2, time) == 116,
              "Offset de time deve ser 116.");
static_assert(offsetof(SystaliaSurfacePropsV2, hasClientTexture) == 120,
              "Offset de hasClientTexture deve ser 120.");
static_assert(offsetof(SystaliaSurfacePropsV2, titleBarHeight) == 124,
              "Offset de titleBarHeight deve ser 124.");
static_assert(offsetof(SystaliaSurfacePropsV2, specularStrokeWidth) == 128,
              "Offset de specularStrokeWidth deve ser 128.");
static_assert(offsetof(SystaliaSurfacePropsV2, geometryMode) == 132,
              "Offset de geometryMode deve ser 132.");
static_assert(offsetof(SystaliaSurfacePropsV2, shadowIntensity) == 136,
              "Offset de shadowIntensity deve ser 136.");
static_assert(offsetof(SystaliaSurfacePropsV2, shadowOffsetY) == 140,
              "Offset de shadowOffsetY deve ser 140.");
static_assert(offsetof(SystaliaSurfacePropsV2, volumetricTint) == 144,
              "Offset de volumetricTint deve ser 144.");

} // namespace Systalia::Gfx::V2

