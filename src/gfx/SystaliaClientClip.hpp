#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QVector2D>
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <limits>

inline constexpr float SystaliaClientClipInset = 8.0f;
inline constexpr float SystaliaDefaultTitleBarHeight = 36.0f;

struct SystaliaClientClipGeometry {
    QRectF bounds;
    QVector4D radii;

    static SystaliaClientClipGeometry fromWindow(QSizeF windowSize,
                                                float titleBarHeight,
                                                float inset,
                                                QVector4D outerRadii) {
        const qreal width = std::max<qreal>(0.0, windowSize.width() - 2.0 * inset);
        const qreal height = std::max<qreal>(0.0, windowSize.height() - titleBarHeight - 2.0 * inset);
        const float innerBottomLeft = std::max(0.0f, outerRadii.z() - inset);
        const float innerBottomRight = std::max(0.0f, outerRadii.w() - inset);
        return {
            QRectF(inset, titleBarHeight + inset, width, height),
            QVector4D(0.0f, 0.0f, innerBottomLeft, innerBottomRight)
        };
    }

    QPointF textureCoordinates(QPointF localPosition) const {
        if (bounds.width() <= 0.0 || bounds.height() <= 0.0)
            return QPointF(0.0, 0.0);
        return QPointF((localPosition.x() - bounds.left()) / bounds.width(),
                       (localPosition.y() - bounds.top()) / bounds.height());
    }

    float signedDistance(QPointF localPosition, float slant = 0.0f) const {
        if (bounds.width() <= 0.0 || bounds.height() <= 0.0)
            return std::numeric_limits<float>::infinity();

        const QVector2D halfSize(static_cast<float>(bounds.width() * 0.5),
                                 static_cast<float>(bounds.height() * 0.5));
        const QVector2D position(static_cast<float>(localPosition.x() - bounds.center().x()),
                                 static_cast<float>(localPosition.y() - bounds.center().y()));
        const float slantFactor = 1.0f - (position.y() / std::max(halfSize.y(), 1.0f)) * slant * 0.25f;
        QVector2D slantedPosition(position.x() / std::max(slantFactor, 0.05f), position.y());

        const float radius = selectRadius(slantedPosition, radii);
        const float maxRadius = std::max(0.0f, std::min(halfSize.x(), halfSize.y()) - 0.5f);
        const float clampedRadius = std::clamp(radius, 0.0f, maxRadius);
        const QVector2D q(std::abs(slantedPosition.x()) - halfSize.x() + clampedRadius,
                          std::abs(slantedPosition.y()) - halfSize.y() + clampedRadius);
        const QVector2D positive(std::max(q.x(), 0.0f), std::max(q.y(), 0.0f));
        const float fourthPowerDistance = std::pow(std::pow(positive.x(), 4.0f)
                                                       + std::pow(positive.y(), 4.0f),
                                                   0.25f)
                                          - clampedRadius;
        const float distance = (q.x() > 0.0f && q.y() > 0.0f)
                                   ? fourthPowerDistance
                                   : std::max(q.x(), q.y()) - clampedRadius;
        return distance * slantFactor;
    }

private:
    static float selectRadius(QVector2D position, QVector4D cornerRadii) {
        if (position.x() < 0.0f)
            return position.y() < 0.0f ? cornerRadii.x() : cornerRadii.z();
        return position.y() < 0.0f ? cornerRadii.y() : cornerRadii.w();
    }
};
