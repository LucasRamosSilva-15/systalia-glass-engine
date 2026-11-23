#include "DrmVulkanMatcher.hpp"

#include <sys/stat.h>
#include <sys/sysmacros.h>

namespace Systalia::Platform::Drm {

bool queryDrmFdMajorMinor(int drmFd, uint32_t *majorOut, uint32_t *minorOut) noexcept {
    if (drmFd < 0) {
        return false;
    }
    struct stat st {};
    if (::fstat(drmFd, &st) != 0) {
        return false;
    }
    if (majorOut) {
        *majorOut = static_cast<uint32_t>(major(st.st_rdev));
    }
    if (minorOut) {
        *minorOut = static_cast<uint32_t>(minor(st.st_rdev));
    }
    return true;
}

bool matchesDrmDevice(const DrmVulkanDeviceIds &vkIds,
                      uint32_t drmMajor,
                      uint32_t drmMinor) noexcept {
    if (vkIds.hasPrimary && vkIds.primaryMajor == drmMajor && vkIds.primaryMinor == drmMinor) {
        return true;
    }
    // Em nós renderD128 da mesma GPU DRM (ex.: card1=226:1 e renderD128=226:128)
    // o major DRM (226) coincide e o minor de render é offset +128 do primary.
    if (vkIds.hasRender && vkIds.renderMajor == drmMajor
        && (vkIds.renderMinor == drmMinor || vkIds.renderMinor == drmMinor + 128u)) {
        return true;
    }
    return false;
}

} // namespace Systalia::Platform::Drm
