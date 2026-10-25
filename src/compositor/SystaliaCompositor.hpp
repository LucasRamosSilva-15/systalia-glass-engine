#pragma once

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QtWaylandCompositor/QWaylandXdgDecorationManagerV1>
#include "SystaliaSurfaceExtension.hpp"
#include "WindowStackModel.hpp"
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QtQml/qqmlregistration.h>

class QEvent;
class QKeyEvent;
class SystaliaCompositor : public QWaylandCompositor {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(SystaliaSurfaceExtension *surfaceExtension READ surfaceExtension CONSTANT)
    Q_PROPERTY(WindowStackModel *windowStackModel READ windowStackModel CONSTANT)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged)
    Q_PROPERTY(bool taskSwitcherVisible READ taskSwitcherVisible NOTIFY taskSwitcherVisibleChanged)
    Q_PROPERTY(int taskSwitcherIndex READ taskSwitcherIndex NOTIFY taskSwitcherIndexChanged)
    Q_PROPERTY(int windowCount READ windowCount NOTIFY windowCountChanged)
    Q_PROPERTY(QStringList activeAppIds READ activeAppIds NOTIFY activeAppIdsChanged)
    Q_PROPERTY(bool hasMaximizedToplevel READ hasMaximizedToplevel NOTIFY activeToplevelStateChanged)
    Q_PROPERTY(QWaylandXdgShell *xdgShell READ xdgShell CONSTANT)
    Q_PROPERTY(QWaylandXdgDecorationManagerV1 *decorationManager READ decorationManager CONSTANT)

public:
    explicit SystaliaCompositor(QObject *parent = nullptr);
    ~SystaliaCompositor() override;

    SystaliaSurfaceExtension *surfaceExtension() const { return m_surfaceExtension; }
    WindowStackModel *windowStackModel() const { return m_windowStackModel; }
    int clientCount() const { return m_clientCount; }
    bool taskSwitcherVisible() const { return m_windowStackModel->taskSwitcherVisible(); }
    int taskSwitcherIndex() const { return m_windowStackModel->taskSwitcherIndex(); }
    int windowCount() const { return m_windowStackModel->windowCount(); }
    QStringList activeAppIds() const;
    bool hasMaximizedToplevel() const;
    QWaylandXdgShell *xdgShell() const { return m_xdgShell; }
    QWaylandXdgDecorationManagerV1 *decorationManager() const { return m_decorationManager; }

    Q_INVOKABLE void create() override;
    Q_INVOKABLE bool closeToplevel(QObject *toplevelObject);
    Q_INVOKABLE bool activateToplevel(QObject *toplevelObject);
    Q_INVOKABLE bool selectWindow(int index);
    Q_INVOKABLE void attachTaskSwitcherWindow(QQuickWindow *window);
    Q_INVOKABLE bool toggleToplevelMaximized(QObject *toplevelObject,
                                              const QSize &size);
    Q_INVOKABLE void requestToplevelResize(QWaylandXdgToplevel *toplevel,
                                           const QSize &newSize, bool isResizing);

signals:
    void clientCountChanged();
    void taskSwitcherVisibleChanged();
    void taskSwitcherIndexChanged();
    void windowCountChanged();
    void windowSelected(QObject *toplevelObject);
    void activeAppIdsChanged();
    void activeToplevelStateChanged();
    void toplevelCreated(QWaylandXdgToplevel *toplevel);
    void toplevelCreatedForQml(QObject *toplevelObject);
    void popupCreated(QWaylandXdgPopup *popup);

private slots:
    void onSurfaceCreated(QWaylandSurface *surface);
    void onSurfaceAboutToBeDestroyed(QWaylandSurface *surface);
    void onToplevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface);
    void onPopupCreated(QWaylandXdgPopup *popup, QWaylandXdgSurface *xdgSurface);

private:
    void recalculateClientCount();
    bool handleTaskSwitcherKeyEvent(QKeyEvent *event);
    bool eventFilter(QObject *watched, QEvent *event) override;

    SystaliaSurfaceExtension *m_surfaceExtension = nullptr;
    WindowStackModel *m_windowStackModel = nullptr;
    int m_clientCount = 0;
    QSet<QWaylandSurface *> m_activeSurfaces;
    QSet<QWaylandXdgToplevel *> m_activeToplevels;
    QWaylandXdgShell *m_xdgShell = nullptr;
    QWaylandXdgDecorationManagerV1 *m_decorationManager = nullptr;
    QPointer<QQuickWindow> m_taskSwitcherWindow;
    QMetaObject::Connection m_keyboardFocusConnection;
    bool m_altPressWasForwarded = false;
};
