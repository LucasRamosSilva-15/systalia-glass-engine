#include "LogindSessionService.hpp"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusError>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDebug>
#include <QRegularExpression>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace {
LogindSessionService *s_instance = nullptr;

int duplicateFdCloexec(int sourceFd) noexcept {
    if (sourceFd < 0) {
        return -1;
    }
    const int duplicated = ::fcntl(sourceFd, F_DUPFD_CLOEXEC, 0);
    if (duplicated >= 0) {
        return duplicated;
    }
    return ::dup(sourceFd);
}
} // namespace

LogindSessionService::LogindSessionService(QObject *parent)
    : QObject(parent)
    , m_connection(QDBusConnection::systemBus())
    , m_hasBusConnection(m_connection.isConnected()) {
    s_instance = this;
}

LogindSessionService::LogindSessionService(const QDBusConnection &connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
    , m_hasBusConnection(m_connection.isConnected()) {
    s_instance = this;
}

LogindSessionService::~LogindSessionService() {
    releaseAllDevices();
    if (m_hasControl) {
        releaseControl();
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

LogindSessionService *LogindSessionService::instance() noexcept {
    return s_instance;
}

QDBusMessage LogindSessionService::buildGetSessionByPidMessage(quint32 pid) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindService),
        QString::fromLatin1(kLogindManagerPath),
        QString::fromLatin1(kLogindManagerInterface),
        QStringLiteral("GetSessionByPID"));
    msg << QVariant::fromValue<uint>(pid);
    return msg;
}

QDBusMessage LogindSessionService::buildTakeControlMessage(const QString &sessionPath, bool force) {
    const QString targetPath = sessionPath.trimmed().isEmpty()
                                   ? QString::fromLatin1(kLogindAutoSessionPath)
                                   : sessionPath.trimmed();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindService),
        targetPath,
        QString::fromLatin1(kLogindSessionInterface),
        QStringLiteral("TakeControl"));
    msg << QVariant::fromValue<bool>(force);
    return msg;
}

QDBusMessage LogindSessionService::buildReleaseControlMessage(const QString &sessionPath) {
    const QString targetPath = sessionPath.trimmed().isEmpty()
                                   ? QString::fromLatin1(kLogindAutoSessionPath)
                                   : sessionPath.trimmed();
    return QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindService),
        targetPath,
        QString::fromLatin1(kLogindSessionInterface),
        QStringLiteral("ReleaseControl"));
}

QDBusMessage LogindSessionService::buildTakeDeviceMessage(const QString &sessionPath,
                                                          quint32 major,
                                                          quint32 minor) {
    const QString targetPath = sessionPath.trimmed().isEmpty()
                                   ? QString::fromLatin1(kLogindAutoSessionPath)
                                   : sessionPath.trimmed();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindService),
        targetPath,
        QString::fromLatin1(kLogindSessionInterface),
        QStringLiteral("TakeDevice"));
    msg << QVariant::fromValue<uint>(major) << QVariant::fromValue<uint>(minor);
    return msg;
}

QDBusMessage LogindSessionService::buildReleaseDeviceMessage(const QString &sessionPath,
                                                             quint32 major,
                                                             quint32 minor) {
    const QString targetPath = sessionPath.trimmed().isEmpty()
                                   ? QString::fromLatin1(kLogindAutoSessionPath)
                                   : sessionPath.trimmed();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindService),
        targetPath,
        QString::fromLatin1(kLogindSessionInterface),
        QStringLiteral("ReleaseDevice"));
    msg << QVariant::fromValue<uint>(major) << QVariant::fromValue<uint>(minor);
    return msg;
}

QDBusMessage LogindSessionService::buildPauseDeviceCompleteMessage(const QString &sessionPath,
                                                                   quint32 major,
                                                                   quint32 minor) {
    const QString targetPath = sessionPath.trimmed().isEmpty()
                                   ? QString::fromLatin1(kLogindAutoSessionPath)
                                   : sessionPath.trimmed();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindService),
        targetPath,
        QString::fromLatin1(kLogindSessionInterface),
        QStringLiteral("PauseDeviceComplete"));
    msg << QVariant::fromValue<uint>(major) << QVariant::fromValue<uint>(minor);
    return msg;
}

