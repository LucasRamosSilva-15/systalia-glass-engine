#include <QtTest>

#include "DrmBackendController.hpp"
#include "GbmBufferManager.hpp"
#include "KmsOutputDiscovery.hpp"
#include "LogindDrmProvider.hpp"
#include "LogindSessionService.hpp"
#include "VulkanQuickContracts.hpp"

#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <fcntl.h>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

int makePipeReadEndFd() {
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return -1;
    }
    ::close(fds[1]);
    return fds[0];
}

class MockPresenter final : public IDrmPresenter {
public:
    int lastFd = -1;
    bool paused = false;
    int shutdownCalls = 0;

    bool initializeOnFd(int drmFd) override {
        lastFd = drmFd;
        return drmFd >= 0;
    }
    bool configureOutput(const DrmOutputMode &mode) override {
        return mode.isValid();
    }
    bool presentFrame(uint64_t, int drmFd) override {
        return !paused && drmFd == lastFd;
    }
    void suspendPresentation() override {
        paused = true;
    }
    bool resumePresentation(int drmFd) override {
        paused = false;
        lastFd = drmFd;
        return drmFd >= 0;
    }
    void shutdown() override {
        ++shutdownCalls;
    }
};

class MockKmsBackend final : public IKmsResourceBackend {
public:
    KmsDiscoverySnapshot nextSnapshot;
    KmsDiscoverySnapshot probeResources(int drmFd) override {
        if (drmFd < 0) {
            return KmsDiscoverySnapshot{};
        }
        return nextSnapshot;
    }
};

class MockGbmBackend final : public IGbmBackend {
public:
    bool failCreateDevice = false;
    int failOnAllocIndex = -1;
    int allocCount = 0;
    int activeDevices = 0;
    int activeBuffers = 0;

    void *createDevice(int drmFd) override {
        if (failCreateDevice || drmFd < 0) {
            return nullptr;
        }
        ++activeDevices;
        return reinterpret_cast<void *>(0x1234);
    }

    void destroyDevice(void *device) override {
        if (device) {
            --activeDevices;
        }
    }

    bool allocateBuffer(void *device,
                        const GbmBufferAllocationRequest &req,
                        GbmExportedBuffer *outBuffer) override {
        ++allocCount;
        if (!device || !outBuffer || (failOnAllocIndex >= 0 && allocCount == failOnAllocIndex)) {
            return false;
        }
        ++activeBuffers;
        outBuffer->width = req.width;
        outBuffer->height = req.height;
        outBuffer->format = req.format;
        outBuffer->stride = req.width * 4;
        outBuffer->modifier = 0;
        outBuffer->dmabufFd = makePipeReadEndFd();
        outBuffer->vulkanExternalMemoryVerified = false;
        outBuffer->nativeHandle = reinterpret_cast<void *>(static_cast<uintptr_t>(allocCount));
        return outBuffer->dmabufFd >= 0;
    }

    void destroyBuffer(GbmExportedBuffer *buffer) override {
        if (!buffer) {
            return;
        }
        if (buffer->dmabufFd >= 0) {
            ::close(buffer->dmabufFd);
            buffer->dmabufFd = -1;
        }
        if (buffer->acquireFenceFd >= 0) {
            ::close(buffer->acquireFenceFd);
            buffer->acquireFenceFd = -1;
        }
        if (buffer->releaseFenceFd >= 0) {
            ::close(buffer->releaseFenceFd);
            buffer->releaseFenceFd = -1;
        }
        if (buffer->nativeHandle) {
            --activeBuffers;
            buffer->nativeHandle = nullptr;
        }
    }
};

} // namespace

class tst_DrmGbmIntegration final : public QObject {
    Q_OBJECT

private slots:
    void logindProviderAcquisitionRollbackAndFdReplacement();
    void kmsDiscoveryConnectorsModesPlanesAndNoMonitors();
    void gbmBufferLifecycleFailuresAndCleanup();
    void vulkanQuickContractRequiresExplicitExport();
};

