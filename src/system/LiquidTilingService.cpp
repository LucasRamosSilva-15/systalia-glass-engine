#include "LiquidTilingService.hpp"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
LiquidTilingService *s_instance = nullptr;
} // namespace

LiquidTilingService::LiquidTilingService(QObject *parent)
    : QObject(parent) {
    s_instance = this;
    m_pendingFlushTimer.setSingleShot(true);
    connect(&m_pendingFlushTimer, &QTimer::timeout, this, [this]() {
        m_resizeThrottleTimer.restart();
        ++m_dispatchedConfigureCount;
        emit throttledResizeFlushRequested(m_splitRatio);
    });
}

LiquidTilingService::~LiquidTilingService() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

LiquidTilingService *LiquidTilingService::instance() noexcept {
    return s_instance;
}

void LiquidTilingService::setSplitRatio(qreal ratio) {
    const qreal clamped = std::clamp(ratio, kMinSplitRatio, kMaxSplitRatio);
    if (qFuzzyCompare(m_splitRatio, clamped)) {
        return;
    }

    m_splitRatio = clamped;
    emit splitRatioChanged(m_splitRatio);
    bumpTilingRevision();
}

void LiquidTilingService::setSplitterGap(qreal gap) {
    const qreal sanitized = std::isfinite(gap) ? std::max<qreal>(0.0, gap) : 0.0;
    if (qFuzzyCompare(1.0 + m_splitterGap, 1.0 + sanitized)) {
        return;
    }

    m_splitterGap = sanitized;
    emit splitterGapChanged(m_splitterGap);
    bumpTilingRevision();
}

void LiquidTilingService::setTargetRefreshRateHz(int refreshRateHz) {
    const int clamped = std::clamp(refreshRateHz, 30, 360);
    if (m_targetRefreshRateHz == clamped) {
        return;
    }
    m_targetRefreshRateHz = clamped;
    emit targetRefreshRateHzChanged(m_targetRefreshRateHz);
}

bool LiquidTilingService::shouldDispatchInteractiveResize(bool interactive) {
    if (!interactive) {
        m_pendingFlushTimer.stop();
        m_resizeThrottleTimer.restart();
        ++m_dispatchedConfigureCount;
        return true;
    }

    const qint64 intervalNs = static_cast<qint64>(vblankIntervalMs() * 1000000.0);
    if (!m_resizeThrottleTimer.isValid() || m_resizeThrottleTimer.nsecsElapsed() >= intervalNs) {
        m_pendingFlushTimer.stop();
        m_resizeThrottleTimer.restart();
        ++m_dispatchedConfigureCount;
        return true;
    }

    ++m_throttledConfigureCount;
    if (!m_pendingFlushTimer.isActive()) {
        const double elapsedMs = static_cast<double>(m_resizeThrottleTimer.nsecsElapsed()) / 1000000.0;
        const int remainingMs = std::max(1, static_cast<int>(std::ceil(vblankIntervalMs() - elapsedMs)));
        m_pendingFlushTimer.start(remainingMs);
    }
    return false;
}

void LiquidTilingService::resetConfigureAuditCounters() noexcept {
    m_pendingFlushTimer.stop();
    m_resizeThrottleTimer.invalidate();
    m_dispatchedConfigureCount = 0;
    m_throttledConfigureCount = 0;
}

