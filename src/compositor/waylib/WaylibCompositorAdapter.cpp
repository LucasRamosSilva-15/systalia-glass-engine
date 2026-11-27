#include "WaylibCompositorAdapter.hpp"

#include "ProcessLauncherService.hpp"
#include "SystemControlService.hpp"
#include "SystemTelemetryService.hpp"
#include "WorkspaceManagerService.hpp"

#include <wbackend.h>
#include <wcursor.h>
#include <winputdevice.h>
#include <woutputrenderwindow.h>
#include <wquickcursor.h>
#include <wrenderhelper.h>
#include <wseat.h>
#include <wserver.h>
#include <wsocket.h>
#include <wxdgshell.h>

#include <wlr_all.h>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QQmlEngine>

void qml_register_types_Systalia_Engine();

WAYLIB_SERVER_USE_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

void WaylibCompositorAdapter::registerQmlTypes() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    qml_register_types_Systalia_Engine();
    qmlRegisterSingletonType<SystemTelemetryService>(
        "Systalia.System", 1, 0, "SystemTelemetry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new SystemTelemetryService();
        });
    qmlRegisterSingletonType<ProcessLauncherService>(
        "Systalia.System", 1, 0, "ProcessLauncher",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new ProcessLauncherService();
        });
    qmlRegisterSingletonType<SystemControlService>(
        "Systalia.System", 1, 0, "SystemControl",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new SystemControlService();
        });
    qmlRegisterSingletonType<WorkspaceManagerService>(
        "Systalia.System", 1, 0, "WorkspaceManager",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new WorkspaceManagerService();
        });
}

WaylibCompositorAdapter::WaylibCompositorAdapter(QObject *parent)
    : QObject(parent),
      m_server(new WServer(this)),
      m_outputAdapter(new WaylibOutputAdapter(m_server, this)),
      m_surfaceAdapter(new WaylibSurfaceAdapter(this)),
      m_cursor(new WCursor(this)) {
    m_seat = m_server->attach<WSeat>();
    m_cursor->setCursor(QCursor(Qt::ArrowCursor));
    m_cursor->setVisible(true);
    m_seat->setCursor(m_cursor);
    m_cursor->setLayout(m_outputAdapter->outputLayout());
    m_surfaceAdapter->attachSeat(m_seat.data());
}

WaylibCompositorAdapter::~WaylibCompositorAdapter() {
    shutdown();
}

