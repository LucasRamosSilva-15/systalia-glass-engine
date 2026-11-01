#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRect>
#include <QRectF>
#include <QScreen>
#include <QString>
#include <QVariantList>
#include <QVector2D>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandXdgOutputManagerV1>

class OutputManagerService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int screenCount READ screenCount NOTIFY screensChanged FINAL)
    Q_PROPERTY(QVariantList screenGeometries READ screenGeometries NOTIFY screensChanged FINAL)
    Q_PROPERTY(QVariantList screenOutputs READ screenOutputs NOTIFY screensChanged FINAL)
    Q_PROPERTY(qreal primaryScale READ primaryScale WRITE setPrimaryScale
               NOTIFY primaryScaleChanged FINAL)
    Q_PROPERTY(int activeCursorScreen READ activeCursorScreen WRITE setActiveCursorScreen
               NOTIFY activeCursorScreenChanged FINAL)
    Q_PROPERTY(bool replicateTopBarOnAllScreens READ replicateTopBarOnAllScreens
               WRITE setReplicateTopBarOnAllScreens
               NOTIFY replicateTopBarOnAllScreensChanged FINAL)
    Q_PROPERTY(QRect virtualCanvasBounds READ virtualCanvasBounds NOTIFY screensChanged FINAL)

public:
    struct ManagedOutputInfo {
        QString name;
        QRect geometry;
        qreal scale = 1.0;
        int refreshRateMilliHz = 60000;
        QPointer<QScreen> hostScreen;
        QPointer<QWaylandOutput> waylandOutput;
        QPointer<QWaylandXdgOutputV1> xdgOutput;
    };

    explicit OutputManagerService(QObject *parent = nullptr);
    ~OutputManagerService() override;

    static OutputManagerService *instance() noexcept;

    void attachCompositor(QWaylandCompositor *compositor);
    [[nodiscard]] QWaylandXdgOutputManagerV1 *xdgOutputManager() const noexcept {
        return m_xdgOutputManager.data();
    }

    [[nodiscard]] int screenCount() const noexcept { return static_cast<int>(m_outputs.size()); }
    [[nodiscard]] QVariantList screenGeometries() const;
    [[nodiscard]] QVariantList screenOutputs() const;
    [[nodiscard]] qreal primaryScale() const noexcept;
    [[nodiscard]] int activeCursorScreen() const noexcept { return m_activeCursorScreen; }
    [[nodiscard]] bool replicateTopBarOnAllScreens() const noexcept {
        return m_replicateTopBarOnAllScreens;
    }
    [[nodiscard]] QRect virtualCanvasBounds() const noexcept;

    Q_INVOKABLE void setPrimaryScale(qreal scale);
    Q_INVOKABLE void setActiveCursorScreen(int screenIndex);
    Q_INVOKABLE void setReplicateTopBarOnAllScreens(bool replicate);

    Q_INVOKABLE void clearScreens();
    Q_INVOKABLE int addVirtualScreen(const QString &name, const QRect &geometry,
                                     qreal scale = 1.0, int refreshRateMilliHz = 60000);
    Q_INVOKABLE bool removeScreen(int index);
    Q_INVOKABLE bool removeScreenByName(const QString &name);
    Q_INVOKABLE QRectF clampWindowToPrimaryScreen(const QRectF &windowRect) const;
    Q_INVOKABLE void setScreenScale(int index, qreal scale);
    Q_INVOKABLE void syncWithHostScreens();

    Q_INVOKABLE int windowCenterToScreenIndex(const QRectF &windowRect) const;
    Q_INVOKABLE int pointToScreenIndex(const QPointF &globalPoint) const;
    Q_INVOKABLE QRect screenGeometryAt(int index) const;
    Q_INVOKABLE qreal screenScaleAt(int index) const;
    Q_INVOKABLE int screenScale120thsAt(int index) const;
    Q_INVOKABLE QVector2D physicalResolutionAt(int index) const;
    Q_INVOKABLE QVector2D projectBackdropUV(const QPointF &globalPos, int screenIndex) const;

    Q_INVOKABLE int updateWindowOutputRouting(QObject *windowObj, QWaylandSurface *surface,
                                              const QRectF &windowRect);
    Q_INVOKABLE int windowScreenIndex(QObject *windowObj) const;
    Q_INVOKABLE void updateCursorPosition(const QPointF &globalPos);

    [[nodiscard]] static int toFractionalScale120ths(qreal scale) noexcept;
    [[nodiscard]] static QVector2D computePhysicalResolution(const QSize &logicalSize,
                                                             qreal scale) noexcept;

signals:
    void screensChanged();
    void primaryScaleChanged(qreal primaryScale);
    void activeCursorScreenChanged(int screenIndex);
    void replicateTopBarOnAllScreensChanged(bool replicate);
    void windowScreenTransitioned(QObject *windowObj, int newScreenIndex,
                                  int previousScreenIndex, qreal newScale);
    void windowMigratedOnHotUnplug(QObject *windowObj, int removedScreenIndex,
                                   int targetScreenIndex, const QRectF &migratedRect);

private:
    void ensureXdgOutputFor(QWaylandOutput *output);
    void createWaylandOutputForEntry(ManagedOutputInfo &entry);
    void initializeWaylandExtensions();
    void unregisterWindowObject(QObject *obj);

    QPointer<QWaylandCompositor> m_compositor;
    QPointer<QWaylandXdgOutputManagerV1> m_xdgOutputManager;
    QList<ManagedOutputInfo> m_outputs;
    QHash<QWaylandOutput *, QPointer<QWaylandXdgOutputV1>> m_outputXdgMap;
    QHash<QObject *, int> m_windowScreenMap;
    QHash<QObject *, QPointer<QWaylandSurface>> m_windowSurfaceMap;
    int m_activeCursorScreen = 0;
    bool m_replicateTopBarOnAllScreens = false;
};

