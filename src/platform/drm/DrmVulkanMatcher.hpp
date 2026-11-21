#pragma once

#include <cstdint>

namespace Systalia::Platform::Drm {

struct DrmVulkanDeviceIds {
    bool hasPrimary = false;
    uint32_t primaryMajor = 0;
    uint32_t primaryMinor = 0;
    bool hasRender = false;
    uint32_t renderMajor = 0;
    uint32_t renderMinor = 0;
};

/**
 * @brief Obtém major/minor de um descritor DRM já aberto sem abrir /dev/dri/.
 */
[[nodiscard]] bool queryDrmFdMajorMinor(int drmFd, uint32_t *majorOut, uint32_t *minorOut) noexcept;

/**
 * @brief Verifica correspondência entre VkPhysicalDeviceDrmPropertiesEXT e um nó DRM.
 */
[[nodiscard]] bool matchesDrmDevice(const DrmVulkanDeviceIds &vkIds,
                                    uint32_t drmMajor,
                                    uint32_t drmMinor) noexcept;

} // namespace Systalia::Platform::Drm