QString LogindSessionService::messageSignature(const QDBusMessage &message) {
    if (!message.signature().isEmpty()) {
        return message.signature();
    }
    QString sig;
    const QList<QVariant> args = message.arguments();
    for (const QVariant &arg : args) {
        const int typeId = arg.metaType().id();
        if (typeId == QMetaType::Bool) {
            sig.append(QLatin1Char('b'));
        } else if (typeId == QMetaType::UInt) {
            sig.append(QLatin1Char('u'));
        } else if (typeId == QMetaType::Int) {
            sig.append(QLatin1Char('i'));
        } else if (typeId == QMetaType::QString) {
            sig.append(QLatin1Char('s'));
        } else if (arg.userType() == qMetaTypeId<QDBusObjectPath>()) {
            sig.append(QLatin1Char('o'));
        } else if (arg.userType() == qMetaTypeId<QDBusUnixFileDescriptor>()) {
            sig.append(QLatin1Char('h'));
        }
    }
    return sig;
}

bool LogindSessionService::statDeviceMajorMinor(const QString &devicePath,
                                                quint32 *majorOut,
                                                quint32 *minorOut) noexcept {
    if (!majorOut || !minorOut) {
        return false;
    }
    const QString trimmed = devicePath.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    struct stat st {};
    if (::stat(trimmed.toLocal8Bit().constData(), &st) == 0
        && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))) {
        *majorOut = static_cast<quint32>(major(st.st_rdev));
        *minorOut = static_cast<quint32>(minor(st.st_rdev));
        return true;
    }

    // Fallback determinístico para caminhos padrão Linux DRM e evdev em ambientes container/CI
    static const QRegularExpression kDrmCardRegex(QStringLiteral(R"(^/dev/dri/card(\d+)$)"));
    const QRegularExpressionMatch drmMatch = kDrmCardRegex.match(trimmed);
    if (drmMatch.hasMatch()) {
        bool ok = false;
        const quint32 cardIndex = drmMatch.captured(1).toUInt(&ok);
        if (ok) {
            *majorOut = kDrmMajorNumber;
            *minorOut = cardIndex;
            return true;
        }
    }

    static const QRegularExpression kInputEventRegex(QStringLiteral(R"(^/dev/input/event(\d+)$)"));
    const QRegularExpressionMatch inputMatch = kInputEventRegex.match(trimmed);
    if (inputMatch.hasMatch()) {
        bool ok = false;
        const quint32 eventIndex = inputMatch.captured(1).toUInt(&ok);
        if (ok) {
            *majorOut = kInputMajorNumber;
            *minorOut = 64u + eventIndex;
            return true;
        }
    }

    return false;
}

void LogindSessionService::setCustomCallHandler(DbusCallHandler handler) {
    m_customCallHandler = std::move(handler);
}

void LogindSessionService::clearCustomCallHandler() noexcept {
    m_customCallHandler = nullptr;
}

QDBusMessage LogindSessionService::dispatchDbusCall(const QDBusMessage &request) {
    m_outboundMessages.push_back(request);
    if (m_customCallHandler) {
        return m_customCallHandler(request);
    }
    if (!m_hasBusConnection || !m_connection.isConnected()) {
        return QDBusMessage::createError(
            QDBusError::Disconnected,
            QStringLiteral("System D-Bus connection is not available"));
    }
    return m_connection.call(request, QDBus::Block, 3000);
}

