#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRect>
#include <QSize>
#include <QSocketNotifier>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <QtWaylandCompositor/QWaylandSurface>

struct wl_client;
struct xcb_connection_t;

class SystaliaCompositor;

/**
 * @brief Contentor de superfície e metadados para janelas X11 legadas geridas pelo XWayland.
 *
 * Expõe a mesma interface de propriedades QML que `QWaylandXdgToplevel`, permitindo que
 * `WaylandWindowContainer.qml`, `WindowStackModel` e `WorkspaceManagerService` operem de forma
 * homogénea sobre clientes Wayland nativos e clientes X11 legados com o chassi VALO Glass.
 */
class XWaylandSurfaceWrapper final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged FINAL)
    Q_PROPERTY(QString appId READ appId NOTIFY appIdChanged FINAL)
    Q_PROPERTY(QWaylandSurface *surface READ surface NOTIFY surfaceChanged FINAL)
    Q_PROPERTY(QRect windowGeometry READ windowGeometry NOTIFY windowGeometryChanged FINAL)
    Q_PROPERTY(bool maximized READ maximized NOTIFY maximizedChanged FINAL)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged FINAL)
    Q_PROPERTY(bool minimized READ minimized NOTIFY minimizedChanged FINAL)
    Q_PROPERTY(QSize minSize READ minSize NOTIFY sizeLimitsChanged FINAL)
    Q_PROPERTY(QSize maxSize READ maxSize NOTIFY sizeLimitsChanged FINAL)
    Q_PROPERTY(bool isXWayland READ isXWayland CONSTANT FINAL)
    Q_PROPERTY(quint32 x11WindowId READ x11WindowId CONSTANT FINAL)

public:
    explicit XWaylandSurfaceWrapper(quint32 x11WindowId, QObject *parent = nullptr);
    ~XWaylandSurfaceWrapper() override = default;

    [[nodiscard]] QString title() const { return m_title; }
    void setTitle(const QString &title);

    [[nodiscard]] QString appId() const { return m_appId; }
    void setAppId(const QString &appId);

    [[nodiscard]] QWaylandSurface *surface() const { return m_surface.data(); }
    void setSurface(QWaylandSurface *surface);

    [[nodiscard]] QRect windowGeometry() const { return m_windowGeometry; }
    void setWindowGeometry(const QRect &geometry);

    [[nodiscard]] bool maximized() const { return m_maximized; }
    void setMaximized(bool maximized);

    [[nodiscard]] bool fullscreen() const { return m_fullscreen; }
    void setFullscreen(bool fullscreen);

    [[nodiscard]] bool minimized() const { return m_minimized; }
    void setMinimized(bool minimized);

    [[nodiscard]] QSize minSize() const { return m_minSize; }
    [[nodiscard]] QSize maxSize() const { return m_maxSize; }
    void setSizeLimits(const QSize &minSize, const QSize &maxSize);

    [[nodiscard]] bool isXWayland() const noexcept { return true; }
    [[nodiscard]] quint32 x11WindowId() const noexcept { return m_x11WindowId; }

    [[nodiscard]] quint32 pendingWlSurfaceId() const noexcept { return m_pendingWlSurfaceId; }
    void setPendingWlSurfaceId(quint32 id) noexcept { m_pendingWlSurfaceId = id; }

    [[nodiscard]] bool supportsDeleteWindow() const noexcept { return m_supportsDeleteWindow; }
    void setSupportsDeleteWindow(bool supported) noexcept { m_supportsDeleteWindow = supported; }

    [[nodiscard]] static QString parseWmClassPayload(const QByteArray &rawPayload);

signals:
    void titleChanged();
    void appIdChanged();
    void surfaceChanged();
    void windowGeometryChanged();
    void maximizedChanged();
    void fullscreenChanged();
    void minimizedChanged();
    void sizeLimitsChanged();

private:
    quint32 m_x11WindowId = 0;
    quint32 m_pendingWlSurfaceId = 0;
    QString m_title;
    QString m_appId;
    QPointer<QWaylandSurface> m_surface;
    QRect m_windowGeometry{0, 0, 800, 500};
    QSize m_minSize{240, 160};
    QSize m_maxSize{0, 0};
    bool m_maximized = false;
    bool m_fullscreen = false;
    bool m_minimized = false;
    bool m_supportsDeleteWindow = false;
};

/**
 * @brief Gestor do servidor XWayland embutido, Window Manager X11 (XCB) e Ponte de Clipboard.
 *
 * Responsável por:
 * 1. Criação dos pares de sockets Unix (`WAYLAND_SOCKET`, `-listenfd`, `-wm`, `-displayfd`).
 * 2. Execução e supervisão do binário `/usr/bin/Xwayland` em modo `-rootless`.
 * 3. Leitura assíncrona de `-displayfd` e exportação de `DISPLAY=:<num>` para o ambiente.
 * 4. Gestão de janelas X11 via XCB (`SubstructureRedirect`, `CompositeRedirectManual`, átomos ICCCM/EWMH)
 *    e associação com `QWaylandSurface` para renderização no `WaylandWindowContainer.qml`.
 * 5. Sincronização bidirecional de área de transferência (`CLIPBOARD` e `PRIMARY` em `UTF8_STRING`).
 */
class XWaylandManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged FINAL)
    Q_PROPERTY(bool ready READ isReady NOTIFY displayReady FINAL)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayReady FINAL)
    Q_PROPERTY(int displayNumber READ displayNumber NOTIFY displayReady FINAL)
    Q_PROPERTY(QString xwaylandExecutable READ xwaylandExecutable
               WRITE setXwaylandExecutable NOTIFY executableChanged FINAL)
    Q_PROPERTY(int managedWindowCount READ managedWindowCount NOTIFY managedWindowsChanged FINAL)
    Q_PROPERTY(QString clipboardText READ clipboardText NOTIFY clipboardSynchronized FINAL)
    Q_PROPERTY(QString primarySelectionText READ primarySelectionText NOTIFY clipboardSynchronized FINAL)

public:
    enum class ClipboardDirection {
        WaylandToX11 = 0,
        X11ToWayland = 1
    };
    Q_ENUM(ClipboardDirection)

    explicit XWaylandManager(QObject *parent = nullptr);
    ~XWaylandManager() override;

    static XWaylandManager *instance() noexcept;

    void attachCompositor(SystaliaCompositor *compositor);
    [[nodiscard]] SystaliaCompositor *compositor() const noexcept;

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] bool isReady() const noexcept { return m_ready; }
    [[nodiscard]] QString displayName() const { return m_displayName; }
    [[nodiscard]] int displayNumber() const noexcept { return m_displayNumber; }

    [[nodiscard]] QString xwaylandExecutable() const { return m_xwaylandExecutable; }
    void setXwaylandExecutable(const QString &executablePath);

    [[nodiscard]] bool autoRestartEnabled() const noexcept { return m_autoRestartEnabled; }
    void setAutoRestartEnabled(bool enabled) noexcept { m_autoRestartEnabled = enabled; }

    [[nodiscard]] int crashRestartCount() const noexcept { return m_crashRestartCount; }
    [[nodiscard]] int managedWindowCount() const noexcept { return m_windowsByXid.size(); }

    [[nodiscard]] ::wl_client *waylandClient() const noexcept { return m_waylandClient; }

    [[nodiscard]] int waylandServerFd() const noexcept { return m_waylandServerFd; }
    [[nodiscard]] int waylandClientFd() const noexcept { return m_waylandClientFd; }
    [[nodiscard]] int wmMasterFd() const noexcept { return m_wmMasterFd; }
    [[nodiscard]] int wmClientFd() const noexcept { return m_wmClientFd; }
    [[nodiscard]] int displayReadFd() const noexcept { return m_displayReadFd; }
    [[nodiscard]] int displayWriteFd() const noexcept { return m_displayWriteFd; }
    [[nodiscard]] int x11ListenFd() const noexcept { return m_x11ListenFd; }

    [[nodiscard]] static QStringList buildCommandLineArguments(const QString &displayCandidate,
                                                               int listenFd,
                                                               int wmFd,
                                                               int displayFd);
    [[nodiscard]] QStringList currentCommandLineArguments() const { return m_lastArguments; }

    [[nodiscard]] static QString parseDisplayFdPayload(const QByteArray &payload, bool *ok = nullptr);
    [[nodiscard]] static int findAvailableDisplayNumber(int preferredDisplay = 1);

    Q_INVOKABLE bool createSocketPairs(int displayNumber = 1);
    Q_INVOKABLE bool attachDisplayFdReadDescriptor(int readFd);
    Q_INVOKABLE bool start(int preferredDisplay = -1);
    Q_INVOKABLE void stop();

    // Operações de gestão de janelas X11 invocadas pelo SystaliaCompositor
    void notifySurfaceCreated(QWaylandSurface *surface);
    bool closeWindow(XWaylandSurfaceWrapper *window);
    bool minimizeWindow(XWaylandSurfaceWrapper *window, bool minimized);
    bool maximizeWindow(XWaylandSurfaceWrapper *window, bool maximized, const QSize &size);
    bool resizeWindow(XWaylandSurfaceWrapper *window, const QSize &size);
    bool activateWindow(XWaylandSurfaceWrapper *window);

    [[nodiscard]] XWaylandSurfaceWrapper *windowForXid(quint32 xid) const;
    [[nodiscard]] XWaylandSurfaceWrapper *windowForSurface(QWaylandSurface *surface) const;
    [[nodiscard]] QList<XWaylandSurfaceWrapper *> managedWindows() const;

    // API pública para testes e simulação de criação/associação de janelas X11
    XWaylandSurfaceWrapper *registerOrUpdateX11Window(quint32 x11WindowId,
                                                      const QString &title,
                                                      const QString &wmClass,
                                                      const QRect &geometry);
    bool associateSurfaceWithWindow(quint32 x11WindowId, QWaylandSurface *surface);
    void unregisterX11Window(quint32 x11WindowId);

    // Ponte Bidirecional de Área de Transferência (Clipboard Bridge Wayland <-> X11)
    [[nodiscard]] QString clipboardText() const { return m_clipboardText; }
    [[nodiscard]] QString primarySelectionText() const { return m_primarySelectionText; }
    Q_INVOKABLE void syncWaylandToX11Clipboard(const QString &text, bool primarySelection = false);
    Q_INVOKABLE void syncX11ToWaylandClipboard(const QString &text, bool primarySelection = false);

