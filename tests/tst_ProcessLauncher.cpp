#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include "ProcessLauncherService.hpp"

class TestProcessLauncher final : public QObject {
    Q_OBJECT

private slots:
    void createsWaylandProcessEnvironment();
    void tracksDetachedProcessLifetime();
};

void TestProcessLauncher::createsWaylandProcessEnvironment() {
    const bool hadBackendOverride = qEnvironmentVariableIsSet("QSG_RHI_BACKEND");
    const QByteArray previousBackend = qgetenv("QSG_RHI_BACKEND");
    qputenv("QSG_RHI_BACKEND", QByteArrayLiteral("vulkan"));
    const QProcessEnvironment environment = ProcessLauncherService::waylandProcessEnvironment();
    if (hadBackendOverride) {
        qputenv("QSG_RHI_BACKEND", previousBackend);
    } else {
        qunsetenv("QSG_RHI_BACKEND");
    }

    QCOMPARE(environment.value(QStringLiteral("WAYLAND_DISPLAY")),
             QStringLiteral("wayland-systalia-0"));
    QCOMPARE(environment.value(QStringLiteral("QT_QPA_PLATFORM")),
             QStringLiteral("wayland"));
    QCOMPARE(environment.value(QStringLiteral("GDK_BACKEND")),
             QStringLiteral("wayland"));
    QVERIFY(!environment.contains(QStringLiteral("QSG_RHI_BACKEND")));
    QVERIFY(environment.contains(QStringLiteral("PATH")));
}

void TestProcessLauncher::tracksDetachedProcessLifetime() {
    const QString sleepProgram = QStandardPaths::findExecutable(QStringLiteral("sleep"));
    QVERIFY2(!sleepProgram.isEmpty(), "The sleep utility is required for this process lifecycle test.");

    ProcessLauncherService service;
    QSignalSpy started(&service, &ProcessLauncherService::processStarted);
    QSignalSpy finished(&service, &ProcessLauncherService::processFinished);
    QSignalSpy applicationsChanged(&service, &ProcessLauncherService::runningApplicationsChanged);
    QVERIFY(started.isValid());
    QVERIFY(finished.isValid());
    QVERIFY(applicationsChanged.isValid());

    QVERIFY(service.launch(sleepProgram, {QStringLiteral("0.4")}));
    QCOMPARE(started.count(), 1);
    const QString appId = started.at(0).at(0).toString();
    const qint64 processId = started.at(0).at(1).toLongLong();
    QCOMPARE(appId, QStringLiteral("sleep"));
    QVERIFY(processId > 0);
    QVERIFY(service.isAppRunning(QStringLiteral("SLEEP")));
    QVERIFY(service.runningApplications().contains(QStringLiteral("sleep")));

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
    QCOMPARE(finished.at(0).at(0).toString(), appId);
    QCOMPARE(finished.at(0).at(1).toLongLong(), processId);
    QVERIFY(!service.isAppRunning(appId));
    QVERIFY(service.runningApplications().isEmpty());
    QVERIFY(applicationsChanged.count() >= 2);
}

QTEST_GUILESS_MAIN(TestProcessLauncher)
#include "tst_ProcessLauncher.moc"
