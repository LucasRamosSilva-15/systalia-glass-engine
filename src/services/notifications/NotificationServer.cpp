#include "NotificationServer.hpp"

#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusReply>
#include <QDebug>
#include <QMetaType>

namespace {

constexpr auto notificationService = "org.freedesktop.Notifications";
constexpr auto notificationObjectPath = "/org/freedesktop/Notifications";

} // namespace

NotificationServer::NotificationServer(bool registerOnSessionBus, QObject *parent)
    : QAbstractListModel(parent),
      m_sessionBus(QDBusConnection::sessionBus()) {
    auto *adaptor = new NotificationServerAdaptor(this);
    connect(this, &NotificationServer::NotificationClosed,
            adaptor, &NotificationServerAdaptor::NotificationClosed);

    if (!registerOnSessionBus) {
        return;
    }

    if (!m_sessionBus.isConnected()) {
        qWarning().noquote() << "[SGE-DBUS] Sessão D-Bus indisponível:" << m_sessionBus.lastError().message();
        return;
    }

    m_registeredService = m_sessionBus.registerService(QString::fromLatin1(notificationService));
    if (!m_registeredService) {
        const QString serviceName = QString::fromLatin1(notificationService);
        const QDBusError registrationError = m_sessionBus.lastError();
        QDBusConnectionInterface *busInterface = m_sessionBus.interface();
        const QDBusReply<QString> ownerReply = busInterface
            ? busInterface->serviceOwner(serviceName) : QDBusReply<QString>();
        if (ownerReply.isValid() && !ownerReply.value().isEmpty()) {
            qWarning().noquote() << "[SGE-DBUS] Não foi possível registar" << notificationService
                                 << ": o serviço já pertence a" << ownerReply.value()
                                 << ". O SGE continuará sem receber notificações D-Bus.";
        } else {
            qWarning().noquote() << "[SGE-DBUS] Não foi possível registar" << notificationService
                                 << ":" << registrationError.name() << registrationError.message();
        }
        return;
    }

    m_registeredObject = m_sessionBus.registerObject(
        QString::fromLatin1(notificationObjectPath), this,
        QDBusConnection::ExportAdaptors);
    if (!m_registeredObject) {
        qWarning().noquote() << "[SGE-DBUS] Não foi possível exportar o serviço de notificações:"
                             << m_sessionBus.lastError().message();
        m_sessionBus.unregisterService(QString::fromLatin1(notificationService));
        m_registeredService = false;
        return;
    }

    qInfo().noquote() << "[SGE-DBUS] Servidor org.freedesktop.Notifications ativo.";
}

NotificationServerAdaptor::NotificationServerAdaptor(NotificationServer *server)
    : QDBusAbstractAdaptor(server),
      m_server(server) {
}

uint NotificationServerAdaptor::Notify(const QString &app_name, uint replaces_id,
                                       const QString &app_icon, const QString &summary,
                                       const QString &body, const QStringList &actions,
                                       const QVariantMap &hints, int expire_timeout) {
    return m_server->Notify(app_name, replaces_id, app_icon, summary, body,
                            actions, hints, expire_timeout);
}

void NotificationServerAdaptor::CloseNotification(uint id) {
    m_server->CloseNotification(id);
}

QStringList NotificationServerAdaptor::GetCapabilities() {
    return m_server->GetCapabilities();
}

void NotificationServerAdaptor::GetServerInformation(QString &name, QString &vendor,
                                                     QString &version, QString &spec_version) {
    m_server->GetServerInformation(name, vendor, version, spec_version);
}

NotificationServer::~NotificationServer() {
    if (m_registeredObject) {
        m_sessionBus.unregisterObject(QString::fromLatin1(notificationObjectPath));
    }
    if (m_registeredService) {
        m_sessionBus.unregisterService(QString::fromLatin1(notificationService));
    }
}

int NotificationServer::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_notifications.size();
}

