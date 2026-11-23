#pragma once

#include <wglobal.h>

#include <QList>
#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WXdgShell;
class WXdgToplevelSurface;
WAYLIB_SERVER_END_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

class WaylibSurfaceAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QObject *> surfaces READ surfaces NOTIFY surfacesChanged)
    Q_PROPERTY(int surfaceCount READ surfaceCount NOTIFY surfacesChanged)

public:
    explicit WaylibSurfaceAdapter(QObject *parent = nullptr);
    ~WaylibSurfaceAdapter() override = default;

    void attachXdgShell(WAYLIB_SERVER_NAMESPACE::WXdgShell *xdgShell);
    void reset();

    [[nodiscard]] QList<QObject *> surfaces() const { return m_surfaces; }
    [[nodiscard]] int surfaceCount() const noexcept { return m_surfaces.size(); }

Q_SIGNALS:
    void surfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void surfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void surfacesChanged();

private:
    void onToplevelAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void onToplevelRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);

    QList<QObject *> m_surfaces;
};

} // namespace Systalia::Compositor::WaylibBackend
