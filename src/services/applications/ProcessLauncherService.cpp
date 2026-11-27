#include "ProcessLauncherService.hpp"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
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

QString ProcessLauncherService::waylandDisplay() const {
    return m_waylandDisplay;
}

void ProcessLauncherService::setWaylandDisplay(const QString &display) {
    const QString trimmed = display.trimmed();
    const QString next = trimmed.isEmpty() ? QStringLiteral("wayland-systalia-0") : trimmed;
    if (m_waylandDisplay == next) {
        return;
    }
    m_waylandDisplay = next;
    emit waylandDisplayChanged();
}

bool ProcessLauncherService::isolatedNestedSession() const {
    return m_isolatedNestedSession;
}

void ProcessLauncherService::setIsolatedNestedSession(bool isolated) {
    if (m_isolatedNestedSession == isolated) {
        return;
    }
    m_isolatedNestedSession = isolated;
    emit isolatedNestedSessionChanged();
}

bool ProcessLauncherService::launch(const QString &command, const QStringList &arguments) {
    const QString trimmedCommand = command.trimmed();
    const QString appId = applicationIdForCommand(trimmedCommand);
    if (trimmedCommand.isEmpty() || appId.isEmpty()) {
        return false;
    }

    const QStringList launchArgs = effectiveArgumentsForLaunch(
        trimmedCommand, arguments, m_waylandDisplay, m_isolatedNestedSession);
    if (m_isolatedNestedSession && (appId == QStringLiteral("code") || appId == QStringLiteral("codium"))) {
        for (const QString &arg : launchArgs) {
            if (arg.startsWith(QStringLiteral("--user-data-dir="))) {
                QDir().mkpath(arg.mid(16));
                break;
            }
        }
    }

    QProcess process;
    process.setProgram(trimmedCommand);
    process.setArguments(launchArgs);
    process.setProcessEnvironment(waylandProcessEnvironment(m_waylandDisplay, m_isolatedNestedSession));

    qint64 processId = 0;
    if (!process.startDetached(&processId) || processId <= 0) {
        qWarning() << "[SGE-LAUNCHER] Falha ao iniciar aplicativo:" << trimmedCommand
                   << "WAYLAND_DISPLAY=" << m_waylandDisplay;
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

bool ProcessLauncherService::launchInTerminal(const QString &command, const QStringList &arguments) {
    if (command.trimmed().isEmpty())
        return false;
    // Each command and argument stays a separate argv entry; no shell interpolation.
    const QStringList candidates{QStringLiteral("xdg-terminal-exec"), QStringLiteral("foot"),
        QStringLiteral("alacritty"), QStringLiteral("konsole"), QStringLiteral("gnome-terminal"),
        QStringLiteral("xterm")};
    for (const QString &candidate : candidates) {
        const QString executable = QStandardPaths::findExecutable(candidate);
        if (executable.isEmpty())
            continue;
        QStringList terminalArguments;
        if (candidate == QStringLiteral("gnome-terminal"))
            terminalArguments.append(QStringLiteral("--"));
        else if (candidate != QStringLiteral("xdg-terminal-exec") && candidate != QStringLiteral("foot"))
            terminalArguments.append(QStringLiteral("-e"));
        terminalArguments.append(command);
        terminalArguments.append(arguments);
        if (launch(executable, terminalArguments))
            return true;
    }
    return false;
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

QProcessEnvironment ProcessLauncherService::waylandProcessEnvironment(const QString &waylandDisplay,
                                                                      bool isolatedNestedSession) {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QSG_RHI_BACKEND"));
    const QString resolvedDisplay = waylandDisplay.trimmed().isEmpty()
                                        ? QStringLiteral("wayland-systalia-0")
                                        : waylandDisplay.trimmed();
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), resolvedDisplay);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("GDK_BACKEND"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("ELECTRON_OZONE_PLATFORM_HINT"), QStringLiteral("wayland"));
    if (isolatedNestedSession) {
        environment.remove(QStringLiteral("DISPLAY"));
    }
    return environment;
}

QStringList ProcessLauncherService::effectiveArgumentsForLaunch(const QString &command,
                                                                const QStringList &arguments,
                                                                const QString &waylandDisplay,
                                                                bool isolatedNestedSession) {
    QStringList result = arguments;
    if (!isolatedNestedSession) {
        return result;
    }
    const QString appId = applicationIdForCommand(command);
    if (appId != QStringLiteral("code") && appId != QStringLiteral("codium")) {
        return result;
    }

    const bool hasUserDataDir = std::any_of(result.cbegin(), result.cend(), [](const QString &arg) {
        return arg == QStringLiteral("--user-data-dir")
               || arg.startsWith(QStringLiteral("--user-data-dir="));
    });
    if (!hasUserDataDir) {
        const QString runtimeBase = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        const QString baseDir = runtimeBase.isEmpty() ? QDir::tempPath() : runtimeBase;
        const QString socketSegment = waylandDisplay.trimmed().isEmpty()
                                          ? QStringLiteral("default")
                                          : waylandDisplay.trimmed();
        result.append(QStringLiteral("--user-data-dir=%1/sge-nested-apps/%2/%3")
                          .arg(baseDir, socketSegment, appId));
    }
    if (!result.contains(QStringLiteral("--new-window"))) {
        result.append(QStringLiteral("--new-window"));
    }
    const bool hasOzone = std::any_of(result.cbegin(), result.cend(), [](const QString &arg) {
        return arg.startsWith(QStringLiteral("--ozone-platform="));
    });
    if (!hasOzone) {
        result.append(QStringLiteral("--ozone-platform=wayland"));
    }
    return result;
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
