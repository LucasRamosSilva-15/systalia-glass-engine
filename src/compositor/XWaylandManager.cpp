#include "XWaylandManager.hpp"
#include "SystaliaCompositor.hpp"

#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-server-core.h>
#endif

#if defined(SGE_HAVE_XCB)
#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/xproto.h>
#endif

// ============================================================================
// XWaylandSurfaceWrapper
// ============================================================================

XWaylandSurfaceWrapper::XWaylandSurfaceWrapper(quint32 x11WindowId, QObject *parent)
    : QObject(parent),
      m_x11WindowId(x11WindowId),
      m_title(QStringLiteral("Janela X11")),
      m_appId(QStringLiteral("x11-legacy")) {
}

void XWaylandSurfaceWrapper::setTitle(const QString &title) {
    const QString cleaned = title.trimmed();
    const QString nextTitle = cleaned.isEmpty() ? QStringLiteral("Janela X11") : cleaned;
    if (m_title == nextTitle) {
        return;
    }
    m_title = nextTitle;
    emit titleChanged();
}

void XWaylandSurfaceWrapper::setAppId(const QString &appId) {
    const QString cleaned = appId.trimmed();
    const QString nextAppId = cleaned.isEmpty() ? QStringLiteral("x11-legacy") : cleaned;
    if (m_appId == nextAppId) {
        return;
    }
    m_appId = nextAppId;
    emit appIdChanged();
}

void XWaylandSurfaceWrapper::setSurface(QWaylandSurface *surface) {
    if (m_surface == surface) {
        return;
    }
    m_surface = surface;
    if (m_surface) {
        const QSize dest = m_surface->destinationSize();
        if (dest.width() > 0 && dest.height() > 0) {
            setWindowGeometry(QRect(0, 0, dest.width(), dest.height()));
        }
        connect(m_surface, &QWaylandSurface::destinationSizeChanged, this, [this]() {
            if (!m_surface) {
                return;
            }
            const QSize s = m_surface->destinationSize();
            if (s.width() > 0 && s.height() > 0) {
                setWindowGeometry(QRect(0, 0, s.width(), s.height()));
            }
        });
    }
    emit surfaceChanged();
}

void XWaylandSurfaceWrapper::setWindowGeometry(const QRect &geometry) {
    if (geometry.width() <= 0 || geometry.height() <= 0) {
        return;
    }
    const QRect normalized(0, 0, geometry.width(), geometry.height());
    if (m_windowGeometry == normalized) {
        return;
    }
    m_windowGeometry = normalized;
    emit windowGeometryChanged();
}

void XWaylandSurfaceWrapper::setMaximized(bool maximized) {
    if (m_maximized == maximized) {
        return;
    }
    m_maximized = maximized;
    emit maximizedChanged();
}

void XWaylandSurfaceWrapper::setFullscreen(bool fullscreen) {
    if (m_fullscreen == fullscreen) {
        return;
    }
    m_fullscreen = fullscreen;
    emit fullscreenChanged();
}

void XWaylandSurfaceWrapper::setMinimized(bool minimized) {
    if (m_minimized == minimized) {
        return;
    }
    m_minimized = minimized;
    emit minimizedChanged();
}

void XWaylandSurfaceWrapper::setSizeLimits(const QSize &minSize, const QSize &maxSize) {
    const QSize nextMin = minSize.expandedTo(QSize(240, 160));
    if (m_minSize == nextMin && m_maxSize == maxSize) {
        return;
    }
    m_minSize = nextMin;
    m_maxSize = maxSize;
    emit sizeLimitsChanged();
}

QString XWaylandSurfaceWrapper::parseWmClassPayload(const QByteArray &rawPayload) {
    if (rawPayload.isEmpty()) {
        return QString();
    }
    const QList<QByteArray> parts = rawPayload.split('\0');
    // WM_CLASS contém "instance\0class\0". Preferimos a classe ou a instância não vazia.
    QString instanceName;
    QString className;
    if (!parts.isEmpty()) {
        instanceName = QString::fromUtf8(parts.at(0)).trimmed();
    }
    if (parts.size() > 1) {
        className = QString::fromUtf8(parts.at(1)).trimmed();
    }
    if (!className.isEmpty()) {
        return className.toLower();
    }
    return instanceName.toLower();
}

// ============================================================================
// XWaylandManager
// ============================================================================

XWaylandManager *XWaylandManager::s_instance = nullptr;

XWaylandManager::XWaylandManager(QObject *parent)
    : QObject(parent),
      m_xwaylandExecutable(QStandardPaths::findExecutable(QStringLiteral("Xwayland"))) {
    if (m_xwaylandExecutable.isEmpty()) {
        m_xwaylandExecutable = QStringLiteral("/usr/bin/Xwayland");
    }
    if (!s_instance) {
        s_instance = this;
    }

    if (auto *guiApp = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        if (QClipboard *clipboard = guiApp->clipboard()) {
            connect(clipboard, &QClipboard::dataChanged, this, [this, clipboard]() {
                if (m_updatingClipboardInternal) {
                    return;
                }
                const QString text = clipboard->text(QClipboard::Clipboard);
                if (!text.isEmpty() && text != m_clipboardText) {
                    syncWaylandToX11Clipboard(text, false);
                }
            });
            connect(clipboard, &QClipboard::selectionChanged, this, [this, clipboard]() {
                if (m_updatingClipboardInternal) {
                    return;
                }
                const QString text = clipboard->text(QClipboard::Selection);
                if (!text.isEmpty() && text != m_primarySelectionText) {
                    syncWaylandToX11Clipboard(text, true);
                }
            });
        }
    }
}

