#include <QtTest>

#include "GbmBufferManager.hpp"
#include "VulkanQuickRenderer.hpp"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

class AuditMockGbm final : public IGbmBackend {
public:
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &, GbmExportedBuffer *) override { return true; }
    void destroyBuffer(GbmExportedBuffer *) override {}
};

int makeUnsignaledPipeFd(int *writeEndOut) {
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return -1;
    }
    *writeEndOut = fds[1];
    return fds[0];
}

} // namespace

class tst_VulkanDmabufAudit final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void auditsExternalMemoryAndDrmModifierCapabilitiesOnRadv();
    void runsSafeMatrixScenariosAandBWithDeterministicCleanup();
    void rejectsUnsignaledReleaseFenceAndIncompatibleModifiersSafely();
};

void tst_VulkanDmabufAudit::auditsExternalMemoryAndDrmModifierCapabilitiesOnRadv() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));

    const auto caps = renderer.capabilities();
    QVERIFY(caps.supportsExternalMemoryFd);
    QVERIFY(caps.supportsExternalMemoryDmabuf);
    QVERIFY(caps.supportsDrmFormatModifier);
    QVERIFY(caps.supportsExternalSemaphoreFd);
    QVERIFY(caps.supportsSyncFdExport);
    QVERIFY(caps.supportsSyncFdImport);

    const auto modCaps = renderer.queryDrmModifierCapabilities();
    QVERIFY(!modCaps.isEmpty());

    const auto singlePlaneExportable = renderer.querySupportedDrmModifiers();
    QVERIFY(!singlePlaneExportable.isEmpty());
    QVERIFY(singlePlaneExportable.contains(DRM_FORMAT_MOD_LINEAR));

    for (const auto &entry : modCaps) {
        if (singlePlaneExportable.contains(entry.modifier)) {
            QCOMPARE(entry.planeCount, 1U);
            QVERIFY(entry.dmabufExportable);
        }
    }

    renderer.shutdown();
}

void tst_VulkanDmabufAudit::runsSafeMatrixScenariosAandBWithDeterministicCleanup() {
    // Cenário A: 128x128, Vulkan interno, efeitos desativados
    {
        VulkanQuickRenderer rendererA;
        QVERIFY2(rendererA.initialize(QSize(128, 128)), qPrintable(rendererA.lastError()));
        QVERIFY(rendererA.loadQmlSource(
            "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#102030' }\n"));
        for (int i = 0; i < 3; ++i) {
            QVERIFY(rendererA.renderFrame());
            QVERIFY(!rendererA.isDeviceLost());
        }
        rendererA.shutdown();
    }

    // Cenário B: 128x128, pool DMA-BUF (2 slots), efeitos desativados
    {
        VulkanQuickRenderer rendererB;
        QVERIFY2(rendererB.initialize(QSize(128, 128)), qPrintable(rendererB.lastError()));
        AuditMockGbm gbm;
        GbmBufferManager pool(gbm);
        QVERIFY(rendererB.createSharedPoolSlots(pool, QSize(128, 128), 2, {DRM_FORMAT_MOD_LINEAR}));
        QCOMPARE(pool.bufferCount(), 2);

        const int fd0 = pool.buffers().at(0).dmabufFd;
        const int fd1 = pool.buffers().at(1).dmabufFd;
        QCOMPARE(pool.buffers().at(0).modifier, DRM_FORMAT_MOD_LINEAR);
        QVERIFY(pool.buffers().at(0).stride >= 128U * 4U);

        QVERIFY(rendererB.loadQmlSource(
            "import QtQuick 2.15\nRectangle { width: 128; height: 128; color: '#228866' }\n"));

        for (int i = 0; i < 3; ++i) {
            auto *slot = pool.acquireBufferForRendering();
            QVERIFY(slot != nullptr);
            QVERIFY(rendererB.renderIntoBuffer(*slot));
            QVERIFY(slot->acquireFenceFd >= 0);
            QVERIFY(VulkanQuickRenderer::isSyncFdSignaled(slot->acquireFenceFd, 2000));
            ::close(slot->acquireFenceFd);
            slot->acquireFenceFd = -1;
            QVERIFY(pool.releaseBufferToAvailable(slot->id));
        }

        rendererB.destroySharedPoolSlots(pool);
        pool.shutdown();
        rendererB.shutdown();

        QCOMPARE(::fcntl(fd0, F_GETFD), -1);
        QCOMPARE(::fcntl(fd1, F_GETFD), -1);
    }
}

void tst_VulkanDmabufAudit::rejectsUnsignaledReleaseFenceAndIncompatibleModifiersSafely() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));

    AuditMockGbm gbm;
    GbmBufferManager pool(gbm);

    // Modificador incompatível falha sem vazar slots
    QVERIFY(!renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {0xdeadbeefcafebabeULL}));
    QCOMPARE(pool.bufferCount(), 0);

    // Criação válida e verificação de isSyncFdSignaled em fence não sinalizada
    QVERIFY(renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {DRM_FORMAT_MOD_LINEAR}));
    int writeEnd = -1;
    const int unsignaledReadFd = makeUnsignaledPipeFd(&writeEnd);
    QVERIFY(unsignaledReadFd >= 0);
    QVERIFY(!VulkanQuickRenderer::isSyncFdSignaled(unsignaledReadFd, 5));
    ::close(writeEnd);
    ::close(unsignaledReadFd);

    renderer.destroySharedPoolSlots(pool);
    pool.shutdown();
    renderer.shutdown();
}

QTEST_MAIN(tst_VulkanDmabufAudit)
#include "tst_VulkanDmabufAudit.moc"
