#pragma once

#include <QMatrix4x4>

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

