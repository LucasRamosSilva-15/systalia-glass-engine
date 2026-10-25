#include "ProcessLauncherService.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QProcess>
#include <QSet>
#include <cerrno>

#if defined(Q_OS_UNIX)
#include <signal.h>
#include <sys/types.h>
#endif

ProcessLauncherService::ProcessLauncherService(QObject *parent)
    : QObject(parent) {
    m_pollTimer.setInterval(100);
    m_pollTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_pollTimer, &QTimer::timeout,
            this, &ProcessLauncherService::pollProcesses);
}

QStringList ProcessLauncherService::runningApplications() const {
    QSet<QString> appIds;
    for (const TrackedProcess &process : m_processes) {
        if (isProcessAlive(process.processId)) {
            appIds.insert(process.appId);
        }
    }

    QStringList result = appIds.values();
    result.sort(Qt::CaseInsensitive);
    return result;
}

bool ProcessLauncherService::launch(const QString &command, const QStringList &arguments) {
    const QString trimmedCommand = command.trimmed();
    const QString appId = applicationIdForCommand(trimmedCommand);
    if (trimmedCommand.isEmpty() || appId.isEmpty()) {
        return false;
    }

    QProcess process;
    process.setProgram(trimmedCommand);
    process.setArguments(arguments);
    process.setProcessEnvironment(waylandProcessEnvironment());

    qint64 processId = 0;
    if (!process.startDetached(&processId) || processId <= 0) {
        return false;
    }

    const bool wasRunning = isAppRunning(appId);
    m_processes.append({processId, appId});
    if (!wasRunning) {
        emit runningApplicationsChanged();
    }
    emit processStarted(appId, processId);
    m_pollTimer.start();
    return true;
}

bool ProcessLauncherService::isAppRunning(const QString &appId) const {
    const QString normalizedId = normalizeAppId(appId);
    if (normalizedId.isEmpty()) {
        return false;
    }

    for (const TrackedProcess &process : m_processes) {
        if (process.appId == normalizedId && isProcessAlive(process.processId)) {
            return true;
        }
    }
    return false;
}

QProcessEnvironment ProcessLauncherService::waylandProcessEnvironment() {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QSG_RHI_BACKEND"));
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("wayland-systalia-0"));
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("GDK_BACKEND"), QStringLiteral("wayland"));
    return environment;
}

QString ProcessLauncherService::applicationIdForCommand(const QString &command) {
    return normalizeAppId(QFileInfo(command).fileName());
}

void ProcessLauncherService::pollProcesses() {
    bool anyFinished = false;

    for (qsizetype index = m_processes.size(); index > 0; --index) {
        const qsizetype processIndex = index - 1;
        const TrackedProcess process = m_processes.at(processIndex);
        if (isProcessAlive(process.processId)) {
            continue;
        }

        m_processes.removeAt(processIndex);
        anyFinished = true;
        emit processFinished(process.appId, process.processId);
    }

    if (anyFinished) {
        emit runningApplicationsChanged();
    }
    if (m_processes.isEmpty()) {
        m_pollTimer.stop();
    }
}

QString ProcessLauncherService::normalizeAppId(const QString &appId) {
    QString normalizedId = QFileInfo(appId.trimmed()).fileName().toCaseFolded();
    if (normalizedId.endsWith(QStringLiteral(".desktop"))) {
        normalizedId.chop(8);
    }
    return normalizedId;
}

bool ProcessLauncherService::isProcessAlive(qint64 processId) {
    if (processId <= 0) {
        return false;
    }

#if defined(Q_OS_UNIX)
    if (::kill(static_cast<pid_t>(processId), 0) != 0 && errno != EPERM) {
        return false;
    }

    QFile processStat(QStringLiteral("/proc/%1/stat").arg(processId));
    if (processStat.open(QIODevice::ReadOnly)) {
        const QByteArray contents = processStat.readAll();
        const qsizetype commandEnd = contents.lastIndexOf(')');
        const qsizetype stateIndex = commandEnd + 2;
        if (commandEnd >= 0 && stateIndex < contents.size()) {
            const char state = contents.at(stateIndex);
            return state != 'Z' && state != 'X';
        }
    }
    return true;
#else
    Q_UNUSED(processId);
    return false;
#endif
}
