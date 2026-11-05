#pragma once

#include <QByteArray>
#include <QList>
#include <QHash>
#include <QEvent>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QSocketNotifier>
#include <QString>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSeat>

#ifdef SGE_HAVE_LIBINPUT
#include <libinput.h>
#else
struct libinput;
struct libinput_interface {
    int (*open_restricted)(const char *path, int flags, void *user_data);
    void (*close_restricted)(int fd, void *user_data);
};
#endif
struct udev;

class InputDeviceManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString platformMode READ platformMode NOTIFY platformModeChanged FINAL)
    Q_PROPERTY(bool isBareMetal READ isBareMetal NOTIFY platformModeChanged FINAL)
    Q_PROPERTY(QPointF pointerPosition READ pointerPosition NOTIFY pointerPositionChanged FINAL)
    Q_PROPERTY(qreal pointerAcceleration READ pointerAcceleration WRITE setPointerAcceleration
               NOTIFY pointerAccelerationChanged FINAL)
    Q_PROPERTY(QRect kmsDisplayBounds READ kmsDisplayBounds WRITE setKmsDisplayBounds
               NOTIFY kmsDisplayBoundsChanged FINAL)
    Q_PROPERTY(QString activeDrmDevice READ activeDrmDevice NOTIFY kmsConfigChanged FINAL)
    Q_PROPERTY(int targetRefreshRateHz READ targetRefreshRateHz NOTIFY kmsConfigChanged FINAL)
    Q_PROPERTY(int dispatchedKeyEvents READ dispatchedKeyEvents NOTIFY inputTelemetryChanged FINAL)
    Q_PROPERTY(int dispatchedPointerEvents READ dispatchedPointerEvents NOTIFY inputTelemetryChanged FINAL)
    Q_PROPERTY(int dispatchedSwipeGestures READ dispatchedSwipeGestures NOTIFY inputTelemetryChanged FINAL)

public:
    enum PlatformSessionMode {
        SessionHeadless = 0,
        SessionNested = 1,
        SessionBareMetalKms = 2
    };
    Q_ENUM(PlatformSessionMode)

    enum LibinputEventType {
        LibinputEventNone = 0,
        LibinputEventDeviceAdded = 1,
        LibinputEventDeviceRemoved = 2,
        LibinputEventKeyboardKey = 300,
        LibinputEventPointerMotion = 400,
        LibinputEventPointerMotionAbsolute = 401,
        LibinputEventPointerButton = 402,
        LibinputEventPointerAxis = 403,
        LibinputEventGestureSwipeBegin = 800,
        LibinputEventGestureSwipeUpdate = 801,
        LibinputEventGestureSwipeEnd = 802
    };
    Q_ENUM(LibinputEventType)

    struct PlatformEnvironmentConfig {
        PlatformSessionMode mode = SessionNested;
        QByteArray qtQpaPlatform;
        QByteArray qtQpaEglfsIntegration;
        QByteArray qsgRhiBackend;
        QByteArray eglfsKmsConfigPath;
        bool atomicDrmEnabled = true;
    };

    struct KmsConnectorMode {
        QString connectorName;
        QSize resolution{1920, 1080};
        int refreshRateHz = 60;
        bool connected = true;
        bool preferred = false;
    };

    struct SimulatedLibinputEvent {
        LibinputEventType type = LibinputEventNone;
        quint32 linuxScanCode = 0; // Linux evdev code (e.g. KEY_A = 30)
        bool pressed = false;
        qreal dx = 0.0;
        qreal dy = 0.0;
        quint32 linuxButtonCode = 0; // BTN_LEFT = 0x110 (272), BTN_RIGHT = 0x111 (273), BTN_MIDDLE = 0x112 (274)
        int fingerCount = 3;
        bool cancelled = false;
    };

    explicit InputDeviceManager(QObject *parent = nullptr);
    ~InputDeviceManager() override;

    static InputDeviceManager *instance() noexcept;

    // 1. Deteção e configuração automática de backend (Nested vs Bare-Metal DRM/KMS vs Headless)
    [[nodiscard]] static PlatformEnvironmentConfig evaluatePlatformEnvironment(
        const QByteArray &waylandDisplay,
        const QByteArray &x11Display,
        const QByteArray &existingQpaPlatform = QByteArray(),
        const QString &kmsConfigPath = QString()) noexcept;

    static PlatformEnvironmentConfig configureProcessPlatformEnvironment(
        const QString &kmsConfigPath = QString());

    // 2. Seleção de modo nativo KMS / Atomic Mode Setting (maior resolução e refresh rate)
    [[nodiscard]] static KmsConnectorMode selectBestKmsConnectorMode(
        const QList<KmsConnectorMode> &candidates) noexcept;
    [[nodiscard]] QByteArray generateEglfsKmsConfigJson(
        const QString &drmDevice,
        const KmsConnectorMode &preferredMode) const;
    [[nodiscard]] QByteArray generateEglfsKmsConfigJson(
        const QString &drmDevice = QStringLiteral("/dev/dri/card0")) const;
    Q_INVOKABLE bool writeEglfsKmsConfigFile(
        const QString &filePath,
        const QString &drmDevice = QStringLiteral("/dev/dri/card0"));

    // 3. Integração com QWaylandCompositor e QWaylandSeat
    void attachCompositor(QWaylandCompositor *compositor);
    [[nodiscard]] QWaylandSeat *targetSeat() const noexcept;

    [[nodiscard]] QString platformMode() const noexcept { return m_platformModeName; }
    [[nodiscard]] PlatformSessionMode sessionMode() const noexcept { return m_sessionMode; }
    [[nodiscard]] bool isBareMetal() const noexcept { return m_sessionMode == SessionBareMetalKms; }
    [[nodiscard]] QPointF pointerPosition() const noexcept { return m_pointerPosition; }
    [[nodiscard]] qreal pointerAcceleration() const noexcept { return m_pointerAcceleration; }
    [[nodiscard]] QRect kmsDisplayBounds() const noexcept { return m_kmsDisplayBounds; }
    [[nodiscard]] QString activeDrmDevice() const noexcept { return m_activeDrmDevice; }
    [[nodiscard]] int targetRefreshRateHz() const noexcept { return m_targetRefreshRateHz; }
    [[nodiscard]] int dispatchedKeyEvents() const noexcept { return m_dispatchedKeyEvents; }
    [[nodiscard]] int dispatchedPointerEvents() const noexcept { return m_dispatchedPointerEvents; }
    [[nodiscard]] int dispatchedSwipeGestures() const noexcept { return m_dispatchedSwipeGestures; }
    [[nodiscard]] bool isHardwareBackendInitialized() const noexcept { return m_hardwareInitialized; }

    Q_INVOKABLE void setPointerAcceleration(qreal factor);
    Q_INVOKABLE void setKmsDisplayBounds(const QRect &bounds);
    Q_INVOKABLE void setPointerPosition(const QPointF &pos);
    Q_INVOKABLE void resetTelemetryCounters() noexcept;

    // 4. Pipeline de eventos Libinput -> QWaylandSeat / Qt Events
    Q_INVOKABLE bool initializeHardwareContext();
    Q_INVOKABLE void shutdownHardwareContext();

    [[nodiscard]] QPointF applyPointerMotionDelta(qreal dx, qreal dy) noexcept;
    [[nodiscard]] static QPointF clampPointerToBounds(const QPointF &candidate,
                                                      const QRect &bounds) noexcept;
    [[nodiscard]] static Qt::MouseButton translateLinuxButtonToQt(quint32 linuxButton) noexcept;
    [[nodiscard]] static int translateLinuxKeyToQtKey(quint32 linuxScanCode) noexcept;
    [[nodiscard]] static quint32 linuxScanCodeToXkbKeycode(quint32 linuxScanCode) noexcept {
        return linuxScanCode + 8u;
    }

    Q_INVOKABLE bool processLibinputKeyEvent(quint32 linuxScanCode, bool pressed);
    Q_INVOKABLE QPointF processLibinputPointerMotion(qreal dx, qreal dy);
    Q_INVOKABLE QPointF processLibinputPointerMotionAbsolute(qreal normalizedX, qreal normalizedY);
    Q_INVOKABLE bool processLibinputPointerButton(quint32 linuxButtonCode, bool pressed);
    Q_INVOKABLE bool processLibinputSwipeGesture(int fingerCount, qreal cumulativeDx,
                                                 qreal cumulativeDy, bool ended,
                                                 bool cancelled = false);

    bool dispatchSimulatedEvent(const SimulatedLibinputEvent &ev);

    [[nodiscard]] static const libinput_interface &restrictedInterface() noexcept;

