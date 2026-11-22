#pragma once

#include "DrmVulkanMatcher.hpp"
#include "IndependentFrameLoop.hpp"

#include <QByteArray>
#include <QImage>
#include <QObject>
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

    [[nodiscard]] bool initialize(const QSize &pixelSize);
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
    [[nodiscard]] QQuickItem *rootItem() const noexcept { return m_rootItem; }
    [[nodiscard]] QQuickRenderControl *renderControl() const noexcept { return m_renderControl.get(); }
    [[nodiscard]] QQmlEngine *qmlEngine() const noexcept { return m_activeEngine; }

private:
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
    VkDeviceMemory m_colorMemory = VK_NULL_HANDLE;
    VkFormat m_vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkDeviceSize m_imageAllocationSize = 0;
    QSize m_pixelSize;

    std::unique_ptr<QQuickRenderControl> m_renderControl;
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QQmlEngine> m_ownedQmlEngine;
    QQmlEngine *m_activeEngine = nullptr;
    std::unique_ptr<QQmlComponent> m_qmlComponent;
    QQuickItem *m_rootItem = nullptr;

    VulkanDeviceCapabilities m_capabilities;
    QString m_lastError;
    bool m_initialized = false;
    bool m_suspended = false;
    bool m_deviceLost = false;
    uint64_t m_frameCounter = 0;
};

} // namespace Systalia::Platform::Drm