XWaylandManager::~XWaylandManager() {
    stop();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

XWaylandManager *XWaylandManager::instance() noexcept {
    return s_instance;
}

void XWaylandManager::attachCompositor(SystaliaCompositor *compositor) {
    m_compositor = compositor;
}

SystaliaCompositor *XWaylandManager::compositor() const noexcept {
    return m_compositor;
}

bool XWaylandManager::isRunning() const noexcept {
    return m_process && m_process->state() != QProcess::NotRunning;
}

void XWaylandManager::setXwaylandExecutable(const QString &executablePath) {
    const QString trimmed = executablePath.trimmed();
    if (trimmed.isEmpty() || m_xwaylandExecutable == trimmed) {
        return;
    }
    m_xwaylandExecutable = trimmed;
    emit executableChanged();
}

QStringList XWaylandManager::buildCommandLineArguments(const QString &displayCandidate,
                                                        int listenFd,
                                                        int wmFd,
                                                        int displayFd) {
    QString normalizedDisplay = displayCandidate.trimmed();
    if (normalizedDisplay.isEmpty()) {
        normalizedDisplay = QStringLiteral(":1");
    } else if (!normalizedDisplay.startsWith(QLatin1Char(':'))) {
        normalizedDisplay.prepend(QLatin1Char(':'));
    }

    return QStringList{
        normalizedDisplay,
        QStringLiteral("-rootless"),
        QStringLiteral("-terminate"),
        QStringLiteral("-core"),
        QStringLiteral("-listenfd"),
        QString::number(listenFd),
        QStringLiteral("-wm"),
        QString::number(wmFd),
        QStringLiteral("-displayfd"),
        QString::number(displayFd)
    };
}

QString XWaylandManager::parseDisplayFdPayload(const QByteArray &payload, bool *ok) {
    if (ok) {
        *ok = false;
    }
    QByteArray line = payload;
    const qsizetype newlinePos = line.indexOf('\n');
    if (newlinePos >= 0) {
        line.truncate(newlinePos);
    }
    line = line.trimmed();
    if (line.startsWith(':')) {
        line.remove(0, 1);
    }
    if (line.isEmpty()) {
        return QString();
    }

    bool numericOk = false;
    const int displayNum = line.toInt(&numericOk);
    if (!numericOk || displayNum < 0 || displayNum > 65535) {
        return QString();
    }

    if (ok) {
        *ok = true;
    }
    return QStringLiteral(":%1").arg(displayNum);
}

int XWaylandManager::findAvailableDisplayNumber(int preferredDisplay) {
    const int startDisplay = std::max(0, preferredDisplay);
    for (int candidate = startDisplay; candidate < startDisplay + 64; ++candidate) {
        const QString socketPath = QStringLiteral("/tmp/.X11-unix/X%1").arg(candidate);
        const QString lockPath = QStringLiteral("/tmp/.X%1-lock").arg(candidate);
        if (!QFileInfo::exists(socketPath) && !QFileInfo::exists(lockPath)) {
            return candidate;
        }
    }
    return startDisplay;
}

void XWaylandManager::closeFdSafely(int &fd) noexcept {
#if defined(Q_OS_UNIX)
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
#else
    fd = -1;
#endif
}

void XWaylandManager::cleanupListenSocketFile() noexcept {
    if (!m_listenSocketPath.isEmpty()) {
        QFile::remove(m_listenSocketPath);
        m_listenSocketPath.clear();
    }
}

void XWaylandManager::closeAllFileDescriptors() noexcept {
    if (m_displayFdNotifier) {
        m_displayFdNotifier->setEnabled(false);
        delete m_displayFdNotifier;
        m_displayFdNotifier = nullptr;
    }
    closeFdSafely(m_waylandServerFd);
    closeFdSafely(m_waylandClientFd);
    closeFdSafely(m_wmMasterFd);
    closeFdSafely(m_wmClientFd);
    closeFdSafely(m_displayReadFd);
    closeFdSafely(m_displayWriteFd);
    closeFdSafely(m_x11ListenFd);
    cleanupListenSocketFile();
}

bool XWaylandManager::createSocketPairs(int displayNumber) {
#if defined(Q_OS_UNIX)
    closeAllFileDescriptors();

    int wlPair[2] = {-1, -1};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, wlPair) != 0) {
        return false;
    }
    m_waylandServerFd = wlPair[0];
    m_waylandClientFd = wlPair[1];
    ::fcntl(m_waylandServerFd, F_SETFD, FD_CLOEXEC);

    int wmPair[2] = {-1, -1};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, wmPair) != 0) {
        closeAllFileDescriptors();
        return false;
    }
    m_wmMasterFd = wmPair[0];
    m_wmClientFd = wmPair[1];
    ::fcntl(m_wmMasterFd, F_SETFD, FD_CLOEXEC);

    int displayPair[2] = {-1, -1};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, displayPair) != 0) {
        closeAllFileDescriptors();
        return false;
    }
    m_displayReadFd = displayPair[0];
    m_displayWriteFd = displayPair[1];
    ::fcntl(m_displayReadFd, F_SETFD, FD_CLOEXEC);
    ::fcntl(m_displayReadFd, F_SETFL, ::fcntl(m_displayReadFd, F_GETFL, 0) | O_NONBLOCK);

    // Criação do socket de escuta X11 (-listenfd)
    const int listenSock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenSock < 0) {
        closeAllFileDescriptors();
        return false;
    }

    bool boundSuccessfully = false;
    if (displayNumber >= 0) {
        QDir().mkpath(QStringLiteral("/tmp/.X11-unix"));
        const QString candidatePath = QStringLiteral("/tmp/.X11-unix/X%1").arg(displayNumber);
        const QByteArray pathBytes = candidatePath.toUtf8();

        if (!QFileInfo::exists(candidatePath)
            && static_cast<size_t>(pathBytes.size()) < sizeof(sockaddr_un::sun_path)) {
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::memcpy(addr.sun_path, pathBytes.constData(), static_cast<size_t>(pathBytes.size()));
            if (::bind(listenSock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0
                && ::listen(listenSock, 128) == 0) {
                boundSuccessfully = true;
                m_listenSocketPath = candidatePath;
            }
        }
    }

    if (!boundSuccessfully) {
        sockaddr_un autoAddr{};
        autoAddr.sun_family = AF_UNIX;
        if (::bind(listenSock, reinterpret_cast<const sockaddr *>(&autoAddr), sizeof(sa_family_t)) == 0
            && ::listen(listenSock, 128) == 0) {
            boundSuccessfully = true;
        }
    }

    if (!boundSuccessfully) {
        ::close(listenSock);
        closeAllFileDescriptors();
        return false;
    }

    m_x11ListenFd = listenSock;
    return true;
#else
    Q_UNUSED(displayNumber);
    return false;
#endif
}

bool XWaylandManager::attachDisplayFdReadDescriptor(int readFd) {
#if defined(Q_OS_UNIX)
    if (readFd < 0) {
        return false;
    }
    if (m_displayFdNotifier) {
        m_displayFdNotifier->setEnabled(false);
        delete m_displayFdNotifier;
        m_displayFdNotifier = nullptr;
    }
    if (m_displayReadFd >= 0 && m_displayReadFd != readFd) {
        closeFdSafely(m_displayReadFd);
    }

    m_displayReadFd = readFd;
    m_displayFdBuffer.clear();
    ::fcntl(m_displayReadFd, F_SETFL, ::fcntl(m_displayReadFd, F_GETFL, 0) | O_NONBLOCK);

    m_displayFdNotifier = new QSocketNotifier(m_displayReadFd, QSocketNotifier::Read, this);
    connect(m_displayFdNotifier, &QSocketNotifier::activated,
            this, &XWaylandManager::onDisplayFdReadyRead);
    return true;
#else
    Q_UNUSED(readFd);
    return false;
#endif
}

void XWaylandManager::onDisplayFdReadyRead() {
#if defined(Q_OS_UNIX)
    if (m_displayReadFd < 0) {
        return;
    }

    char chunk[128] = {};
    const ssize_t bytesRead = ::read(m_displayReadFd, chunk, sizeof(chunk));
    if (bytesRead <= 0) {
        if (bytesRead == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            if (m_displayFdNotifier) {
                m_displayFdNotifier->setEnabled(false);
            }
        }
        return;
    }

    m_displayFdBuffer.append(chunk, static_cast<qsizetype>(bytesRead));
    if (!m_displayFdBuffer.contains('\n')) {
        return;
    }

    bool ok = false;
    const QString parsedDisplay = parseDisplayFdPayload(m_displayFdBuffer, &ok);
    if (!ok || parsedDisplay.isEmpty()) {
        qWarning().noquote() << "[SGE-XWAYLAND] Payload inválido recebido em -displayfd:"
                             << QString::fromUtf8(m_displayFdBuffer).trimmed();
        return;
    }

    if (m_displayFdNotifier) {
        m_displayFdNotifier->setEnabled(false);
    }
    closeFdSafely(m_displayReadFd);

    m_displayName = parsedDisplay;
    m_displayNumber = m_displayName.mid(1).toInt();
    m_ready = true;

    // Exporta DISPLAY=:<num> globalmente no processo do compositor
    qputenv("DISPLAY", m_displayName.toUtf8());

    qInfo().noquote() << "[SGE-XWAYLAND] Handshake -displayfd concluído. Display ativo:"
                      << m_displayName;

    if (m_wmMasterFd >= 0) {
        initializeXcbWindowManager();
    }

    emit displayReady(m_displayName);
    emit runningChanged();
#endif
}

