#include "VulkanQuickRenderer.hpp"
#include "GbmBufferManager.hpp"

#include <drm_fourcc.h>
#include <QDebug>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QThread>
#include <QUrl>
#include <QVulkanDeviceFunctions>
#include <QVulkanFunctions>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <cstring>
#include <poll.h>
#include <unistd.h>
#include <vector>

extern "C" PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char *pName);

namespace Systalia::Platform::Drm {

namespace {

qint64 monotonicNs() noexcept {
    static QElapsedTimer s_timer;
    if (!s_timer.isValid()) {
        s_timer.start();
    }
    return s_timer.nsecsElapsed();
}

bool hasExtension(const std::vector<VkExtensionProperties> &exts, const char *name) {
    for (const auto &ext : exts) {
        if (std::strcmp(ext.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

void GpuSubmissionTracker::reset(uint64_t newGeneration) noexcept {
    m_generation = newGeneration;
    m_records.clear();
}

void GpuSubmissionTracker::markRecorded(uint64_t frameId, uint64_t slotId, uint64_t semaphoreHandle) {
    if (m_records.size() >= 64) {
        m_records.removeFirst();
    }
    GpuSubmissionRecord rec;
    rec.frameId = frameId;
    rec.slotId = slotId;
    rec.generation = m_generation;
    rec.semaphoreHandle = semaphoreHandle;
    rec.state = GpuSubmissionState::Recorded;
    m_records.append(rec);
}

bool GpuSubmissionTracker::markSubmitted(uint64_t frameId, int syncFd) {
    GpuSubmissionRecord *rec = findMutable(frameId);
    if (!rec || rec->state != GpuSubmissionState::Recorded) {
        return false;
    }
    rec->syncFd = syncFd;
    rec->submittedAtNs = monotonicNs();
    rec->state = GpuSubmissionState::Submitted;
    return true;
}

bool GpuSubmissionTracker::markCompleted(uint64_t frameId) {
    GpuSubmissionRecord *rec = findMutable(frameId);
    if (!rec || rec->state != GpuSubmissionState::Submitted) {
        return false;
    }
    rec->completedAtNs = monotonicNs();
    rec->state = GpuSubmissionState::Completed;
    return true;
}

bool GpuSubmissionTracker::markTimedOut(uint64_t frameId) {
    GpuSubmissionRecord *rec = findMutable(frameId);
    if (!rec || rec->state != GpuSubmissionState::Submitted) {
        return false;
    }
    rec->state = GpuSubmissionState::CompletionTimedOut;
    return true;
}

void GpuSubmissionTracker::markFailed(uint64_t frameId,
                                      GpuSubmissionState failureState,
                                      const QString &reason) {
    GpuSubmissionRecord *rec = findMutable(frameId);
    if (!rec) {
        return;
    }
    rec->state = failureState;
    rec->failureReason = reason;
}

const GpuSubmissionRecord *GpuSubmissionTracker::find(uint64_t frameId) const noexcept {
    for (const auto &rec : m_records) {
        if (rec.frameId == frameId) {
            return &rec;
        }
    }
    return nullptr;
}

GpuSubmissionRecord *GpuSubmissionTracker::findMutable(uint64_t frameId) noexcept {
    for (auto &rec : m_records) {
        if (rec.frameId == frameId) {
            return &rec;
        }
    }
    return nullptr;
}

VulkanQuickRenderer::VulkanQuickRenderer(QObject *parent)
    : QObject(parent) {}

VulkanQuickRenderer::~VulkanQuickRenderer() {
    shutdown();
}

QQuickItem *VulkanQuickRenderer::rootItem() const noexcept {
    return m_rootItem.data();
}

QQmlEngine *VulkanQuickRenderer::qmlEngine() const noexcept {
    return m_activeEngine.data();
}

bool VulkanQuickRenderer::initialize(const QSize &pixelSize, bool allowDmabufExport) {
    if (m_initialized || m_shuttingDown) {
        return false;
    }
    if (!pixelSize.isValid() || pixelSize.width() <= 0 || pixelSize.height() <= 0) {
        m_lastError = QStringLiteral("Resolução inválida para render target Vulkan.");
        return false;
    }

    m_lastError.clear();
    m_pixelSize = pixelSize;
    m_allowDmabufExportOnInternalImage = allowDmabufExport;

    if (!createVulkanDevice()) {
        shutdown();
        return false;
    }
    if (!createCommandPool()) {
        shutdown();
        return false;
    }
    if (!createOffscreenImage(m_pixelSize)) {
        shutdown();
        return false;
    }

    m_renderControl = std::make_unique<QQuickRenderControl>();
    m_window = std::make_unique<QQuickWindow>(m_renderControl.get());
    m_window->setVulkanInstance(&m_vulkanInstance);
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
        m_physicalDevice, m_device, static_cast<int>(m_graphicsQueueFamily), 0));
    m_window->setGeometry(0, 0, m_pixelSize.width(), m_pixelSize.height());
    m_window->contentItem()->setSize(QSizeF(m_pixelSize));

    if (!m_renderControl->initialize()) {
        m_lastError = QStringLiteral("Falha em QQuickRenderControl::initialize() sobre Vulkan.");
        shutdown();
        return false;
    }

    m_window->setRenderTarget(QQuickRenderTarget::fromVulkanImage(
        m_colorImage, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1));

    m_ownedQmlEngine = std::make_unique<QQmlEngine>();
    m_activeEngine = m_ownedQmlEngine.get();
    m_initialized = true;
    if (graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] INTERNAL_RENDER_TARGET_CREATED size=" << m_pixelSize
                << "vkFormat=" << static_cast<int>(m_vkFormat)
                << "dmabufExport=" << m_capabilities.dmabufExportImageCreated;
    }
    return true;
}

bool VulkanQuickRenderer::loadQmlSource(const QByteArray &qmlSource) {
    if (!m_initialized || !m_activeEngine || !m_window) {
        m_lastError = QStringLiteral("Renderer não inicializado para carregar QML.");
        return false;
    }

    if (m_rootItem) {
        m_rootItem->setParentItem(nullptr);
        delete m_rootItem;
        m_rootItem = nullptr;
    }
    m_qmlComponent = std::make_unique<QQmlComponent>(m_activeEngine);
    m_qmlComponent->setData(qmlSource, QUrl());
    if (m_qmlComponent->isError()) {
        m_lastError = m_qmlComponent->errorString();
        return false;
    }

    QObject *obj = m_qmlComponent->create();
    auto *item = qobject_cast<QQuickItem *>(obj);
    if (!item) {
        delete obj;
        m_lastError = QStringLiteral("Objeto raiz QML não é QQuickItem.");
        return false;
    }

    m_rootItem = item;
    m_rootItem->setParentItem(m_window->contentItem());
    m_rootItem->setSize(QSizeF(m_pixelSize));
    return true;
}

bool VulkanQuickRenderer::loadQmlUrl(const QUrl &qmlUrl, QQmlEngine *externalEngine) {
    if (!m_initialized || !m_window) {
        m_lastError = QStringLiteral("Renderer não inicializado para carregar URL QML.");
        return false;
    }
    if (externalEngine) {
        m_activeEngine = externalEngine;
        m_ownedQmlEngine.reset();
    }
    if (!m_activeEngine) {
        m_lastError = QStringLiteral("QQmlEngine indisponível.");
        return false;
    }

    if (m_rootItem) {
        m_rootItem->setParentItem(nullptr);
        delete m_rootItem;
        m_rootItem = nullptr;
    }

    m_qmlComponent = std::make_unique<QQmlComponent>(m_activeEngine, qmlUrl);
    if (m_qmlComponent->isError()) {
        m_lastError = m_qmlComponent->errorString();
        return false;
    }

    QObject *obj = m_qmlComponent->createWithInitialProperties(
        {{QStringLiteral("hostWindow"), QVariant::fromValue(static_cast<QObject *>(m_window.get()))}});
    if (m_qmlComponent->isError()) {
        delete obj;
        m_lastError = m_qmlComponent->errorString();
        return false;
    }

    auto *item = qobject_cast<QQuickItem *>(obj);
    if (!item) {
        delete obj;
        m_lastError = QStringLiteral("Objeto raiz QML não é QQuickItem (verifique ShellRoot.qml).");
        return false;
    }

    m_rootItem = item;
    m_rootItem->setParentItem(m_window->contentItem());
    m_rootItem->setSize(QSizeF(m_pixelSize));
    return true;
}

bool VulkanQuickRenderer::resize(const QSize &pixelSize) {
    if (!m_initialized || !pixelSize.isValid() || pixelSize.width() <= 0 || pixelSize.height() <= 0) {
        return false;
    }
    if (pixelSize == m_pixelSize) {
        return true;
    }

    if (QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device)) {
        df->vkDeviceWaitIdle(m_device);
    }
    m_window->setRenderTarget(QQuickRenderTarget{});
    destroyOffscreenImage();

    m_pixelSize = pixelSize;
    if (!createOffscreenImage(m_pixelSize)) {
        m_lastError = QStringLiteral("Falha ao recriar VkImage após resize.");
        return false;
    }

    m_window->setGeometry(0, 0, m_pixelSize.width(), m_pixelSize.height());
    m_window->setRenderTarget(QQuickRenderTarget::fromVulkanImage(
        m_colorImage, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1));
    if (m_rootItem) {
        m_rootItem->setSize(QSizeF(m_pixelSize));
    }
    return true;
}

bool VulkanQuickRenderer::graphicsDiagnosticsEnabled() noexcept {
    static const bool enabled = qEnvironmentVariableIntValue("SGE_GRAPHICS_DIAGNOSTICS") > 0;
    return enabled;
}

void VulkanQuickRenderer::simulateDeviceLostForTest(const QString &reason) {
    if (m_deviceLost) {
        return;
    }
    m_deviceLost = true;
    m_lastError = reason;
    qCritical() << "[SGE-VULKAN] Device lost:" << m_lastError;
    emit deviceLost(m_lastError);
}

bool VulkanQuickRenderer::renderFrame() {
    if (!m_initialized || m_shuttingDown || m_suspended || m_deviceLost || !m_renderControl || m_colorImage == VK_NULL_HANDLE) {
        return false;
    }

    const uint64_t frameId = ++m_frameCounter;
    if (graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] FRAME_START frameId=" << frameId
                << "mode=internal size=" << m_pixelSize
                << "vkFormat=" << static_cast<int>(m_vkFormat)
                << "dmabufExport=" << m_capabilities.dmabufExportImageCreated;
    }