signals:
    void runningChanged();
    void executableChanged();
    void displayReady(const QString &displayName);
    void processFailed(const QString &reason);
    void processRestarted(int attemptCount);
    void xwaylandWindowCreated(XWaylandSurfaceWrapper *window);
    void xwaylandWindowDestroyed(XWaylandSurfaceWrapper *window);
    void managedWindowsChanged();
    void clipboardSynchronized(const QString &text, XWaylandManager::ClipboardDirection direction);

private slots:
    void onDisplayFdReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError error);
    void pollXcbEvents();

private:
    static void closeFdSafely(int &fd) noexcept;
    void closeAllFileDescriptors() noexcept;
    void cleanupListenSocketFile() noexcept;

    bool initializeXcbWindowManager();
    void teardownXcbWindowManager() noexcept;
    void queryX11WindowProperties(quint32 xid, XWaylandSurfaceWrapper *wrapper);
    void updateX11WindowStateAtoms(quint32 xid, XWaylandSurfaceWrapper *wrapper);
    void claimX11SelectionOwnership(bool primarySelection);
    void requestX11SelectionText(bool primarySelection);

    static XWaylandManager *s_instance;

    QPointer<SystaliaCompositor> m_compositor;
    QString m_xwaylandExecutable;
    QString m_displayName;
    QString m_listenSocketPath;
    int m_displayNumber = -1;
    bool m_ready = false;
    bool m_stopping = false;
    bool m_autoRestartEnabled = true;
    bool m_updatingClipboardInternal = false;
    int m_crashRestartCount = 0;

    int m_waylandServerFd = -1;
    int m_waylandClientFd = -1;
    int m_wmMasterFd = -1;
    int m_wmClientFd = -1;
    int m_displayReadFd = -1;
    int m_displayWriteFd = -1;
    int m_x11ListenFd = -1;

    QByteArray m_displayFdBuffer;
    QStringList m_lastArguments;

    QProcess *m_process = nullptr;
    QSocketNotifier *m_displayFdNotifier = nullptr;
    QSocketNotifier *m_xcbNotifier = nullptr;
    ::wl_client *m_waylandClient = nullptr;

    // Estado do XCB Window Manager e Átomos X11
    ::xcb_connection_t *m_xcbConn = nullptr;
    quint32 m_rootWindow = 0;
    quint32 m_wmWindow = 0;
    quint32 m_xfixesFirstEvent = 0;

    quint32 m_atomWmS0 = 0;
    quint32 m_atomWlSurfaceId = 0;
    quint32 m_atomWlSurfaceSerial = 0;
    quint32 m_atomWmProtocols = 0;
    quint32 m_atomWmDeleteWindow = 0;
    quint32 m_atomWmTakeFocus = 0;
    quint32 m_atomWmState = 0;
    quint32 m_atomWmChangeState = 0;
    quint32 m_atomWmName = 0;
    quint32 m_atomWmClass = 0;
    quint32 m_atomNetWmName = 0;
    quint32 m_atomNetWmState = 0;
    quint32 m_atomNetWmStateMaximizedVert = 0;
    quint32 m_atomNetWmStateMaximizedHorz = 0;
    quint32 m_atomNetWmStateFullscreen = 0;
    quint32 m_atomNetWmStateHidden = 0;
    quint32 m_atomNetSupportingWmCheck = 0;
    quint32 m_atomNetSupported = 0;
    quint32 m_atomNetActiveWindow = 0;
    quint32 m_atomUtf8String = 0;
    quint32 m_atomClipboard = 0;
    quint32 m_atomPrimary = 0;
    quint32 m_atomTargets = 0;
    quint32 m_atomText = 0;
    quint32 m_atomString = 0;
    quint32 m_atomSgeSelectionProperty = 0;

    QHash<quint32, XWaylandSurfaceWrapper *> m_windowsByXid;
    QList<QPointer<QWaylandSurface>> m_unassignedXWaylandSurfaces;

    QString m_clipboardText;
    QString m_primarySelectionText;
};