bool XWaylandManager::start(int preferredDisplay) {
#if defined(Q_OS_UNIX)
    if (isRunning()) {
        return true;
    }

    m_stopping = false;
    m_ready = false;

    const int chosenDisplay = (preferredDisplay >= 0)
        ? preferredDisplay
        : findAvailableDisplayNumber(1);
    m_displayNumber = chosenDisplay;
    m_displayName = QStringLiteral(":%1").arg(chosenDisplay);

    if (!createSocketPairs(chosenDisplay)) {
        emit processFailed(QStringLiteral("Falha ao criar pares de sockets Unix para XWayland."));
        return false;
    }

    // Regista o cliente Wayland dedicado para o XWayland no compositor se disponível
    if (m_compositor && m_compositor->display() && m_waylandServerFd >= 0) {
        m_waylandClient = ::wl_client_create(m_compositor->display(), m_waylandServerFd);
        if (m_waylandClient) {
            // wl_client_create assume a posse do descritor m_waylandServerFd
            m_waylandServerFd = -1;
        }
    }

    m_lastArguments = buildCommandLineArguments(
        m_displayName, m_x11ListenFd, m_wmClientFd, m_displayWriteFd);

    attachDisplayFdReadDescriptor(m_displayReadFd);

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::finished,
                this, &XWaylandManager::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred,
                this, &XWaylandManager::onProcessErrorOccurred);
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(m_waylandClientFd));
    if (m_compositor && !m_compositor->socketName().isEmpty()) {
        env.insert(QStringLiteral("WAYLAND_DISPLAY"),
                   QString::fromUtf8(m_compositor->socketName()));
    }
    m_process->setProcessEnvironment(env);
    m_process->setProgram(m_xwaylandExecutable);
    m_process->setArguments(m_lastArguments);

    const int childWaylandFd = m_waylandClientFd;
    const int childListenFd = m_x11ListenFd;
    const int childWmFd = m_wmClientFd;
    const int childDisplayFd = m_displayWriteFd;

    m_process->setChildProcessModifier([childWaylandFd, childListenFd, childWmFd, childDisplayFd]() {
        const auto clearCloExec = [](int fd) {
            if (fd >= 0) {
                const int flags = ::fcntl(fd, F_GETFD);
                if (flags >= 0) {
                    ::fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
                }
            }
        };
        clearCloExec(childWaylandFd);
        clearCloExec(childListenFd);
        clearCloExec(childWmFd);
        clearCloExec(childDisplayFd);
    });

    qInfo().noquote() << "[SGE-XWAYLAND] Iniciando processo:" << m_xwaylandExecutable
                      << m_lastArguments.join(QLatin1Char(' '));

    m_process->start();
    if (!m_process->waitForStarted(3000)) {
        const QString err = m_process->errorString();
        qWarning().noquote() << "[SGE-XWAYLAND] Falha ao iniciar Xwayland:" << err;
        closeAllFileDescriptors();
        emit processFailed(err);
        return false;
    }

    // Fecha as pontas exclusivas do processo filho no processo pai
    closeFdSafely(m_waylandClientFd);
    closeFdSafely(m_wmClientFd);
    closeFdSafely(m_displayWriteFd);

    emit runningChanged();
    return true;
#else
    Q_UNUSED(preferredDisplay);
    return false;
#endif
}

void XWaylandManager::stop() {
    m_stopping = true;
    m_ready = false;

    teardownXcbWindowManager();

    // Limpa janelas X11 ativas
    const QList<quint32> xids = m_windowsByXid.keys();
    for (quint32 xid : xids) {
        unregisterX11Window(xid);
    }
    m_unassignedXWaylandSurfaces.clear();

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(1500)) {
                m_process->kill();
                m_process->waitForFinished(500);
            }
        }
    }

    m_waylandClient = nullptr;
    closeAllFileDescriptors();
    emit runningChanged();
}

void XWaylandManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qInfo().noquote() << "[SGE-XWAYLAND] Processo Xwayland finalizado. Código:" << exitCode
                      << "| Status:" << (exitStatus == QProcess::CrashExit ? "CrashExit" : "NormalExit");

    m_ready = false;
    m_waylandClient = nullptr;
    teardownXcbWindowManager();
    closeAllFileDescriptors();
    emit runningChanged();

    // Recuperação automática em caso de encerramento inesperado
    if (!m_stopping && m_autoRestartEnabled && exitStatus == QProcess::CrashExit
        && m_crashRestartCount < 5) {
        ++m_crashRestartCount;
        emit processRestarted(m_crashRestartCount);
        QTimer::singleShot(250, this, [this]() {
            if (!m_stopping) {
                start(m_displayNumber);
            }
        });
    }
}

void XWaylandManager::onProcessErrorOccurred(QProcess::ProcessError error) {
    if (m_stopping) {
        return;
    }
    qWarning().noquote() << "[SGE-XWAYLAND] Erro no processo Xwayland (" << error << "):"
                         << (m_process ? m_process->errorString() : QString());
}

// ============================================================================
// Gestão de Janelas X11 (XCB) e Mapeamento para WaylandWindowContainer.qml
// ============================================================================