QString LogindSessionService::resolveSessionPathForPid(quint32 pid) {
    const quint32 targetPid = (pid == 0u)
                                  ? static_cast<quint32>(QCoreApplication::applicationPid())
                                  : pid;
    const QDBusMessage reply = dispatchDbusCall(buildGetSessionByPidMessage(targetPid));
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        const QVariant firstArg = reply.arguments().constFirst();
        QString resolvedPath;
        if (firstArg.userType() == qMetaTypeId<QDBusObjectPath>()) {
            resolvedPath = firstArg.value<QDBusObjectPath>().path();
        } else {
            resolvedPath = firstArg.toString();
        }
        if (!resolvedPath.trimmed().isEmpty()) {
            if (m_sessionPath != resolvedPath) {
                m_sessionPath = resolvedPath;
                emit sessionPathChanged(m_sessionPath);
            }
            return m_sessionPath;
        }
    }

    // Fallback canônico do systemd v243+: /org/freedesktop/login1/session/auto
    if (m_sessionPath.isEmpty()) {
        m_sessionPath = QString::fromLatin1(kLogindAutoSessionPath);
        emit sessionPathChanged(m_sessionPath);
    }
    return m_sessionPath;
}

bool LogindSessionService::initializeSession(bool forceTakeControl) {
    resolveSessionPathForPid(0);
    subscribeSessionSignals();
    setSessionState(Connected);

    if (!takeControl(forceTakeControl)) {
        qWarning().noquote()
            << "[SGE-LOGIND] Não foi possível assumir controlo da sessão via TakeControl em"
            << m_sessionPath;
        return false;
    }

    m_sessionActive = true;
    emit sessionActiveChanged(true);
    setGpuSubmissionSuspended(false);
    setSessionState(Active);
    return true;
}

bool LogindSessionService::takeControl(bool force) {
    const QDBusMessage reply = dispatchDbusCall(buildTakeControlMessage(m_sessionPath, force));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning().noquote() << "[SGE-LOGIND] TakeControl falhou:" << reply.errorName()
                             << reply.errorMessage();
        return false;
    }

    if (!m_hasControl) {
        m_hasControl = true;
        emit sessionControlChanged(true);
    }
    if (!m_sessionActive) {
        m_sessionActive = true;
        emit sessionActiveChanged(true);
    }
    setGpuSubmissionSuspended(false);
    setSessionState(Active);
    return true;
}

bool LogindSessionService::releaseControl() {
    if (!m_hasControl) {
        return true;
    }

    releaseAllDevices();

    const QDBusMessage reply = dispatchDbusCall(buildReleaseControlMessage(m_sessionPath));
    m_hasControl = false;
    m_sessionActive = false;
    emit sessionControlChanged(false);
    emit sessionActiveChanged(false);
    setSessionState(Disconnected);

    return reply.type() != QDBusMessage::ErrorMessage;
}

int LogindSessionService::takeDevice(quint32 major, quint32 minor, bool *inactiveOut) {
    if (inactiveOut) {
        *inactiveOut = false;
    }

    const QDBusMessage reply = dispatchDbusCall(buildTakeDeviceMessage(m_sessionPath, major, minor));
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        qWarning().noquote() << "[SGE-LOGIND] TakeDevice(" << major << "," << minor << ") falhou:"
                             << reply.errorName() << reply.errorMessage();
        return -1;
    }

    int extractedFd = -1;
    bool inactive = false;

    const QList<QVariant> args = reply.arguments();
    const QVariant &first = args.constFirst();
    if (first.userType() == qMetaTypeId<QDBusUnixFileDescriptor>()) {
        const QDBusUnixFileDescriptor dbusFd = first.value<QDBusUnixFileDescriptor>();
        if (dbusFd.isValid()) {
            extractedFd = duplicateFdCloexec(dbusFd.fileDescriptor());
        }
    } else if (first.metaType().id() == QMetaType::Int) {
        extractedFd = duplicateFdCloexec(first.toInt());
    }

    if (args.size() >= 2) {
        inactive = args.at(1).toBool();
    }
    if (inactiveOut) {
        *inactiveOut = inactive;
    }

    if (extractedFd < 0) {
        return -1;
    }

    const quint64 key = deviceKey(major, minor);
    auto existingIt = m_devices.find(key);
    if (existingIt != m_devices.end() && existingIt->fd >= 0 && existingIt->fd != extractedFd) {
        ::close(existingIt->fd);
    }

    ManagedDevice dev;
    dev.major = major;
    dev.minor = minor;
    dev.fd = extractedFd;
    dev.paused = inactive;
    m_devices.insert(key, dev);

    if (major == kDrmMajorNumber && m_primaryDrmFd < 0) {
        m_primaryDrmFd = extractedFd;
    }

    emit devicesChanged();
    return extractedFd;
}