bool WaylibCompositorAdapter::initialize(WOutputRenderWindow *window,
                                         QQmlEngine *qmlEngine,
                                         BackendMode mode,
                                         const QString &requestedSocketName) {
    if (m_running) {
        return true;
    }
    if (!window || !qmlEngine) {
        m_lastError = QStringLiteral("WOutputRenderWindow ou QQmlEngine inválido para inicializar Waylib.");
        return false;
    }

    // Segurança obrigatória (Etapa 03.2.5.1): nunca abrir DRM/KMS físico nem libinput nesta etapa.
    if (mode == BackendMode::Headless) {
        qputenv("WLR_BACKENDS", "headless");
    } else {
        if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
            m_lastError = QStringLiteral("WAYLAND_DISPLAY ausente para modo nested Wayland.");
            return false;
        }
        qputenv("WLR_BACKENDS", "wayland");
    }

    m_backend = m_server->attach<WBackend>();
    m_xdgShell = m_server->attach<WXdgShell>(5);
    m_surfaceAdapter->attachXdgShell(m_xdgShell);

    m_server->start();
    if (!m_server->isRunning() || !m_server->handle()) {
        m_lastError = QStringLiteral("WServer::start() falhou ao criar wl_display.");
        shutdown();
        return false;
    }

    m_socket = new WSocket(false, this);
    bool socketOk = false;
    if (!requestedSocketName.isEmpty()) {
        const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
        const QString fullPath = QFileInfo(requestedSocketName).isAbsolute()
                                     ? requestedSocketName
                                     : (runtimeDir.isEmpty()
                                            ? QDir::tempPath() + QLatin1Char('/') + requestedSocketName
                                            : runtimeDir + QLatin1Char('/') + requestedSocketName);
        socketOk = m_socket->create(fullPath);
    } else {
        socketOk = m_socket->autoCreate();
    }
    if (!socketOk) {
        m_lastError = QStringLiteral("Falha ao criar socket Wayland isolado.");
        shutdown();
        return false;
    }
    m_server->addSocket(m_socket);
    m_socketName = QFileInfo(m_socket->fullServerName()).fileName();
    Q_EMIT socketNameChanged();

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer) {
        m_lastError = QStringLiteral("WRenderHelper::createRenderer() falhou.");
        shutdown();
        return false;
    }

    m_cursor->setEventWindow(window);
    if (m_seat) {
        m_seat->setKeyboardFocusWindow(window);
    }

    m_outputAdapter->attachBackend(m_backend, window, qmlEngine);
    for (WInputDevice *existingDevice : m_backend->inputDeviceList()) {
        onInputAdded(existingDevice);
    }
    connect(m_backend, &WBackend::inputAdded, this, &WaylibCompositorAdapter::onInputAdded);
    connect(m_backend, &WBackend::inputRemoved, this, &WaylibCompositorAdapter::onInputRemoved);

    m_allocator = wlr_allocator_autocreate(m_backend->handle(), m_renderer);
    if (!m_allocator) {
        m_lastError = QStringLiteral("wlr_allocator_autocreate() falhou.");
        shutdown();
        return false;
    }

    wlr_renderer_init_wl_display(m_renderer, m_server->handle());
    m_compositor = wlr_compositor_create(m_server->handle(), 6, m_renderer);
    wlr_subcompositor_create(m_server->handle());

    window->init(m_renderer, m_allocator);
    if (!wlr_backend_start(m_backend->handle())) {
        m_lastError = QStringLiteral("wlr_backend_start() falhou.");
        shutdown();
        return false;
    }

    for (WInputDevice *postStartDevice : m_backend->inputDeviceList()) {
        onInputAdded(postStartDevice);
    }

    m_running = true;
    Q_EMIT runningChanged();
    Q_EMIT initialized();
    qInfo() << "[SGE-WAYLIB] Compositor nested/headless iniciado no socket:" << m_socketName;
    return true;
}

void WaylibCompositorAdapter::shutdown() {
    const bool wasRunning = m_running;
    m_running = false;
    if (m_backend) {
        m_backend->disconnect(this);
        for (WInputDevice *device : m_backend->inputDeviceList()) {
            onInputRemoved(device);
        }
    }
    if (m_seat && m_seat->handle()) {
        m_seat->clearKeyboardFocusWindow();
    }
    if (m_cursor) {
        m_cursor->setEventWindow(nullptr);
    }
    m_surfaceAdapter->reset();
    m_outputAdapter->reset();

    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (!m_socketName.isEmpty()) {
        m_socketName.clear();
        Q_EMIT socketNameChanged();
    }

    if (m_server && m_server->isRunning()) {
        m_server->stop();
    }
    if (m_allocator) {
        wlr_allocator_destroy(m_allocator);
        m_allocator = nullptr;
    }
    if (m_renderer) {
        wlr_renderer_destroy(m_renderer);
        m_renderer = nullptr;
    }
    m_compositor = nullptr;

    if (wasRunning) {
        Q_EMIT runningChanged();
        Q_EMIT stopped();
    }
}

void WaylibCompositorAdapter::onInputAdded(WInputDevice *device) {
    if (m_seat && device && !m_seat->deviceList().contains(device)) {
        m_seat->attachInputDevice(device);
    }
}

void WaylibCompositorAdapter::onInputRemoved(WInputDevice *device) {
    if (m_seat && device) {
        m_seat->detachInputDevice(device);
    }
}

} // namespace Systalia::Compositor::WaylibBackend