bool XWaylandManager::initializeXcbWindowManager() {
#if defined(SGE_HAVE_XCB)
    teardownXcbWindowManager();

    if (m_wmMasterFd < 0) {
        return false;
    }

    m_xcbConn = ::xcb_connect_to_fd(m_wmMasterFd, nullptr);
    if (!m_xcbConn || ::xcb_connection_has_error(m_xcbConn) != 0) {
        qWarning().noquote() << "[SGE-XWAYLAND] Falha ao estabelecer conexão XCB no descritor -wm.";
        if (m_xcbConn) {
            ::xcb_disconnect(m_xcbConn);
            m_xcbConn = nullptr;
        }
        m_wmMasterFd = -1;
        return false;
    }
    // xcb_connect_to_fd passa a gerir o ciclo de vida do descritor m_wmMasterFd
    m_wmMasterFd = -1;

    const xcb_setup_t *setup = ::xcb_get_setup(m_xcbConn);
    if (!setup) {
        teardownXcbWindowManager();
        return false;
    }
    xcb_screen_iterator_t iter = ::xcb_setup_roots_iterator(setup);
    if (!iter.data) {
        teardownXcbWindowManager();
        return false;
    }
    xcb_screen_t *screen = iter.data;
    m_rootWindow = screen->root;

    const auto internAtom = [this](const char *name) -> quint32 {
        xcb_intern_atom_cookie_t cookie = ::xcb_intern_atom(
            m_xcbConn, 0, static_cast<uint16_t>(std::strlen(name)), name);
        xcb_intern_atom_reply_t *reply = ::xcb_intern_atom_reply(m_xcbConn, cookie, nullptr);
        if (!reply) {
            return 0;
        }
        const quint32 atom = reply->atom;
        std::free(reply);
        return atom;
    };

    m_atomWmS0 = internAtom("WM_S0");
    m_atomWlSurfaceId = internAtom("WL_SURFACE_ID");
    m_atomWlSurfaceSerial = internAtom("WL_SURFACE_SERIAL");
    m_atomWmProtocols = internAtom("WM_PROTOCOLS");
    m_atomWmDeleteWindow = internAtom("WM_DELETE_WINDOW");
    m_atomWmTakeFocus = internAtom("WM_TAKE_FOCUS");
    m_atomWmState = internAtom("WM_STATE");
    m_atomWmChangeState = internAtom("WM_CHANGE_STATE");
    m_atomWmName = internAtom("WM_NAME");
    m_atomWmClass = internAtom("WM_CLASS");
    m_atomNetWmName = internAtom("_NET_WM_NAME");
    m_atomNetWmState = internAtom("_NET_WM_STATE");
    m_atomNetWmStateMaximizedVert = internAtom("_NET_WM_STATE_MAXIMIZED_VERT");
    m_atomNetWmStateMaximizedHorz = internAtom("_NET_WM_STATE_MAXIMIZED_HORZ");
    m_atomNetWmStateFullscreen = internAtom("_NET_WM_STATE_FULLSCREEN");
    m_atomNetWmStateHidden = internAtom("_NET_WM_STATE_HIDDEN");
    m_atomNetSupportingWmCheck = internAtom("_NET_SUPPORTING_WM_CHECK");
    m_atomNetSupported = internAtom("_NET_SUPPORTED");
    m_atomNetActiveWindow = internAtom("_NET_ACTIVE_WINDOW");
    m_atomUtf8String = internAtom("UTF8_STRING");
    m_atomClipboard = internAtom("CLIPBOARD");
    m_atomPrimary = internAtom("PRIMARY");
    m_atomTargets = internAtom("TARGETS");
    m_atomText = internAtom("TEXT");
    m_atomString = internAtom("STRING");
    m_atomSgeSelectionProperty = internAtom("_SGE_XWAYLAND_SELECTION");

    // Redirecionamento Composite Manual na janela raiz (obrigatório para XWayland rootless)
    ::xcb_composite_redirect_subwindows(
        m_xcbConn, m_rootWindow, XCB_COMPOSITE_REDIRECT_MANUAL);

    // SubstructureRedirect + SubstructureNotify + PropertyChange na raiz
    const uint32_t rootValues[1] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_PROPERTY_CHANGE
    };
    ::xcb_change_window_attributes(
        m_xcbConn, m_rootWindow, XCB_CW_EVENT_MASK, rootValues);

    // Janela auxiliar do WM para seleção WM_S0, EWMH e Clipboard Bridge
    m_wmWindow = ::xcb_generate_id(m_xcbConn);
    ::xcb_create_window(
        m_xcbConn, XCB_COPY_FROM_PARENT, m_wmWindow, m_rootWindow,
        0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
        screen->root_visual, 0, nullptr);

    const char wmTitle[] = "Systalia XWayland WM";
    ::xcb_change_property(
        m_xcbConn, XCB_PROP_MODE_REPLACE, m_wmWindow, m_atomNetWmName,
        m_atomUtf8String, 8, static_cast<uint32_t>(sizeof(wmTitle) - 1), wmTitle);
    ::xcb_change_property(
        m_xcbConn, XCB_PROP_MODE_REPLACE, m_wmWindow, m_atomNetSupportingWmCheck,
        XCB_ATOM_WINDOW, 32, 1, &m_wmWindow);
    ::xcb_change_property(
        m_xcbConn, XCB_PROP_MODE_REPLACE, m_rootWindow, m_atomNetSupportingWmCheck,
        XCB_ATOM_WINDOW, 32, 1, &m_wmWindow);

    const uint32_t supportedAtoms[] = {
        m_atomNetWmName,
        m_atomNetWmState,
        m_atomNetWmStateMaximizedVert,
        m_atomNetWmStateMaximizedHorz,
        m_atomNetWmStateFullscreen,
        m_atomNetWmStateHidden,
        m_atomNetActiveWindow
    };
    ::xcb_change_property(
        m_xcbConn, XCB_PROP_MODE_REPLACE, m_rootWindow, m_atomNetSupported,
        XCB_ATOM_ATOM, 32, static_cast<uint32_t>(std::size(supportedAtoms)), supportedAtoms);

    // Adquire posse de WM_S0 sinalizando ao XWayland que o WM está pronto
    ::xcb_set_selection_owner(m_xcbConn, m_wmWindow, m_atomWmS0, XCB_CURRENT_TIME);

    // Inicializa XFixes para monitorizar alterações de CLIPBOARD e PRIMARY no X11
    const xcb_query_extension_reply_t *xfixesExt =
        ::xcb_get_extension_data(m_xcbConn, &xcb_xfixes_id);
    if (xfixesExt && xfixesExt->present) {
        m_xfixesFirstEvent = xfixesExt->first_event;
        xcb_xfixes_query_version_cookie_t xfCookie =
            ::xcb_xfixes_query_version(m_xcbConn, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
        xcb_xfixes_query_version_reply_t *xfReply =
            ::xcb_xfixes_query_version_reply(m_xcbConn, xfCookie, nullptr);
        std::free(xfReply);

        const uint32_t mask = XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER;
        ::xcb_xfixes_select_selection_input(m_xcbConn, m_wmWindow, m_atomClipboard, mask);
        ::xcb_xfixes_select_selection_input(m_xcbConn, m_wmWindow, m_atomPrimary, mask);
    }

    ::xcb_flush(m_xcbConn);

    const int xcbFd = ::xcb_get_file_descriptor(m_xcbConn);
    if (xcbFd >= 0) {
        m_xcbNotifier = new QSocketNotifier(xcbFd, QSocketNotifier::Read, this);
        connect(m_xcbNotifier, &QSocketNotifier::activated,
                this, &XWaylandManager::pollXcbEvents);
    }

    qInfo().noquote() << "[SGE-XWAYLAND] Window Manager XCB inicializado (CompositeRedirectManual + WM_S0).";
    return true;
#else
    return false;
#endif
}

void XWaylandManager::teardownXcbWindowManager() noexcept {
    if (m_xcbNotifier) {
        m_xcbNotifier->setEnabled(false);
        delete m_xcbNotifier;
        m_xcbNotifier = nullptr;
    }
#if defined(SGE_HAVE_XCB)
    if (m_xcbConn) {
        ::xcb_disconnect(m_xcbConn);
        m_xcbConn = nullptr;
    }
#endif
    m_rootWindow = 0;
    m_wmWindow = 0;
    m_xfixesFirstEvent = 0;
}

