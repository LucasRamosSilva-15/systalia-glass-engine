#pragma once

#include "WaylibOutputAdapter.hpp"
#include "WaylibSurfaceAdapter.hpp"

#include <wglobal.h>
#include <wseat.h>

#include <QObject>
#include <QPointer>
#include <QString>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WBackend;
class WCursor;
class WInputDevice;
class WOutputRenderWindow;
class WSeat;
class WServer;
class WSocket;
class WXdgShell;
WAYLIB_SERVER_END_NAMESPACE

struct wlr_allocator;
struct wlr_compositor;
struct wlr_renderer;

class QQmlEngine;

namespace Systalia::Compositor::WaylibBackend {

class WaylibCompositorAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(WaylibOutputAdapter *outputAdapter READ outputAdapter CONSTANT)
    Q_PROPERTY(WaylibSurfaceAdapter *surfaceAdapter READ surfaceAdapter CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WSeat *seat READ seat CONSTANT)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString socketName READ socketName NOTIFY socketNameChanged)

public:
    enum class BackendMode {
        NestedWayland,
        Headless
    };
    Q_ENUM(BackendMode)

    static void registerQmlTypes();

    explicit WaylibCompositorAdapter(QObject *parent = nullptr);
    ~WaylibCompositorAdapter() override;

    [[nodiscard]] bool initialize(WAYLIB_SERVER_NAMESPACE::WOutputRenderWindow *window,
                                  QQmlEngine *qmlEngine,
                                  BackendMode mode = BackendMode::NestedWayland,
                                  const QString &requestedSocketName = QString());
    void shutdown();

    [[nodiscard]] bool isRunning() const noexcept { return m_running; }
    [[nodiscard]] QString socketName() const noexcept { return m_socketName; }
    [[nodiscard]] QString lastError() const noexcept { return m_lastError; }
    [[nodiscard]] WaylibOutputAdapter *outputAdapter() const noexcept { return m_outputAdapter; }
    [[nodiscard]] WaylibSurfaceAdapter *surfaceAdapter() const noexcept { return m_surfaceAdapter; }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WSeat *seat() const noexcept { return m_seat.data(); }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WServer *server() const noexcept { return m_server; }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WBackend *backend() const noexcept { return m_backend; }

Q_SIGNALS:
    void runningChanged();
    void socketNameChanged();
    void initialized();
    void stopped();

private:
    void onInputAdded(WAYLIB_SERVER_NAMESPACE::WInputDevice *device);
    void onInputRemoved(WAYLIB_SERVER_NAMESPACE::WInputDevice *device);

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;
    WaylibOutputAdapter *m_outputAdapter = nullptr;
    WaylibSurfaceAdapter *m_surfaceAdapter = nullptr;
    WAYLIB_SERVER_NAMESPACE::WCursor *m_cursor = nullptr;
    QPointer<WAYLIB_SERVER_NAMESPACE::WSeat> m_seat;
    WAYLIB_SERVER_NAMESPACE::WBackend *m_backend = nullptr;
    WAYLIB_SERVER_NAMESPACE::WXdgShell *m_xdgShell = nullptr;
    WAYLIB_SERVER_NAMESPACE::WSocket *m_socket = nullptr;

    wlr_renderer *m_renderer = nullptr;
    wlr_allocator *m_allocator = nullptr;
    wlr_compositor *m_compositor = nullptr;

    bool m_running = false;
    QString m_socketName;
    QString m_lastError;
};

} // namespace Systalia::Compositor::WaylibBackend
