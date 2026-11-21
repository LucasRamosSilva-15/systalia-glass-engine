#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>

class LogindSessionService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sessionPath READ sessionPath NOTIFY sessionPathChanged FINAL)
    Q_PROPERTY(SessionState sessionState READ sessionState NOTIFY sessionStateChanged FINAL)
    Q_PROPERTY(bool hasControl READ hasSessionControl NOTIFY sessionControlChanged FINAL)
    Q_PROPERTY(bool active READ isSessionActive NOTIFY sessionActiveChanged FINAL)
    Q_PROPERTY(bool gpuSubmissionSuspended READ isGpuSubmissionSuspended
               NOTIFY gpuSubmissionSuspendedChanged FINAL)
    Q_PROPERTY(int activeDeviceCount READ activeDeviceCount NOTIFY devicesChanged FINAL)

public:
    enum SessionState {
        Disconnected = 0,
        Connected = 1,
        Active = 2,
        Suspended = 3
    };
    Q_ENUM(SessionState)

    struct ManagedDevice {
        quint32 major = 0;
        quint32 minor = 0;
        int fd = -1;
        bool paused = false;
        QString devicePath;
    };

    using DbusCallHandler = std::function<QDBusMessage(const QDBusMessage &)>;

    static constexpr const char *kLogindService = "org.freedesktop.login1";
    static constexpr const char *kLogindManagerPath = "/org/freedesktop/login1";
    static constexpr const char *kLogindManagerInterface = "org.freedesktop.login1.Manager";
    static constexpr const char *kLogindAutoSessionPath = "/org/freedesktop/login1/session/auto";
    static constexpr const char *kLogindSessionInterface = "org.freedesktop.login1.Session";
    static constexpr const char *kDbusPropertiesInterface = "org.freedesktop.DBus.Properties";

    // Linux DRM major number (226) e Input evdev major number (13)
    static constexpr quint32 kDrmMajorNumber = 226u;
    static constexpr quint32 kInputMajorNumber = 13u;

    explicit LogindSessionService(QObject *parent = nullptr);
    explicit LogindSessionService(const QDBusConnection &connection, QObject *parent = nullptr);
    ~LogindSessionService() override;

    static LogindSessionService *instance() noexcept;

    // 1. Construtores puros de mensagens D-Bus (validação estrita de rotas e assinaturas)
    [[nodiscard]] static QDBusMessage buildGetSessionByPidMessage(quint32 pid);
    [[nodiscard]] static QDBusMessage buildTakeControlMessage(const QString &sessionPath, bool force = false);
    [[nodiscard]] static QDBusMessage buildReleaseControlMessage(const QString &sessionPath);
    [[nodiscard]] static QDBusMessage buildTakeDeviceMessage(const QString &sessionPath,
                                                             quint32 major, quint32 minor);
    [[nodiscard]] static QDBusMessage buildReleaseDeviceMessage(const QString &sessionPath,
                                                                quint32 major, quint32 minor);
    [[nodiscard]] static QDBusMessage buildPauseDeviceCompleteMessage(const QString &sessionPath,
                                                                      quint32 major, quint32 minor);
    [[nodiscard]] static QString messageSignature(const QDBusMessage &message);

    // 2. Resolução de major/minor de nós de dispositivo (/dev/dri/card0, /dev/input/event*)
    [[nodiscard]] static bool statDeviceMajorMinor(const QString &devicePath,
                                                   quint32 *majorOut,
                                                   quint32 *minorOut) noexcept;
    [[nodiscard]] static constexpr quint64 deviceKey(quint32 major, quint32 minor) noexcept {
        return (static_cast<quint64>(major) << 32u) | static_cast<quint64>(minor);
    }

    // 3. Injeção de transport handler para testes unitários determinísticos (Mock D-Bus)
    void setCustomCallHandler(DbusCallHandler handler);
    void clearCustomCallHandler() noexcept;
    [[nodiscard]] QList<QDBusMessage> recordedOutboundMessages() const { return m_outboundMessages; }
    void clearRecordedMessages() noexcept { m_outboundMessages.clear(); }

    // 4. Ciclo de vida da sessão Logind (Rootless Device Acquisition)
    Q_INVOKABLE bool initializeSession(bool forceTakeControl = false);
    Q_INVOKABLE QString resolveSessionPathForPid(quint32 pid = 0);
    Q_INVOKABLE bool takeControl(bool force = false);
    Q_INVOKABLE bool releaseControl();

    Q_INVOKABLE int takeDevice(quint32 major, quint32 minor, bool *inactiveOut = nullptr);
    Q_INVOKABLE int takeDeviceByPath(const QString &devicePath, bool *inactiveOut = nullptr);
    Q_INVOKABLE int acquirePrimaryDrmDevice(const QString &drmPath = QStringLiteral("/dev/dri/card0"));
    Q_INVOKABLE bool releaseDevice(quint32 major, quint32 minor);
    Q_INVOKABLE bool releaseDeviceByFd(int fd);
    Q_INVOKABLE void releaseAllDevices();
    Q_INVOKABLE bool pauseDeviceComplete(quint32 major, quint32 minor);

    // 5. Consultas de estado
    [[nodiscard]] QString sessionPath() const noexcept { return m_sessionPath; }
    [[nodiscard]] SessionState sessionState() const noexcept { return m_sessionState; }
    [[nodiscard]] bool hasSessionControl() const noexcept { return m_hasControl; }
    [[nodiscard]] bool isSessionActive() const noexcept { return m_sessionActive; }
    [[nodiscard]] bool isGpuSubmissionSuspended() const noexcept { return m_gpuSubmissionSuspended; }
    [[nodiscard]] int activeDeviceCount() const noexcept { return static_cast<int>(m_devices.size()); }
    [[nodiscard]] int deviceFileDescriptor(quint32 major, quint32 minor) const noexcept;
    [[nodiscard]] bool hasPausedDevices() const noexcept;
    [[nodiscard]] bool isDevicePaused(quint32 major, quint32 minor) const noexcept;
    [[nodiscard]] int primaryDrmFileDescriptor() const noexcept { return m_primaryDrmFd; }
    [[nodiscard]] int pauseCompleteAckCount() const noexcept { return m_pauseCompleteAckCount; }

