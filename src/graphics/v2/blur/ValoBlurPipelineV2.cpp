#include "ValoBlurPipelineV2.hpp"

#include <algorithm>
#include <cmath>
#include <QColor>

namespace Systalia::Gfx::V2 {

namespace {

struct Vec4 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;

    Vec4 operator+(const Vec4 &o) const {
        return {r + o.r, g + o.g, b + o.b, a + o.a};
    }
    Vec4 operator*(float s) const {
        return {r * s, g * s, b * s, a * s};
    }
    Vec4 operator/(float s) const {
        return {r / s, g / s, b / s, a / s};
    }
    Vec4& operator+=(const Vec4 &o) {
        r += o.r; g += o.g; b += o.b; a += o.a;
        return *this;
    }
};

inline Vec4 fetchPixelClamped(const QImage &img, int x, int y) {
    x = std::clamp(x, 0, img.width() - 1);
    y = std::clamp(y, 0, img.height() - 1);
    const QRgb p = img.pixel(x, y);
    return {
        static_cast<float>(qRed(p)),
        static_cast<float>(qGreen(p)),
        static_cast<float>(qBlue(p)),
        static_cast<float>(qAlpha(p))
    };
}

inline Vec4 sampleBilinear(const QImage &img, float x, float y) {
    const float x0 = std::floor(x - 0.5f);
    const float y0 = std::floor(y - 0.5f);
    const float fx = (x - 0.5f) - x0;
    const float fy = (y - 0.5f) - y0;

    const int ix0 = static_cast<int>(x0);
    const int iy0 = static_cast<int>(y0);
    const int ix1 = ix0 + 1;
    const int iy1 = iy0 + 1;

    const Vec4 p00 = fetchPixelClamped(img, ix0, iy0);
    const Vec4 p10 = fetchPixelClamped(img, ix1, iy0);
    const Vec4 p01 = fetchPixelClamped(img, ix0, iy1);
    const Vec4 p11 = fetchPixelClamped(img, ix1, iy1);

    const Vec4 top = p00 * (1.0f - fx) + p10 * fx;
    const Vec4 bot = p01 * (1.0f - fx) + p11 * fx;

    return top * (1.0f - fy) + bot * fy;
}

} // namespace

ValoBlurPipelineV2::ValoBlurPipelineV2(QObject *parent)
    : QObject(parent)
{
}

int ValoBlurPipelineV2::passCountForProfile(BlurProfile profile) {
    switch (profile) {
    case BlurProfile::Performance:
        return 0;
    case BlurProfile::Balanced:
        return 2;
    case BlurProfile::Quality:
    default:
        return 4;
    }
}

QImage ValoBlurPipelineV2::downsampleStepOffline(const QImage &src, float offset) {
    if (src.isNull()) return {};

    const int dstW = std::max(1, src.width() / 2);
    const int dstH = std::max(1, src.height() / 2);

    QImage dst(dstW, dstH, QImage::Format_ARGB32_Premultiplied);

    const float scaleX = static_cast<float>(src.width()) / static_cast<float>(dstW);
    const float scaleY = static_cast<float>(src.height()) / static_cast<float>(dstH);

    const float hpx = offset * 0.75f;
    const float hpy = offset * 0.75f;

    for (int y = 0; y < dstH; ++y) {
        auto *line = reinterpret_cast<QRgb*>(dst.scanLine(y));
        const float py = (y + 0.5f) * scaleY;

        for (int x = 0; x < dstW; ++x) {
            const float px = (x + 0.5f) * scaleX;

            Vec4 sum = sampleBilinear(src, px, py) * 4.0f;
            sum += sampleBilinear(src, px - hpx, py - hpy);
            sum += sampleBilinear(src, px + hpx, py - hpy);
            sum += sampleBilinear(src, px - hpx, py + hpy);
            sum += sampleBilinear(src, px + hpx, py + hpy);

            const Vec4 result = sum * 0.125f;

            const int r = std::clamp(static_cast<int>(std::round(result.r)), 0, 255);
            const int g = std::clamp(static_cast<int>(std::round(result.g)), 0, 255);
            const int b = std::clamp(static_cast<int>(std::round(result.b)), 0, 255);
            const int a = std::clamp(static_cast<int>(std::round(result.a)), 0, 255);

            line[x] = qRgba(r, g, b, a);
        }
    }

    return dst;
}

