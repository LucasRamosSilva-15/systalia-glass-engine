#pragma once

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QSet>
#include <QString>
#include <QByteArray>
#include <QtQml/qqmlregistration.h>

class SystaliaCompositor : public QWaylandCompositor {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged)
    Q_PROPERTY(QWaylandXdgShell *xdgShell READ xdgShell CONSTANT)

public:
    explicit SystaliaCompositor(QObject *parent = nullptr);
    ~SystaliaCompositor() override;

    int clientCount() const { return m_clientCount; }
    QWaylandXdgShell *xdgShell() const { return m_xdgShell; }

    Q_INVOKABLE void create() override;

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

    int m_clientCount = 0;
    QSet<QWaylandSurface *> m_activeSurfaces;
    QWaylandXdgShell *m_xdgShell = nullptr;
};