int LogindSessionService::takeDeviceByPath(const QString &devicePath, bool *inactiveOut) {
    quint32 majorNum = 0;
    quint32 minorNum = 0;
    if (!statDeviceMajorMinor(devicePath, &majorNum, &minorNum)) {
        return -1;
    }

    const int fd = takeDevice(majorNum, minorNum, inactiveOut);
    if (fd >= 0) {
        const quint64 key = deviceKey(majorNum, minorNum);
        if (m_devices.contains(key)) {
            m_devices[key].devicePath = devicePath;
        }
    }
    return fd;
}

int LogindSessionService::acquirePrimaryDrmDevice(const QString &drmPath) {
    const int fd = takeDeviceByPath(drmPath);
    if (fd >= 0) {
        m_primaryDrmFd = fd;
    }
    return fd;
}

bool LogindSessionService::releaseDevice(quint32 major, quint32 minor) {
    const quint64 key = deviceKey(major, minor);
    auto it = m_devices.find(key);
    if (it != m_devices.end()) {
        if (it->fd >= 0) {
            if (it->fd == m_primaryDrmFd) {
                m_primaryDrmFd = -1;
            }
            ::close(it->fd);
        }
        m_devices.erase(it);
        emit devicesChanged();
    }

    const QDBusMessage reply = dispatchDbusCall(buildReleaseDeviceMessage(m_sessionPath, major, minor));
    return reply.type() != QDBusMessage::ErrorMessage;
}

bool LogindSessionService::releaseDeviceByFd(int fd) {
    if (fd < 0) {
        return false;
    }
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        if (it->fd == fd) {
            return releaseDevice(it->major, it->minor);
        }
    }
    return false;
}

void LogindSessionService::releaseAllDevices() {
    const QList<ManagedDevice> devicesSnapshot = m_devices.values();
    for (const ManagedDevice &dev : devicesSnapshot) {
        releaseDevice(dev.major, dev.minor);
    }
}

bool LogindSessionService::pauseDeviceComplete(quint32 major, quint32 minor) {
    const QDBusMessage reply = dispatchDbusCall(
        buildPauseDeviceCompleteMessage(m_sessionPath, major, minor));
    ++m_pauseCompleteAckCount;
    return reply.type() != QDBusMessage::ErrorMessage;
}

int LogindSessionService::deviceFileDescriptor(quint32 major, quint32 minor) const noexcept {
    const auto it = m_devices.constFind(deviceKey(major, minor));
    if (it == m_devices.cend()) {
        return -1;
    }
    return it->fd;
}

bool LogindSessionService::isDevicePaused(quint32 major, quint32 minor) const noexcept {
    const auto it = m_devices.constFind(deviceKey(major, minor));
    if (it == m_devices.cend()) {
        return false;
    }
    return it->paused;
}

void LogindSessionService::onPauseDevice(uint major, uint minor, const QString &type) {
    const quint64 key = deviceKey(major, minor);
    if (type == QLatin1String("gone")) {
        auto it = m_devices.find(key);
        if (it != m_devices.end()) {
            if (it->fd >= 0) {
                if (it->fd == m_primaryDrmFd) {
                    m_primaryDrmFd = -1;
                }
                ::close(it->fd);
            }
            m_devices.erase(it);
            emit devicesChanged();
        }
        emit devicePaused(major, minor, type);
        return;
    }

    auto it = m_devices.find(key);
    if (it != m_devices.end()) {
        it->paused = true;
    } else {
        ManagedDevice dev;
        dev.major = major;
        dev.minor = minor;
        dev.fd = -1;
        dev.paused = true;
        m_devices.insert(key, dev);
    }

    // Suspende imediatamente a submissão de comandos KMS/GPU antes de confirmar ao logind
    setGpuSubmissionSuspended(true);
    setSessionState(Suspended);

    if (type == QLatin1String("pause")) {
        pauseDeviceComplete(major, minor);
    }

    emit devicePaused(major, minor, type);
}

