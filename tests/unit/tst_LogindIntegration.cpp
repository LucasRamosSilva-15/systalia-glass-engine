#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTest>
#include <unistd.h>
#include <fcntl.h>

#include "InputDeviceManager.hpp"
#include "LogindSessionService.hpp"

#ifndef SGE_SOURCE_DIR
#define SGE_SOURCE_DIR "."
#endif

class TestLogindIntegration final : public QObject {
    Q_OBJECT

private slots:
    void dbusObjectPathsAndMethodSignaturesValidation();
    void rootlessSessionInitializationAndDeviceAcquisition();
    void pauseDeviceTransitionsToSuspendedAndSendsPauseDeviceComplete();
    void resumeDeviceRestoresFileDescriptorAndSwapchain();
    void activePropertyChangeUpdatesSessionState();
    void inputConsumerReopensResumedLease();
    void sddmDesktopEntryAndSessionScriptValidation();
};

void TestLogindIntegration::dbusObjectPathsAndMethodSignaturesValidation() {
    const QString sessionPath = QStringLiteral("/org/freedesktop/login1/session/auto");

    const QDBusMessage getSessionMsg = LogindSessionService::buildGetSessionByPidMessage(4242u);
    QCOMPARE(getSessionMsg.service(), QStringLiteral("org.freedesktop.login1"));
    QCOMPARE(getSessionMsg.path(), QStringLiteral("/org/freedesktop/login1"));
    QCOMPARE(getSessionMsg.interface(), QStringLiteral("org.freedesktop.login1.Manager"));
    QCOMPARE(getSessionMsg.member(), QStringLiteral("GetSessionByPID"));
    QCOMPARE(LogindSessionService::messageSignature(getSessionMsg), QStringLiteral("u"));
    QCOMPARE(getSessionMsg.arguments().size(), 1);
    QCOMPARE(getSessionMsg.arguments().at(0).toUInt(), 4242u);

    const QDBusMessage takeControlMsg = LogindSessionService::buildTakeControlMessage(sessionPath, false);
    QCOMPARE(takeControlMsg.service(), QStringLiteral("org.freedesktop.login1"));
    QCOMPARE(takeControlMsg.path(), sessionPath);
    QCOMPARE(takeControlMsg.interface(), QStringLiteral("org.freedesktop.login1.Session"));
    QCOMPARE(takeControlMsg.member(), QStringLiteral("TakeControl"));
    QCOMPARE(LogindSessionService::messageSignature(takeControlMsg), QStringLiteral("b"));
    QCOMPARE(takeControlMsg.arguments().size(), 1);
    QCOMPARE(takeControlMsg.arguments().at(0).toBool(), false);

    const QDBusMessage releaseControlMsg = LogindSessionService::buildReleaseControlMessage(sessionPath);
    QCOMPARE(releaseControlMsg.service(), QStringLiteral("org.freedesktop.login1"));
    QCOMPARE(releaseControlMsg.path(), sessionPath);
    QCOMPARE(releaseControlMsg.interface(), QStringLiteral("org.freedesktop.login1.Session"));
    QCOMPARE(releaseControlMsg.member(), QStringLiteral("ReleaseControl"));
    QCOMPARE(LogindSessionService::messageSignature(releaseControlMsg), QStringLiteral(""));
    QCOMPARE(releaseControlMsg.arguments().size(), 0);

    const QDBusMessage takeDeviceMsg = LogindSessionService::buildTakeDeviceMessage(
        sessionPath, LogindSessionService::kDrmMajorNumber, 0u);
    QCOMPARE(takeDeviceMsg.service(), QStringLiteral("org.freedesktop.login1"));
    QCOMPARE(takeDeviceMsg.path(), sessionPath);
    QCOMPARE(takeDeviceMsg.interface(), QStringLiteral("org.freedesktop.login1.Session"));
    QCOMPARE(takeDeviceMsg.member(), QStringLiteral("TakeDevice"));
    QCOMPARE(LogindSessionService::messageSignature(takeDeviceMsg), QStringLiteral("uu"));
    QCOMPARE(takeDeviceMsg.arguments().size(), 2);
    QCOMPARE(takeDeviceMsg.arguments().at(0).toUInt(), 226u);
    QCOMPARE(takeDeviceMsg.arguments().at(1).toUInt(), 0u);

    const QDBusMessage releaseDeviceMsg = LogindSessionService::buildReleaseDeviceMessage(
        sessionPath, LogindSessionService::kDrmMajorNumber, 0u);
    QCOMPARE(releaseDeviceMsg.member(), QStringLiteral("ReleaseDevice"));
    QCOMPARE(LogindSessionService::messageSignature(releaseDeviceMsg), QStringLiteral("uu"));

    const QDBusMessage pauseCompleteMsg = LogindSessionService::buildPauseDeviceCompleteMessage(
        sessionPath, LogindSessionService::kDrmMajorNumber, 0u);
    QCOMPARE(pauseCompleteMsg.member(), QStringLiteral("PauseDeviceComplete"));
    QCOMPARE(LogindSessionService::messageSignature(pauseCompleteMsg), QStringLiteral("uu"));
    QCOMPARE(pauseCompleteMsg.arguments().at(0).toUInt(), 226u);
    QCOMPARE(pauseCompleteMsg.arguments().at(1).toUInt(), 0u);
}