int LiquidTilingService::calculateSnapTarget(const QPointF &cursorPos,
                                             const QRectF &screenRect) const {
    if (!screenRect.isValid() || screenRect.width() <= 0.0 || screenRect.height() <= 0.0) {
        return SnapNone;
    }

    const QRectF activationBounds = screenRect.adjusted(
        -kSnapEdgeThresholdPx, -kSnapEdgeThresholdPx,
        kSnapEdgeThresholdPx, kSnapEdgeThresholdPx);
    if (!activationBounds.contains(cursorPos)) {
        return SnapNone;
    }

    const qreal distLeft = std::max<qreal>(0.0, cursorPos.x() - screenRect.left());
    const qreal distRight = std::max<qreal>(0.0, screenRect.right() - cursorPos.x());
    const qreal distTop = std::max<qreal>(0.0, cursorPos.y() - screenRect.top());
    const qreal distBottom = std::max<qreal>(0.0, screenRect.bottom() - cursorPos.y());

    const bool nearLeft = distLeft <= kSnapEdgeThresholdPx;
    const bool nearRight = distRight <= kSnapEdgeThresholdPx;
    const bool nearTop = distTop <= kSnapEdgeThresholdPx;
    const bool nearBottom = distBottom <= kSnapEdgeThresholdPx;

    if ((nearLeft && distTop <= kSnapCornerThresholdPx)
            || (nearTop && distLeft <= kSnapCornerThresholdPx)) {
        return SnapTopLeft;
    }

    if ((nearRight && distTop <= kSnapCornerThresholdPx)
            || (nearTop && distRight <= kSnapCornerThresholdPx)) {
        return SnapTopRight;
    }

    if ((nearLeft && distBottom <= kSnapCornerThresholdPx)
            || (nearBottom && distLeft <= kSnapCornerThresholdPx)) {
        return SnapBottomLeft;
    }

    if ((nearRight && distBottom <= kSnapCornerThresholdPx)
            || (nearBottom && distRight <= kSnapCornerThresholdPx)) {
        return SnapBottomRight;
    }

    if (nearLeft) {
        return SnapLeft;
    }

    if (nearRight) {
        return SnapRight;
    }

    if (nearTop) {
        return SnapTop;
    }

    return SnapNone;
}

QRectF LiquidTilingService::getSnapGeometry(int snapTarget, const QRectF &availableArea,
                                            qreal splitRatio) const {
    if (!availableArea.isValid() || availableArea.width() <= 0.0 || availableArea.height() <= 0.0) {
        return QRectF();
    }

    const qreal clampedRatio = std::clamp(splitRatio, kMinSplitRatio, kMaxSplitRatio);
    const qreal gap = std::max<qreal>(0.0, m_splitterGap);
    const qreal usableWidth = std::max<qreal>(0.0, availableArea.width() - gap);
    const qreal usableHeight = std::max<qreal>(0.0, availableArea.height() - gap);

    const qreal leftWidth = usableWidth * clampedRatio;
    const qreal rightWidth = usableWidth - leftWidth;
    const qreal rightX = availableArea.x() + leftWidth + gap;

    const qreal topHeight = usableHeight * 0.5;
    const qreal bottomHeight = usableHeight - topHeight;
    const qreal bottomY = availableArea.y() + topHeight + gap;

    switch (static_cast<SnapZone>(snapTarget)) {
    case SnapLeft:
        return QRectF(availableArea.x(), availableArea.y(), leftWidth, availableArea.height());
    case SnapRight:
        return QRectF(rightX, availableArea.y(), rightWidth, availableArea.height());
    case SnapTop:
        return QRectF(availableArea.x(), availableArea.y(),
                      availableArea.width(), availableArea.height());
    case SnapTopLeft:
        return QRectF(availableArea.x(), availableArea.y(), leftWidth, topHeight);
    case SnapTopRight:
        return QRectF(rightX, availableArea.y(), rightWidth, topHeight);
    case SnapBottomLeft:
        return QRectF(availableArea.x(), bottomY, leftWidth, bottomHeight);
    case SnapBottomRight:
        return QRectF(rightX, bottomY, rightWidth, bottomHeight);
    case SnapNone:
    default:
        return QRectF();
    }
}