void tst_DrmGbmIntegration::logindProviderAcquisitionRollbackAndFdReplacement() {
    LogindSessionService logind;
    bool returnInactive = false;
    bool failTakeDevice = false;
    int releaseCount = 0;

    logind.setCustomCallHandler([&](const QDBusMessage &msg) -> QDBusMessage {
        const QString member = msg.member();
        if (member == QStringLiteral("GetSessionByPID")) {
            auto reply = msg.createReply();
            reply << QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/login1/session/_39")));
            return reply;
        }
        if (member == QStringLiteral("TakeControl") || member == QStringLiteral("ReleaseControl")
            || member == QStringLiteral("PauseDeviceComplete")) {
            return msg.createReply();
        }
        if (member == QStringLiteral("ReleaseDevice")) {
            ++releaseCount;
            return msg.createReply();
        }
        if (member == QStringLiteral("TakeDevice")) {
            if (failTakeDevice) {
                return msg.createErrorReply(QStringLiteral("System.Error.EBUSY"),
                                            QStringLiteral("Device or resource busy"));
            }
            const int rawFd = makePipeReadEndFd();
            QDBusUnixFileDescriptor dbusFd(rawFd);
            ::close(rawFd);
            auto reply = msg.createReply();
            reply << QVariant::fromValue(dbusFd) << returnInactive;
            return reply;
        }
        return msg.createReply();
    });

    QVERIFY(logind.initializeSession(false));

    auto statStub = [](const QString &path, uint32_t *maj, uint32_t *min) {
        if (path == QStringLiteral("/dev/dri/card1")) {
            *maj = 226;
            *min = 1;
            return true;
        }
        return false;
    };

    LogindDrmProvider provider(logind, statStub);
    MockPresenter presenter;
    DrmBackendController controller(provider, presenter);
    provider.bindController(&controller);

    const DrmOutputMode mode{QStringLiteral("DP-1"), 1920, 1080, 60, true};

    // 1. Falha de TakeDevice (EBUSY)
    failTakeDevice = true;
    QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), mode));
    QCOMPARE(controller.state(), DrmOwnershipState::Failed);

    // 2. Lease inativo provoca rollback imediato via ReleaseDevice
    failTakeDevice = false;
    returnInactive = true;
    QVERIFY(!controller.initialize(QStringLiteral("/dev/dri/card1"), mode));
    QCOMPARE(releaseCount, 1);

    // 3. Aquisição bem-sucedida, pausa, substituição de FD em ResumeDevice e shutdown idempotente
    returnInactive = false;
    QVERIFY(controller.initialize(QStringLiteral("/dev/dri/card1"), mode));
    QCOMPARE(controller.state(), DrmOwnershipState::Acquired);
    const int initialFd = controller.activeFd();
    QVERIFY(initialFd >= 0);
    QVERIFY(controller.renderAndPresentFrame());

    logind.onPauseDevice(226, 1, QStringLiteral("pause"));
    QCOMPARE(controller.state(), DrmOwnershipState::Suspended);
    QVERIFY(!controller.renderAndPresentFrame());

    const int resumedRawFd = makePipeReadEndFd();
    QVERIFY(resumedRawFd >= 0);
    QDBusUnixFileDescriptor resumedDbusFd(resumedRawFd);
    ::close(resumedRawFd);
    logind.onResumeDevice(226, 1, resumedDbusFd);
    QCOMPARE(controller.state(), DrmOwnershipState::Acquired);
    QVERIFY(controller.activeFd() >= 0);
    QCOMPARE(presenter.lastFd, controller.activeFd());
    QVERIFY(controller.renderAndPresentFrame());

    controller.shutdown();
    controller.shutdown();
    QCOMPARE(controller.state(), DrmOwnershipState::Released);
    QCOMPARE(releaseCount, 2);
}

void tst_DrmGbmIntegration::kmsDiscoveryConnectorsModesPlanesAndNoMonitors() {
    MockKmsBackend backend;
    KmsOutputDiscovery discovery(backend);

    // FD inválido retorna snapshot inválido
    QVERIFY(!discovery.discover(-1).valid);

    // Sem monitores conectados
    backend.nextSnapshot.valid = true;
    backend.nextSnapshot.atomicSupported = true;
    KmsConnectorInfo disconnected;
    disconnected.connectorId = 10;
    disconnected.name = QStringLiteral("HDMI-A-1");
    disconnected.connected = false;
    backend.nextSnapshot.connectors = {disconnected};

    const auto emptySnap = discovery.discover(15);
    QVERIFY(emptySnap.valid);
    QVERIFY(emptySnap.connectedConnectors().isEmpty());
    QCOMPARE(emptySnap.primaryConnector(), nullptr);

    // Conector conectado + CRTC + plano primário
    KmsCrtcInfo crtc{55, 0, true, 0, 0, 1920, 1080};
    KmsPlaneInfo primaryPlane{77, 0x1, 55, KmsPlaneType::Primary, {0x34325258}};
    KmsConnectorInfo dp;
    dp.connectorId = 20;
    dp.name = QStringLiteral("DP-1");
    dp.connected = true;
    dp.possibleCrtcs = 0x1;
    dp.modes = {
        DrmOutputMode{QStringLiteral("DP-1"), 1280, 720, 60, false},
        DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true}
    };
    dp.preferredMode = KmsOutputDiscovery::selectBestMode(dp.modes);

    backend.nextSnapshot.crtcs = {crtc};
    backend.nextSnapshot.planes = {primaryPlane};
    backend.nextSnapshot.connectors = {disconnected, dp};

    const auto snap = discovery.discover(15);
    QCOMPARE(snap.connectedConnectors().size(), 1);
    QVERIFY(snap.primaryConnector() != nullptr);
    QCOMPARE(snap.primaryConnector()->name, QStringLiteral("DP-1"));
    QCOMPARE(snap.primaryConnector()->preferredMode.width, 1920);
    QCOMPARE(KmsOutputDiscovery::selectCompatibleCrtc(*snap.primaryConnector(), snap.crtcs), 55U);
    QVERIFY(snap.primaryPlaneForCrtc(55) != nullptr);
    QCOMPARE(snap.primaryPlaneForCrtc(55)->planeId, 77U);
}

