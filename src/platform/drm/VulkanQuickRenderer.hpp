#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVulkanInstance>
#include <vulkan/vulkan.h>

#include <memory>

class QQmlComponent;
class QQmlEngine;
class QQuickItem;
class QQuickRenderControl;
class QQuickWindow;

namespace Systalia::Platform::Drm {

struct VulkanDeviceCapabilities {
    QString deviceName;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    bool isSoftware = false;
    bool supportsExternalMemoryFd = false;
    bool supportsExternalMemoryDmabuf = false;
    bool supportsDrmFormatModifier = false;
    bool supportsExternalSemaphoreFd = false;
    bool dmabufExportImageCreated = false;
};

/**
 * @brief Ponte isolada entre Vulkan (QVulkanInstance/VkDevice) e Qt Quick (QQuickRenderControl).
 *
 * Renderiza cenas QML em uma VkImage offscreen sem abrir /dev/dri/card* e sem usar eglfs_kms.
 */
class VulkanQuickRenderer final : public QObject {
    Q_OBJECT

public:
    explicit VulkanQuickRenderer(QObject *parent = nullptr);
    ~VulkanQuickRenderer() override;

    VulkanQuickRenderer(const VulkanQuickRenderer &) = delete;
    VulkanQuickRenderer &operator=(const VulkanQuickRenderer &) = delete;

    [[nodiscard]] bool initialize(const QSize &pixelSize);
    [[nodiscard]] bool loadQmlSource(const QByteArray &qmlSource);
    [[nodiscard]] bool resize(const QSize &pixelSize);
    [[nodiscard]] bool renderFrame();
    [[nodiscard]] QImage readbackImage();
    [[nodiscard]] int exportDmabufFd();

    void setSuspended(bool suspended) noexcept;
    void shutdown();

    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] bool isSuspended() const noexcept { return m_suspended; }
    [[nodiscard]] QSize pixelSize() const noexcept { return m_pixelSize; }
    [[nodiscard]] VkImage colorImage() const noexcept { return m_colorImage; }
    [[nodiscard]] VkFormat colorFormat() const noexcept { return m_vkFormat; }
    [[nodiscard]] VulkanDeviceCapabilities capabilities() const noexcept { return m_capabilities; }
    [[nodiscard]] QString lastError() const noexcept { return m_lastError; }

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
    std::unique_ptr<QQmlEngine> m_qmlEngine;
    std::unique_ptr<QQmlComponent> m_qmlComponent;
    QQuickItem *m_rootItem = nullptr;

    VulkanDeviceCapabilities m_capabilities;
    QString m_lastError;
    bool m_initialized = false;
    bool m_suspended = false;
};

} // namespace Systalia::Platform::Drm
