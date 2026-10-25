#include <QSignalSpy>
#include <QTest>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>

#include "NotificationServer.hpp"

class TestNotificationServer final : public QObject {
    Q_OBJECT

private slots:
    void notifyInsertsNotificationIntoModel();
    void replacingNotificationPreservesItsIdentifier();
    void closeNotificationRemovesEntryAndEmitsDismissedReason();
    void expiresNotificationsAndEmitsCloseReason();
    void exposesFreedesktopCapabilities();
    void exposesFreedesktopServerInformation();
    void registersFreedesktopServiceAndAnswersInformationCall();
    void dbusNotifyCreatesModelEntry();
    void dbusCloseRemovesModelEntry();
};

void TestNotificationServer::notifyInsertsNotificationIntoModel() {
    NotificationServer server(false);
    QSignalSpy countChanged(&server, &NotificationServer::notificationCountChanged);
    QVERIFY(countChanged.isValid());

    const uint id = server.Notify(QStringLiteral("Test App"), 0, QStringLiteral("test-icon"),
                                  QStringLiteral("First title"), QStringLiteral("First body"),
                                  {}, {}, 0);
    QVERIFY(id > 0);
    QCOMPARE(server.rowCount(), 1);
    QCOMPARE(server.notificationCount(), 1);
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::NotificationIdRole).toUInt(), id);
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::AppNameRole).toString(),
             QStringLiteral("Test App"));
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::SummaryRole).toString(),
             QStringLiteral("First title"));
    QCOMPARE(countChanged.count(), 1);
}

void TestNotificationServer::replacingNotificationPreservesItsIdentifier() {
    NotificationServer server(false);
    const uint id = server.Notify(QStringLiteral("Test App"), 0, QStringLiteral("test-icon"),
                                  QStringLiteral("First title"), QStringLiteral("First body"),
                                  {}, {}, 0);
    QVERIFY(id > 0);
    const uint replacementId = server.Notify(QStringLiteral("Test App"), id,
                                              QStringLiteral("updated-icon"),
                                              QStringLiteral("Updated title"),
                                              QStringLiteral("Updated body"), {}, {}, 0);
    QCOMPARE(replacementId, id);
    QCOMPARE(server.rowCount(), 1);
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::NotificationIdRole).toUInt(), id);
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::SummaryRole).toString(),
             QStringLiteral("Updated title"));
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::BodyRole).toString(),
             QStringLiteral("Updated body"));
}

void TestNotificationServer::closeNotificationRemovesEntryAndEmitsDismissedReason() {
    NotificationServer server(false);
    QSignalSpy closed(&server, &NotificationServer::NotificationClosed);
    QSignalSpy countChanged(&server, &NotificationServer::notificationCountChanged);
    QVERIFY(closed.isValid());
    QVERIFY(countChanged.isValid());
    const uint id = server.Notify(QStringLiteral("Test App"), 0, QString(),
                                  QStringLiteral("Title"), QStringLiteral("Body"), {}, {}, 0);
    QVERIFY(id > 0);
    server.CloseNotification(id);
    QCOMPARE(server.rowCount(), 0);
    QCOMPARE(closed.count(), 1);
    QCOMPARE(closed.at(0).at(0).toUInt(), id);
    QCOMPARE(closed.at(0).at(1).toUInt(), 3U);
    QCOMPARE(countChanged.count(), 2);
}

void TestNotificationServer::expiresNotificationsAndEmitsCloseReason() {
    NotificationServer server(false);
    QSignalSpy closed(&server, &NotificationServer::NotificationClosed);
    QVERIFY(closed.isValid());

    const uint id = server.Notify(QStringLiteral("Timer App"), 0, QString(),
                                  QStringLiteral("Short notice"), QStringLiteral("Expires soon"),
                                  {}, {}, 30);
    QVERIFY(id > 0);
    QTRY_COMPARE_WITH_TIMEOUT(server.rowCount(), 0, 1000);
    QCOMPARE(closed.count(), 1);
    QCOMPARE(closed.at(0).at(0).toUInt(), id);
    QCOMPARE(closed.at(0).at(1).toUInt(), 1U);
}

