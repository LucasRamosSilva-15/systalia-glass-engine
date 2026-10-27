#include "SystemControlService.hpp"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtMath>
#include <algorithm>
#include <memory>

SystemControlService::SystemControlService(QObject *parent)
    : QObject(parent),
      m_wpctlPath(QStandardPaths::findExecutable(QStringLiteral("wpctl"))),
      m_pactlPath(QStandardPaths::findExecutable(QStringLiteral("pactl"))),
      m_brightnessctlPath(QStandardPaths::findExecutable(QStringLiteral("brightnessctl"))),
      m_nmcliPath(QStandardPaths::findExecutable(QStringLiteral("nmcli"))),
      m_bluetoothctlPath(QStandardPaths::findExecutable(QStringLiteral("bluetoothctl"))) {
    m_refreshTimer.setInterval(2500);
    m_refreshTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, &SystemControlService::refresh);

    m_volumeDebounce.setSingleShot(true);
    m_volumeDebounce.setInterval(120);
    connect(&m_volumeDebounce, &QTimer::timeout, this, &SystemControlService::applyVolume);

    m_brightnessDebounce.setSingleShot(true);
    m_brightnessDebounce.setInterval(120);
    connect(&m_brightnessDebounce, &QTimer::timeout,
            this, &SystemControlService::applyBrightness);

    refresh();
    m_refreshTimer.start();
}

void SystemControlService::refresh() {
    probeAudio();
    probeBrightness();
    probeWifi();
    probeBluetooth();
}

void SystemControlService::setVolume(int value) {
    m_requestedVolume = std::clamp(value, 0, 100);
    setVolumeValue(m_requestedVolume);
    m_volumeDebounce.start();
}

void SystemControlService::toggleMute() {
    const bool muted = !m_isMuted;
    setMutedValue(muted);
    applyMute();
}

void SystemControlService::setBrightness(int value) {
    m_requestedBrightness = std::clamp(value, 0, 100);
    setBrightnessValue(m_requestedBrightness);
    m_brightnessDebounce.start();
}

void SystemControlService::toggleWifi() {
    if (!m_nmcliPath.isEmpty()) {
        const bool enabled = !m_wifiEnabled;
        runCommand(m_nmcliPath, {QStringLiteral("radio"), QStringLiteral("wifi"),
                                 enabled ? QStringLiteral("on") : QStringLiteral("off")},
                   [this, enabled](int exitCode, const QByteArray &, const QByteArray &) {
            if (exitCode == 0) {
                setWifiValue(enabled);
            }
        });
    }
}

void SystemControlService::toggleBluetooth() {
    if (!m_bluetoothctlPath.isEmpty()) {
        const bool enabled = !m_bluetoothEnabled;
        runCommand(m_bluetoothctlPath, {QStringLiteral("power"),
                                        enabled ? QStringLiteral("on") : QStringLiteral("off")},
                   [this, enabled](int exitCode, const QByteArray &, const QByteArray &) {
            if (exitCode == 0) {
                setBluetoothValue(enabled);
            }
        });
    }
}

void SystemControlService::setDoNotDisturb(bool enabled) {
    if (m_doNotDisturb == enabled) {
        return;
    }
    m_doNotDisturb = enabled;
    emit doNotDisturbChanged();
}

