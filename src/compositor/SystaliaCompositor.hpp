#pragma once

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QtWaylandCompositor/QWaylandXdgDecorationManagerV1>
#include "SystaliaSurfaceExtension.hpp"
#include <QSet>
#include <QString>
#include <QByteArray>
#include <QtQml/qqmlregistration.h>

class SystaliaCompositor : public QWaylandCompositor {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(SystaliaSurfaceExtension *surfaceExtension READ surfaceExtension CONSTANT)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged)
    Q_PROPERTY(QWaylandXdgShell *xdgShell READ xdgShell CONSTANT)
    Q_PROPERTY(QWaylandXdgDecorationManagerV1 *decorationManager READ decorationManager CONSTANT)

public:
    explicit SystaliaCompositor(QObject *parent = nullptr);
    ~SystaliaCompositor() override;

    SystaliaSurfaceExtension *surfaceExtension() const { return m_surfaceExtension; }
    int clientCount() const { return m_clientCount; }
    QWaylandXdgShell *xdgShell() const { return m_xdgShell; }
    QWaylandXdgDecorationManagerV1 *decorationManager() const { return m_decorationManager; }

    Q_INVOKABLE void create() override;
    Q_INVOKABLE void requestToplevelResize(QWaylandXdgToplevel *toplevel,
                                           const QSize &newSize, bool isResizing);

signals:
    void clientCountChanged();
    void toplevelCreated(QWaylandXdgToplevel *toplevel);
    void popupCreated(QWaylandXdgPopup *popup);

private slots:
    void onSurfaceCreated(QWaylandSurface *surface);
    void onSurfaceAboutToBeDestroyed(QWaylandSurface *surface);
    void onToplevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface);
    void onPopupCreated(QWaylandXdgPopup *popup, QWaylandXdgSurface *xdgSurface);

private:
    void recalculateClientCount();

    SystaliaSurfaceExtension *m_surfaceExtension = nullptr;
    int m_clientCount = 0;
    QSet<QWaylandSurface *> m_activeSurfaces;
    QWaylandXdgShell *m_xdgShell = nullptr;
    QWaylandXdgDecorationManagerV1 *m_decorationManager = nullptr;
};