void TestLogindIntegration::rootlessSessionInitializationAndDeviceAcquisition() {
    int drmPipe[2] = {-1, -1};
    int inputPipe[2] = {-1, -1};
    QVERIFY(::pipe(drmPipe) == 0);
    QVERIFY(::pipe(inputPipe) == 0);

    LogindSessionService service;
    service.setCustomCallHandler([&](const QDBusMessage &req) -> QDBusMessage {
        if (req.member() == QLatin1String("GetSessionByPID")) {
            QDBusMessage reply = req.createReply();
            reply << QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/login1/session/_31")));
            return reply;
        }
        if (req.member() == QLatin1String("TakeControl")) {
            return req.createReply();
        }
        if (req.member() == QLatin1String("TakeDevice")) {
            const quint32 major = req.arguments().at(0).toUInt();
            const int sourceFd = (major == LogindSessionService::kDrmMajorNumber)
                                     ? drmPipe[0]
                                     : inputPipe[0];
            QDBusMessage reply = req.createReply();
            reply << QVariant::fromValue(QDBusUnixFileDescriptor(sourceFd))
                  << QVariant::fromValue(false);
            return reply;
        }
        if (req.member() == QLatin1String("ReleaseDevice")
            || req.member() == QLatin1String("ReleaseControl")) {
            return req.createReply();
        }
        return QDBusMessage::createError(QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                                         QStringLiteral("Unknown method"));
    });

    QVERIFY(service.initializeSession(false));
    QCOMPARE(service.sessionPath(), QStringLiteral("/org/freedesktop/login1/session/_31"));
    QVERIFY(service.hasSessionControl());
    QVERIFY(service.isSessionActive());
    QCOMPARE(service.sessionState(), LogindSessionService::Active);
    QVERIFY(!service.isGpuSubmissionSuspended());

    const int drmFd = service.acquirePrimaryDrmDevice(QStringLiteral("/dev/dri/card0"));
    QVERIFY(drmFd >= 0);
    QCOMPARE(service.primaryDrmFileDescriptor(), drmFd);
    QCOMPARE(service.deviceFileDescriptor(226u, 0u), drmFd);

    // Verifica delegação da libinput (open_restricted / close_restricted) ao LogindSessionService
    const libinput_interface &iface = InputDeviceManager::restrictedInterface();
    InputDeviceManager input;
    const int evdevFd = iface.open_restricted("/dev/input/event0", 0, &input);
    QVERIFY(evdevFd >= 0);
    QCOMPARE(service.activeDeviceCount(), 2);

    iface.close_restricted(evdevFd, &input);
    QCOMPARE(service.activeDeviceCount(), 1);

    QVERIFY(service.releaseControl());
    QVERIFY(!service.hasSessionControl());
    QCOMPARE(service.activeDeviceCount(), 0);
    QCOMPARE(service.sessionState(), LogindSessionService::Disconnected);

    ::close(drmPipe[0]);
    ::close(drmPipe[1]);
    ::close(inputPipe[0]);
    ::close(inputPipe[1]);
}

