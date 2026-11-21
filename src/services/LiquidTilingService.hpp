#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>
#include <QSizeF>
#include <QTimer>
#include <QVector4D>

class LiquidTilingService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal splitRatio READ splitRatio WRITE setSplitRatio NOTIFY splitRatioChanged FINAL)
    Q_PROPERTY(qreal splitterGap READ splitterGap WRITE setSplitterGap NOTIFY splitterGapChanged FINAL)
    Q_PROPERTY(int previewSnapTarget READ previewSnapTarget NOTIFY previewChanged FINAL)
    Q_PROPERTY(QRectF previewGeometry READ previewGeometry NOTIFY previewChanged FINAL)
    Q_PROPERTY(QVector4D previewRadii READ previewRadii NOTIFY previewChanged FINAL)
    Q_PROPERTY(bool previewVisible READ previewVisible NOTIFY previewChanged FINAL)
    Q_PROPERTY(int tilingRevision READ tilingRevision NOTIFY tilingLayoutChanged FINAL)
    Q_PROPERTY(int targetRefreshRateHz READ targetRefreshRateHz WRITE setTargetRefreshRateHz
               NOTIFY targetRefreshRateHzChanged FINAL)
    Q_PROPERTY(qreal vblankIntervalMs READ vblankIntervalMs NOTIFY targetRefreshRateHzChanged FINAL)

public:
    enum SnapZone {
        SnapNone = 0,
        SnapLeft = 1,
        SnapRight = 2,
        SnapTop = 3,
        SnapTopLeft = 4,
        SnapTopRight = 5,
        SnapBottomLeft = 6,
        SnapBottomRight = 7
    };
    Q_ENUM(SnapZone)

    static constexpr qreal kSnapEdgeThresholdPx = 18.0;
    static constexpr qreal kSnapCornerThresholdPx = 48.0;
    static constexpr qreal kMinSplitRatio = 0.20;
    static constexpr qreal kMaxSplitRatio = 0.80;
    static constexpr qreal kDefaultSplitRatio = 0.50;
    static constexpr qreal kDefaultSplitterGap = 8.0;
    static constexpr qreal kDefaultBaseRadius = 24.0;

    explicit LiquidTilingService(QObject *parent = nullptr);
    ~LiquidTilingService() override;

    static LiquidTilingService *instance() noexcept;

    [[nodiscard]] qreal splitRatio() const noexcept { return m_splitRatio; }
    [[nodiscard]] qreal splitterGap() const noexcept { return m_splitterGap; }
    [[nodiscard]] int previewSnapTarget() const noexcept { return m_previewSnapTarget; }
    [[nodiscard]] QRectF previewGeometry() const noexcept { return m_previewGeometry; }
    [[nodiscard]] QVector4D previewRadii() const noexcept { return m_previewRadii; }
    [[nodiscard]] bool previewVisible() const noexcept { return m_previewVisible; }
    [[nodiscard]] int tilingRevision() const noexcept { return m_tilingRevision; }
    [[nodiscard]] int targetRefreshRateHz() const noexcept { return m_targetRefreshRateHz; }
    [[nodiscard]] qreal vblankIntervalMs() const noexcept {
        return 1000.0 / static_cast<qreal>(std::max(1, m_targetRefreshRateHz));
    }
    [[nodiscard]] int dispatchedConfigureCount() const noexcept { return m_dispatchedConfigureCount; }
    [[nodiscard]] int throttledConfigureCount() const noexcept { return m_throttledConfigureCount; }

    Q_INVOKABLE void setSplitRatio(qreal ratio);
    Q_INVOKABLE void setSplitterGap(qreal gap);
    Q_INVOKABLE void setTargetRefreshRateHz(int refreshRateHz);
    Q_INVOKABLE bool shouldDispatchInteractiveResize(bool interactive = true);
    Q_INVOKABLE void resetConfigureAuditCounters() noexcept;

    Q_INVOKABLE int calculateSnapTarget(const QPointF &cursorPos, const QRectF &screenRect) const;
    Q_INVOKABLE QRectF getSnapGeometry(int snapTarget, const QRectF &availableArea,
                                       qreal splitRatio = kDefaultSplitRatio) const;
    Q_INVOKABLE QVector4D calculateFusedRadii(int snapTarget,
                                              qreal baseRadius = kDefaultBaseRadius) const;
    Q_INVOKABLE bool isValidFusedRadii(const QVector4D &radii) const noexcept;
    Q_INVOKABLE float evaluateSafeG2Distance(const QPointF &localPoint,
                                             const QSizeF &windowSize,
                                             const QVector4D &radii) const noexcept;

    Q_INVOKABLE void updateSnapPreview(const QPointF &cursorPos, const QRectF &screenRect,
                                       const QRectF &availableArea,
                                       qreal baseRadius = kDefaultBaseRadius);
    Q_INVOKABLE void clearSnapPreview();

    Q_INVOKABLE void registerWindowSnap(QQuickItem *window, int snapTarget,
                                        int workspaceIndex = 0);
    Q_INVOKABLE void unregisterWindowSnap(QQuickItem *window);
    Q_INVOKABLE int windowSnapTarget(QQuickItem *window) const;
    Q_INVOKABLE bool hasActiveSplitInWorkspace(int workspaceIndex) const;
    Q_INVOKABLE qreal splitterCenterX(const QRectF &availableArea) const;

    [[nodiscard]] static bool isLeftZone(int snapTarget) noexcept;
    [[nodiscard]] static bool isRightZone(int snapTarget) noexcept;

signals:
    void splitRatioChanged(qreal splitRatio);
    void splitterGapChanged(qreal splitterGap);
    void targetRefreshRateHzChanged(int targetRefreshRateHz);
    void throttledResizeFlushRequested(qreal splitRatio);
    void previewChanged();
    void tilingLayoutChanged();
    void windowSnapChanged(QQuickItem *window, int snapTarget, int workspaceIndex);

private:
    struct WindowSnapEntry {
        int snapTarget = SnapNone;
        int workspaceIndex = 0;
    };

    void unregisterDestroyedWindow(QObject *windowObj);
    void bumpTilingRevision();

    qreal m_splitRatio = kDefaultSplitRatio;
    qreal m_splitterGap = kDefaultSplitterGap;
    int m_targetRefreshRateHz = 60;
    int m_dispatchedConfigureCount = 0;
    int m_throttledConfigureCount = 0;
    QElapsedTimer m_resizeThrottleTimer;
    QTimer m_pendingFlushTimer;
    int m_previewSnapTarget = SnapNone;
    QRectF m_previewGeometry;
    QVector4D m_previewRadii{24.0f, 24.0f, 24.0f, 24.0f};
    bool m_previewVisible = false;
    int m_tilingRevision = 0;
    QHash<QObject *, WindowSnapEntry> m_snappedWindows;
};