void XWaylandManager::queryX11WindowProperties(quint32 xid, XWaylandSurfaceWrapper *wrapper) {
#if defined(SGE_HAVE_XCB)
    if (!m_xcbConn || !wrapper || xid == 0) {
        return;
    }

    // 1. Consulta _NET_WM_NAME (UTF8_STRING) e fallback para WM_NAME
    auto readStringProp = [this, xid](quint32 propAtom, quint32 typeAtom) -> QByteArray {
        xcb_get_property_cookie_t cookie = ::xcb_get_property(
            m_xcbConn, 0, xid, propAtom, typeAtom, 0, 1024);
        xcb_get_property_reply_t *reply = ::xcb_get_property_reply(m_xcbConn, cookie, nullptr);
        if (!reply) {
            return {};
        }
        QByteArray data(
             static_cast<const char *>(::xcb_get_property_value(reply)),
             ::xcb_get_property_value_length(reply));
        std::free(reply);
        return data;
    };

    QByteArray netWmName = readStringProp(m_atomNetWmName, m_atomUtf8String);
    if (netWmName.isEmpty()) {
        netWmName = readStringProp(m_atomNetWmName, XCB_ATOM_ANY);
    }
    if (!netWmName.isEmpty()) {
        wrapper->setTitle(QString::fromUtf8(netWmName));
    } else {
        const QByteArray wmName = readStringProp(m_atomWmName, XCB_ATOM_ANY);
        if (!wmName.isEmpty()) {
            wrapper->setTitle(QString::fromUtf8(wmName));
        }
    }

    // 2. Consulta WM_CLASS
    const QByteArray wmClassRaw = readStringProp(m_atomWmClass, XCB_ATOM_STRING);
    const QString parsedAppId = XWaylandSurfaceWrapper::parseWmClassPayload(wmClassRaw);
    if (!parsedAppId.isEmpty()) {
        wrapper->setAppId(parsedAppId);
    }

    // 3. Consulta WM_PROTOCOLS (verifica suporte a WM_DELETE_WINDOW)
    xcb_get_property_cookie_t protoCookie = ::xcb_get_property(
        m_xcbConn, 0, xid, m_atomWmProtocols, XCB_ATOM_ATOM, 0, 64);
    if (xcb_get_property_reply_t *protoReply =
            ::xcb_get_property_reply(m_xcbConn, protoCookie, nullptr)) {
        const auto *atoms = static_cast<const uint32_t *>(::xcb_get_property_value(protoReply));
        const int count = ::xcb_get_property_value_length(protoReply) / static_cast<int>(sizeof(uint32_t));
        bool supportsDelete = false;
        for (int i = 0; i < count; ++i) {
            if (atoms[i] == m_atomWmDeleteWindow) {
                supportsDelete = true;
                break;
            }
        }
        wrapper->setSupportsDeleteWindow(supportsDelete);
        std::free(protoReply);
    }

    // 4. Consulta _NET_WM_STATE
    xcb_get_property_cookie_t stateCookie = ::xcb_get_property(
        m_xcbConn, 0, xid, m_atomNetWmState, XCB_ATOM_ATOM, 0, 64);
    if (xcb_get_property_reply_t *stateReply =
            ::xcb_get_property_reply(m_xcbConn, stateCookie, nullptr)) {
        const auto *atoms = static_cast<const uint32_t *>(::xcb_get_property_value(stateReply));
        const int count = ::xcb_get_property_value_length(stateReply) / static_cast<int>(sizeof(uint32_t));
        bool maxH = false;
        bool maxV = false;
        bool full = false;
        bool hidden = false;
        for (int i = 0; i < count; ++i) {
            if (atoms[i] == m_atomNetWmStateMaximizedHorz) maxH = true;
            if (atoms[i] == m_atomNetWmStateMaximizedVert) maxV = true;
            if (atoms[i] == m_atomNetWmStateFullscreen) full = true;
            if (atoms[i] == m_atomNetWmStateHidden) hidden = true;
        }
        wrapper->setMaximized(maxH || maxV);
        wrapper->setFullscreen(full);
        wrapper->setMinimized(hidden);
        std::free(stateReply);
    }
#else
    Q_UNUSED(xid);
    Q_UNUSED(wrapper);
#endif
}

void XWaylandManager::updateX11WindowStateAtoms(quint32 xid, XWaylandSurfaceWrapper *wrapper) {
#if defined(SGE_HAVE_XCB)
    if (!m_xcbConn || !wrapper || xid == 0) {
        return;
    }
    QList<uint32_t> states;
    if (wrapper->maximized()) {
        states.append(m_atomNetWmStateMaximizedHorz);
        states.append(m_atomNetWmStateMaximizedVert);
    }
    if (wrapper->fullscreen()) {
        states.append(m_atomNetWmStateFullscreen);
    }
    if (wrapper->minimized()) {
        states.append(m_atomNetWmStateHidden);
    }
    ::xcb_change_property(
        m_xcbConn, XCB_PROP_MODE_REPLACE, xid, m_atomNetWmState,
        XCB_ATOM_ATOM, 32, static_cast<uint32_t>(states.size()),
        states.isEmpty() ? nullptr : states.constData());
    ::xcb_flush(m_xcbConn);
#else
    Q_UNUSED(xid);
    Q_UNUSED(wrapper);
#endif
}

