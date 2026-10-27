#pragma once

#include <QAbstractListModel>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

class NotificationServer final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int notificationCount READ notificationCount NOTIFY notificationCountChanged)

public:
    enum Role {
        NotificationIdRole = Qt::UserRole + 1,
        AppNameRole,
        SummaryRole,
        BodyRole,
        AppIconRole,
        TimeoutRole
    };
    Q_ENUM(Role)

    struct Notification {
        uint id = 0;
        QString appName;
        QString summary;
        QString body;
        QString appIcon;
        int timeout = 0;
    };

    explicit NotificationServer(bool registerOnSessionBus = true, QObject *parent = nullptr);
    ~NotificationServer() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int notificationCount() const;
    bool isServiceRegistered() const { return m_registeredService && m_registeredObject; }

    Q_INVOKABLE void closeNotificationFromUi(uint id);

public slots:
    uint Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                const QString &summary, const QString &body, const QStringList &actions,
                const QVariantMap &hints, int expire_timeout);
    void CloseNotification(uint id);
    QStringList GetCapabilities();
    void GetServerInformation(QString &name, QString &vendor, QString &version,
                              QString &spec_version);

signals:
    void NotificationClosed(uint id, uint reason);
    void notificationCountChanged();

private:
    int rowForId(uint id) const;
    int effectiveTimeout(int requestedTimeout) const;
    void startExpirationTimer(uint id, int timeout);
    void removeNotification(uint id, uint reason);

    QDBusConnection m_sessionBus;
    QVector<Notification> m_notifications;
    QHash<uint, QTimer *> m_expirationTimers;
    uint m_nextId = 1;
    bool m_registeredService = false;
    bool m_registeredObject = false;
};

class NotificationServerAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationServerAdaptor(NotificationServer *server);

public slots:
    uint Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                const QString &summary, const QString &body, const QStringList &actions,
                const QVariantMap &hints, int expire_timeout);
    void CloseNotification(uint id);
    QStringList GetCapabilities();
    void GetServerInformation(QString &name, QString &vendor, QString &version,
                              QString &spec_version);

signals:
    void NotificationClosed(uint id, uint reason);

private:
    NotificationServer *m_server = nullptr;
};
