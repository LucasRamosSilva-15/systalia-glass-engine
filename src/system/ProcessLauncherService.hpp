#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

class ProcessLauncherService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList runningApplications READ runningApplications NOTIFY runningApplicationsChanged)

public:
    explicit ProcessLauncherService(QObject *parent = nullptr);

    QStringList runningApplications() const;
    Q_INVOKABLE bool launch(const QString &command, const QStringList &arguments = {});
    Q_INVOKABLE bool isAppRunning(const QString &appId) const;

    static QProcessEnvironment waylandProcessEnvironment();
    static QString applicationIdForCommand(const QString &command);

signals:
    void processStarted(const QString &appId, qint64 processId);
    void processFinished(const QString &appId, qint64 processId);
    void runningApplicationsChanged();

private slots:
    void pollProcesses();

private:
    struct TrackedProcess {
        qint64 processId = 0;
        QString appId;
    };

    static QString normalizeAppId(const QString &appId);
    static bool isProcessAlive(qint64 processId);

    QTimer m_pollTimer;
    QVector<TrackedProcess> m_processes;
};