    m_activeRenderImage = m_colorImage;
    m_window->setRenderTarget(QQuickRenderTarget::fromVulkanImage(
        m_colorImage, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1));
    m_renderControl->polishItems();
    m_renderControl->beginFrame();
    const bool synced = m_renderControl->sync();
    if (synced) {
        m_renderControl->render();
        m_submissionTracker.markRecorded(frameId, 0, 0);
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] CPU_COMMAND_RECORDED frameId=" << frameId << "mode=internal";
        }
    }
    m_renderControl->endFrame();

    if (m_window && m_window->rhi() && m_window->rhi()->isDeviceLost()) {
        m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                       QStringLiteral("QRhi::isDeviceLost() após endFrame()"));
        simulateDeviceLostForTest(QStringLiteral("QRhi::isDeviceLost() após endFrame()"));
        return false;
    }
    if (!synced) {
        return false;
    }

    Q_UNUSED(m_submissionTracker.markSubmitted(frameId, -1));
    if (graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] GPU_SUBMITTED frameId=" << frameId << "mode=internal syncFd=-1";
    }
    return observeInternalGpuCompletion(frameId);
}

bool VulkanQuickRenderer::observeInternalGpuCompletion(uint64_t frameId, uint64_t timeoutNs) {
    if (m_deviceLost || m_device == VK_NULL_HANDLE || m_graphicsQueue == VK_NULL_HANDLE) {
        m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                       QStringLiteral("Dispositivo Vulkan indisponível para VkFence."));
        return false;
    }
    QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device);
    if (!df) {
        m_submissionTracker.markFailed(frameId, GpuSubmissionState::SubmissionFailed,
                                       QStringLiteral("QVulkanDeviceFunctions indisponível para VkFence."));
        return false;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    const VkResult createRc = df->vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
    if (createRc != VK_SUCCESS || fence == VK_NULL_HANDLE) {
        if (createRc == VK_ERROR_DEVICE_LOST) {
            m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                           QStringLiteral("VK_ERROR_DEVICE_LOST em vkCreateFence"));
            simulateDeviceLostForTest(QStringLiteral("VK_ERROR_DEVICE_LOST em vkCreateFence"));
        } else {
            m_submissionTracker.markFailed(frameId, GpuSubmissionState::SubmissionFailed,
                                           QStringLiteral("Falha em vkCreateFence"));
        }
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    const VkResult submitRc = df->vkQueueSubmit(m_graphicsQueue, 0, nullptr, fence);
    if (submitRc != VK_SUCCESS) {
        df->vkDestroyFence(m_device, fence, nullptr);
        if (submitRc == VK_ERROR_DEVICE_LOST) {
            m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                           QStringLiteral("VK_ERROR_DEVICE_LOST em vkQueueSubmit(fence)"));
            simulateDeviceLostForTest(QStringLiteral("VK_ERROR_DEVICE_LOST em vkQueueSubmit(fence)"));
        } else {
            m_submissionTracker.markFailed(frameId, GpuSubmissionState::SubmissionFailed,
                                           QStringLiteral("Falha em vkQueueSubmit(fence)"));
        }
        return false;
    }

    const VkResult waitRc = df->vkWaitForFences(m_device, 1, &fence, VK_TRUE, timeoutNs);
    df->vkDestroyFence(m_device, fence, nullptr);

    if (waitRc == VK_SUCCESS) {
        const qint64 gpuLatencyUs = timer.nsecsElapsed() / 1000;
        Q_UNUSED(m_submissionTracker.markCompleted(frameId));
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] GPU_EXECUTION_COMPLETED frameId=" << frameId
                    << "slotId=0 method=VkFence gpuLatencyUs=" << gpuLatencyUs;
        }
        return true;
    }
    if (waitRc == VK_ERROR_DEVICE_LOST) {
        m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                       QStringLiteral("VK_ERROR_DEVICE_LOST em vkWaitForFences"));
        simulateDeviceLostForTest(QStringLiteral("VK_ERROR_DEVICE_LOST em vkWaitForFences"));
        return false;
    }

    Q_UNUSED(m_submissionTracker.markTimedOut(frameId));
    m_lastError = QStringLiteral("Timeout/falha aguardando VkFence no caminho Vulkan interno.");
    if (graphicsDiagnosticsEnabled()) {
        qWarning() << "[SGE-DIAG] GPU_COMPLETION_TIMED_OUT frameId=" << frameId
                   << "method=VkFence vkResult=" << static_cast<int>(waitRc);
    }
    return false;
}

