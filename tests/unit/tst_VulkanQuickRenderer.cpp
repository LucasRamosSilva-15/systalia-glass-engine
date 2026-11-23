#include <QtTest>

#include "AtomicKmsPresenter.hpp"
#include "VulkanQuickRenderer.hpp"

#include <QColor>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

class MockKms final : public IKmsResourceBackend {
public:
    bool failProbe = false;
    KmsDiscoverySnapshot probeResources(int drmFd) override {
        KmsDiscoverySnapshot snap;
        if (failProbe || drmFd < 0) {
            return snap;
        }
        snap.valid = true;
        snap.atomicSupported = true;
        snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 1920, 1080});
        snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {0x34325258}});
        KmsConnectorInfo dp;
        dp.connectorId = 20;
        dp.name = QStringLiteral("DP-1");
        dp.connected = true;
        dp.possibleCrtcs = 0x1;
        dp.modes.append(DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true});
        dp.preferredMode = dp.modes.first();
        snap.connectors.append(dp);
        return snap;
    }
};

class MockGbm final : public IGbmBackend {
public:
    bool failAlloc = false;
    int activeBuffers = 0;

    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &req, GbmExportedBuffer *out) override {
        if (failAlloc) {
            return false;
        }
        ++activeBuffers;
        out->width = req.width;
        out->height = req.height;
        out->format = req.format;
        out->stride = req.width * 4;
        out->nativeHandle = reinterpret_cast<void *>(0x2);
        return true;
    }
    void destroyBuffer(GbmExportedBuffer *buf) override {
        if (buf && buf->nativeHandle) {
            --activeBuffers;
            buf->nativeHandle = nullptr;
        }
    }
};

class MockAtomic final : public IAtomicKmsBackend {
public:
    bool failCommit = false;
    int commitCount = 0;
    AtomicCommitRequest lastReq;

    uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override { return static_cast<uint32_t>(b.id); }
    void removeFramebuffer(int, uint32_t) override {}
    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
        ++commitCount;
        lastReq = req;
        return AtomicCommitResult{!failCommit, -1};
    }
};

} // namespace

class tst_VulkanQuickRenderer final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void rejectsInvalidTargetAndUninitializedCalls();
    void rendersQmlSceneAndReadsBackExpectedPixels();
    void suspendResizeAndIdempotentShutdown();
    void atomicKmsPresenterCycleRollbackAndResume();
};

void tst_VulkanQuickRenderer::rejectsInvalidTargetAndUninitializedCalls() {
    VulkanQuickRenderer renderer;
    QVERIFY(!renderer.isInitialized());
    QVERIFY(!renderer.renderFrame());
    QVERIFY(renderer.readbackImage().isNull());
    QCOMPARE(renderer.exportDmabufFd(), -1);
    QVERIFY(!renderer.initialize(QSize(0, 0)));
    QVERIFY(!renderer.initialize(QSize(-10, 100)));
}

void tst_VulkanQuickRenderer::rendersQmlSceneAndReadsBackExpectedPixels() {
    VulkanQuickRenderer renderer;
    if (!renderer.initialize(QSize(64, 64))) {
        QSKIP(qPrintable(QStringLiteral("Vulkan ICD indisponível no ambiente: ") + renderer.lastError()));
    }

    const QByteArray qml =
        "import QtQuick 2.15\n"
        "Rectangle {\n"
        "  width: 64; height: 64; color: '#102030'\n"
        "  Rectangle { x: 16; y: 16; width: 32; height: 32; color: '#00ff00' }\n"
        "}\n";
    QVERIFY2(renderer.loadQmlSource(qml), qPrintable(renderer.lastError()));
    QVERIFY(renderer.renderFrame());

    const QImage img = renderer.readbackImage();
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(64, 64));

    const QColor corner = img.pixelColor(4, 4);
    const QColor center = img.pixelColor(32, 32);
    QVERIFY(qAbs(corner.red() - 0x10) <= 4);
    QVERIFY(qAbs(corner.green() - 0x20) <= 4);
    QVERIFY(qAbs(corner.blue() - 0x30) <= 4);
    QVERIFY(center.green() >= 250 && center.red() <= 4 && center.blue() <= 4);

    if (renderer.capabilities().dmabufExportImageCreated) {
        const int fd = renderer.exportDmabufFd();
        QVERIFY(fd >= 0);
        ::close(fd);
    }
}

void tst_VulkanQuickRenderer::suspendResizeAndIdempotentShutdown() {
    VulkanQuickRenderer renderer;
    if (!renderer.initialize(QSize(64, 64))) {
        QSKIP("Vulkan ICD indisponível no ambiente.");
    }

    renderer.setSuspended(true);
    QVERIFY(renderer.isSuspended());
    QVERIFY(!renderer.renderFrame());

    renderer.setSuspended(false);
    QVERIFY(renderer.resize(QSize(96, 96)));
    QCOMPARE(renderer.pixelSize(), QSize(96, 96));

    renderer.shutdown();
    renderer.shutdown();
    QVERIFY(!renderer.isInitialized());
}

void tst_VulkanQuickRenderer::atomicKmsPresenterCycleRollbackAndResume() {
    MockKms kms;
    MockGbm gbm;
    MockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);

    QVERIFY(presenter.initializeOnFd(10));
    QCOMPARE(presenter.activeConnectorId(), 20U);
    QCOMPARE(presenter.activeCrtcId(), 55U);
    QCOMPARE(presenter.activePlaneId(), 77U);

    QVERIFY(presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true}));
    QCOMPARE(presenter.bufferManager().bufferCount(), 2);

    // Apresentação válida e page-flip completion
    QVERIFY(presenter.presentFrame(1, 10));
    QCOMPARE(atomic.commitCount, 1);
    QCOMPARE(presenter.bufferManager().countInState(GbmBufferState::PendingFlip), 1);
    QVERIFY(presenter.onPageFlipCompleted(atomic.lastReq.framebufferId));
    QCOMPARE(presenter.bufferManager().countInState(GbmBufferState::Presented), 1);

    // Falha de commit devolve buffer para Available sem prendê-lo
    atomic.failCommit = true;
    QVERIFY(!presenter.presentFrame(2, 10));
    QCOMPARE(presenter.bufferManager().countInState(GbmBufferState::Available), 1);

    // Suspensão bloqueia commits; retomada substitui FD
    atomic.failCommit = false;
    presenter.suspendPresentation();
    QVERIFY(presenter.isSuspended());
    QVERIFY(!presenter.presentFrame(3, 10));
    QVERIFY(presenter.resumePresentation(25));
    QVERIFY(!presenter.isSuspended());
    QVERIFY(presenter.presentFrame(4, 25));

    presenter.shutdown();
    presenter.shutdown();
    QCOMPARE(gbm.activeBuffers, 0);
}

QTEST_MAIN(tst_VulkanQuickRenderer)
#include "tst_VulkanQuickRenderer.moc"
