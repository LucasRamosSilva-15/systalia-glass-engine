#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <atomic>
#include <string_view>

class QQuickWindow;

class SystemTelemetryService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(float cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(float ramUsageGb READ ramUsageGb NOTIFY ramUsageGbChanged)
    Q_PROPERTY(float ramTotalGb READ ramTotalGb NOTIFY ramTotalGbChanged)
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(QString systemTime READ systemTime NOTIFY systemTimeChanged)

public:
    struct CpuCounters {
        quint64 total = 0;
        quint64 idle = 0;
    };

    struct MemoryCounters {
        quint64 totalKiB = 0;
        quint64 availableKiB = 0;
    };

    explicit SystemTelemetryService(QObject *parent = nullptr);

    float cpuUsage() const { return m_cpuUsage; }
    float ramUsageGb() const { return m_ramUsageGb; }
    float ramTotalGb() const { return m_ramTotalGb; }
    int fps() const { return m_fps; }
    QString systemTime() const { return m_systemTime; }

    Q_INVOKABLE void attachWindow(QQuickWindow *window);

    static bool parseCpuStat(std::string_view line, CpuCounters &counters) noexcept;
    static float calculateCpuUsage(const CpuCounters &previous,
                                   const CpuCounters &current) noexcept;
    static bool parseMemInfo(std::string_view contents, MemoryCounters &counters) noexcept;
    static float kibibytesToGigabytes(quint64 kibibytes) noexcept;
    static QString formatSystemTime(const QDateTime &dateTime);

signals:
    void cpuUsageChanged();
    void ramUsageGbChanged();
    void ramTotalGbChanged();
    void fpsChanged();
    void systemTimeChanged();

private slots:
    void sampleSystem();

private:
    bool readCpuStat(CpuCounters &counters);
    bool readMemInfo(MemoryCounters &counters);
    void setCpuUsage(float value);
    void setMemory(float usedGb, float totalGb);
    void setFps(int value);
    void setSystemTime(const QString &value);

    QFile m_cpuStatFile{QStringLiteral("/proc/stat")};
    QFile m_memInfoFile{QStringLiteral("/proc/meminfo")};
    QTimer m_sampleTimer;
    QElapsedTimer m_fpsSamplingClock;
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_frameSwapConnection;
    std::atomic<quint32> m_pendingFrames{0};
    CpuCounters m_previousCpuCounters;
    bool m_havePreviousCpuCounters = false;
    float m_cpuUsage = 0.0f;
    float m_ramUsageGb = 0.0f;
    float m_ramTotalGb = 0.0f;
    int m_fps = 0;
    QString m_systemTime;
};