bool VulkanQuickRenderer::renderIntoBuffer(GbmExportedBuffer &slot) {
    if (!m_initialized || m_suspended || m_deviceLost || !m_window || !m_renderControl) {
        return false;
    }
    if (slot.vkImageHandle == 0) {
        // Fallback para buffers simulados sem VkImage dedicada
        return renderFrame();
    }

    auto *vkImg = reinterpret_cast<VkImage>(slot.vkImageHandle);
    const uint64_t frameId = ++m_frameCounter;
    if (graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] FRAME_START frameId=" << frameId
                << "slotId=" << slot.id
                << "mode=dmabufPool size=" << m_pixelSize
                << "vkFormat=" << static_cast<int>(m_vkFormat)
                << "modifier=0x" << QString::number(slot.modifier, 16);
        qInfo() << "[SGE-DIAG] frameId=" << frameId
                << "slotId=" << slot.id
                << "vkImage=0x" << Qt::hex << reinterpret_cast<quintptr>(vkImg)
                << "waitSem=" << slot.vkWaitSemaphoreHandle
                << "signalSem=" << slot.vkSignalSemaphoreHandle << Qt::dec
                << "size=" << m_pixelSize
                << "vkFormat=" << static_cast<int>(m_vkFormat)
                << "modifier=0x" << QString::number(slot.modifier, 16)
                << "prevLayout=UNDEFINED expectedLayout=UNDEFINED->COLOR_ATTACHMENT_OPTIMAL"
                << "releaseFenceFd=" << slot.releaseFenceFd;
    }

    QVulkanFunctions *f = m_vulkanInstance.functions();
    bool hasImportedWaitSemaphore = false;
    VkSemaphore waitSem = reinterpret_cast<VkSemaphore>(slot.vkWaitSemaphoreHandle);
    VkSemaphore signalSem = reinterpret_cast<VkSemaphore>(slot.vkSignalSemaphoreHandle);

    if (slot.releaseFenceFd >= 0) {
        const int observedFd = slot.releaseFenceFd;
        // Verifica primeiro via poll(sync_file) para evitar importar OUT_FENCE_PTR de KMS
        // (especialmente fences já sinalizadas após KMS_PAGE_FLIP_COMPLETED) em vkQueueSubmit/RADV.
        const bool alreadySignaled = isSyncFdSignaled(observedFd, 0);
        const bool signaled = alreadySignaled || isSyncFdSignaled(observedFd, 2000);
        if (signaled && graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] RELEASE_FENCE_SIGNALED frameId=" << frameId
                    << "slotId=" << slot.id
                    << "releaseFenceFd=" << observedFd
                    << "nonBlocking=" << alreadySignaled
                    << "importResult=skipped_sync_file_poll";
        }
        ::close(slot.releaseFenceFd);
        slot.releaseFenceFd = -1;
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] RELEASE_FENCE_CLOSED slotId=" << slot.id
                    << "releaseFenceFd=" << observedFd;
        }
        if (!signaled) {
            m_lastError = QStringLiteral("Timeout aguardando releaseFenceFd antes de reutilizar slot.");
            return false;
        }
    }

    m_activeRenderImage = vkImg;
    m_window->setRenderTarget(QQuickRenderTarget::fromVulkanImage(
        vkImg, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1));

    m_renderControl->polishItems();
    m_renderControl->beginFrame();
    const bool synced = m_renderControl->sync();
    if (synced) {
        m_renderControl->render();
        m_submissionTracker.markRecorded(frameId, slot.id, slot.vkSignalSemaphoreHandle);
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] CPU_COMMAND_RECORDED frameId=" << frameId << "slotId=" << slot.id;
        }
    }

    QRhi *rhi = m_window->rhi();
    QRhiVulkanQueueSubmitParams submitParams{};
    if (synced && rhi && signalSem != VK_NULL_HANDLE) {
        submitParams.waitSemaphoreCount = hasImportedWaitSemaphore ? 1U : 0U;
        submitParams.waitSemaphores = hasImportedWaitSemaphore ? &waitSem : nullptr;
        submitParams.signalSemaphoreCount = 1U;
        submitParams.signalSemaphores = &signalSem;
        rhi->setQueueSubmitParams(&submitParams);
    }

    m_renderControl->endFrame();
    if (rhi) {
        rhi->setQueueSubmitParams(nullptr);
    }

    if (rhi && rhi->isDeviceLost()) {
        m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                       QStringLiteral("QRhi::isDeviceLost() após endFrame()"));
        simulateDeviceLostForTest(QStringLiteral("QRhi::isDeviceLost() após renderIntoBuffer::endFrame()"));
        return false;
    }

    if (!synced) {
        // Se importamos um SYNC_FD temporário mas não submetemos, coloca waitSem em quarentena e cria novo.
        if (hasImportedWaitSemaphore && waitSem != VK_NULL_HANDLE) {
            quarantineSemaphore(waitSem);
            if (QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device)) {
                VkSemaphoreCreateInfo waitSemInfo{};
                waitSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                VkSemaphore freshWait = VK_NULL_HANDLE;
                if (df->vkCreateSemaphore(m_device, &waitSemInfo, nullptr, &freshWait) == VK_SUCCESS) {
                    slot.vkWaitSemaphoreHandle = reinterpret_cast<uint64_t>(freshWait);
                } else {
                    slot.vkWaitSemaphoreHandle = 0;
                }
            }
        }
        return false;
    }

    if (graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] GPU_SUBMITTED frameId=" << frameId << "slotId=" << slot.id;
    }

    if (m_capabilities.supportsSyncFdExport && signalSem != VK_NULL_HANDLE && f) {
        auto fnGetSemFd = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
            f->vkGetDeviceProcAddr(m_device, "vkGetSemaphoreFdKHR"));
        if (!fnGetSemFd) {
            m_lastError = QStringLiteral("vkGetSemaphoreFdKHR indisponível no dispositivo Vulkan.");
            m_submissionTracker.markFailed(frameId, GpuSubmissionState::SubmissionFailed, m_lastError);
            return false;
        }
        VkSemaphoreGetFdInfoKHR getFdInfo{};
        getFdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        getFdInfo.semaphore = signalSem;
        getFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
        int syncFd = -1;
        const VkResult fdRc = fnGetSemFd(m_device, &getFdInfo, &syncFd);
        if (fdRc == VK_ERROR_DEVICE_LOST) {
            m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                           QStringLiteral("VK_ERROR_DEVICE_LOST em vkGetSemaphoreFdKHR"));
            simulateDeviceLostForTest(QStringLiteral("VK_ERROR_DEVICE_LOST em vkGetSemaphoreFdKHR"));
            return false;
        }
        if (fdRc != VK_SUCCESS || syncFd < 0) {
            // signalSem pode estar referenciado pela submissão pendente na GPU: nunca destruir imediatamente!
            quarantineSemaphore(signalSem);
            if (QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device)) {
                VkExportSemaphoreCreateInfo exportSemInfo{};
                exportSemInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
                exportSemInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
                VkSemaphoreCreateInfo semInfo{};
                semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                semInfo.pNext = &exportSemInfo;
                VkSemaphore freshSig = VK_NULL_HANDLE;
                if (df->vkCreateSemaphore(m_device, &semInfo, nullptr, &freshSig) == VK_SUCCESS) {
                    slot.vkSignalSemaphoreHandle = reinterpret_cast<uint64_t>(freshSig);
                } else {
                    slot.vkSignalSemaphoreHandle = 0;
                }
            }
            m_lastError = QStringLiteral("Falha ao exportar SYNC_FD após endFrame() do Qt Quick.");
            m_submissionTracker.markFailed(frameId, GpuSubmissionState::SubmissionFailed, m_lastError);
            return false;
        }
        if (slot.acquireFenceFd >= 0) {
            ::close(slot.acquireFenceFd);
        }
        slot.acquireFenceFd = syncFd;
        Q_UNUSED(m_submissionTracker.markSubmitted(frameId, syncFd));
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] SYNC_FD exported frameId=" << frameId
                    << "slotId=" << slot.id
                    << "acquireFenceFd=" << syncFd;
        }
    } else {
        Q_UNUSED(m_submissionTracker.markSubmitted(frameId, -1));
    }

    return true;
}

