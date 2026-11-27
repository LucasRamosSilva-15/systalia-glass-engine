#include "WaylibCompositorAdapter.hpp"

#include "AppRegistryService.hpp"
#include "LiquidTilingService.hpp"
#include "NotificationServer.hpp"
#include "ProcessLauncherService.hpp"
#include "SystemControlService.hpp"
#include "SystemTelemetryService.hpp"
#include "WorkspaceManagerService.hpp"

#include <wbackend.h>
#include <wcursor.h>
#include <wcursorshapemanagerv1.h>
#include <winputdevice.h>
#include <woutput.h>
#include <woutputrenderwindow.h>
#include <wquickcursor.h>
#include <wrenderhelper.h>
#include <wseat.h>
#include <wserver.h>
#include <wsocket.h>
#include <wxdgdecorationmanager.h>
#include <wxdgshell.h>

#include <wlr_all.h>
extern "C" {
#include <wlr/types/wlr_xdg_activation_v1.h>
}

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
    qmlRegisterSingletonType<LiquidTilingService>(
        "Systalia.System", 1, 0, "LiquidTiling",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new LiquidTilingService();
        });
    qmlRegisterSingletonType<AppRegistryService>(
        "Systalia.System", 1, 0, "AppRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new AppRegistryService();
        });
    qmlRegisterSingletonType<NotificationServer>(
        "Systalia.System", 1, 0, "Notifications",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new NotificationServer();
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
    m_surfaceAdapter->attachOutputAdapter(m_outputAdapter);
    connect(m_outputAdapter, &WaylibOutputAdapter::outputReady,
            this, &WaylibCompositorAdapter::bindCursorToOutput);
    connect(m_cursor, &WCursor::cursorChanged,
            this, &WaylibCompositorAdapter::updateCursorShapeKey);
    connect(m_cursor, &WCursor::requestedCursorShapeChanged,
            this, &WaylibCompositorAdapter::updateCursorShapeKey);
    connect(m_cursor, &WCursor::requestedCursorSurfaceChanged,
            this, &WaylibCompositorAdapter::updateCursorShapeKey);
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
    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    m_xdgDecorationManager->setPreferredMode(WXdgDecorationManager::Server);
    if (!m_server->findInterface<WCursorShapeManagerV1>()) {
        m_server->attach<WCursorShapeManagerV1>();
    }
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
    wlr_viewporter_create(m_server->handle());
    wlr_data_device_manager_create(m_server->handle());
    wlr_primary_selection_v1_device_manager_create(m_server->handle());
    wlr_xdg_activation_v1_create(m_server->handle());
    wlr_fractional_scale_manager_v1_create(m_server->handle(), 1);
    wlr_text_input_manager_v3_create(m_server->handle());

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
        if (device->type() == WInputDevice::Type::Pointer) {
            if (WOutput *primary = m_outputAdapter->primaryOutput()) {
                bindCursorToOutput(primary);
            }
        }
    }
}

void WaylibCompositorAdapter::bindCursorToOutput(WOutput *output) {
    if (!output || !m_cursor) {
        return;
    }
    if (!output->cursorList().contains(m_cursor)) {
        output->addCursor(m_cursor);
    }
    if (m_cursor->handle() && output->handle() && m_seat) {
        for (WInputDevice *device : m_seat->deviceList()) {
            if (device && device->type() == WInputDevice::Type::Pointer && device->handle()) {
                wlr_cursor_map_input_to_output(m_cursor->handle(),
                                               device->handle(),
                                               output->handle());
            }
        }
    }
}

void WaylibCompositorAdapter::onInputRemoved(WInputDevice *device) {
    if (m_seat && device) {
        m_seat->detachInputDevice(device);
    }
}

QString WaylibCompositorAdapter::cursorShapeKeyForQtShape(Qt::CursorShape shape) {
    switch (shape) {
    case Qt::PointingHandCursor:
        return QStringLiteral("pointer");
    case Qt::OpenHandCursor:
        return QStringLiteral("grab");
    case Qt::ClosedHandCursor:
        return QStringLiteral("grabbing");
    case Qt::SizeHorCursor:
    case Qt::SplitHCursor:
        return QStringLiteral("resize-ew");
    case Qt::SizeVerCursor:
    case Qt::SplitVCursor:
        return QStringLiteral("resize-ns");
    case Qt::SizeFDiagCursor:
        return QStringLiteral("resize-nwse");
    case Qt::SizeBDiagCursor:
        return QStringLiteral("resize-nesw");
    case Qt::IBeamCursor:
        return QStringLiteral("text");
    case Qt::BusyCursor:
    case Qt::WaitCursor:
        return QStringLiteral("busy");
    case Qt::ForbiddenCursor:
        return QStringLiteral("not-allowed");
    case Qt::ArrowCursor:
    default:
        return QStringLiteral("default");
    }
}