signals:
    void platformModeChanged();
    void pointerPositionChanged(const QPointF &position);
    void pointerAccelerationChanged(qreal acceleration);
    void kmsDisplayBoundsChanged(const QRect &bounds);
    void kmsConfigChanged();
    void inputTelemetryChanged();
    void keyEventDispatched(quint32 linuxScanCode, quint32 xkbKeycode, int qtKey, bool pressed);
    void pointerMotionDispatched(const QPointF &clampedPosition, qreal acceleratedDx, qreal acceleratedDy);
    void pointerButtonDispatched(Qt::MouseButton button, bool pressed, const QPointF &position);
    void workspaceSwipeTriggered(int direction);

private:
    static int openRestrictedCallback(const char *path, int flags, void *userData);
    static void closeRestrictedCallback(int fd, void *userData);
    void pollHardwareEvents();
    void setHardwareSuspended(bool suspended);
    bool dispatchPointerToScene(QEvent::Type type, Qt::MouseButton button = Qt::NoButton);


    bool m_hardwareSuspended = false;
    QHash<int, QPair<quint32, quint32>> m_leasedDevices;
    QPointer<QWaylandCompositor> m_compositor;
    PlatformSessionMode m_sessionMode = SessionNested;
    QString m_platformModeName = QStringLiteral("nested");
    QString m_activeDrmDevice = QStringLiteral("/dev/dri/card0");
    int m_targetRefreshRateHz = 60;
    QRect m_kmsDisplayBounds{0, 0, 1920, 1080};
    QPointF m_pointerPosition{960.0, 540.0};
    qreal m_pointerAcceleration = 1.0;
    Qt::MouseButtons m_pressedButtons = Qt::NoButton;
    qreal m_accumulatedSwipeDx = 0.0;
    int m_dispatchedKeyEvents = 0;
    int m_dispatchedPointerEvents = 0;
    int m_dispatchedSwipeGestures = 0;
    bool m_hardwareInitialized = false;

    udev *m_udev = nullptr;
    libinput *m_libinput = nullptr;
    QSocketNotifier *m_libinputNotifier = nullptr;
};
