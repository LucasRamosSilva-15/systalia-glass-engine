#pragma once

#include "DrmVulkanMatcher.hpp"
#include "IndependentFrameLoop.hpp"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QVulkanInstance>
#include <vulkan/vulkan.h>

#include <memory>

class QQmlComponent;
class QQmlEngine;
class QQuickItem;
class QQuickRenderControl;
class QQuickWindow;

namespace Systalia::Platform::Drm {

class GbmBufferManager;

enum class GpuSubmissionState {
    Recorded,
    Submitted,
    Completed,
    SubmissionFailed,
    CompletionTimedOut,
    DeviceLost,
    Unknown
};

struct GpuSubmissionRecord {
    uint64_t frameId = 0;
    uint64_t slotId = 0;
    uint64_t generation = 1;
    uint64_t semaphoreHandle = 0;
    int syncFd = -1;
    qint64 submittedAtNs = 0;
    qint64 completedAtNs = 0;
    GpuSubmissionState state = GpuSubmissionState::Unknown;
    QString failureReason;
};

class GpuSubmissionTracker final {
public:
    void reset(uint64_t newGeneration = 1) noexcept;
    void markRecorded(uint64_t frameId, uint64_t slotId, uint64_t semaphoreHandle);
    [[nodiscard]] bool markSubmitted(uint64_t frameId, int syncFd);
    [[nodiscard]] bool markCompleted(uint64_t frameId);
    [[nodiscard]] bool markTimedOut(uint64_t frameId);
    void markFailed(uint64_t frameId, GpuSubmissionState failureState, const QString &reason);

    [[nodiscard]] const GpuSubmissionRecord *find(uint64_t frameId) const noexcept;
    [[nodiscard]] uint64_t generation() const noexcept { return m_generation; }
    [[nodiscard]] size_t recordCount() const noexcept { return m_records.size(); }

private:
    GpuSubmissionRecord *findMutable(uint64_t frameId) noexcept;

    uint64_t m_generation = 1;
    QList<GpuSubmissionRecord> m_records;
};

struct DrmModifierCapability {
    uint64_t modifier = 0;
    uint32_t planeCount = 0;
    uint32_t tilingFeatures = 0;
    bool dmabufExportable = false;
    bool dmabufImportable = false;
};

struct VulkanDeviceCapabilities {
    QString deviceName;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    bool isSoftware = false;
    bool supportsExternalMemoryFd = false;
    bool supportsExternalMemoryDmabuf = false;
    bool supportsDrmFormatModifier = false;
    bool supportsExternalSemaphoreFd = false;
    bool supportsSyncFdExport = false;
    bool supportsSyncFdImport = false;
    bool dmabufExportImageCreated = false;
    DrmVulkanDeviceIds drmDeviceIds;
};

/**
 * @brief Ponte isolada entre Vulkan (QVulkanInstance/VkDevice) e Qt Quick (QQuickRenderControl).
 *
 * Renderiza cenas QML (incluindo ShellRoot.qml) em uma VkImage offscreen sem abrir
 * /dev/dri/card* e sem usar eglfs_kms.
 */
class VulkanQuickRenderer final : public QObject, public IControlledRenderSource {
    Q_OBJECT

public:
    explicit VulkanQuickRenderer(QObject *parent = nullptr);
    ~VulkanQuickRenderer() override;

    VulkanQuickRenderer(const VulkanQuickRenderer &) = delete;
    VulkanQuickRenderer &operator=(const VulkanQuickRenderer &) = delete;

    [[nodiscard]] bool initialize(const QSize &pixelSize, bool allowDmabufExport = true);
    [[nodiscard]] bool loadQmlSource(const QByteArray &qmlSource);
    [[nodiscard]] bool loadQmlUrl(const QUrl &qmlUrl, QQmlEngine *externalEngine = nullptr);
    [[nodiscard]] bool resize(const QSize &pixelSize);
    [[nodiscard]] bool renderFrame();
    [[nodiscard]] bool renderControlledFrame() override { return renderFrame(); }
    [[nodiscard]] bool renderIntoBuffer(GbmExportedBuffer &slot) override;
    [[nodiscard]] QImage readbackImage();
    [[nodiscard]] int exportDmabufFd();

