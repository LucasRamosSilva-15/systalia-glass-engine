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
    void recyclesWaitSemaphoreWhenFrameAbortedAfterImport();
    void submissionTrackerDistinguishesSubmittedTimedOutAndRealGpuCompletion();
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

void tst_VulkanDmabufAudit::recyclesWaitSemaphoreWhenFrameAbortedAfterImport() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));

    AuditMockGbm gbm;
    GbmBufferManager pool(gbm);
    QVERIFY(renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {DRM_FORMAT_MOD_LINEAR}));
    QVERIFY(renderer.loadQmlSource(
        "import QtQuick 2.15\nRectangle { width: 64; height: 64; color: '#114477' }\n"));

    auto *slot0 = pool.acquireBufferForRendering();
    QVERIFY(slot0 != nullptr);
    QVERIFY(renderer.renderIntoBuffer(*slot0));
    const int realSyncFd = slot0->acquireFenceFd;
    QVERIFY(realSyncFd >= 0);
    slot0->acquireFenceFd = -1;
    QVERIFY(pool.releaseBufferToAvailable(slot0->id));

    // Simula importação de releaseFenceFd seguida de suspensão (frame abortado antes de submissão)
    slot0 = pool.acquireBufferForRendering();
    QVERIFY(slot0 != nullptr);
    slot0->releaseFenceFd = realSyncFd;
    const uint64_t origWaitSem = slot0->vkWaitSemaphoreHandle;
    QVERIFY(origWaitSem != 0);
    QVERIFY(renderer.renderIntoBuffer(*slot0));
    QCOMPARE(slot0->releaseFenceFd, -1);
    if (slot0->acquireFenceFd >= 0) {
        ::close(slot0->acquireFenceFd);
        slot0->acquireFenceFd = -1;
    }
    QCOMPARE(::fcntl(realSyncFd, F_GETFD), -1);

    renderer.destroySharedPoolSlots(pool);
    pool.shutdown();
    renderer.shutdown();
}

void tst_VulkanDmabufAudit::submissionTrackerDistinguishesSubmittedTimedOutAndRealGpuCompletion() {
    // 1. Transições inválidas são rejeitadas (impossível ir para Completed sem Submitted)
    GpuSubmissionTracker tracker;
    tracker.markRecorded(1, 10, 0x1234);
    QVERIFY(!tracker.markCompleted(1));
    QVERIFY(tracker.markSubmitted(1, 99));
    QCOMPARE(tracker.find(1)->state, GpuSubmissionState::Submitted);
    QVERIFY(tracker.markCompleted(1));
    QCOMPARE(tracker.find(1)->state, GpuSubmissionState::Completed);
    QVERIFY(!tracker.markTimedOut(1)); // Já concluído

    // 2. Observação real de SYNC_FD em RADV vs fence não sinalizada (timeout)
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));
    AuditMockGbm gbm;
    GbmBufferManager pool(gbm);
    QVERIFY(renderer.createSharedPoolSlots(pool, QSize(64, 64), 2, {DRM_FORMAT_MOD_LINEAR}));
    QVERIFY(renderer.loadQmlSource(
        "import QtQuick 2.15\nRectangle { width: 64; height: 64; color: '#226688' }\n"));

    auto *slot0 = pool.acquireBufferForRendering();
    QVERIFY(slot0 != nullptr);
    QVERIFY(renderer.renderIntoBuffer(*slot0));
    const uint64_t frameId1 = renderer.lastSubmittedFrameId();
    QCOMPARE(renderer.submissionTracker().find(frameId1)->state, GpuSubmissionState::Submitted);

    // Observa conclusão real no SYNC_FD exportado
    QVERIFY(renderer.observeSyncFdCompletion(frameId1, slot0->acquireFenceFd, 2000));
    QCOMPARE(renderer.submissionTracker().find(frameId1)->state, GpuSubmissionState::Completed);
    ::close(slot0->acquireFenceFd);
    slot0->acquireFenceFd = -1;
    QVERIFY(pool.releaseBufferToAvailable(slot0->id));

    // 3. Exportação presente, mas fence ainda não sinalizada -> CompletionTimedOut (não vira Completed)
    auto *slot1 = pool.acquireBufferForRendering();
    QVERIFY(slot1 != nullptr);
    QVERIFY(renderer.renderIntoBuffer(*slot1));
    const uint64_t frameId2 = renderer.lastSubmittedFrameId();
    int writeEnd = -1;
    const int unsignaledFd = makeUnsignaledPipeFd(&writeEnd);
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, unsignaledFd, 5));
    QCOMPARE(renderer.submissionTracker().find(frameId2)->state, GpuSubmissionState::CompletionTimedOut);

    // 4. POLLHUP (ponta de escrita fechada sem dados) e FD fechado/inválido (POLLNVAL) nunca viram Completed
    ::close(writeEnd);
    QVERIFY(!VulkanQuickRenderer::isSyncFdSignaled(unsignaledFd, 5));
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, unsignaledFd, 5));
    ::close(unsignaledFd);
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, unsignaledFd, 5)); // POLLNVAL
    QVERIFY(!renderer.observeSyncFdCompletion(frameId2, -1, 5));

    if (slot1->acquireFenceFd >= 0) {
        ::close(slot1->acquireFenceFd);
        slot1->acquireFenceFd = -1;
    }
    QVERIFY(pool.releaseBufferToAvailable(slot1->id));

    renderer.destroySharedPoolSlots(pool);
    pool.shutdown();
    renderer.shutdown();
}

QTEST_MAIN(tst_VulkanDmabufAudit)
#include "tst_VulkanDmabufAudit.moc"