QList<DrmModifierCapability> VulkanQuickRenderer::queryDrmModifierCapabilities() const {
    QList<DrmModifierCapability> result;
    if (!m_initialized || !m_capabilities.supportsDrmFormatModifier) {
        return result;
    }
    QVulkanFunctions *f = m_vulkanInstance.functions();
    if (!f) {
        return result;
    }

    auto fnGetProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(
        vkGetInstanceProcAddr(m_vulkanInstance.vkInstance(), "vkGetPhysicalDeviceFormatProperties2"));
    auto fnGetImgProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
        vkGetInstanceProcAddr(m_vulkanInstance.vkInstance(), "vkGetPhysicalDeviceImageFormatProperties2"));
    if (!fnGetProps2) {
        return result;
    }

    VkDrmFormatModifierPropertiesListEXT modList{};
    modList.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
    VkFormatProperties2 fmtProps{};
    fmtProps.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    fmtProps.pNext = &modList;

    fnGetProps2(m_physicalDevice, m_vkFormat, &fmtProps);
    if (modList.drmFormatModifierCount == 0) {
        return result;
    }

    std::vector<VkDrmFormatModifierPropertiesEXT> props(modList.drmFormatModifierCount);
    modList.pDrmFormatModifierProperties = props.data();
    fnGetProps2(m_physicalDevice, m_vkFormat, &fmtProps);

    for (const auto &entry : props) {
        DrmModifierCapability cap;
        cap.modifier = entry.drmFormatModifier;
        cap.planeCount = entry.drmFormatModifierPlaneCount;
        cap.tilingFeatures = entry.drmFormatModifierTilingFeatures;

        if (fnGetImgProps2) {
            VkPhysicalDeviceImageDrmFormatModifierInfoEXT modInfo{};
            modInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
            modInfo.drmFormatModifier = entry.drmFormatModifier;
            modInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VkPhysicalDeviceExternalImageFormatInfo extImgFmtInfo{};
            extImgFmtInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
            extImgFmtInfo.pNext = &modInfo;
            extImgFmtInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

            VkPhysicalDeviceImageFormatInfo2 imgFmtInfo{};
            imgFmtInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
            imgFmtInfo.pNext = &extImgFmtInfo;
            imgFmtInfo.format = m_vkFormat;
            imgFmtInfo.type = VK_IMAGE_TYPE_2D;
            imgFmtInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
            imgFmtInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT;

            VkExternalImageFormatProperties extImgProps{};
            extImgProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
            VkImageFormatProperties2 outProps{};
            outProps.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
            outProps.pNext = &extImgProps;

            if (fnGetImgProps2(m_physicalDevice, &imgFmtInfo, &outProps) == VK_SUCCESS) {
                const auto feats = extImgProps.externalMemoryProperties.externalMemoryFeatures;
                cap.dmabufExportable = (feats & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) != 0;
                cap.dmabufImportable = (feats & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0;
            }
        }
        result.append(cap);
    }
    return result;
}

