#pragma once

#include <QByteArrayList>
#include <QString>
#include <cstdint>

namespace Systalia::Platform::Drm {

/**
 * @brief Contrato de capacidades Vulkan/Qt Quick exigidas para apresentação
 *        offscreen sobre DMA-BUF/GBM + Atomic KMS (Etapa 03.2.4.3).
 *
 * Nota: QQuickRenderControl não fornece swapchain KMS nem exporta DMA-BUF
 * automaticamente. A camada de apresentação precisa:
 *  1. Criar QVulkanInstance com extensões de memória/semáforo externos.
 *  2. Vincular VkPhysicalDevice/VkDevice/VkQueue via QQuickGraphicsDevice::fromDeviceObjects().
 *  3. Importar/exportar VkImage (VK_KHR_external_memory_fd + VK_EXT_image_drm_format_modifier)
 *     como QQuickRenderTarget::fromVulkanImage().
 *  4. Inserir barreira de layout COLOR_ATTACHMENT_OPTIMAL -> GENERAL / EXTERNAL
 *     antes do drmModeAtomicCommit e exportar sync_file via VK_KHR_external_semaphore_fd.
 */
struct VulkanQuickRenderContract {
    bool requiresOffscreenRenderControl = true;
    bool requiresExplicitModifierImport = true;
    bool requiresSyncFileFence = true;

    QByteArrayList requiredInstanceExtensions{
        QByteArrayLiteral("VK_KHR_get_physical_device_properties2"),
        QByteArrayLiteral("VK_KHR_external_memory_capabilities"),
        QByteArrayLiteral("VK_KHR_external_semaphore_capabilities")
    };

    QByteArrayList requiredDeviceExtensions{
        QByteArrayLiteral("VK_KHR_external_memory"),
        QByteArrayLiteral("VK_KHR_external_memory_fd"),
        QByteArrayLiteral("VK_KHR_external_semaphore"),
        QByteArrayLiteral("VK_KHR_external_semaphore_fd"),
        QByteArrayLiteral("VK_EXT_external_memory_dma_buf"),
        QByteArrayLiteral("VK_EXT_image_drm_format_modifier")
    };

    uint32_t expectedGraphicsQueueFamilyIndex = 0;
    bool supportsVtSuspensionRecovery = true;
    bool verifiedOnPhysicalGpu = false;
};

} // namespace Systalia::Platform::Drm