QVector4D LiquidTilingService::calculateFusedRadii(int snapTarget, qreal baseRadius) const {
    const qreal sanitized = std::isfinite(baseRadius)
                            ? std::clamp<qreal>(baseRadius, 0.0, 512.0)
                            : 0.0;
    const float r = static_cast<float>(sanitized);
    switch (static_cast<SnapZone>(snapTarget)) {
    case SnapLeft:
        return QVector4D(r, 0.0f, r, 0.0f);
    case SnapRight:
        return QVector4D(0.0f, r, 0.0f, r);
    case SnapTop:
        return QVector4D(0.0f, 0.0f, 0.0f, 0.0f);
    case SnapTopLeft:
        return QVector4D(r, 0.0f, 0.0f, 0.0f);
    case SnapTopRight:
        return QVector4D(0.0f, r, 0.0f, 0.0f);
    case SnapBottomLeft:
        return QVector4D(0.0f, 0.0f, r, 0.0f);
    case SnapBottomRight:
        return QVector4D(0.0f, 0.0f, 0.0f, r);
    case SnapNone:
    default:
        return QVector4D(r, r, r, r);
    }
}

bool LiquidTilingService::isValidFusedRadii(const QVector4D &radii) const noexcept {
    return std::isfinite(radii.x()) && std::isfinite(radii.y())
           && std::isfinite(radii.z()) && std::isfinite(radii.w())
           && radii.x() >= 0.0f && radii.y() >= 0.0f
           && radii.z() >= 0.0f && radii.w() >= 0.0f;
}

float LiquidTilingService::evaluateSafeG2Distance(const QPointF &localPoint,
                                                  const QSizeF &windowSize,
                                                  const QVector4D &radii) const noexcept {
    if (!windowSize.isValid() || windowSize.width() <= 0.0 || windowSize.height() <= 0.0) {
        return 0.0f;
    }

    const float bx = static_cast<float>(windowSize.width() * 0.5);
    const float by = static_cast<float>(windowSize.height() * 0.5);
    const float px = static_cast<float>(localPoint.x()) - bx;
    const float py = static_cast<float>(localPoint.y()) - by;

    float rawRad = 0.0f;
    if (px < 0.0f) {
        rawRad = (py < 0.0f) ? radii.x() : radii.z();
    } else {
        rawRad = (py < 0.0f) ? radii.y() : radii.w();
    }

    if (!std::isfinite(rawRad) || rawRad < 0.0f) {
        rawRad = 0.0f;
    }

    const float maxRad = std::max(0.0f, std::min(bx, by) - 0.5f);
    const float rad = std::clamp(rawRad, 0.0f, maxRad);

    const float qx = std::abs(px) - bx + rad;
    const float qy = std::abs(py) - by + rad;
    const float qpx = std::max(qx, 0.0f);
    const float qpy = std::max(qy, 0.0f);

    const float l4 = std::pow(qpx, 4.0f) + std::pow(qpy, 4.0f);
    const float cornerDist = (l4 > 1e-12f) ? (std::pow(l4, 0.25f) - rad) : -rad;
    const float dist = (qx > 0.0f && qy > 0.0f) ? cornerDist : (std::max(qx, qy) - rad);
    return std::isfinite(dist) ? dist : 0.0f;
}

void LiquidTilingService::updateSnapPreview(const QPointF &cursorPos, const QRectF &screenRect,
                                            const QRectF &availableArea, qreal baseRadius) {
    const int target = calculateSnapTarget(cursorPos, screenRect);
    if (target == SnapNone) {
        clearSnapPreview();
        return;
    }

    const QRectF nextGeo = getSnapGeometry(target, availableArea, m_splitRatio);
    const QVector4D nextRadii = calculateFusedRadii(target, baseRadius);

    if (m_previewVisible
            && m_previewSnapTarget == target
            && m_previewGeometry == nextGeo
            && m_previewRadii == nextRadii) {
        return;
    }

    m_previewSnapTarget = target;
    m_previewGeometry = nextGeo;
    m_previewRadii = nextRadii;
    m_previewVisible = nextGeo.isValid() && nextGeo.width() > 0.0 && nextGeo.height() > 0.0;
    emit previewChanged();
}