QList<uint64_t> VulkanQuickRenderer::querySupportedDrmModifiers() const {
    QList<uint64_t> result;
    for (const auto &cap : queryDrmModifierCapabilities()) {
        if ((cap.tilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0
            && cap.planeCount == 1
            && cap.dmabufExportable) {
            result.append(cap.modifier);
        }
    }
    return result;
}

bool VulkanQuickRenderer::createSharedPoolSlots(GbmBufferManager &pool,
                                                const QSize &pixelSize,
                                                int slotCount,
                                                const QList<uint64_t> &kmsModifiers) {
    if (!m_initialized || slotCount <= 0 || !pixelSize.isValid()
        || !m_capabilities.supportsExternalMemoryFd || !m_capabilities.supportsExternalMemoryDmabuf) {
        return false;
    }
    QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device);
    QVulkanFunctions *f = m_vulkanInstance.functions();
    if (!df || !f) {
        return false;
    }

    auto fnGetMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        f->vkGetDeviceProcAddr(m_device, "vkGetMemoryFdKHR"));
    if (!fnGetMemoryFd) {
        return false;
    }

    const QList<uint64_t> vkMods = querySupportedDrmModifiers();
    std::vector<uint64_t> candidateMods;
    if (!kmsModifiers.isEmpty()) {
        const auto neg = GbmBufferManager::negotiateModifiers(DRM_FORMAT_XRGB8888, kmsModifiers, vkMods);
        if (!neg.compatible) {
            m_lastError = neg.failureReason;
            return false;
        }
        for (uint64_t mod : neg.negotiatedModifiers) {
            candidateMods.push_back(mod);
        }
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] KMS_MODIFIER_NEGOTIATED count=" << candidateMods.size()
                    << "preferred=0x" << QString::number(candidateMods.front(), 16);
        }
    } else {
        for (uint64_t mod : vkMods) {
            candidateMods.push_back(mod);
        }
    }

    auto fnGetImgMod = reinterpret_cast<PFN_vkGetImageDrmFormatModifierPropertiesEXT>(
        f->vkGetDeviceProcAddr(m_device, "vkGetImageDrmFormatModifierPropertiesEXT"));

    for (int i = 0; i < slotCount; ++i) {
        VkExternalMemoryImageCreateInfo extImageInfo{};
        extImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        extImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        VkImageDrmFormatModifierListCreateInfoEXT modListInfo{};
        modListInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
        modListInfo.drmFormatModifierCount = static_cast<uint32_t>(candidateMods.size());
        modListInfo.pDrmFormatModifiers = candidateMods.empty() ? nullptr : candidateMods.data();

        const bool useModifierTiling = m_capabilities.supportsDrmFormatModifier && !candidateMods.empty();
        if (useModifierTiling) {
            extImageInfo.pNext = &modListInfo;
        }

        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.pNext = &extImageInfo;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = m_vkFormat;
        imgInfo.extent = {static_cast<uint32_t>(pixelSize.width()),
                          static_cast<uint32_t>(pixelSize.height()), 1};
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = useModifierTiling ? VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT : VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage slotImage = VK_NULL_HANDLE;
        if (df->vkCreateImage(m_device, &imgInfo, nullptr, &slotImage) != VK_SUCCESS) {
            destroySharedPoolSlots(pool);
            return false;
        }

        VkMemoryDedicatedRequirements dedicatedReqs{};
        dedicatedReqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
        VkMemoryRequirements2 memReq2{};
        memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        memReq2.pNext = &dedicatedReqs;

        auto fnGetImgMemReqs2 = reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
            f->vkGetDeviceProcAddr(m_device, "vkGetImageMemoryRequirements2"));
        if (fnGetImgMemReqs2) {
            VkImageMemoryRequirementsInfo2 reqInfo{};
            reqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
            reqInfo.image = slotImage;
            fnGetImgMemReqs2(m_device, &reqInfo, &memReq2);
        } else {
            df->vkGetImageMemoryRequirements(m_device, slotImage, &memReq2.memoryRequirements);
        }

        const VkMemoryRequirements &memReq = memReq2.memoryRequirements;
        const uint32_t memType = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == UINT32_MAX) {
            df->vkDestroyImage(m_device, slotImage, nullptr);
            destroySharedPoolSlots(pool);
            return false;
        }

        VkMemoryDedicatedAllocateInfo dedicatedAlloc{};
        dedicatedAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedAlloc.image = slotImage;

        VkExportMemoryAllocateInfo exportAlloc{};
        exportAlloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        exportAlloc.pNext = &dedicatedAlloc;
        exportAlloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = &exportAlloc;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memType;

        VkDeviceMemory slotMem = VK_NULL_HANDLE;
        if (df->vkAllocateMemory(m_device, &allocInfo, nullptr, &slotMem) != VK_SUCCESS
            || df->vkBindImageMemory(m_device, slotImage, slotMem, 0) != VK_SUCCESS) {
            if (slotMem != VK_NULL_HANDLE) {
                df->vkFreeMemory(m_device, slotMem, nullptr);
            }
            df->vkDestroyImage(m_device, slotImage, nullptr);
            destroySharedPoolSlots(pool);
            return false;
        }

        VkMemoryGetFdInfoKHR fdInfo{};
        fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
        fdInfo.memory = slotMem;
        fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        int dmabufFd = -1;
        if (fnGetMemoryFd(m_device, &fdInfo, &dmabufFd) != VK_SUCCESS || dmabufFd < 0) {
            df->vkFreeMemory(m_device, slotMem, nullptr);
            df->vkDestroyImage(m_device, slotImage, nullptr);
            destroySharedPoolSlots(pool);
            return false;
        }

        uint64_t chosenModifier = DRM_FORMAT_MOD_INVALID;
        if (useModifierTiling && fnGetImgMod) {
            VkImageDrmFormatModifierPropertiesEXT modProps{};
            modProps.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;
            if (fnGetImgMod(m_device, slotImage, &modProps) == VK_SUCCESS) {
                chosenModifier = modProps.drmFormatModifier;
            }
        }

        VkImageSubresource subres{};
        subres.aspectMask = useModifierTiling ? VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT : VK_IMAGE_ASPECT_COLOR_BIT;
        VkSubresourceLayout layout{};
        df->vkGetImageSubresourceLayout(m_device, slotImage, &subres, &layout);

        GbmExportedBuffer slot;
        slot.width = static_cast<uint32_t>(pixelSize.width());
        slot.height = static_cast<uint32_t>(pixelSize.height());
        slot.format = DRM_FORMAT_XRGB8888;
        slot.stride = layout.rowPitch > 0 ? static_cast<uint32_t>(layout.rowPitch)
                                          : static_cast<uint32_t>(pixelSize.width() * 4);
        slot.offset = static_cast<uint32_t>(layout.offset);
        slot.planeCount = 1;
        slot.modifier = chosenModifier;
        slot.dmabufFd = dmabufFd;
        slot.vulkanExternalMemoryVerified = true;
        slot.vkImageHandle = reinterpret_cast<uint64_t>(slotImage);
        slot.vkMemoryHandle = reinterpret_cast<uint64_t>(slotMem);

        if (m_capabilities.supportsSyncFdExport) {
            VkExportSemaphoreCreateInfo exportSemInfo{};
            exportSemInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
            exportSemInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

            VkSemaphoreCreateInfo semInfo{};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semInfo.pNext = &exportSemInfo;

            VkSemaphore sigSem = VK_NULL_HANDLE;
            if (df->vkCreateSemaphore(m_device, &semInfo, nullptr, &sigSem) == VK_SUCCESS) {
                slot.vkSignalSemaphoreHandle = reinterpret_cast<uint64_t>(sigSem);
            }
        }
        if (m_capabilities.supportsSyncFdImport) {
            VkSemaphoreCreateInfo waitSemInfo{};
            waitSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkSemaphore waitSem = VK_NULL_HANDLE;
            if (df->vkCreateSemaphore(m_device, &waitSemInfo, nullptr, &waitSem) == VK_SUCCESS) {
                slot.vkWaitSemaphoreHandle = reinterpret_cast<uint64_t>(waitSem);
            }
        }

        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] slot created index=" << i
                    << "size=" << pixelSize
                    << "modifier=0x" << QString::number(chosenModifier, 16)
                    << "stride=" << slot.stride
                    << "offset=" << slot.offset
                    << "allocBytes=" << memReq.size
                    << "memType=" << memType
                    << "requiresDedicated=" << (dedicatedReqs.requiresDedicatedAllocation == VK_TRUE)
                    << "prefersDedicated=" << (dedicatedReqs.prefersDedicatedAllocation == VK_TRUE)
                    << "dmabufFd=" << dmabufFd;
        }
        Q_UNUSED(pool.registerExternalSlot(std::move(slot)));
    }
    return true;
}

void VulkanQuickRenderer::quarantineSemaphore(VkSemaphore sem) {
    if (sem != VK_NULL_HANDLE && !m_quarantinedSemaphores.contains(sem)) {
        m_quarantinedSemaphores.append(sem);
    }
}

void VulkanQuickRenderer::flushQuarantinedSemaphores() {
    if (m_device == VK_NULL_HANDLE || m_quarantinedSemaphores.isEmpty()) {
        return;
    }
    if (QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device)) {
        for (VkSemaphore sem : m_quarantinedSemaphores) {
            if (sem != VK_NULL_HANDLE) {
                df->vkDestroySemaphore(m_device, sem, nullptr);
            }
        }
    }
    m_quarantinedSemaphores.clear();
}

bool VulkanQuickRenderer::observeSyncFdCompletion(uint64_t frameId, int syncFd, int timeoutMs) {
    if (m_deviceLost) {
        m_submissionTracker.markFailed(frameId, GpuSubmissionState::DeviceLost,
                                       QStringLiteral("Dispositivo perdido antes da confirmação."));
        return false;
    }
    if (syncFd < 0) {
        m_submissionTracker.markFailed(frameId, GpuSubmissionState::Unknown,
                                       QStringLiteral("SYNC_FD inválido para observação."));
        return false;
    }
    const int boundedTimeoutMs = qMax(0, timeoutMs);
    QElapsedTimer timer;
    timer.start();
    int rc = 0;
    pollfd pfd{syncFd, POLLIN, 0};
    while (true) {
        const int elapsed = static_cast<int>(timer.elapsed());
        const int remaining = qMax(0, boundedTimeoutMs - elapsed);
        pfd.revents = 0;
        rc = ::poll(&pfd, 1, remaining);
        if (rc < 0 && errno == EINTR && timer.elapsed() < boundedTimeoutMs) {
            continue;
        }
        break;
    }

    if (rc > 0) {
        if ((pfd.revents & (POLLERR | POLLNVAL | POLLHUP)) != 0) {
            m_submissionTracker.markFailed(
                frameId, GpuSubmissionState::Unknown,
                QStringLiteral("poll() retornou POLLERR/POLLNVAL/POLLHUP em SYNC_FD."));
            return false;
        }
        if ((pfd.revents & POLLIN) != 0) {
            const bool marked = m_submissionTracker.markCompleted(frameId);
            if (marked && graphicsDiagnosticsEnabled()) {
                const auto *rec = m_submissionTracker.find(frameId);
                const qint64 elapsedNs = (rec && rec->completedAtNs >= rec->submittedAtNs)
                                             ? (rec->completedAtNs - rec->submittedAtNs)
                                             : 0;
                qInfo() << "[SGE-DIAG] GPU_EXECUTION_COMPLETED frameId=" << frameId
                        << "slotId=" << (rec ? rec->slotId : 0)
                        << "syncFd=" << syncFd
                        << "gpuLatencyUs=" << (elapsedNs / 1000);
            }
            return marked;
        }
    }
    if (rc == 0) {
        Q_UNUSED(m_submissionTracker.markTimedOut(frameId));
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] GPU_COMPLETION_TIMED_OUT frameId=" << frameId << "syncFd=" << syncFd;
        }
        return false;
    }
    m_submissionTracker.markFailed(frameId, GpuSubmissionState::Unknown,
                                   QStringLiteral("Erro em poll() do SYNC_FD."));
    return false;
}

