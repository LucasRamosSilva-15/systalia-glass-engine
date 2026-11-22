#include <QtTest>

#include "AtomicKmsPresenter.hpp"
#include "DrmEventDispatcher.hpp"
#include "GbmBufferManager.hpp"
#include "IndependentFrameLoop.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"

#include <QColor>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

class MockKms final : public IKmsResourceBackend {
public:
    KmsDiscoverySnapshot probeResources(int drmFd) override {
        KmsDiscoverySnapshot snap;
        if (drmFd < 0) {
            return snap;
        }
        snap.valid = true;
        snap.atomicSupported = true;
        snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 128, 128});
        snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
        KmsConnectorInfo dp;
        dp.connectorId = 20;
        dp.name = QStringLiteral("DP-1");
        dp.connected = true;
        dp.possibleCrtcs = 0x1;
        dp.modes.append(DrmOutputMode{QStringLiteral("DP-1"), 128, 128, 60, true});
        dp.preferredMode = dp.modes.first();
        snap.connectors.append(dp);
        return snap;
    }
};

class MockGbm final : public IGbmBackend {
public:
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &, GbmExportedBuffer *) override {
        return true;
    }
    void destroyBuffer(GbmExportedBuffer *) override {}
};

class MockAtomic final : public IAtomicKmsBackend {
public:
    bool failAddFb = false;
    uint32_t nextFbId = 500;
    QList<uint32_t> createdFbs;
    QList<uint32_t> removedFbs;
    QList<int> seenDmabufFds;
    QList<uint32_t> seenStrides;
    QList<AtomicCommitRequest> commits;

    uint32_t addFramebuffer(int, const GbmExportedBuffer &buffer) override {
        if (failAddFb || buffer.dmabufFd < 0 || buffer.stride == 0) {
            return 0;
        }
        const uint32_t fb = nextFbId++;
        createdFbs.append(fb);
        seenDmabufFds.append(buffer.dmabufFd);
        seenStrides.append(buffer.stride);
        return fb;
    }

    void removeFramebuffer(int, uint32_t fbId) override {
        removedFbs.append(fbId);
    }

    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
        commits.append(req);
        return AtomicCommitResult{req.framebufferId != 0, -1};
    }
};

class MockSink final : public IWaylandOutputFrameSink {
public:
    int started = 0;
    int sent = 0;
    void setAutomaticCallbacks(bool) override {}
    void notifyFrameStarted() override { ++started; }
    void notifySendFrameCallbacks() override { ++sent; }
};

} // namespace

class tst_UnifiedVulkanGbmPool final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void formatAndModifierNegotiationRejectsIncompatibleSets();
    void doubleAndTripleBufferingOnRealRadvGpuAlternatesSlotsWithReadback();
    void addFramebufferFailureRollsBackWithoutLeaking();
};

void tst_UnifiedVulkanGbmPool::formatAndModifierNegotiationRejectsIncompatibleSets() {
    const auto failEmpty = GbmBufferManager::negotiateModifiers(DRM_FORMAT_XRGB8888, {}, {0ULL});
    QVERIFY(!failEmpty.compatible);
    QVERIFY(!failEmpty.failureReason.isEmpty());

    const auto failMismatch = GbmBufferManager::negotiateModifiers(
        DRM_FORMAT_XRGB8888, {0x1234ULL}, {0ULL});
    QVERIFY(!failMismatch.compatible);

    const auto ok = GbmBufferManager::negotiateModifiers(
        DRM_FORMAT_XRGB8888, {0x9999ULL, DRM_FORMAT_MOD_LINEAR}, {DRM_FORMAT_MOD_LINEAR});
    QVERIFY(ok.compatible);
    QCOMPARE(ok.negotiatedModifiers, QList<uint64_t>({DRM_FORMAT_MOD_LINEAR}));
}