public slots:
    // Slots conectados aos sinais D-Bus de org.freedesktop.login1.Session e Properties
    void onPauseDevice(uint major, uint minor, const QString &type);
    void onResumeDevice(uint major, uint minor, const QDBusUnixFileDescriptor &fd);
    void onPropertiesChanged(const QString &interfaceName,
                             const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);

signals:
    void sessionPathChanged(const QString &sessionPath);
    void sessionStateChanged(LogindSessionService::SessionState state);
    void sessionControlChanged(bool hasControl);
    void sessionActiveChanged(bool active);
    void gpuSubmissionSuspendedChanged(bool suspended);
    void devicesChanged();
    void devicePauseRequested(quint32 major, quint32 minor);
    void devicePaused(quint32 major, quint32 minor, const QString &type);
    void deviceResumed(quint32 major, quint32 minor, int fd);
    void swapchainRestoreRequested(quint32 major, quint32 minor, int fd);

private:
    QDBusMessage dispatchDbusCall(const QDBusMessage &request);
    void subscribeSessionSignals();
    void setSessionState(SessionState newState);
    void setGpuSubmissionSuspended(bool suspended);

    QDBusConnection m_connection;
    bool m_hasBusConnection = false;
    QString m_sessionPath = QString::fromLatin1(kLogindAutoSessionPath);
    SessionState m_sessionState = Disconnected;
    bool m_hasControl = false;
    bool m_sessionActive = false;
    bool m_gpuSubmissionSuspended = false;
    int m_primaryDrmFd = -1;
    int m_pauseCompleteAckCount = 0;

    QHash<quint64, ManagedDevice> m_devices;
    DbusCallHandler m_customCallHandler;
    QList<QDBusMessage> m_outboundMessages;
};