QImage ValoBlurPipelineV2::upsampleStepOffline(const QImage &src, const QSize &targetSize, float offset) {
    if (src.isNull() || targetSize.isEmpty()) return {};

    const int dstW = targetSize.width();
    const int dstH = targetSize.height();

    QImage dst(dstW, dstH, QImage::Format_ARGB32_Premultiplied);

    const float scaleX = static_cast<float>(src.width()) / static_cast<float>(dstW);
    const float scaleY = static_cast<float>(src.height()) / static_cast<float>(dstH);

    const float hpx = offset * 0.75f;
    const float hpy = offset * 0.75f;

    for (int y = 0; y < dstH; ++y) {
        auto *line = reinterpret_cast<QRgb*>(dst.scanLine(y));
        const float py = (y + 0.5f) * scaleY;

        for (int x = 0; x < dstW; ++x) {
            const float px = (x + 0.5f) * scaleX;

            Vec4 sum = {0.0f, 0.0f, 0.0f, 0.0f};

            // 4 diagonais (peso 2.0 cada)
            sum += sampleBilinear(src, px - hpx, py - hpy) * 2.0f;
            sum += sampleBilinear(src, px + hpx, py - hpy) * 2.0f;
            sum += sampleBilinear(src, px - hpx, py + hpy) * 2.0f;
            sum += sampleBilinear(src, px + hpx, py + hpy) * 2.0f;

            // 4 cardinais (peso 1.0 cada a 2 * hp)
            sum += sampleBilinear(src, px - 2.0f * hpx, py);
            sum += sampleBilinear(src, px + 2.0f * hpx, py);
            sum += sampleBilinear(src, px, py - 2.0f * hpy);
            sum += sampleBilinear(src, px, py + 2.0f * hpy);

            const Vec4 result = sum / 12.0f;

            const int r = std::clamp(static_cast<int>(std::round(result.r)), 0, 255);
            const int g = std::clamp(static_cast<int>(std::round(result.g)), 0, 255);
            const int b = std::clamp(static_cast<int>(std::round(result.b)), 0, 255);
            const int a = std::clamp(static_cast<int>(std::round(result.a)), 0, 255);

            line[x] = qRgba(r, g, b, a);
        }
    }

    return dst;
}

QImage ValoBlurPipelineV2::filterOffline(const QImage &source, BlurProfile profile) {
    if (source.isNull()) return {};

    const int passes = passCountForProfile(profile);
    if (passes <= 0) {
        return source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    // Pirâmide de downsample
    std::vector<QImage> pyramid;
    pyramid.reserve(passes + 1);
    pyramid.push_back(source.convertToFormat(QImage::Format_ARGB32_Premultiplied));

    for (int i = 0; i < passes; ++i) {
        const float offset = 1.0f + static_cast<float>(i) * 0.25f;
        pyramid.push_back(downsampleStepOffline(pyramid.back(), offset));
    }

    // Passos de upsample
    QImage current = pyramid.back();
    for (int i = passes - 1; i >= 0; --i) {
        const float offset = 1.0f + static_cast<float>(i) * 0.25f;
        const QSize targetSize = pyramid[static_cast<size_t>(i)].size();
        current = upsampleStepOffline(current, targetSize, offset);
    }

    return current;
}

float ValoBlurPipelineV2::measureImpulseSpread(const QImage &blurredImpulse) {
    if (blurredImpulse.isNull()) return 0.0f;

    const int cx = blurredImpulse.width() / 2;
    const int cy = blurredImpulse.height() / 2;

    float totalWeight = 0.0f;
    float weightedDistSq = 0.0f;

    for (int y = 0; y < blurredImpulse.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb*>(blurredImpulse.constScanLine(y));
        const float dy = static_cast<float>(y - cy);

        for (int x = 0; x < blurredImpulse.width(); ++x) {
            const float dx = static_cast<float>(x - cx);
            const float lum = static_cast<float>(qRed(line[x]) + qGreen(line[x]) + qBlue(line[x])) / (3.0f * 255.0f);

            if (lum > 0.001f) {
                const float distSq = dx * dx + dy * dy;
                weightedDistSq += distSq * lum;
                totalWeight += lum;
            }
        }
    }

    if (totalWeight <= 0.0001f) return 0.0f;
    return std::sqrt(weightedDistSq / totalWeight);
}

} // namespace Systalia::Gfx::V2