void VulkanQuickRenderer::destroySharedPoolSlots(GbmBufferManager &pool) {
    QVulkanDeviceFunctions *df = (m_device != VK_NULL_HANDLE)
                                     ? m_vulkanInstance.deviceFunctions(m_device)
                                     : nullptr;
    if (df && !m_deviceLost) {
        if (df->vkDeviceWaitIdle(m_device) == VK_ERROR_DEVICE_LOST) {
            m_deviceLost = true;
        }
    }
    if (m_window) {
        m_window->setRenderTarget(QQuickRenderTarget{});
    }
    flushQuarantinedSemaphores();
    for (auto &slot : pool.buffers()) {
        if (graphicsDiagnosticsEnabled()) {
            qInfo() << "[SGE-DIAG] SLOT_RESOURCES_DESTROYED slotId=" << slot.id
                    << "vkImage=0x" << Qt::hex << slot.vkImageHandle
                    << "vkWaitSem=" << slot.vkWaitSemaphoreHandle
                    << "vkSignalSem=" << slot.vkSignalSemaphoreHandle << Qt::dec;
        }
        if (slot.dmabufFd >= 0) {
            ::close(slot.dmabufFd);
            slot.dmabufFd = -1;
        }
        if (slot.acquireFenceFd >= 0) {
            ::close(slot.acquireFenceFd);
            slot.acquireFenceFd = -1;
        }
        if (slot.releaseFenceFd >= 0) {
            ::close(slot.releaseFenceFd);
            slot.releaseFenceFd = -1;
        }
        if (df && slot.vkSignalSemaphoreHandle != 0) {
            df->vkDestroySemaphore(m_device, reinterpret_cast<VkSemaphore>(slot.vkSignalSemaphoreHandle), nullptr);
            slot.vkSignalSemaphoreHandle = 0;
        }
        if (df && slot.vkWaitSemaphoreHandle != 0) {
            df->vkDestroySemaphore(m_device, reinterpret_cast<VkSemaphore>(slot.vkWaitSemaphoreHandle), nullptr);
            slot.vkWaitSemaphoreHandle = 0;
        }
        if (df && slot.vkImageHandle != 0) {
            if (reinterpret_cast<VkImage>(slot.vkImageHandle) == m_activeRenderImage) {
                m_activeRenderImage = m_colorImage;
            }
            df->vkDestroyImage(m_device, reinterpret_cast<VkImage>(slot.vkImageHandle), nullptr);
            slot.vkImageHandle = 0;
        }
        if (df && slot.vkMemoryHandle != 0) {
            df->vkFreeMemory(m_device, reinterpret_cast<VkDeviceMemory>(slot.vkMemoryHandle), nullptr);
            slot.vkMemoryHandle = 0;
        }
    }
}

QImage VulkanQuickRenderer::readbackImage() {
    if (!m_initialized || m_colorImage == VK_NULL_HANDLE || m_commandPool == VK_NULL_HANDLE) {
        return {};
    }
    QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device);
    if (!df) {
        return {};
    }

    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(m_pixelSize.width())
                                    * static_cast<VkDeviceSize>(m_pixelSize.height()) * 4;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    if (df->vkCreateBuffer(m_device, &bufInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        return {};
    }

    VkMemoryRequirements memReq{};
    df->vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReq);
    const uint32_t memType = findMemoryType(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == UINT32_MAX) {
        df->vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        return {};
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memType;

    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (df->vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        df->vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        return {};
    }
    df->vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = m_commandPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (df->vkAllocateCommandBuffers(m_device, &cmdAlloc, &cmd) != VK_SUCCESS) {
        df->vkFreeMemory(m_device, stagingMemory, nullptr);
        df->vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        return {};
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    const VkImage readbackTarget = (m_activeRenderImage != VK_NULL_HANDLE) ? m_activeRenderImage : m_colorImage;
    toTransfer.image = readbackTarget;
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    df->vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {static_cast<uint32_t>(m_pixelSize.width()),
                          static_cast<uint32_t>(m_pixelSize.height()), 1};
    df->vkCmdCopyImageToBuffer(cmd, readbackTarget, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               stagingBuffer, 1, &region);

    VkImageMemoryBarrier toColor = toTransfer;
    toColor.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    df->vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &toColor);

    df->vkEndCommandBuffer(cmd);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    QImage result;

    if (df->vkCreateFence(m_device, &fenceInfo, nullptr, &fence) == VK_SUCCESS) {
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        if (df->vkQueueSubmit(m_graphicsQueue, 1, &submit, fence) == VK_SUCCESS
            && df->vkWaitForFences(m_device, 1, &fence, VK_TRUE, 2000000000ULL) == VK_SUCCESS) {
            void *mapped = nullptr;
            if (df->vkMapMemory(m_device, stagingMemory, 0, bufferSize, 0, &mapped) == VK_SUCCESS && mapped) {
                QImage img(m_pixelSize, QImage::Format_ARGB32);
                const auto *src = static_cast<const uint8_t *>(mapped);
                const size_t rowBytes = static_cast<size_t>(m_pixelSize.width()) * 4;
                for (int y = 0; y < m_pixelSize.height(); ++y) {
                    std::memcpy(img.scanLine(y), src + static_cast<size_t>(y) * rowBytes, rowBytes);
                }
                df->vkUnmapMemory(m_device, stagingMemory);
                result = img;
            }
        }
        df->vkDestroyFence(m_device, fence, nullptr);
    }

    df->vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
    df->vkFreeMemory(m_device, stagingMemory, nullptr);
    df->vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    return result;
}

int VulkanQuickRenderer::exportDmabufFd() {
    if (!m_initialized || !m_capabilities.dmabufExportImageCreated || m_colorMemory == VK_NULL_HANDLE) {
        return -1;
    }
    QVulkanFunctions *f = m_vulkanInstance.functions();
    if (!f) {
        return -1;
    }

    auto fnGetMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        f->vkGetDeviceProcAddr(m_device, "vkGetMemoryFdKHR"));
    if (!fnGetMemoryFd) {
        return -1;
    }

    VkMemoryGetFdInfoKHR fdInfo{};
    fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fdInfo.memory = m_colorMemory;
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    int fd = -1;
    if (fnGetMemoryFd(m_device, &fdInfo, &fd) != VK_SUCCESS) {
        return -1;
    }
    return fd;
}

void VulkanQuickRenderer::setSuspended(bool suspended) noexcept {
    m_suspended = suspended;
}

