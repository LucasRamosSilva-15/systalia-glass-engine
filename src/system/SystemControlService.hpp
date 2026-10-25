#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>

class SystemControlService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool isMuted READ isMuted NOTIFY isMutedChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY brightnessChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY bluetoothEnabledChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)

public:
    explicit SystemControlService(QObject *parent = nullptr);

    int volume() const { return m_volume; }
    bool isMuted() const { return m_isMuted; }
    int brightness() const { return m_brightness; }
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool bluetoothEnabled() const { return m_bluetoothEnabled; }
    bool doNotDisturb() const { return m_doNotDisturb; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setVolume(int value);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void setBrightness(int value);
    Q_INVOKABLE void toggleWifi();
    Q_INVOKABLE void toggleBluetooth();
    Q_INVOKABLE void setDoNotDisturb(bool enabled);

signals:
    void volumeChanged();
    void isMutedChanged();
    void brightnessChanged();
    void wifiEnabledChanged();
    void bluetoothEnabledChanged();
    void doNotDisturbChanged();

private:
    using CommandFinished = std::function<void(int, const QByteArray &, const QByteArray &)>;

    void runCommand(const QString &program, const QStringList &arguments,
                    CommandFinished callback);
    void probeAudio();
    void probePactlVolume();
    void probeBrightness();
    void probeWifi();
    void probeBluetooth();
    void applyVolume();
    void applyMute();
    void applyBrightness();
    void setVolumeValue(int value);
    void setMutedValue(bool value);
    void setBrightnessValue(int value);
    void setWifiValue(bool value);
    void setBluetoothValue(bool value);
    static int percentFromOutput(const QByteArray &output);

    QString m_wpctlPath;
    QString m_pactlPath;
    QString m_brightnessctlPath;
    QString m_nmcliPath;
    QString m_bluetoothctlPath;
    int m_volume = 50;
    bool m_isMuted = false;
    int m_brightness = 70;
    bool m_wifiEnabled = false;
    bool m_bluetoothEnabled = false;
    bool m_doNotDisturb = false;
    int m_requestedVolume = 50;
    int m_requestedBrightness = 70;
    bool m_audioProbeRunning = false;
    bool m_brightnessProbeRunning = false;
    bool m_wifiProbeRunning = false;
    bool m_bluetoothProbeRunning = false;
    QTimer m_refreshTimer;
    QTimer m_volumeDebounce;
    QTimer m_brightnessDebounce;
};