    [[nodiscard]] QList<uint64_t> querySupportedDrmModifiers() const;
    [[nodiscard]] QList<DrmModifierCapability> queryDrmModifierCapabilities() const;
    [[nodiscard]] bool createSharedPoolSlots(GbmBufferManager &pool,
                                             const QSize &pixelSize,
                                             int slotCount = 2,
                                             const QList<uint64_t> &kmsModifiers = {});
    void destroySharedPoolSlots(GbmBufferManager &pool);

    void setSuspended(bool suspended) noexcept;
    void shutdown();

    [[nodiscard]] static bool isSyncFdSignaled(int fenceFd, int timeoutMs = 0);
    [[nodiscard]] static bool graphicsDiagnosticsEnabled() noexcept;
    [[nodiscard]] bool observeSyncFdCompletion(uint64_t frameId, int syncFd, int timeoutMs = 2000);

    /// Apenas para testes determinísticos de fail-closed sem provocar device lost físico.
    void simulateDeviceLostForTest(const QString &reason = QStringLiteral("Simulated VK_ERROR_DEVICE_LOST"));

signals:
    void deviceLost(const QString &reason);

public:
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] bool isSuspended() const noexcept { return m_suspended; }
    [[nodiscard]] bool isDeviceLost() const noexcept { return m_deviceLost; }
    [[nodiscard]] QSize pixelSize() const noexcept { return m_pixelSize; }
    [[nodiscard]] VkImage colorImage() const noexcept { return m_colorImage; }
    [[nodiscard]] VkFormat colorFormat() const noexcept { return m_vkFormat; }
    [[nodiscard]] VulkanDeviceCapabilities capabilities() const noexcept { return m_capabilities; }
    [[nodiscard]] QString lastError() const noexcept { return m_lastError; }
    [[nodiscard]] QQuickWindow *window() const noexcept { return m_window.get(); }
    [[nodiscard]] QQuickItem *rootItem() const noexcept;
    [[nodiscard]] QQuickRenderControl *renderControl() const noexcept { return m_renderControl.get(); }
    [[nodiscard]] QQmlEngine *qmlEngine() const noexcept;
    [[nodiscard]] uint64_t lastSubmittedFrameId() const noexcept { return m_frameCounter; }
    [[nodiscard]] const GpuSubmissionTracker &submissionTracker() const noexcept { return m_submissionTracker; }
    [[nodiscard]] GpuSubmissionTracker &submissionTracker() noexcept { return m_submissionTracker; }
    [[nodiscard]] size_t quarantinedSemaphoreCount() const noexcept { return m_quarantinedSemaphores.size(); }

private:
    [[nodiscard]] bool observeInternalGpuCompletion(uint64_t frameId, uint64_t timeoutNs = 2000000000ULL);
    void quarantineSemaphore(VkSemaphore sem);
    void flushQuarantinedSemaphores();
    bool createVulkanDevice();
    bool createOffscreenImage(const QSize &pixelSize);
    void destroyOffscreenImage();
    bool createCommandPool();
    void destroyVulkanDevice();
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

    QVulkanInstance m_vulkanInstance;
    VkInstance m_ownedVkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    VkImage m_colorImage = VK_NULL_HANDLE;
    VkImage m_activeRenderImage = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMemory = VK_NULL_HANDLE;
    VkFormat m_vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkDeviceSize m_imageAllocationSize = 0;
    QSize m_pixelSize;

    std::unique_ptr<QQuickRenderControl> m_renderControl;
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QQmlEngine> m_ownedQmlEngine;
    QPointer<QQmlEngine> m_activeEngine;
    std::unique_ptr<QQmlComponent> m_qmlComponent;
    QPointer<QQuickItem> m_rootItem;

    VulkanDeviceCapabilities m_capabilities;
    QString m_lastError;
    bool m_initialized = false;
    bool m_suspended = false;
    bool m_deviceLost = false;
    bool m_shuttingDown = false;
    bool m_allowDmabufExportOnInternalImage = true;
    uint64_t m_frameCounter = 0;
    GpuSubmissionTracker m_submissionTracker;
    QList<VkSemaphore> m_quarantinedSemaphores;
};

} // namespace Systalia::Platform::Drm