QString WaylibCompositorAdapter::cursorShapeKeyForWaylibShape(WGlobal::CursorShape shape) {
    using CS = WGlobal::CursorShape;
    switch (shape) {
    case CS::Pointer:
    case CS::Hand1:
    case CS::Alias:
    case CS::Copy:
    case CS::ContextMenu:
        return QStringLiteral("pointer");
    case CS::Grab:
    case CS::AllScroll:
        return QStringLiteral("grab");
    case CS::Grabbing:
    case CS::Move:
    case CS::DndAsk:
        return QStringLiteral("grabbing");
    case CS::EWResize:
    case CS::WResize:
    case CS::EResize:
    case CS::LeftSide:
    case CS::RightSide:
    case CS::ColResize:
        return QStringLiteral("resize-ew");
    case CS::NSResize:
    case CS::NResize:
    case CS::SResize:
    case CS::TopSide:
    case CS::BottomSide:
    case CS::RowResize:
        return QStringLiteral("resize-ns");
    case CS::NWSEResize:
    case CS::NWResize:
    case CS::SEResize:
    case CS::TopLeftCorner:
    case CS::BottomRightCorner:
        return QStringLiteral("resize-nwse");
    case CS::NESWResize:
    case CS::NEResize:
    case CS::SWResize:
    case CS::TopRightCorner:
    case CS::BottomLeftCorner:
        return QStringLiteral("resize-nesw");
    case CS::Text:
    case CS::Xterm:
    case CS::VerticalText:
        return QStringLiteral("text");
    case CS::Watch:
    case CS::Wait:
    case CS::Progress:
        return QStringLiteral("busy");
    case CS::NotAllowed:
    case CS::NoDrop:
        return QStringLiteral("not-allowed");
    case CS::Default:
    case CS::Invalid:
    default:
        return QStringLiteral("default");
    }
}

void WaylibCompositorAdapter::setCompositorCursorOverride(const QString &overrideKey) {
    if (m_compositorCursorOverride == overrideKey) {
        return;
    }
    m_compositorCursorOverride = overrideKey;
    Q_EMIT compositorCursorOverrideChanged();
    updateCursorShapeKey();
}

void WaylibCompositorAdapter::setHoveredCursorKey(const QString &hoveredKey) {
    if (m_hoveredCursorKey == hoveredKey) {
        return;
    }
    m_hoveredCursorKey = hoveredKey;
    Q_EMIT hoveredCursorKeyChanged();
    updateCursorShapeKey();
}

QString WaylibCompositorAdapter::resolveCursorShapeKey() const {
    if (!m_compositorCursorOverride.isEmpty()) {
        return m_compositorCursorOverride;
    }
    if (!m_hoveredCursorKey.isEmpty()) {
        return m_hoveredCursorKey;
    }
    if (m_cursor) {
        const Qt::CursorShape qtShape = m_cursor->cursor().shape();
        if (qtShape != Qt::ArrowCursor && qtShape != Qt::CustomCursor) {
            return cursorShapeKeyForQtShape(qtShape);
        }
        if (m_seat && m_seat->pointerFocusSurface() != nullptr) {
            const WGlobal::CursorShape reqShape = m_cursor->requestedCursorShape();
            if (reqShape != WGlobal::CursorShape::Default && reqShape != WGlobal::CursorShape::Invalid) {
                return cursorShapeKeyForWaylibShape(reqShape);
            }
        }
    }
    return QStringLiteral("default");
}

void WaylibCompositorAdapter::updateCursorShapeKey() {
    const QString nextKey = resolveCursorShapeKey();
    if (m_cursorShapeKey == nextKey) {
        return;
    }
    m_cursorShapeKey = nextKey;
    Q_EMIT cursorVisualChanged();
}

} // namespace Systalia::Compositor::WaylibBackend
