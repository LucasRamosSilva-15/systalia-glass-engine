#include "WaylibSurfaceAdapter.hpp"

#include <wxdgshell.h>
#include <wxdgtoplevelsurface.h>

#include <QDebug>

WAYLIB_SERVER_USE_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

WaylibSurfaceAdapter::WaylibSurfaceAdapter(QObject *parent)
    : QObject(parent) {}

void WaylibSurfaceAdapter::attachXdgShell(WXdgShell *xdgShell) {
    if (!xdgShell) {
        return;
    }
    connect(xdgShell, &WXdgShell::toplevelSurfaceAdded,
            this, &WaylibSurfaceAdapter::onToplevelAdded);
    connect(xdgShell, &WXdgShell::toplevelSurfaceRemoved,
            this, &WaylibSurfaceAdapter::onToplevelRemoved);
}

void WaylibSurfaceAdapter::reset() {
    if (!m_surfaces.isEmpty()) {
        m_surfaces.clear();
        Q_EMIT surfacesChanged();
    }
}

void WaylibSurfaceAdapter::onToplevelAdded(WXdgToplevelSurface *surface) {
    if (!surface || m_surfaces.contains(surface)) {
        return;
    }
    m_surfaces.append(surface);
    surface->setActivate(true);
    connect(surface, &QObject::destroyed, this, [this, surface]() {
        if (m_surfaces.removeOne(surface)) {
            Q_EMIT surfacesChanged();
        }
    });
    qInfo() << "[SGE-WAYLIB] XDG toplevel added:" << surface->title() << "appId=" << surface->appId();
    Q_EMIT surfaceAdded(surface);
    Q_EMIT surfacesChanged();
}

void WaylibSurfaceAdapter::onToplevelRemoved(WXdgToplevelSurface *surface) {
    if (!surface || !m_surfaces.removeOne(surface)) {
        return;
    }
    qInfo() << "[SGE-WAYLIB] XDG toplevel removed:" << surface->title();
    Q_EMIT surfaceRemoved(surface);
    Q_EMIT surfacesChanged();
}

} // namespace Systalia::Compositor::WaylibBackend