void TestLogindIntegration::pauseDeviceTransitionsToSuspendedAndSendsPauseDeviceComplete() {
    int drmPipe[2] = {-1, -1};
    QVERIFY(::pipe(drmPipe) == 0);

    LogindSessionService service;
    service.setCustomCallHandler([&](const QDBusMessage &req) -> QDBusMessage {
        if (req.member() == QLatin1String("TakeDevice")) {
            QDBusMessage reply = req.createReply();
            reply << QVariant::fromValue(QDBusUnixFileDescriptor(drmPipe[0]))
                  << QVariant::fromValue(false);
            return reply;
        }
        return req.createReply();
    });

    QVERIFY(service.initializeSession(false));
    QVERIFY(service.acquirePrimaryDrmDevice(QStringLiteral("/dev/dri/card0")) >= 0);
    service.clearRecordedMessages();

    QSignalSpy stateSpy(&service, &LogindSessionService::sessionStateChanged);
    QSignalSpy gpuSuspendedSpy(&service, &LogindSessionService::gpuSubmissionSuspendedChanged);
    QSignalSpy pausedSpy(&service, &LogindSessionService::devicePaused);

    // Simula a emissão de PauseDevice pelo systemd-logind (ex.: troca de VT Ctrl+Alt+F2)
    service.onPauseDevice(LogindSessionService::kDrmMajorNumber, 0u, QStringLiteral("pause"));

    QCOMPARE(service.sessionState(), LogindSessionService::Suspended);
    QVERIFY(service.isGpuSubmissionSuspended());
    QVERIFY(service.isDevicePaused(LogindSessionService::kDrmMajorNumber, 0u));
    QCOMPARE(service.pauseCompleteAckCount(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(gpuSuspendedSpy.count(), 1);
    QCOMPARE(gpuSuspendedSpy.at(0).at(0).toBool(), true);
    QCOMPARE(pausedSpy.count(), 1);

    const QList<QDBusMessage> outbound = service.recordedOutboundMessages();
    QCOMPARE(outbound.size(), 1);
    QCOMPARE(outbound.first().member(), QStringLiteral("PauseDeviceComplete"));
    QCOMPARE(outbound.first().arguments().at(0).toUInt(), 226u);
    QCOMPARE(outbound.first().arguments().at(1).toUInt(), 0u);

    ::close(drmPipe[0]);
    ::close(drmPipe[1]);
}

void TestLogindIntegration::resumeDeviceRestoresFileDescriptorAndSwapchain() {
    int initialPipe[2] = {-1, -1};
    int resumedPipe[2] = {-1, -1};
    QVERIFY(::pipe(initialPipe) == 0);
    QVERIFY(::pipe(resumedPipe) == 0);

    LogindSessionService service;
    service.setCustomCallHandler([&](const QDBusMessage &req) -> QDBusMessage {
        if (req.member() == QLatin1String("TakeDevice")) {
            QDBusMessage reply = req.createReply();
            reply << QVariant::fromValue(QDBusUnixFileDescriptor(initialPipe[0]))
                  << QVariant::fromValue(false);
            return reply;
        }
        return req.createReply();
    });

    QVERIFY(service.initializeSession(false));
    QVERIFY(service.acquirePrimaryDrmDevice(QStringLiteral("/dev/dri/card0")) >= 0);

    // Coloca em estado Suspended via PauseDevice
    service.onPauseDevice(LogindSessionService::kDrmMajorNumber, 0u, QStringLiteral("pause"));
    QCOMPARE(service.sessionState(), LogindSessionService::Suspended);
    QVERIFY(service.isGpuSubmissionSuspended());

    QSignalSpy resumedSpy(&service, &LogindSessionService::deviceResumed);
    QSignalSpy swapchainSpy(&service, &LogindSessionService::swapchainRestoreRequested);

    // Simula ResumeDevice com novo descritor de ficheiro delegado pelo logind
    const QDBusUnixFileDescriptor newDbusFd(resumedPipe[0]);
    service.onResumeDevice(LogindSessionService::kDrmMajorNumber, 0u, newDbusFd);

    QCOMPARE(service.sessionState(), LogindSessionService::Active);
    QVERIFY(!service.isGpuSubmissionSuspended());
    QVERIFY(!service.isDevicePaused(LogindSessionService::kDrmMajorNumber, 0u));
    QCOMPARE(resumedSpy.count(), 1);
    QCOMPARE(swapchainSpy.count(), 1);

    const int restoredFd = service.deviceFileDescriptor(LogindSessionService::kDrmMajorNumber, 0u);
    QVERIFY(restoredFd >= 0);
    QCOMPARE(service.primaryDrmFileDescriptor(), restoredFd);

    // Confirma que o descritor de ficheiro reatribuído está operacional e lê dados reais do kernel
    const char payload = 'V';
    QCOMPARE(::write(resumedPipe[1], &payload, 1), static_cast<ssize_t>(1));
    char readBack = 0;
    QCOMPARE(::read(restoredFd, &readBack, 1), static_cast<ssize_t>(1));
    QCOMPARE(readBack, 'V');

    ::close(initialPipe[0]);
    ::close(initialPipe[1]);
    ::close(resumedPipe[0]);
    ::close(resumedPipe[1]);
}

void TestLogindIntegration::inputConsumerReopensResumedLease() {
    int initialPipe[2], resumedPipe[2];
    QVERIFY(::pipe(initialPipe) == 0);
    QVERIFY(::pipe(resumedPipe) == 0);
    LogindSessionService service;
    int takeCalls = 0;
    service.setCustomCallHandler([&](const QDBusMessage &request) {
        auto reply = request.createReply();
        if (request.member() == "TakeDevice") {
            ++takeCalls;
            reply << QVariant::fromValue(QDBusUnixFileDescriptor(initialPipe[0])) << false;
        }
        return reply;
    });
    QVERIFY(service.initializeSession(false));
    InputDeviceManager input;
    const auto &callbacks = InputDeviceManager::restrictedInterface();
    const int originalConsumer = callbacks.open_restricted("/dev/input/event0", O_RDONLY, &input);
    QVERIFY(originalConsumer >= 0);
    QVERIFY(originalConsumer != service.deviceFileDescriptor(13, 64));
    connect(&service, &LogindSessionService::devicePauseRequested, this, [&](quint32, quint32) {
        callbacks.close_restricted(originalConsumer, &input);
    });
    service.onPauseDevice(13, 64, "pause");
    QCOMPARE(::fcntl(originalConsumer, F_GETFD), -1);
    QCOMPARE(service.activeDeviceCount(), 1);
    QVERIFY(::fcntl(service.deviceFileDescriptor(13, 64), F_GETFD) >= 0);
    service.onResumeDevice(13, 64, QDBusUnixFileDescriptor(resumedPipe[0]));
    const int resumedConsumer = callbacks.open_restricted("/dev/input/event0", O_RDONLY, &input);
    QVERIFY(resumedConsumer >= 0);
    QCOMPARE(takeCalls, 1); // The existing lease is reused, not requested twice from logind.
    const char sent = 'R';
    QCOMPARE(::write(resumedPipe[1], &sent, 1), ssize_t(1));
    char received = 0;
    QCOMPARE(::read(resumedConsumer, &received, 1), ssize_t(1));
    QCOMPARE(received, sent);
    callbacks.close_restricted(resumedConsumer, &input);
    QCOMPARE(service.activeDeviceCount(), 0);
    ::close(initialPipe[0]); ::close(initialPipe[1]);
    ::close(resumedPipe[0]); ::close(resumedPipe[1]);
}

void TestLogindIntegration::activePropertyChangeUpdatesSessionState() {
    LogindSessionService service;
    service.setCustomCallHandler([](const QDBusMessage &req) -> QDBusMessage {
        return req.createReply();
    });

    QVERIFY(service.initializeSession(false));
    QVERIFY(service.isSessionActive());
    QCOMPARE(service.sessionState(), LogindSessionService::Active);

    QVariantMap propsInactive;
    propsInactive.insert(QStringLiteral("Active"), false);
    service.onPropertiesChanged(
        QString::fromLatin1(LogindSessionService::kLogindSessionInterface),
        propsInactive,
        QStringList());
    QVERIFY(!service.isSessionActive());
    QCOMPARE(service.sessionState(), LogindSessionService::Suspended);
    QVERIFY(service.isGpuSubmissionSuspended());

    QVariantMap propsActive;
    propsActive.insert(QStringLiteral("Active"), true);
    service.onPropertiesChanged(
        QString::fromLatin1(LogindSessionService::kLogindSessionInterface),
        propsActive,
        QStringList());
    QVERIFY(service.isSessionActive());
    QCOMPARE(service.sessionState(), LogindSessionService::Active);
    QVERIFY(!service.isGpuSubmissionSuspended());
}

void TestLogindIntegration::sddmDesktopEntryAndSessionScriptValidation() {
    const QString scriptPath = QStringLiteral(SGE_SOURCE_DIR "/data/systalia-session");
    const QString desktopPath = QStringLiteral(SGE_SOURCE_DIR "/data/systalia.desktop");

    QFileInfo scriptInfo(scriptPath);
    QVERIFY2(scriptInfo.exists(), qPrintable(scriptPath));
    QVERIFY(scriptInfo.permission(QFileDevice::ExeOwner));

    QFile scriptFile(scriptPath);
    QVERIFY(scriptFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString scriptContent = QString::fromUtf8(scriptFile.readAll());
    QVERIFY(scriptContent.startsWith(QStringLiteral("#!/bin/sh")));
    QVERIFY(scriptContent.contains(QStringLiteral("export XDG_CURRENT_DESKTOP=\"Systalia:VALO\"")));
    QVERIFY(scriptContent.contains(QStringLiteral("export XDG_SESSION_TYPE=\"wayland\"")));
    QVERIFY(scriptContent.contains(QStringLiteral("export XDG_SESSION_DESKTOP=\"systalia\"")));
    QVERIFY(scriptContent.contains(QStringLiteral("export QT_QPA_PLATFORM=\"wayland;eglfs\"")));
    QVERIFY(scriptContent.contains(QStringLiteral("exec /usr/bin/systalia_glass_engine")));

    QFile desktopFile(desktopPath);
    QVERIFY2(desktopFile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(desktopPath));
    const QString desktopContent = QString::fromUtf8(desktopFile.readAll());
    QVERIFY(desktopContent.contains(QStringLiteral("[Desktop Entry]")));
    QVERIFY(desktopContent.contains(QStringLiteral("Name=Systalia (VALO Glass)")));
    QVERIFY(desktopContent.contains(QStringLiteral("Exec=/usr/bin/systalia-session")));
    QVERIFY(desktopContent.contains(QStringLiteral("Type=Application")));
    QVERIFY(desktopContent.contains(QStringLiteral("DesktopNames=Systalia;VALO")));
}

QTEST_MAIN(TestLogindIntegration)
#include "tst_LogindIntegration.moc"