QVariant NotificationServer::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_notifications.size()) {
        return {};
    }

    const Notification &notification = m_notifications.at(index.row());
    switch (role) {
    case NotificationIdRole:
        return notification.id;
    case AppNameRole:
        return notification.appName;
    case SummaryRole:
        return notification.summary;
    case BodyRole:
        return notification.body;
    case AppIconRole:
        return notification.appIcon;
    case TimeoutRole:
        return notification.timeout;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationServer::roleNames() const {
    return {
        {NotificationIdRole, QByteArrayLiteral("notificationId")},
        {AppNameRole, QByteArrayLiteral("appName")},
        {SummaryRole, QByteArrayLiteral("summary")},
        {BodyRole, QByteArrayLiteral("body")},
        {AppIconRole, QByteArrayLiteral("appIcon")},
        {TimeoutRole, QByteArrayLiteral("timeout")}
    };
}

int NotificationServer::notificationCount() const {
    return m_notifications.size();
}

uint NotificationServer::Notify(const QString &app_name, uint replaces_id,
                                const QString &app_icon, const QString &summary,
                                const QString &body, const QStringList &actions,
                                const QVariantMap &hints, int expire_timeout) {
    Q_UNUSED(actions);
    Q_UNUSED(hints);

    const int timeout = effectiveTimeout(expire_timeout);
    int row = rowForId(replaces_id);
    uint notificationId = replaces_id;
    if (row < 0) {
        notificationId = m_nextId++;
        if (notificationId == 0) {
            notificationId = m_nextId++;
        }
        while (rowForId(notificationId) >= 0) {
            notificationId = m_nextId++;
        }
        row = 0;
        beginInsertRows(QModelIndex(), row, row);
        m_notifications.prepend({notificationId, app_name, summary, body, app_icon, timeout});
        endInsertRows();
        emit notificationCountChanged();
    } else {
        m_notifications[row] = {notificationId, app_name, summary, body, app_icon, timeout};
        const QModelIndex changedIndex = index(row, 0);
        emit dataChanged(changedIndex, changedIndex,
                         {NotificationIdRole, AppNameRole, SummaryRole, BodyRole,
                          AppIconRole, TimeoutRole});
    }

    startExpirationTimer(notificationId, timeout);
    return notificationId;
}

void NotificationServer::CloseNotification(uint id) {
    removeNotification(id, 3);
}

QStringList NotificationServer::GetCapabilities() {
    return {QStringLiteral("body"), QStringLiteral("icon-static")};
}

void NotificationServer::GetServerInformation(QString &name, QString &vendor,
                                              QString &version, QString &spec_version) {
    name = QStringLiteral("Systalia Glass Engine");
    vendor = QStringLiteral("Systalia");
    version = QStringLiteral("0.3.5");
    spec_version = QStringLiteral("1.2");
}

void NotificationServer::closeNotificationFromUi(uint id) {
    removeNotification(id, 2);
}

int NotificationServer::rowForId(uint id) const {
    if (id == 0) {
        return -1;
    }
    for (qsizetype row = 0; row < m_notifications.size(); ++row) {
        if (m_notifications.at(row).id == id) {
            return static_cast<int>(row);
        }
    }
    return -1;
}

int NotificationServer::effectiveTimeout(int requestedTimeout) const {
    if (requestedTimeout < 0) {
        return 5000;
    }
    return requestedTimeout;
}

void NotificationServer::startExpirationTimer(uint id, int timeout) {
    if (QTimer *existingTimer = m_expirationTimers.take(id)) {
        existingTimer->stop();
        existingTimer->deleteLater();
    }
    if (timeout <= 0) {
        return;
    }

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    m_expirationTimers.insert(id, timer);
    connect(timer, &QTimer::timeout, this, [this, id]() {
        removeNotification(id, 1);
    });
    timer->start(timeout);
}

void NotificationServer::removeNotification(uint id, uint reason) {
    const int row = rowForId(id);
    if (row < 0) {
        return;
    }

    if (QTimer *timer = m_expirationTimers.take(id)) {
        timer->stop();
        timer->deleteLater();
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_notifications.removeAt(row);
    endRemoveRows();
    emit notificationCountChanged();
    emit NotificationClosed(id, reason);
}