void TestNotificationServer::exposesFreedesktopCapabilities() {
    NotificationServer server(false);
    const QStringList capabilities = server.GetCapabilities();
    QVERIFY(capabilities.contains(QStringLiteral("body")));
    QVERIFY(capabilities.contains(QStringLiteral("icon-static")));
}

void TestNotificationServer::exposesFreedesktopServerInformation() {
    NotificationServer server(false);
    QString name;
    QString vendor;
    QString version;
    QString specVersion;
    server.GetServerInformation(name, vendor, version, specVersion);
    QCOMPARE(name, QStringLiteral("Systalia Glass Engine"));
    QCOMPARE(vendor, QStringLiteral("Systalia"));
    QCOMPARE(version, QStringLiteral("0.3.5"));
    QCOMPARE(specVersion, QStringLiteral("1.2"));
}

void TestNotificationServer::registersFreedesktopServiceAndAnswersInformationCall() {
    NotificationServer server(true);
    QVERIFY(server.isServiceRegistered());

    const QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface interface(QStringLiteral("org.freedesktop.Notifications"),
                             QStringLiteral("/org/freedesktop/Notifications"),
                             QStringLiteral("org.freedesktop.Notifications"), bus);
    QVERIFY(interface.isValid());

    const QDBusMessage information = interface.call(QStringLiteral("GetServerInformation"));
    QVERIFY2(information.type() == QDBusMessage::ReplyMessage,
             qPrintable(information.errorMessage()));
    QCOMPARE(information.arguments().size(), 4);
    QCOMPARE(information.arguments().at(0).toString(), QStringLiteral("Systalia Glass Engine"));
    QCOMPARE(information.arguments().at(1).toString(), QStringLiteral("Systalia"));

}

void TestNotificationServer::dbusNotifyCreatesModelEntry() {
    NotificationServer server(true);
    QDBusInterface interface(QStringLiteral("org.freedesktop.Notifications"),
                             QStringLiteral("/org/freedesktop/Notifications"),
                             QStringLiteral("org.freedesktop.Notifications"),
                             QDBusConnection::sessionBus());
    QVERIFY(interface.isValid());
    const QDBusMessage notification = interface.call(
        QStringLiteral("Notify"), QStringLiteral("D-Bus Test"), uint(0), QString(),
        QStringLiteral("Bus title"), QStringLiteral("Bus body"), QStringList{},
        QVariantMap{}, 0);
    QVERIFY2(notification.type() == QDBusMessage::ReplyMessage,
             qPrintable(notification.errorMessage()));
    QCOMPARE(notification.arguments().size(), 1);
    const uint id = notification.arguments().constFirst().toUInt();
    QVERIFY(id > 0);
    QCOMPARE(server.rowCount(), 1);
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::NotificationIdRole).toUInt(), id);
    QCOMPARE(server.data(server.index(0, 0), NotificationServer::AppNameRole).toString(),
             QStringLiteral("D-Bus Test"));
}

void TestNotificationServer::dbusCloseRemovesModelEntry() {
    NotificationServer server(true);
    QDBusInterface interface(QStringLiteral("org.freedesktop.Notifications"),
                             QStringLiteral("/org/freedesktop/Notifications"),
                             QStringLiteral("org.freedesktop.Notifications"),
                             QDBusConnection::sessionBus());
    QVERIFY(interface.isValid());
    const QDBusReply<uint> created = interface.call(
        QStringLiteral("Notify"), QStringLiteral("D-Bus Close Test"), uint(0), QString(),
        QStringLiteral("Close title"), QStringLiteral("Close body"), QStringList{},
        QVariantMap{}, 0);
    QVERIFY(created.isValid());
    const uint id = created.value();
    QVERIFY(id > 0);
    QCOMPARE(server.rowCount(), 1);
    const QDBusMessage close = interface.call(QStringLiteral("CloseNotification"), id);
    QVERIFY2(close.type() == QDBusMessage::ReplyMessage, qPrintable(close.errorMessage()));
    QCOMPARE(server.rowCount(), 0);
}

QTEST_GUILESS_MAIN(TestNotificationServer)
#include "tst_NotificationServer.moc"