void SystemControlService::runCommand(const QString &program, const QStringList &arguments,
                                      CommandFinished callback) {
    if (program.isEmpty()) {
        callback(-1, {}, {});
        return;
    }

    auto *process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(arguments);
    const auto completed = std::make_shared<bool>(false);
    const auto completion = std::make_shared<CommandFinished>(std::move(callback));

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [process, completed, completion](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!*completed) {
            *completed = true;
            const int result = exitStatus == QProcess::NormalExit ? exitCode : -1;
            (*completion)(result, process->readAllStandardOutput(), process->readAllStandardError());
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this,
            [process, completed, completion](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !*completed) {
            *completed = true;
            (*completion)(-1, {}, process->errorString().toUtf8());
            process->deleteLater();
        }
    });
    QTimer::singleShot(3000, process, [process]() {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
    process->start();
}

void SystemControlService::probeAudio() {
    if (m_audioProbeRunning) {
        return;
    }
    m_audioProbeRunning = true;

    if (m_wpctlPath.isEmpty()) {
        probePactlVolume();
        return;
    }
    runCommand(m_wpctlPath, {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")} ,
               [this](int exitCode, const QByteArray &output, const QByteArray &) {
        const QString text = QString::fromLocal8Bit(output);
        static const QRegularExpression volumePattern(QStringLiteral("Volume:\\s*([0-9]+(?:\\.[0-9]+)?)"));
        const QRegularExpressionMatch match = volumePattern.match(text);
        if (exitCode == 0 && match.hasMatch()) {
            setVolumeValue(qRound(match.captured(1).toDouble() * 100.0));
            setMutedValue(text.contains(QStringLiteral("[MUTED]"), Qt::CaseInsensitive));
            m_audioProbeRunning = false;
            return;
        }
        probePactlVolume();
    });
}

void SystemControlService::probePactlVolume() {
    if (m_pactlPath.isEmpty()) {
        m_audioProbeRunning = false;
        return;
    }
    runCommand(m_pactlPath, {QStringLiteral("get-sink-volume"), QStringLiteral("@DEFAULT_SINK@")} ,
               [this](int exitCode, const QByteArray &output, const QByteArray &) {
        if (exitCode != 0) {
            m_audioProbeRunning = false;
            return;
        }
        const int percent = percentFromOutput(output);
        if (percent >= 0) {
            setVolumeValue(percent);
        }
        runCommand(m_pactlPath, {QStringLiteral("get-sink-mute"), QStringLiteral("@DEFAULT_SINK@")} ,
                   [this](int muteExitCode, const QByteArray &muteOutput, const QByteArray &) {
            if (muteExitCode == 0) {
                setMutedValue(QString::fromLocal8Bit(muteOutput).trimmed()
                                  .compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0);
            }
            m_audioProbeRunning = false;
        });
    });
}

void SystemControlService::probeBrightness() {
    if (m_brightnessProbeRunning) {
        return;
    }
    m_brightnessProbeRunning = true;
    if (!m_brightnessctlPath.isEmpty()) {
        runCommand(m_brightnessctlPath, {QStringLiteral("-m")},
                   [this](int exitCode, const QByteArray &output, const QByteArray &) {
            const int percent = percentFromOutput(output);
            if (exitCode == 0 && percent >= 0) {
                setBrightnessValue(percent);
            }
            m_brightnessProbeRunning = false;
        });
        return;
    }

    const QString script = QStringLiteral(
        "for entry in /sys/class/backlight/*; do "
        "[ -r \"$entry/brightness\" ] && [ -r \"$entry/max_brightness\" ] || continue; "
        "read current < \"$entry/brightness\"; read maximum < \"$entry/max_brightness\"; "
        "[ \"$maximum\" -gt 0 ] || continue; echo $((current * 100 / maximum)); exit 0; "
        "done; exit 1");
    runCommand(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), script},
               [this](int exitCode, const QByteArray &output, const QByteArray &) {
        bool valid = false;
        const int value = QString::fromLocal8Bit(output).trimmed().toInt(&valid);
        if (exitCode == 0 && valid) {
            setBrightnessValue(value);
        }
        m_brightnessProbeRunning = false;
    });
}

void SystemControlService::probeWifi() {
    if (m_nmcliPath.isEmpty() || m_wifiProbeRunning) {
        return;
    }
    m_wifiProbeRunning = true;
    runCommand(m_nmcliPath, {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("WIFI"),
                             QStringLiteral("radio")},
               [this](int exitCode, const QByteArray &output, const QByteArray &) {
        if (exitCode == 0) {
            setWifiValue(QString::fromLocal8Bit(output).trimmed()
                             .compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0);
        }
        m_wifiProbeRunning = false;
    });
}

