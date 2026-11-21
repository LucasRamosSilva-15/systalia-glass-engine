#include <QtTest>

#include "DrmBackendController.hpp"

#include <QList>
#include <optional>
#include <utility>

using namespace Systalia::Platform::Drm;

namespace {

class MockSessionDrmProvider final : public ISessionDrmProvider {
public:
    bool sessionActive = true;
    bool failAcquire = false;
    int nextFd = 42;
    uint32_t major = 226;
    uint32_t minor = 1;

    int acquireCalls = 0;
    int releaseCalls = 0;
    QString lastRequestedPath;
    QList<int> activeHeldFds;

    bool isSessionActive() const override {
        return sessionActive;
    }

    std::optional<DrmDeviceDescriptor> acquireDrmDevice(const QString &devicePath) override {
        ++acquireCalls;
        lastRequestedPath = devicePath;
        if (failAcquire) {
            return std::nullopt;
        }
        const int fd = nextFd++;
        activeHeldFds.append(fd);
        return DrmDeviceDescriptor(fd, major, minor, devicePath);
    }

    bool releaseDrmDevice(uint32_t reqMajor, uint32_t reqMinor, int fd) override {
        ++releaseCalls;
        if (reqMajor != major || reqMinor != minor) {
            return false;
        }
        return activeHeldFds.removeOne(fd);
    }
};

class MockDrmPresenter final : public IDrmPresenter {
public:
    bool failInitialize = false;
    bool failConfigure = false;
    bool failPresent = false;
    bool failResume = false;

    int initCalls = 0;
    int suspendCalls = 0;
    int resumeCalls = 0;
    int shutdownCalls = 0;
    int lastFd = -1;
    DrmOutputMode configuredMode;
    QList<uint64_t> presentedFrameIds;

    bool initializeOnFd(int drmFd) override {
        ++initCalls;
        lastFd = drmFd;
        return !failInitialize && drmFd >= 0;
    }

    bool configureOutput(const DrmOutputMode &mode) override {
        configuredMode = mode;
        return !failConfigure && mode.isValid();
    }

    bool presentFrame(uint64_t frameIndex, int drmFd) override {
        if (failPresent || drmFd != lastFd) {
            return false;
        }
        presentedFrameIds.append(frameIndex);
        return true;
    }

    void suspendPresentation() override {
        ++suspendCalls;
    }

    bool resumePresentation(int drmFd) override {
        ++resumeCalls;
        if (failResume || drmFd < 0) {
            return false;
        }
        lastFd = drmFd;
        return true;
    }

    void shutdown() override {
        ++shutdownCalls;
    }
};

DrmOutputMode defaultMode() {
    return DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true};
}

} // namespace

class tst_DrmBackendPoc final : public QObject {
    Q_OBJECT

private slots:
    void singleFdOwnershipLifecycleAndTeardown();
    void acquisitionFailuresLeaveZeroHeldDescriptors();
    void vtSuspendAndResumeBlockRenderingWhilePaused();
    void descriptorTransferAndMoveSemantics();
};

void tst_DrmBackendPoc::singleFdOwnershipLifecycleAndTeardown() {
    MockSessionDrmProvider session;
    MockDrmPresenter presenter;

    {
        DrmBackendController controller(session, presenter);
        QCOMPARE(controller.state(), DrmOwnershipState::Uninitialized);

        QVERIFY(controller.initialize(QStringLiteral("/dev/dri/card1"), defaultMode()));
        QCOMPARE(controller.state(), DrmOwnershipState::Acquired);
        QCOMPARE(session.acquireCalls, 1);
        QCOMPARE(session.lastRequestedPath, QStringLiteral("/dev/dri/card1"));
        QCOMPARE(session.activeHeldFds.size(), 1);
        QCOMPARE(controller.activeFd(), 42);
        QCOMPARE(presenter.lastFd, 42);
        QVERIFY(controller.isPresentationActive());

        // Segunda inicialização sem shutdown não deve abrir um segundo FD
        QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), defaultMode()));
        QCOMPARE(session.acquireCalls, 1);

        QVERIFY(controller.renderAndPresentFrame());
        QCOMPARE(controller.presentedFrames(), 1ULL);
        QCOMPARE(presenter.presentedFrameIds, QList<uint64_t>({1ULL}));
    }

    // Destrutor deve encerrar apresentador e liberar o único FD sem vazamentos
    QCOMPARE(presenter.shutdownCalls, 1);
    QCOMPARE(session.releaseCalls, 1);
    QVERIFY(session.activeHeldFds.isEmpty());
}

