#pragma once

#include "WaylibOutputAdapter.hpp"
#include "WaylibSurfaceAdapter.hpp"

#include <wbackend.h>
#include <wcursor.h>
#include <wglobal.h>
#include <wseat.h>
#include <wserver.h>
#include <wxdgdecorationmanager.h>

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
class WXdgDecorationManager;
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
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WCursor *cursor READ cursor CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WXdgDecorationManager *xdgDecorationManager READ xdgDecorationManager CONSTANT)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString socketName READ socketName NOTIFY socketNameChanged)
    Q_PROPERTY(QString cursorShapeKey READ cursorShapeKey NOTIFY cursorVisualChanged)
    Q_PROPERTY(QString compositorCursorOverride READ compositorCursorOverride WRITE setCompositorCursorOverride NOTIFY compositorCursorOverrideChanged)
    Q_PROPERTY(QString hoveredCursorKey READ hoveredCursorKey WRITE setHoveredCursorKey NOTIFY hoveredCursorKeyChanged)

public:
    enum class BackendMode {
        NestedWayland,
        Headless
    };
    Q_ENUM(BackendMode)

    static void registerQmlTypes();
    static QString cursorShapeKeyForQtShape(Qt::CursorShape shape);
    static QString cursorShapeKeyForWaylibShape(WAYLIB_SERVER_NAMESPACE::WGlobal::CursorShape shape);

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
    [[nodiscard]] WaylibOutputAdapter *outputAdapter() const noexcept { return m_outputAdapter.data(); }
    [[nodiscard]] WaylibSurfaceAdapter *surfaceAdapter() const noexcept { return m_surfaceAdapter.data(); }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WSeat *seat() const noexcept { return m_seat.data(); }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WCursor *cursor() const noexcept { return m_cursor.data(); }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WXdgDecorationManager *xdgDecorationManager() const noexcept {
        return m_xdgDecorationManager.data();
    }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WServer *server() const noexcept { return m_server.data(); }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WBackend *backend() const noexcept { return m_backend.data(); }
    [[nodiscard]] QString cursorShapeKey() const noexcept { return m_cursorShapeKey; }
    [[nodiscard]] QString compositorCursorOverride() const noexcept { return m_compositorCursorOverride; }
    void setCompositorCursorOverride(const QString &overrideKey);
    [[nodiscard]] QString hoveredCursorKey() const noexcept { return m_hoveredCursorKey; }
    void setHoveredCursorKey(const QString &hoveredKey);

Q_SIGNALS:
    void runningChanged();
    void socketNameChanged();
    void initialized();
    void stopped();
    void cursorVisualChanged();
    void compositorCursorOverrideChanged();
    void hoveredCursorKeyChanged();

private:
    void onInputAdded(WAYLIB_SERVER_NAMESPACE::WInputDevice *device);
    void onInputRemoved(WAYLIB_SERVER_NAMESPACE::WInputDevice *device);
    void bindCursorToOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    [[nodiscard]] QString resolveCursorShapeKey() const;
    void updateCursorShapeKey();

    QPointer<WAYLIB_SERVER_NAMESPACE::WServer> m_server;
    QPointer<WaylibOutputAdapter> m_outputAdapter;
    QPointer<WaylibSurfaceAdapter> m_surfaceAdapter;
    QPointer<WAYLIB_SERVER_NAMESPACE::WCursor> m_cursor;
    QPointer<WAYLIB_SERVER_NAMESPACE::WSeat> m_seat;
    QPointer<WAYLIB_SERVER_NAMESPACE::WBackend> m_backend;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgShell> m_xdgShell;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgDecorationManager> m_xdgDecorationManager;
    QPointer<WAYLIB_SERVER_NAMESPACE::WSocket> m_socket;

    wlr_renderer *m_renderer = nullptr;
    wlr_allocator *m_allocator = nullptr;
    wlr_compositor *m_compositor = nullptr;

    bool m_running = false;
    QString m_socketName;
    QString m_lastError;
    QString m_cursorShapeKey = QStringLiteral("default");
    QString m_compositorCursorOverride;
    QString m_hoveredCursorKey;
};

} // namespace Systalia::Compositor::WaylibBackend