void tst_DrmGbmIntegration::gbmBufferLifecycleFailuresAndCleanup() {
    MockGbmBackend backend;

    // 1. Falha ao criar dispositivo GBM
    backend.failCreateDevice = true;
    {
        GbmBufferManager mgr(backend);
        QVERIFY(!mgr.initializeDevice(10));
        QVERIFY(!mgr.isDeviceReady());
        QCOMPARE(backend.activeDevices, 0);
    }

    // 2. Falha parcial de alocação limpa buffers anteriores sem vazamento
    backend.failCreateDevice = false;
    backend.failOnAllocIndex = 2;
    {
        GbmBufferManager mgr(backend);
        QVERIFY(mgr.initializeDevice(10));
        const GbmBufferAllocationRequest req{1920, 1080, 0x34325258, {}, true, true};
        QVERIFY(!mgr.allocateSwapchainPool(req, 2));
        QCOMPARE(mgr.bufferCount(), 0);
        QCOMPARE(backend.activeBuffers, 0);
    }

    // 3. Ciclo completo de swapchain e limpeza determinística
    backend.failOnAllocIndex = -1;
    {
        GbmBufferManager mgr(backend);
        QVERIFY(mgr.initializeDevice(10));
        const GbmBufferAllocationRequest req{1920, 1080, 0x34325258, {}, true, true};
        QVERIFY(mgr.allocateSwapchainPool(req, 2));
        QCOMPARE(mgr.bufferCount(), 2);
        QCOMPARE(mgr.countInState(GbmBufferState::Available), 2);

        GbmExportedBuffer *buf = mgr.acquireBufferForRendering();
        QVERIFY(buf != nullptr);
        QVERIFY(!buf->vulkanExternalMemoryVerified);
        QCOMPARE(mgr.countInState(GbmBufferState::Rendering), 1);

        QVERIFY(mgr.markBufferReadyForFlip(buf->id));
        QCOMPARE(mgr.countInState(GbmBufferState::PendingFlip), 1);

        QVERIFY(mgr.markBufferPresented(buf->id, 901));
        QCOMPARE(mgr.countInState(GbmBufferState::Presented), 1);
        QCOMPARE(buf->kmsFramebufferId, 901U);

        QVERIFY(mgr.releaseBufferToAvailable(buf->id));
        QCOMPARE(mgr.countInState(GbmBufferState::Available), 2);
        mgr.shutdown();
        mgr.shutdown();
    }
    QCOMPARE(backend.activeBuffers, 0);
    QCOMPARE(backend.activeDevices, 0);
}

void tst_DrmGbmIntegration::vulkanQuickContractRequiresExplicitExport() {
    VulkanQuickRenderContract contract;
    QVERIFY(contract.requiresOffscreenRenderControl);
    QVERIFY(contract.requiresExplicitModifierImport);
    QVERIFY(contract.requiresSyncFileFence);
    QVERIFY(!contract.verifiedOnPhysicalGpu);
    QVERIFY(contract.requiredDeviceExtensions.contains(QByteArrayLiteral("VK_KHR_external_memory_fd")));
    QVERIFY(contract.requiredDeviceExtensions.contains(QByteArrayLiteral("VK_EXT_image_drm_format_modifier")));
}

QTEST_MAIN(tst_DrmGbmIntegration)
#include "tst_DrmGbmIntegration.moc"