bool VulkanQuickRenderer::isSyncFdSignaled(int fenceFd, int timeoutMs) {
    if (fenceFd < 0) {
        return false;
    }
    const int boundedTimeoutMs = qMax(0, timeoutMs);
    QElapsedTimer timer;
    timer.start();
    pollfd pfd{fenceFd, POLLIN, 0};
    int rc = 0;
    while (true) {
        const int elapsed = static_cast<int>(timer.elapsed());
        const int remaining = qMax(0, boundedTimeoutMs - elapsed);
        pfd.revents = 0;
        rc = ::poll(&pfd, 1, remaining);
        if (rc < 0 && errno == EINTR && timer.elapsed() < boundedTimeoutMs) {
            continue;
        }
        break;
    }
    return rc > 0
           && (pfd.revents & (POLLERR | POLLNVAL | POLLHUP)) == 0
           && (pfd.revents & POLLIN) != 0;
}

void VulkanQuickRenderer::shutdown() {
    if (m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;
    m_initialized = false;
    m_suspended = false;

    // Destruir a árvore QML (incluindo QWaylandQuickOutput e SystaliaCompositor)
    // enquanto m_window ainda é válida, sem reatribuir hostWindow/window=null
    // após a inicialização do QWaylandOutput.
    if (m_rootItem) {
        m_rootItem->setParentItem(nullptr);
        delete m_rootItem.data();
        m_rootItem.clear();
    }
    m_qmlComponent.reset();
    m_ownedQmlEngine.reset();
    m_activeEngine.clear();

    if (m_window) {
        m_window->setRenderTarget(QQuickRenderTarget{});
    }
    if (m_renderControl) {
        m_renderControl->invalidate();
    }
    m_window.reset();
    m_renderControl.reset();

    if (m_device != VK_NULL_HANDLE && !m_deviceLost) {
        if (QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device)) {
            if (df->vkDeviceWaitIdle(m_device) == VK_ERROR_DEVICE_LOST) {
                m_deviceLost = true;
            }
        }
    }
    flushQuarantinedSemaphores();
    destroyOffscreenImage();
    if (m_commandPool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        if (QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device)) {
            df->vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        }
        m_commandPool = VK_NULL_HANDLE;
    }
    destroyVulkanDevice();
    m_vulkanInstance.destroy();
    if (m_ownedVkInstance != VK_NULL_HANDLE) {
        if (auto fnDestroy = reinterpret_cast<PFN_vkDestroyInstance>(
                vkGetInstanceProcAddr(m_ownedVkInstance, "vkDestroyInstance"))) {
            fnDestroy(m_ownedVkInstance, nullptr);
        }
        m_ownedVkInstance = VK_NULL_HANDLE;
    }
    if (graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] RENDERER_SHUTDOWN_COMPLETE deviceLost=" << m_deviceLost;
    }
    m_shuttingDown = false;
}

bool VulkanQuickRenderer::createVulkanDevice() {
    const QByteArrayList instExts{
        QByteArrayLiteral("VK_KHR_get_physical_device_properties2"),
        QByteArrayLiteral("VK_KHR_external_memory_capabilities"),
        QByteArrayLiteral("VK_KHR_external_semaphore_capabilities")};
    QByteArrayList enabledInstExts;
    const auto supportedInst = m_vulkanInstance.supportedExtensions();
    for (const QByteArray &ext : instExts) {
        if (supportedInst.contains(ext)) {
            enabledInstExts.append(ext);
        }
    }
    m_vulkanInstance.setExtensions(enabledInstExts);
    if (!m_vulkanInstance.create()) {
        // Fallback para QPA neutro (ex.: offscreen/minimal) que não implementa createPlatformVulkanInstance():
        // cria VkInstance diretamente via libvulkan e envolve com setVkInstance().
        auto fnCreate = reinterpret_cast<PFN_vkCreateInstance>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
        if (fnCreate) {
            std::vector<const char *> rawExts;
            rawExts.reserve(static_cast<size_t>(enabledInstExts.size()));
            for (const QByteArray &ext : enabledInstExts) {
                rawExts.push_back(ext.constData());
            }
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "SystaliaVulkanQuick";
            appInfo.apiVersion = VK_API_VERSION_1_2;

            VkInstanceCreateInfo instInfo{};
            instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instInfo.pApplicationInfo = &appInfo;
            instInfo.enabledExtensionCount = static_cast<uint32_t>(rawExts.size());
            instInfo.ppEnabledExtensionNames = rawExts.empty() ? nullptr : rawExts.data();

            if (fnCreate(&instInfo, nullptr, &m_ownedVkInstance) == VK_SUCCESS) {
                m_vulkanInstance.setVkInstance(m_ownedVkInstance);
                if (!m_vulkanInstance.create()) {
                    m_lastError = QStringLiteral("Falha ao envolver VkInstance com QVulkanInstance::setVkInstance().");
                    return false;
                }
            }
        }
    }
    if (!m_vulkanInstance.isValid()) {
        m_lastError = QStringLiteral("Falha ao criar QVulkanInstance (errorCode=%1).")
                          .arg(m_vulkanInstance.errorCode());
        return false;
    }

    QVulkanFunctions *f = m_vulkanInstance.functions();
    if (!f) {
        m_lastError = QStringLiteral("QVulkanFunctions indisponível.");
        return false;
    }

    uint32_t gpuCount = 0;
    f->vkEnumeratePhysicalDevices(m_vulkanInstance.vkInstance(), &gpuCount, nullptr);
    if (gpuCount == 0) {
        m_lastError = QStringLiteral("Nenhum VkPhysicalDevice Vulkan encontrado.");
        return false;
    }

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    f->vkEnumeratePhysicalDevices(m_vulkanInstance.vkInstance(), &gpuCount, gpus.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    uint32_t chosenQueueFamily = UINT32_MAX;
    VkPhysicalDeviceProperties chosenProps{};

    for (VkPhysicalDevice gpu : gpus) {
        VkPhysicalDeviceProperties props{};
        f->vkGetPhysicalDeviceProperties(gpu, &props);

        uint32_t qCount = 0;
        f->vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        f->vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, qProps.data());

        uint32_t gfxIdx = UINT32_MAX;
        for (uint32_t i = 0; i < qCount; ++i) {
            if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                gfxIdx = i;
                break;
            }
        }
        if (gfxIdx == UINT32_MAX) {
            continue;
        }

        if (chosen == VK_NULL_HANDLE || props.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) {
            chosen = gpu;
            chosenQueueFamily = gfxIdx;
            chosenProps = props;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
                || props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                break;
            }
        }
    }

    if (chosen == VK_NULL_HANDLE) {
        m_lastError = QStringLiteral("Nenhuma fila gráfica Vulkan disponível.");
        return false;
    }

    m_physicalDevice = chosen;
    m_graphicsQueueFamily = chosenQueueFamily;
    m_capabilities.deviceName = QString::fromUtf8(chosenProps.deviceName);
    m_capabilities.vendorId = chosenProps.vendorID;
    m_capabilities.deviceId = chosenProps.deviceID;
    m_capabilities.isSoftware = (chosenProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);

    uint32_t devExtCount = 0;
    f->vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &devExtCount, nullptr);
    std::vector<VkExtensionProperties> devExts(devExtCount);
    f->vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &devExtCount, devExts.data());

    m_capabilities.supportsExternalMemoryFd = hasExtension(devExts, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    m_capabilities.supportsExternalMemoryDmabuf = hasExtension(devExts, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
    m_capabilities.supportsDrmFormatModifier = hasExtension(devExts, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    m_capabilities.supportsExternalSemaphoreFd = hasExtension(devExts, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);

    if (hasExtension(devExts, VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME)) {
        auto fnGetProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            vkGetInstanceProcAddr(m_vulkanInstance.vkInstance(), "vkGetPhysicalDeviceProperties2"));
        if (fnGetProps2) {
            VkPhysicalDeviceDrmPropertiesEXT drmProps{};
            drmProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
            VkPhysicalDeviceProperties2 props2{};
            props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            props2.pNext = &drmProps;
            fnGetProps2(m_physicalDevice, &props2);
            m_capabilities.drmDeviceIds.hasPrimary = (drmProps.hasPrimary == VK_TRUE);
            m_capabilities.drmDeviceIds.primaryMajor = static_cast<uint32_t>(drmProps.primaryMajor);
            m_capabilities.drmDeviceIds.primaryMinor = static_cast<uint32_t>(drmProps.primaryMinor);
            m_capabilities.drmDeviceIds.hasRender = (drmProps.hasRender == VK_TRUE);
            m_capabilities.drmDeviceIds.renderMajor = static_cast<uint32_t>(drmProps.renderMajor);
            m_capabilities.drmDeviceIds.renderMinor = static_cast<uint32_t>(drmProps.renderMinor);
        }
    }

    if (m_capabilities.supportsExternalSemaphoreFd) {
        auto fnGetExtSemProps = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>(
            vkGetInstanceProcAddr(m_vulkanInstance.vkInstance(), "vkGetPhysicalDeviceExternalSemaphoreProperties"));
        if (fnGetExtSemProps) {
            VkPhysicalDeviceExternalSemaphoreInfo semQuery{};
            semQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
            semQuery.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
            VkExternalSemaphoreProperties semProps{};
            semProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
            fnGetExtSemProps(m_physicalDevice, &semQuery, &semProps);
            m_capabilities.supportsSyncFdExport =
                (semProps.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) != 0;
            m_capabilities.supportsSyncFdImport =
                (semProps.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) != 0;
        }
    }

    std::vector<const char *> enabledDevExts;
    if (hasExtension(devExts, VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME)) {
        enabledDevExts.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    }
    if (m_capabilities.supportsExternalMemoryFd) {
        enabledDevExts.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    }
    if (m_capabilities.supportsExternalMemoryDmabuf) {
        enabledDevExts.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
    }
    if (hasExtension(devExts, VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME)) {
        enabledDevExts.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
    }
    if (m_capabilities.supportsExternalSemaphoreFd) {
        enabledDevExts.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    }
    if (m_capabilities.supportsDrmFormatModifier) {
        if (hasExtension(devExts, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME)) {
            enabledDevExts.push_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
        }
        enabledDevExts.push_back(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo devInfo{};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &queueInfo;
    devInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDevExts.size());
    devInfo.ppEnabledExtensionNames = enabledDevExts.empty() ? nullptr : enabledDevExts.data();

    if (f->vkCreateDevice(m_physicalDevice, &devInfo, nullptr, &m_device) != VK_SUCCESS) {
        m_lastError = QStringLiteral("Falha em vkCreateDevice.");
        return false;
    }

    QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device);
    if (!df) {
        m_lastError = QStringLiteral("QVulkanDeviceFunctions indisponível.");
        return false;
    }
    df->vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    if (m_graphicsQueue != VK_NULL_HANDLE && graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] VULKAN_DEVICE_SELECTED deviceName=" << m_capabilities.deviceName
                << "vendorId=0x" << QString::number(m_capabilities.vendorId, 16)
                << "deviceId=0x" << QString::number(m_capabilities.deviceId, 16)
                << "queueFamily=" << m_graphicsQueueFamily
                << "driverVersion=" << chosenProps.driverVersion;
    }
    return m_graphicsQueue != VK_NULL_HANDLE;
}