void tst_DrmBackendPoc::acquisitionFailuresLeaveZeroHeldDescriptors() {
    // 1. Sessão inativa
    {
        MockSessionDrmProvider session;
        session.sessionActive = false;
        MockDrmPresenter presenter;
        DrmBackendController controller(session, presenter);
        QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), defaultMode()));
        QCOMPARE(controller.state(), DrmOwnershipState::Failed);
        QCOMPARE(session.acquireCalls, 0);
        QVERIFY(session.activeHeldFds.isEmpty());
    }

    // 2. Falha em TakeDevice simulado
    {
        MockSessionDrmProvider session;
        session.failAcquire = true;
        MockDrmPresenter presenter;
        DrmBackendController controller(session, presenter);
        QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), defaultMode()));
        QCOMPARE(controller.state(), DrmOwnershipState::Failed);
        QCOMPARE(presenter.initCalls, 0);
        QVERIFY(session.activeHeldFds.isEmpty());
    }

    // 3. Falha na inicialização do apresentador libera o FD imediatamente
    {
        MockSessionDrmProvider session;
        MockDrmPresenter presenter;
        presenter.failInitialize = true;
        DrmBackendController controller(session, presenter);
        QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), defaultMode()));
        QCOMPARE(controller.state(), DrmOwnershipState::Failed);
        QCOMPARE(session.releaseCalls, 1);
        QVERIFY(session.activeHeldFds.isEmpty());
        QCOMPARE(controller.activeFd(), -1);
    }
}

void tst_DrmBackendPoc::vtSuspendAndResumeBlockRenderingWhilePaused() {
    MockSessionDrmProvider session;
    MockDrmPresenter presenter;
    DrmBackendController controller(session, presenter);

    QVERIFY(controller.initialize(QStringLiteral("/dev/dri/card1"), defaultMode()));
    QVERIFY(controller.renderAndPresentFrame());

    // Evento para outro dispositivo é ignorado
    controller.onDevicePaused(13, 64);
    QCOMPARE(controller.state(), DrmOwnershipState::Acquired);

    // Pausa do DRM ativo bloqueia renderização
    controller.onDevicePaused(226, 1);
    QCOMPARE(controller.state(), DrmOwnershipState::Suspended);
    QCOMPARE(presenter.suspendCalls, 1);
    QVERIFY(!controller.renderAndPresentFrame());
    QCOMPARE(controller.presentedFrames(), 1ULL);

    // Retomada com novo FD fornecido pelo logind
    QVERIFY(controller.onDeviceResumed(226, 1, 99));
    QCOMPARE(controller.state(), DrmOwnershipState::Acquired);
    QCOMPARE(controller.activeFd(), 99);
    QCOMPARE(presenter.lastFd, 99);
    QCOMPARE(presenter.resumeCalls, 1);
    QVERIFY(controller.renderAndPresentFrame());
    QCOMPARE(controller.presentedFrames(), 2ULL);

    // Shutdown explícito é idempotente e bloqueia apresentações futuras
    controller.shutdown();
    controller.shutdown();
    QCOMPARE(controller.state(), DrmOwnershipState::Released);
    QCOMPARE(session.releaseCalls, 1);
    QVERIFY(!controller.renderAndPresentFrame());
}

void tst_DrmBackendPoc::descriptorTransferAndMoveSemantics() {
    DrmDeviceDescriptor d1(77, 226, 1, QStringLiteral("/dev/dri/card1"));
    QVERIFY(d1.isValid());

    DrmDeviceDescriptor d2(std::move(d1));
    QVERIFY(!d1.isValid());
    QCOMPARE(d1.fd, -1);
    QVERIFY(d2.isValid());
    QCOMPARE(d2.fd, 77);
    QCOMPARE(d2.majorNumber, 226U);
    QCOMPARE(d2.minorNumber, 1U);
}

QTEST_MAIN(tst_DrmBackendPoc)
#include "tst_DrmBackendPoc.moc"
