#pragma once

#include "ValoStd140Matrix.hpp"

#include <QMatrix4x4>
#include <QVector4D>
#include <QVector2D>
#include <cstddef>
#include <cstring>

/**
 * @brief Bloco de dados uniformes da superfície óptica VALO Glass (128 bytes estritos).
 * Layout alinhado às especificações Vulkan std140 para binding de buffer sem desalinhamento.
 */
struct alignas(16) SystaliaSurfaceProps {
    Matrix4x4_std140 matrix;               // offset 0,  tamanho 64
    QVector4D radii;                       // offset 64, tamanho 16 (TL, TR, BL, BR)
    QVector2D windowSize;                  // offset 80, tamanho 8
    QVector2D screenResolution;            // offset 88, tamanho 8
    float slant = 0.0f;                    // offset 96, tamanho 4
    float ior = 1.25f;                     // offset 100, tamanho 4
    float chromaticDispersion = 0.040f;    // offset 104, tamanho 4
    float causticIntensity = 0.85f;        // offset 108, tamanho 4
    float opacity = 0.85f;                 // offset 112, tamanho 4
    float time = 0.0f;                     // offset 116, tamanho 4 (NOVO: u_Time)
    float hasClientTexture = 0.0f;         // offset 120, tamanho 4
    float titleBarHeight = 36.0f;          // offset 124, tamanho 4
};

static_assert(sizeof(SystaliaSurfaceProps) == 128, "Tamanho deve ser exatamente 128 bytes");
static_assert(sizeof(SystaliaSurfaceProps) % 16 == 0, "Alinhamento deve ser multiplo de 16");
static_assert(alignof(SystaliaSurfaceProps) == 16, "SystaliaSurfaceProps deve ter alinhamento de 16 bytes");
static_assert(offsetof(SystaliaSurfaceProps, matrix) == 0, "Offset de matrix deve ser 0");
static_assert(offsetof(SystaliaSurfaceProps, radii) == 64, "Offset de radii deve ser 64");
static_assert(offsetof(SystaliaSurfaceProps, windowSize) == 80, "Offset de windowSize deve ser 80");
static_assert(offsetof(SystaliaSurfaceProps, screenResolution) == 88, "Offset de screenResolution deve ser 88");
static_assert(offsetof(SystaliaSurfaceProps, slant) == 96, "Offset de slant deve ser 96");
static_assert(offsetof(SystaliaSurfaceProps, ior) == 100, "Offset de ior deve ser 100");
static_assert(offsetof(SystaliaSurfaceProps, chromaticDispersion) == 104, "Offset de chromaticDispersion deve ser 104");
static_assert(offsetof(SystaliaSurfaceProps, causticIntensity) == 108, "Offset de causticIntensity deve ser 108");
static_assert(offsetof(SystaliaSurfaceProps, opacity) == 112, "Offset de opacity deve ser 112");
static_assert(offsetof(SystaliaSurfaceProps, time) == 116, "Offset de time deve ser 116");
static_assert(offsetof(SystaliaSurfaceProps, hasClientTexture) == 120,
              "Offset de hasClientTexture deve ser 120");
static_assert(offsetof(SystaliaSurfaceProps, titleBarHeight) == 124,
              "Offset de titleBarHeight deve ser 124");

// The optical ABI stays 128 bytes; renderer state occupies its own std140 slot.
struct alignas(16) SystaliaRenderUniforms {
    SystaliaSurfaceProps surface;
    float sceneOpacity = 1.0f;
    float padding[3] = {};
};
static_assert(offsetof(SystaliaRenderUniforms, sceneOpacity) == 128);
static_assert(sizeof(SystaliaRenderUniforms) == 144);