void LogindSessionService::onResumeDevice(uint major,
                                          uint minor,
                                          const QDBusUnixFileDescriptor &fd) {
    int resumedFd = -1;
    if (fd.isValid()) {
        resumedFd = duplicateFdCloexec(fd.fileDescriptor());
    }

    const quint64 key = deviceKey(major, minor);
    auto it = m_devices.find(key);
    if (it != m_devices.end()) {
        if (resumedFd >= 0 && it->fd >= 0 && it->fd != resumedFd) {
            ::close(it->fd);
        }
        if (resumedFd >= 0) {
            it->fd = resumedFd;
        }
        it->paused = false;
    } else {
        ManagedDevice dev;
        dev.major = major;
        dev.minor = minor;
        dev.fd = resumedFd;
        dev.paused = false;
        m_devices.insert(key, dev);
    }

    const int effectiveFd = m_devices.value(key).fd;
    if (major == kDrmMajorNumber && effectiveFd >= 0) {
        m_primaryDrmFd = effectiveFd;
    }

    bool anyDrmPaused = false;
    for (auto devIt = m_devices.cbegin(); devIt != m_devices.cend(); ++devIt) {
        if (devIt->major == kDrmMajorNumber && devIt->paused) {
            anyDrmPaused = true;
            break;
        }
    }

    if (!anyDrmPaused) {
        setGpuSubmissionSuspended(false);
        setSessionState(Active);
    }

    emit devicesChanged();
    emit deviceResumed(major, minor, effectiveFd);
    if (major == kDrmMajorNumber || !anyDrmPaused) {
        emit swapchainRestoreRequested(major, minor, effectiveFd);
    }
}

void LogindSessionService::onPropertiesChanged(const QString &interfaceName,
                                               const QVariantMap &changedProperties,
                                               const QStringList &invalidatedProperties) {
    Q_UNUSED(invalidatedProperties);
    if (interfaceName != QLatin1String(kLogindSessionInterface)) {
        return;
    }

    if (changedProperties.contains(QStringLiteral("Active"))) {
        const bool activeNow = changedProperties.value(QStringLiteral("Active")).toBool();
        if (m_sessionActive != activeNow) {
            m_sessionActive = activeNow;
            emit sessionActiveChanged(m_sessionActive);
        }
        if (!activeNow) {
            setGpuSubmissionSuspended(true);
            setSessionState(Suspended);
        } else {
            setGpuSubmissionSuspended(false);
            setSessionState(Active);
        }
    }
}

void LogindSessionService::subscribeSessionSignals() {
    if (!m_hasBusConnection || !m_connection.isConnected()) {
        return;
    }

    m_connection.connect(
        QString::fromLatin1(kLogindService),
        m_sessionPath,
        QString::fromLatin1(kLogindSessionInterface),
        QStringLiteral("PauseDevice"),
        this,
        SLOT(onPauseDevice(uint,uint,QString)));

    m_connection.connect(
        QString::fromLatin1(kLogindService),
        m_sessionPath,
        QString::fromLatin1(kLogindSessionInterface),
        QStringLiteral("ResumeDevice"),
        this,
        SLOT(onResumeDevice(uint,uint,QDBusUnixFileDescriptor)));

    m_connection.connect(
        QString::fromLatin1(kLogindService),
        m_sessionPath,
        QString::fromLatin1(kDbusPropertiesInterface),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
}

void LogindSessionService::setSessionState(SessionState newState) {
    if (m_sessionState == newState) {
        return;
    }
    m_sessionState = newState;
    emit sessionStateChanged(m_sessionState);
}

void LogindSessionService::setGpuSubmissionSuspended(bool suspended) {
    if (m_gpuSubmissionSuspended == suspended) {
        return;
    }
    m_gpuSubmissionSuspended = suspended;
    emit gpuSubmissionSuspendedChanged(m_gpuSubmissionSuspended);
}

