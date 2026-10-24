#pragma once

#include <QMatrix4x4>
#include <QVector4D>
#include <QVector2D>
#include <cstddef>
#include <cstring>

/**
 * @brief Matriz 4x4 compatível com alinhamento e empacotamento std140 (64 bytes).
 */
struct alignas(16) Matrix4x4_std140 {
    float data[16];

    constexpr Matrix4x4_std140() noexcept : data{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    } {}

    Matrix4x4_std140(const QMatrix4x4 &m) noexcept {
        const QMatrix4x4 mt = m.transposed();
        mt.copyDataTo(data);
    }

    Matrix4x4_std140 &operator=(const QMatrix4x4 &m) noexcept {
        const QMatrix4x4 mt = m.transposed();
        mt.copyDataTo(data);
        return *this;
    }

    operator QMatrix4x4() const noexcept {
        return QMatrix4x4(data);
    }
};

static_assert(sizeof(Matrix4x4_std140) == 64, "Matrix4x4_std140 precisa ter 64 bytes");
static_assert(alignof(Matrix4x4_std140) == 16, "Matrix4x4_std140 precisa estar alinhada a 16 bytes");

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
    float ior = 1.15f;                     // offset 100, tamanho 4
    float chromaticDispersion = 0.035f;    // offset 104, tamanho 4
    float causticIntensity = 0.65f;        // offset 108, tamanho 4
    float opacity = 1.0f;                  // offset 112, tamanho 4
    float padding[3] = {0.0f, 0.0f, 0.0f}; // offset 116, tamanho 12
};

static_assert(sizeof(SystaliaSurfaceProps) == 128, "Tamanho deve ser 128 bytes");
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
static_assert(offsetof(SystaliaSurfaceProps, padding) == 116, "Offset de padding deve ser 116");