bool VulkanQuickRenderer::createCommandPool() {
    QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device);
    if (!df) {
        return false;
    }
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return df->vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) == VK_SUCCESS;
}

bool VulkanQuickRenderer::createOffscreenImage(const QSize &pixelSize) {
    QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device);
    if (!df) {
        return false;
    }

    const bool tryDmabufExport = m_allowDmabufExportOnInternalImage
                                 && m_capabilities.supportsExternalMemoryFd
                                 && m_capabilities.supportsExternalMemoryDmabuf;
    m_capabilities.dmabufExportImageCreated = false;

    VkExternalMemoryImageCreateInfo extImageInfo{};
    extImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.pNext = tryDmabufExport ? &extImageInfo : nullptr;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = m_vkFormat;
    imgInfo.extent = {static_cast<uint32_t>(pixelSize.width()),
                      static_cast<uint32_t>(pixelSize.height()), 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                    | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (df->vkCreateImage(m_device, &imgInfo, nullptr, &m_colorImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReq{};
    df->vkGetImageMemoryRequirements(m_device, m_colorImage, &memReq);
    const uint32_t memType = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == UINT32_MAX) {
        return false;
    }

    VkExportMemoryAllocateInfo exportAlloc{};
    exportAlloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportAlloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = tryDmabufExport ? &exportAlloc : nullptr;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memType;

    if (df->vkAllocateMemory(m_device, &allocInfo, nullptr, &m_colorMemory) != VK_SUCCESS) {
        if (!tryDmabufExport) {
            return false;
        }
        allocInfo.pNext = nullptr;
        if (df->vkAllocateMemory(m_device, &allocInfo, nullptr, &m_colorMemory) != VK_SUCCESS) {
            return false;
        }
        m_capabilities.dmabufExportImageCreated = false;
    } else {
        m_capabilities.dmabufExportImageCreated = tryDmabufExport;
    }

    m_imageAllocationSize = memReq.size;
    if (df->vkBindImageMemory(m_device, m_colorImage, m_colorMemory, 0) != VK_SUCCESS) {
        return false;
    }
    m_activeRenderImage = m_colorImage;
    return true;
}

void VulkanQuickRenderer::destroyOffscreenImage() {
    m_activeRenderImage = VK_NULL_HANDLE;
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device);
    if (!df) {
        return;
    }
    if (m_colorImage != VK_NULL_HANDLE) {
        df->vkDestroyImage(m_device, m_colorImage, nullptr);
        m_colorImage = VK_NULL_HANDLE;
    }
    if (m_colorMemory != VK_NULL_HANDLE) {
        df->vkFreeMemory(m_device, m_colorMemory, nullptr);
        m_colorMemory = VK_NULL_HANDLE;
    }
    m_imageAllocationSize = 0;
}

void VulkanQuickRenderer::destroyVulkanDevice() {
    if (m_device != VK_NULL_HANDLE) {
        if (QVulkanDeviceFunctions *df = m_vulkanInstance.deviceFunctions(m_device)) {
            df->vkDestroyDevice(m_device, nullptr);
        }
        m_device = VK_NULL_HANDLE;
    }
    m_graphicsQueue = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
}

uint32_t VulkanQuickRenderer::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    QVulkanFunctions *f = m_vulkanInstance.functions();
    if (!f || m_physicalDevice == VK_NULL_HANDLE) {
        return UINT32_MAX;
    }
    VkPhysicalDeviceMemoryProperties memProps{};
    f->vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) != 0
            && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

} // namespace Systalia::Platform::Drm