void XWaylandManager::pollXcbEvents() {
#if defined(SGE_HAVE_XCB)
    if (!m_xcbConn) {
        return;
    }

    while (xcb_generic_event_t *event = ::xcb_poll_for_event(m_xcbConn)) {
        const uint8_t responseType = event->response_type & ~0x80;

        if (m_xfixesFirstEvent > 0
            && responseType == m_xfixesFirstEvent + XCB_XFIXES_SELECTION_NOTIFY) {
            const auto *selEv = reinterpret_cast<const xcb_xfixes_selection_notify_event_t *>(event);
            if (selEv->owner != 0 && selEv->owner != m_wmWindow) {
                if (selEv->selection == m_atomClipboard) {
                    requestX11SelectionText(false);
                } else if (selEv->selection == m_atomPrimary) {
                    requestX11SelectionText(true);
                }
            }
            std::free(event);
            continue;
        }

        switch (responseType) {
        case XCB_CREATE_NOTIFY: {
            const auto *ce = reinterpret_cast<const xcb_create_notify_event_t *>(event);
            if (!ce->override_redirect && ce->window != m_wmWindow && ce->width > 1 && ce->height > 1) {
                const uint32_t values[1] = {
                    XCB_EVENT_MASK_PROPERTY_CHANGE |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                    XCB_EVENT_MASK_FOCUS_CHANGE
                };
                ::xcb_change_window_attributes(m_xcbConn, ce->window, XCB_CW_EVENT_MASK, values);
                auto *wrapper = registerOrUpdateX11Window(
                    ce->window, QString(), QString(),
                    QRect(ce->x, ce->y, std::max<int>(ce->width, 240), std::max<int>(ce->height, 160)));
                queryX11WindowProperties(ce->window, wrapper);
            }
            break;
        }
        case XCB_CONFIGURE_REQUEST: {
            const auto *cre = reinterpret_cast<const xcb_configure_request_event_t *>(event);
            uint16_t mask = 0;
            uint32_t values[7] = {};
            int idx = 0;
            if (cre->value_mask & XCB_CONFIG_WINDOW_X) {
                mask |= XCB_CONFIG_WINDOW_X;
                values[idx++] = static_cast<uint32_t>(cre->x);
            }
            if (cre->value_mask & XCB_CONFIG_WINDOW_Y) {
                mask |= XCB_CONFIG_WINDOW_Y;
                values[idx++] = static_cast<uint32_t>(cre->y);
            }
            if (cre->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
                mask |= XCB_CONFIG_WINDOW_WIDTH;
                values[idx++] = std::max<uint32_t>(cre->width, 1u);
            }
            if (cre->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
                mask |= XCB_CONFIG_WINDOW_HEIGHT;
                values[idx++] = std::max<uint32_t>(cre->height, 1u);
            }
            if (cre->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
                mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
                values[idx++] = 0;
            }
            if (mask != 0) {
                ::xcb_configure_window(m_xcbConn, cre->window, mask, values);
            }
            if (XWaylandSurfaceWrapper *w = windowForXid(cre->window)) {
                const int nextW = (cre->value_mask & XCB_CONFIG_WINDOW_WIDTH)
                    ? cre->width : w->windowGeometry().width();
                const int nextH = (cre->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
                    ? cre->height : w->windowGeometry().height();
                w->setWindowGeometry(QRect(0, 0, nextW, nextH));
            }
            ::xcb_flush(m_xcbConn);
            break;
        }
        case XCB_MAP_REQUEST: {
            const auto *mre = reinterpret_cast<const xcb_map_request_event_t *>(event);
            const uint32_t values[1] = {
                XCB_EVENT_MASK_PROPERTY_CHANGE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_FOCUS_CHANGE
            };
            ::xcb_change_window_attributes(m_xcbConn, mre->window, XCB_CW_EVENT_MASK, values);

            // Define WM_STATE = NormalState (1)
            const uint32_t wmState[2] = {1, XCB_NONE};
            ::xcb_change_property(
                m_xcbConn, XCB_PROP_MODE_REPLACE, mre->window, m_atomWmState,
                m_atomWmState, 32, 2, wmState);

            ::xcb_map_window(m_xcbConn, mre->window);
            ::xcb_flush(m_xcbConn);

            XWaylandSurfaceWrapper *wrapper = windowForXid(mre->window);
            if (!wrapper) {
                wrapper = registerOrUpdateX11Window(
                    mre->window, QString(), QString(), QRect(0, 0, 800, 500));
            }
            queryX11WindowProperties(mre->window, wrapper);
            break;
        }
        case XCB_MAP_NOTIFY: {
            const auto *mne = reinterpret_cast<const xcb_map_notify_event_t *>(event);
            if (!mne->override_redirect && mne->window != m_wmWindow) {
                XWaylandSurfaceWrapper *wrapper = windowForXid(mne->window);
                if (!wrapper) {
                    wrapper = registerOrUpdateX11Window(
                        mne->window, QString(), QString(), QRect(0, 0, 800, 500));
                }
                queryX11WindowProperties(mne->window, wrapper);
                if (!wrapper->surface() && !m_unassignedXWaylandSurfaces.isEmpty()) {
                    QWaylandSurface *candidate = m_unassignedXWaylandSurfaces.takeFirst();
                    if (candidate) {
                        associateSurfaceWithWindow(mne->window, candidate);
                    }
                }
            }
            break;
        }
        case XCB_PROPERTY_NOTIFY: {
            const auto *pne = reinterpret_cast<const xcb_property_notify_event_t *>(event);
            if (pne->window == m_wmWindow && pne->atom == m_atomSgeSelectionProperty
                && pne->state == XCB_PROPERTY_NEW_VALUE) {
                break;
            }
            if (XWaylandSurfaceWrapper *wrapper = windowForXid(pne->window)) {
                if (pne->atom == m_atomWmName || pne->atom == m_atomNetWmName
                    || pne->atom == m_atomWmClass || pne->atom == m_atomNetWmState
                    || pne->atom == m_atomWmProtocols) {
                    queryX11WindowProperties(pne->window, wrapper);
                }
            }
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            const auto *cme = reinterpret_cast<const xcb_client_message_event_t *>(event);
            if (cme->type == m_atomWlSurfaceId) {
                const uint32_t wlSurfaceId = cme->data.data32[0];
                XWaylandSurfaceWrapper *wrapper = windowForXid(cme->window);
                if (!wrapper) {
                    wrapper = registerOrUpdateX11Window(
                        cme->window, QString(), QString(), QRect(0, 0, 800, 500));
                    queryX11WindowProperties(cme->window, wrapper);
                }
                wrapper->setPendingWlSurfaceId(wlSurfaceId);

                QWaylandSurface *resolvedSurface = nullptr;
                if (m_waylandClient) {
                    if (wl_resource *res = ::wl_client_get_object(m_waylandClient, wlSurfaceId)) {
                        resolvedSurface = QWaylandSurface::fromResource(res);
                    }
                }
                if (!resolvedSurface && !m_unassignedXWaylandSurfaces.isEmpty()) {
                    for (qsizetype i = 0; i < m_unassignedXWaylandSurfaces.size(); ++i) {
                        QWaylandSurface *s = m_unassignedXWaylandSurfaces.at(i);
                        if (s && s->resource()
                            && ::wl_resource_get_id(s->resource()) == wlSurfaceId) {
                            resolvedSurface = s;
                            m_unassignedXWaylandSurfaces.removeAt(i);
                            break;
                        }
                    }
                }
                if (resolvedSurface) {
                    associateSurfaceWithWindow(cme->window, resolvedSurface);
                }
            } else if (cme->type == m_atomNetWmState) {
                if (XWaylandSurfaceWrapper *wrapper = windowForXid(cme->window)) {
                    const uint32_t action = cme->data.data32[0]; // 0=REMOVE, 1=ADD, 2=TOGGLE
                    const uint32_t prop1 = cme->data.data32[1];
                    const uint32_t prop2 = cme->data.data32[2];
                    auto applyState = [action](bool current) {
                        if (action == 0) return false;
                        if (action == 1) return true;
                        return !current;
                    };
                    if (prop1 == m_atomNetWmStateMaximizedHorz || prop1 == m_atomNetWmStateMaximizedVert
                        || prop2 == m_atomNetWmStateMaximizedHorz || prop2 == m_atomNetWmStateMaximizedVert) {
                        wrapper->setMaximized(applyState(wrapper->maximized()));
                    }
                    if (prop1 == m_atomNetWmStateFullscreen || prop2 == m_atomNetWmStateFullscreen) {
                        wrapper->setFullscreen(applyState(wrapper->fullscreen()));
                    }
                    if (prop1 == m_atomNetWmStateHidden || prop2 == m_atomNetWmStateHidden) {
                        wrapper->setMinimized(applyState(wrapper->minimized()));
                    }
                    updateX11WindowStateAtoms(cme->window, wrapper);
                }
            }
            break;
        }
        case XCB_SELECTION_REQUEST: {
            const auto *sre = reinterpret_cast<const xcb_selection_request_event_t *>(event);
            xcb_selection_notify_event_t notify{};
            notify.response_type = XCB_SELECTION_NOTIFY;
            notify.time = sre->time;
            notify.requestor = sre->requestor;
            notify.selection = sre->selection;
            notify.target = sre->target;
            notify.property = XCB_NONE;

            const uint32_t targetProp = (sre->property != XCB_NONE) ? sre->property : sre->target;
            const QString &sourceText = (sre->selection == m_atomPrimary)
                ? m_primarySelectionText
                : m_clipboardText;

            if (sre->target == m_atomTargets) {
                const uint32_t supportedTargets[] = {
                    m_atomTargets,
                    m_atomUtf8String,
                    m_atomText,
                    m_atomString,
                    XCB_ATOM_STRING
                };
                ::xcb_change_property(
                    m_xcbConn, XCB_PROP_MODE_REPLACE, sre->requestor, targetProp,
                    XCB_ATOM_ATOM, 32, static_cast<uint32_t>(std::size(supportedTargets)),
                    supportedTargets);
                notify.property = targetProp;
            } else if (sre->target == m_atomUtf8String || sre->target == m_atomText
                       || sre->target == m_atomString || sre->target == XCB_ATOM_STRING) {
                const QByteArray utf8Bytes = sourceText.toUtf8();
                ::xcb_change_property(
                    m_xcbConn, XCB_PROP_MODE_REPLACE, sre->requestor, targetProp,
                    sre->target, 8, static_cast<uint32_t>(utf8Bytes.size()),
                    utf8Bytes.constData());
                notify.property = targetProp;
            }

            ::xcb_send_event(
                m_xcbConn, 0, sre->requestor, XCB_EVENT_MASK_NO_EVENT,
                reinterpret_cast<const char *>(&notify));
            ::xcb_flush(m_xcbConn);
            break;
        }
        case XCB_SELECTION_NOTIFY: {
            const auto *sne = reinterpret_cast<const xcb_selection_notify_event_t *>(event);
            if (sne->property != XCB_NONE) {
                xcb_get_property_cookie_t cookie = ::xcb_get_property(
                    m_xcbConn, 1, m_wmWindow, sne->property, XCB_ATOM_ANY, 0, 16384);
                if (xcb_get_property_reply_t *reply =
                        ::xcb_get_property_reply(m_xcbConn, cookie, nullptr)) {
                    const QByteArray bytes(
                        static_cast<const char *>(::xcb_get_property_value(reply)),
                        ::xcb_get_property_value_length(reply));
                    std::free(reply);
                    if (!bytes.isEmpty()) {
                        const bool isPrimary = (sne->selection == m_atomPrimary);
                        syncX11ToWaylandClipboard(QString::fromUtf8(bytes), isPrimary);
                    }
                }
            }
            break;
        }
        case XCB_UNMAP_NOTIFY: {
            const auto *une = reinterpret_cast<const xcb_unmap_notify_event_t *>(event);
            unregisterX11Window(une->window);
            break;
        }
        case XCB_DESTROY_NOTIFY: {
            const auto *dne = reinterpret_cast<const xcb_destroy_notify_event_t *>(event);
            unregisterX11Window(dne->window);
            break;
        }
        default:
            break;
        }

        std::free(event);
    }
#endif
}

void XWaylandManager::notifySurfaceCreated(QWaylandSurface *surface) {
    if (!surface) {
        return;
    }

    // Verifica se a superfície pertence ao cliente XWayland (ou aguarda associação por WL_SURFACE_ID)
    const bool isFromXWaylandClient = m_waylandClient
        && (surface->waylandClient() == m_waylandClient);
    if (!isFromXWaylandClient && m_windowsByXid.isEmpty()) {
        return;
    }

#if defined(Q_OS_UNIX)
    const uint32_t resId = surface->resource()
        ? ::wl_resource_get_id(surface->resource())
        : 0;
#else
    const uint32_t resId = 0;
#endif

    // Verifica se alguma janela X11 já recebeu WL_SURFACE_ID para este recurso
    if (resId != 0) {
        for (XWaylandSurfaceWrapper *wrapper : std::as_const(m_windowsByXid)) {
            if (wrapper && !wrapper->surface() && wrapper->pendingWlSurfaceId() == resId) {
                associateSurfaceWithWindow(wrapper->x11WindowId(), surface);
                return;
            }
        }
    }

    // Se houver uma janela X11 mapeada sem superfície atribuída, associa
    for (XWaylandSurfaceWrapper *wrapper : std::as_const(m_windowsByXid)) {
        if (wrapper && !wrapper->surface() && wrapper->pendingWlSurfaceId() == 0) {
            associateSurfaceWithWindow(wrapper->x11WindowId(), surface);
            return;
        }
    }

    if (isFromXWaylandClient) {
        m_unassignedXWaylandSurfaces.append(surface);
    }
}

XWaylandSurfaceWrapper *XWaylandManager::registerOrUpdateX11Window(quint32 x11WindowId,
                                                                    const QString &title,
                                                                    const QString &wmClass,
                                                                    const QRect &geometry) {
    if (x11WindowId == 0) {
        return nullptr;
    }

    XWaylandSurfaceWrapper *wrapper = m_windowsByXid.value(x11WindowId, nullptr);
    const bool isNew = (wrapper == nullptr);
    if (isNew) {
        wrapper = new XWaylandSurfaceWrapper(x11WindowId, this);
        m_windowsByXid.insert(x11WindowId, wrapper);
    }

    if (!title.trimmed().isEmpty()) {
        wrapper->setTitle(title);
    }
    if (!wmClass.trimmed().isEmpty()) {
        wrapper->setAppId(wmClass);
    }
    if (geometry.width() > 0 && geometry.height() > 0) {
        wrapper->setWindowGeometry(geometry);
    }

    if (isNew) {
        emit managedWindowsChanged();
    }
    return wrapper;
}

bool XWaylandManager::associateSurfaceWithWindow(quint32 x11WindowId, QWaylandSurface *surface) {
    if (!surface || x11WindowId == 0) {
        return false;
    }

    XWaylandSurfaceWrapper *wrapper = m_windowsByXid.value(x11WindowId, nullptr);
    if (!wrapper) {
        wrapper = registerOrUpdateX11Window(x11WindowId, QString(), QString(), QRect(0, 0, 800, 500));
    }
    if (!wrapper) {
        return false;
    }

    const bool hadSurface = (wrapper->surface() != nullptr);
    wrapper->setSurface(surface);

    connect(surface, &QWaylandSurface::surfaceDestroyed, this, [this, x11WindowId]() {
        unregisterX11Window(x11WindowId);
    });

    if (!hadSurface) {
        qInfo().noquote() << "[SGE-XWAYLAND] Janela X11 associada a superfície Wayland | XID:"
                          << QStringLiteral("0x%1").arg(x11WindowId, 0, 16)
                          << "| Título:" << wrapper->title()
                          << "| WM_CLASS:" << wrapper->appId();
        emit xwaylandWindowCreated(wrapper);
    }
    return true;
}

void XWaylandManager::unregisterX11Window(quint32 x11WindowId) {
    XWaylandSurfaceWrapper *wrapper = m_windowsByXid.take(x11WindowId);
    if (!wrapper) {
        return;
    }
    emit xwaylandWindowDestroyed(wrapper);
    emit managedWindowsChanged();
    wrapper->deleteLater();
}

XWaylandSurfaceWrapper *XWaylandManager::windowForXid(quint32 xid) const {
    return m_windowsByXid.value(xid, nullptr);
}

XWaylandSurfaceWrapper *XWaylandManager::windowForSurface(QWaylandSurface *surface) const {
    if (!surface) {
        return nullptr;
    }
    for (XWaylandSurfaceWrapper *wrapper : m_windowsByXid) {
        if (wrapper && wrapper->surface() == surface) {
            return wrapper;
        }
    }
    return nullptr;
}

QList<XWaylandSurfaceWrapper *> XWaylandManager::managedWindows() const {
    return m_windowsByXid.values();
}

bool XWaylandManager::closeWindow(XWaylandSurfaceWrapper *window) {
    if (!window) {
        return false;
    }
    const quint32 xid = window->x11WindowId();
#if defined(SGE_HAVE_XCB)
    if (m_xcbConn && xid != 0) {
        if (window->supportsDeleteWindow() && m_atomWmProtocols != 0 && m_atomWmDeleteWindow != 0) {
            xcb_client_message_event_t ev{};
            ev.response_type = XCB_CLIENT_MESSAGE;
            ev.format = 32;
            ev.window = xid;
            ev.type = m_atomWmProtocols;
            ev.data.data32[0] = m_atomWmDeleteWindow;
            ev.data.data32[1] = XCB_CURRENT_TIME;
            ::xcb_send_event(
                m_xcbConn, 0, xid, XCB_EVENT_MASK_NO_EVENT,
                reinterpret_cast<const char *>(&ev));
        } else {
            ::xcb_kill_client(m_xcbConn, xid);
        }
        ::xcb_flush(m_xcbConn);
    }
#else
    Q_UNUSED(xid);
#endif
    return true;
}

bool XWaylandManager::minimizeWindow(XWaylandSurfaceWrapper *window, bool minimized) {
    if (!window) {
        return false;
    }
    window->setMinimized(minimized);
    updateX11WindowStateAtoms(window->x11WindowId(), window);
    return true;
}

bool XWaylandManager::maximizeWindow(XWaylandSurfaceWrapper *window, bool maximized, const QSize &size) {
    if (!window) {
        return false;
    }
    window->setMaximized(maximized);
    updateX11WindowStateAtoms(window->x11WindowId(), window);
    if (size.width() > 0 && size.height() > 0) {
        resizeWindow(window, size);
    }
    return true;
}

bool XWaylandManager::resizeWindow(XWaylandSurfaceWrapper *window, const QSize &size) {
    if (!window || size.width() <= 0 || size.height() <= 0) {
        return false;
    }
    const QSize clamped = size.expandedTo(window->minSize());
    window->setWindowGeometry(QRect(0, 0, clamped.width(), clamped.height()));
#if defined(SGE_HAVE_XCB)
    if (m_xcbConn && window->x11WindowId() != 0) {
        const uint32_t values[2] = {
            static_cast<uint32_t>(clamped.width()),
            static_cast<uint32_t>(clamped.height())
        };
        ::xcb_configure_window(
            m_xcbConn, window->x11WindowId(),
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
        ::xcb_flush(m_xcbConn);
    }
#endif
    return true;
}

bool XWaylandManager::activateWindow(XWaylandSurfaceWrapper *window) {
    if (!window) {
        return false;
    }
    if (window->minimized()) {
        window->setMinimized(false);
        updateX11WindowStateAtoms(window->x11WindowId(), window);
    }
#if defined(SGE_HAVE_XCB)
    if (m_xcbConn && window->x11WindowId() != 0) {
        const uint32_t xid = window->x11WindowId();
        const uint32_t stackMode[1] = {XCB_STACK_MODE_ABOVE};
        ::xcb_configure_window(m_xcbConn, xid, XCB_CONFIG_WINDOW_STACK_MODE, stackMode);
        ::xcb_set_input_focus(
            m_xcbConn, XCB_INPUT_FOCUS_POINTER_ROOT, xid, XCB_CURRENT_TIME);
        if (m_atomNetActiveWindow != 0) {
            ::xcb_change_property(
                m_xcbConn, XCB_PROP_MODE_REPLACE, m_rootWindow,
                m_atomNetActiveWindow, XCB_ATOM_WINDOW, 32, 1, &xid);
        }
        ::xcb_flush(m_xcbConn);
    }
#endif
    return true;
}

// ============================================================================
// Ponte Bidirecional de Área de Transferência (Clipboard Bridge)
// ============================================================================

void XWaylandManager::claimX11SelectionOwnership(bool primarySelection) {
#if defined(SGE_HAVE_XCB)
    if (!m_xcbConn || m_wmWindow == 0) {
        return;
    }
    const uint32_t selectionAtom = primarySelection ? m_atomPrimary : m_atomClipboard;
    if (selectionAtom == 0) {
        return;
    }
    ::xcb_set_selection_owner(m_xcbConn, m_wmWindow, selectionAtom, XCB_CURRENT_TIME);
    ::xcb_flush(m_xcbConn);
#else
    Q_UNUSED(primarySelection);
#endif
}

void XWaylandManager::requestX11SelectionText(bool primarySelection) {
#if defined(SGE_HAVE_XCB)
    if (!m_xcbConn || m_wmWindow == 0) {
        return;
    }
    const uint32_t selectionAtom = primarySelection ? m_atomPrimary : m_atomClipboard;
    if (selectionAtom == 0 || m_atomUtf8String == 0 || m_atomSgeSelectionProperty == 0) {
        return;
    }
    ::xcb_convert_selection(
        m_xcbConn, m_wmWindow, selectionAtom,
        m_atomUtf8String, m_atomSgeSelectionProperty, XCB_CURRENT_TIME);
    ::xcb_flush(m_xcbConn);
#else
    Q_UNUSED(primarySelection);
#endif
}

void XWaylandManager::syncWaylandToX11Clipboard(const QString &text, bool primarySelection) {
    if (primarySelection) {
        m_primarySelectionText = text;
    } else {
        m_clipboardText = text;
    }

    claimX11SelectionOwnership(primarySelection);
    emit clipboardSynchronized(text, ClipboardDirection::WaylandToX11);
}

void XWaylandManager::syncX11ToWaylandClipboard(const QString &text, bool primarySelection) {
    if (primarySelection) {
        m_primarySelectionText = text;
    } else {
        m_clipboardText = text;
    }

    if (auto *guiApp = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        if (QClipboard *clipboard = guiApp->clipboard()) {
            m_updatingClipboardInternal = true;
            clipboard->setText(
                text, primarySelection ? QClipboard::Selection : QClipboard::Clipboard);
            m_updatingClipboardInternal = false;
        }
    }

    if (m_compositor && m_compositor->defaultSeat()) {
        if (QWaylandSurface *focusedSurface = m_compositor->defaultSeat()->keyboardFocus()) {
#if QT_CONFIG(clipboard)
            focusedSurface->updateSelection();
#else
            Q_UNUSED(focusedSurface);
#endif
        }
    }

    emit clipboardSynchronized(text, ClipboardDirection::X11ToWayland);
}