void LiquidTilingService::clearSnapPreview() {
    if (!m_previewVisible && m_previewSnapTarget == SnapNone) {
        return;
    }

    m_previewSnapTarget = SnapNone;
    m_previewVisible = false;
    emit previewChanged();
}

void LiquidTilingService::registerWindowSnap(QQuickItem *window, int snapTarget,
                                             int workspaceIndex) {
    if (!window) {
        return;
    }

    if (snapTarget == SnapNone) {
        unregisterWindowSnap(window);
        return;
    }

    const int sanitizedWs = std::max(0, workspaceIndex);
    const auto existing = m_snappedWindows.constFind(window);
    const bool alreadyTracked = (existing != m_snappedWindows.cend());

    if (alreadyTracked
            && existing->snapTarget == snapTarget
            && existing->workspaceIndex == sanitizedWs) {
        return;
    }

    if (!alreadyTracked) {
        connect(window, &QObject::destroyed, this,
                &LiquidTilingService::unregisterDestroyedWindow, Qt::UniqueConnection);
    }

    m_snappedWindows.insert(window, WindowSnapEntry{snapTarget, sanitizedWs});
    window->setProperty("snapState", snapTarget);
    emit windowSnapChanged(window, snapTarget, sanitizedWs);
    bumpTilingRevision();
}

void LiquidTilingService::unregisterWindowSnap(QQuickItem *window) {
    if (!window) {
        return;
    }

    const auto existing = m_snappedWindows.constFind(window);
    if (existing == m_snappedWindows.cend()) {
        return;
    }

    const int prevWorkspace = existing->workspaceIndex;
    m_snappedWindows.remove(window);
    window->setProperty("snapState", static_cast<int>(SnapNone));
    emit windowSnapChanged(window, SnapNone, prevWorkspace);
    bumpTilingRevision();
}

int LiquidTilingService::windowSnapTarget(QQuickItem *window) const {
    if (!window) {
        return SnapNone;
    }

    const auto it = m_snappedWindows.constFind(window);
    if (it == m_snappedWindows.cend()) {
        return SnapNone;
    }
    return it->snapTarget;
}

bool LiquidTilingService::hasActiveSplitInWorkspace(int workspaceIndex) const {
    bool hasLeft = false;
    bool hasRight = false;

    for (auto it = m_snappedWindows.cbegin(); it != m_snappedWindows.cend(); ++it) {
        if (it->workspaceIndex != workspaceIndex) {
            continue;
        }
        if (isLeftZone(it->snapTarget)) {
            hasLeft = true;
        } else if (isRightZone(it->snapTarget)) {
            hasRight = true;
        }
        if (hasLeft && hasRight) {
            return true;
        }
    }
    return false;
}

qreal LiquidTilingService::splitterCenterX(const QRectF &availableArea) const {
    if (!availableArea.isValid() || availableArea.width() <= 0.0) {
        return 0.0;
    }
    const qreal gap = std::max<qreal>(0.0, m_splitterGap);
    const qreal usableWidth = std::max<qreal>(0.0, availableArea.width() - gap);
    return availableArea.x() + usableWidth * m_splitRatio + gap * 0.5;
}

bool LiquidTilingService::isLeftZone(int snapTarget) noexcept {
    return snapTarget == SnapLeft
           || snapTarget == SnapTopLeft
           || snapTarget == SnapBottomLeft;
}

bool LiquidTilingService::isRightZone(int snapTarget) noexcept {
    return snapTarget == SnapRight
           || snapTarget == SnapTopRight
           || snapTarget == SnapBottomRight;
}

void LiquidTilingService::unregisterDestroyedWindow(QObject *windowObj) {
    if (!windowObj) {
        return;
    }

    if (m_snappedWindows.remove(windowObj) > 0) {
        bumpTilingRevision();
    }
}

void LiquidTilingService::bumpTilingRevision() {
    ++m_tilingRevision;
    emit tilingLayoutChanged();
}