void tst_UnifiedVulkanGbmPool::doubleAndTripleBufferingOnRealRadvGpuAlternatesSlotsWithReadback() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));
    QVERIFY(renderer.capabilities().supportsExternalMemoryFd);
    QVERIFY(renderer.capabilities().supportsExternalMemoryDmabuf);

    const QList<uint64_t> vkMods = renderer.querySupportedDrmModifiers();
    QVERIFY(!vkMods.isEmpty());

    MockKms kms;
    MockGbm gbm;
    MockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));

    // Cria pool unificado double-buffered (2 slots Vulkan->DMA-BUF reais em RADV)
    QVERIFY(renderer.createSharedPoolSlots(presenter.bufferManager(), QSize(128, 128), 2));
    QCOMPARE(presenter.bufferManager().bufferCount(), 2);

    const auto &poolSlots = presenter.bufferManager().buffers();
    QVERIFY(poolSlots[0].dmabufFd >= 0 && poolSlots[1].dmabufFd >= 0);
    QVERIFY(poolSlots[0].dmabufFd != poolSlots[1].dmabufFd);
    QVERIFY(poolSlots[0].stride >= 128 * 4);
    QVERIFY(poolSlots[0].vulkanExternalMemoryVerified);
    const int fd0 = poolSlots[0].dmabufFd;
    const int fd1 = poolSlots[1].dmabufFd;

    const QByteArray qml =
        "import QtQuick 2.15\n"
        "Rectangle {\n"
        "  width: 128; height: 128; color: '#0b1e36'\n"
        "  Rectangle { x: 20; y: 20; width: 88; height: 88; color: '#33dd99' }\n"
        "}\n";
    QVERIFY2(renderer.loadQmlSource(qml), qPrintable(renderer.lastError()));

    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(renderer, presenter, wayland, dispatcher);

    // Frame 1 -> Slot 0
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(atomic.commits.size(), 1);
    QCOMPARE(atomic.seenDmabufFds.at(0), fd0);
    const QImage img0 = renderer.readbackImage();
    QVERIFY(!img0.isNull());
    QVERIFY(qAbs(img0.pixelColor(64, 64).green() - 0xdd) <= 4);

    dispatcher.dispatchSimulatedPageFlip(1, 0);
    QCOMPARE(sink.sent, 1);

    // Frame 2 -> Slot 1 (alternância real de buffer no mesmo pool)
    loop.requestFrame();
    QTRY_VERIFY(loop.isFlipInFlight());
    QCOMPARE(atomic.commits.size(), 2);
    QCOMPARE(atomic.seenDmabufFds.at(1), fd1);
    const QImage img1 = renderer.readbackImage();
    QVERIFY(!img1.isNull());
    QVERIFY(qAbs(img1.pixelColor(64, 64).green() - 0xdd) <= 4);

    dispatcher.dispatchSimulatedPageFlip(2, 0);
    QCOMPARE(sink.sent, 2);

    loop.shutdown();
    renderer.destroySharedPoolSlots(presenter.bufferManager());
    presenter.shutdown();
    renderer.shutdown();

    QCOMPARE(atomic.removedFbs.size(), 2);
    // Confirma que os descritores DMA-BUF foram realmente fechados
    QCOMPARE(::fcntl(fd0, F_GETFD), -1);
    QCOMPARE(::fcntl(fd1, F_GETFD), -1);

    // Triple buffering (3 slots)
    VulkanQuickRenderer renderer3;
    QVERIFY2(renderer3.initialize(QSize(64, 64)), qPrintable(renderer3.lastError()));
    AtomicKmsPresenter presenter3(kms, gbm, atomic);
    QVERIFY(presenter3.initializeOnFd(10));
    QVERIFY(renderer3.createSharedPoolSlots(presenter3.bufferManager(), QSize(64, 64), 3));
    QCOMPARE(presenter3.bufferManager().bufferCount(), 3);
    renderer3.destroySharedPoolSlots(presenter3.bufferManager());
    presenter3.shutdown();
    renderer3.shutdown();
}

void tst_UnifiedVulkanGbmPool::addFramebufferFailureRollsBackWithoutLeaking() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));

    MockKms kms;
    MockGbm gbm;
    MockAtomic atomic;
    atomic.failAddFb = true;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    QVERIFY(presenter.initializeOnFd(10));
    QVERIFY(renderer.createSharedPoolSlots(presenter.bufferManager(), QSize(64, 64), 2));

    const QByteArray qml = "import QtQuick 2.15\nRectangle { width: 64; height: 64; color: '#112233' }\n";
    QVERIFY(renderer.loadQmlSource(qml));

    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(renderer, presenter, wayland, dispatcher);

    loop.requestFrame();
    QTRY_COMPARE(loop.failedCommitCount(), 1ULL);
    QVERIFY(!loop.isFlipInFlight());
    QCOMPARE(sink.sent, 0);
    QCOMPARE(presenter.bufferManager().countInState(GbmBufferState::Available), 2);

    loop.shutdown();
    renderer.destroySharedPoolSlots(presenter.bufferManager());
    presenter.shutdown();
    renderer.shutdown();
}

QTEST_MAIN(tst_UnifiedVulkanGbmPool)
#include "tst_UnifiedVulkanGbmPool.moc"