void SystemControlService::probeBluetooth() {
    if (m_bluetoothctlPath.isEmpty() || m_bluetoothProbeRunning) {
        return;
    }
    m_bluetoothProbeRunning = true;
    runCommand(m_bluetoothctlPath, {QStringLiteral("show")},
               [this](int exitCode, const QByteArray &output, const QByteArray &) {
        if (exitCode == 0) {
            const QString text = QString::fromLocal8Bit(output);
            static const QRegularExpression poweredPattern(QStringLiteral("Powered:\\s*(yes|no)"),
                                                           QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match = poweredPattern.match(text);
            if (match.hasMatch()) {
                setBluetoothValue(match.captured(1).compare(QStringLiteral("yes"),
                                                             Qt::CaseInsensitive) == 0);
            }
        }
        m_bluetoothProbeRunning = false;
    });
}

void SystemControlService::applyVolume() {
    if (!m_wpctlPath.isEmpty()) {
        runCommand(m_wpctlPath, {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                                 QStringLiteral("%1%").arg(m_requestedVolume)},
                   [this](int, const QByteArray &, const QByteArray &) { probeAudio(); });
    } else if (!m_pactlPath.isEmpty()) {
        runCommand(m_pactlPath, {QStringLiteral("set-sink-volume"), QStringLiteral("@DEFAULT_SINK@"),
                                 QStringLiteral("%1%").arg(m_requestedVolume)},
                   [this](int, const QByteArray &, const QByteArray &) { probeAudio(); });
    }
}

void SystemControlService::applyMute() {
    if (!m_wpctlPath.isEmpty()) {
        runCommand(m_wpctlPath, {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                                 m_isMuted ? QStringLiteral("1") : QStringLiteral("0")},
                   [this](int, const QByteArray &, const QByteArray &) { probeAudio(); });
    } else if (!m_pactlPath.isEmpty()) {
        runCommand(m_pactlPath, {QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"),
                                 m_isMuted ? QStringLiteral("1") : QStringLiteral("0")},
                   [this](int, const QByteArray &, const QByteArray &) { probeAudio(); });
    }
}

void SystemControlService::applyBrightness() {
    if (!m_brightnessctlPath.isEmpty()) {
        runCommand(m_brightnessctlPath, {QStringLiteral("set"),
                                         QStringLiteral("%1%").arg(m_requestedBrightness)},
                   [this](int, const QByteArray &, const QByteArray &) { probeBrightness(); });
        return;
    }

    const QString script = QStringLiteral(
        "for entry in /sys/class/backlight/*; do "
        "[ -r \"$entry/max_brightness\" ] && [ -w \"$entry/brightness\" ] || continue; "
        "read maximum < \"$entry/max_brightness\"; "
        "[ \"$maximum\" -gt 0 ] || continue; "
        "printf '%s\\n' $((maximum * %1 / 100)) > \"$entry/brightness\"; exit $?; "
        "done; exit 1").arg(m_requestedBrightness);
    runCommand(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), script},
               [this](int, const QByteArray &, const QByteArray &) { probeBrightness(); });
}

void SystemControlService::setVolumeValue(int value) {
    const int bounded = std::clamp(value, 0, 100);
    if (m_volume == bounded) {
        return;
    }
    m_volume = bounded;
    emit volumeChanged();
}

void SystemControlService::setMutedValue(bool value) {
    if (m_isMuted == value) {
        return;
    }
    m_isMuted = value;
    emit isMutedChanged();
}

void SystemControlService::setBrightnessValue(int value) {
    const int bounded = std::clamp(value, 0, 100);
    if (m_brightness == bounded) {
        return;
    }
    m_brightness = bounded;
    emit brightnessChanged();
}

void SystemControlService::setWifiValue(bool value) {
    if (m_wifiEnabled == value) {
        return;
    }
    m_wifiEnabled = value;
    emit wifiEnabledChanged();
}

void SystemControlService::setBluetoothValue(bool value) {
    if (m_bluetoothEnabled == value) {
        return;
    }
    m_bluetoothEnabled = value;
    emit bluetoothEnabledChanged();
}

int SystemControlService::percentFromOutput(const QByteArray &output) {
    static const QRegularExpression percentPattern(QStringLiteral("([0-9]{1,3})%"));
    const QRegularExpressionMatch match = percentPattern.match(QString::fromLocal8Bit(output));
    if (!match.hasMatch()) {
        return -1;
    }
    return std::clamp(match.captured(1).toInt(), 0, 100);
}
