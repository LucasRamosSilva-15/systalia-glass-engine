#include "GbmBufferManager.hpp"

#include <drm_fourcc.h>
#include <gbm.h>
#include <unistd.h>

#include <utility>
#include <vector>

namespace Systalia::Platform::Drm {

namespace {

void closeFdIfValid(int &fd) noexcept {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

} // namespace

void *LibgbmBackend::createDevice(int drmFd) {
    if (drmFd < 0) {
        return nullptr;
    }
    return gbm_create_device(drmFd);
}

void LibgbmBackend::destroyDevice(void *device) {
    if (device) {
        gbm_device_destroy(static_cast<gbm_device *>(device));
    }
}

bool LibgbmBackend::allocateBuffer(void *device,
                                   const GbmBufferAllocationRequest &req,
                                   GbmExportedBuffer *outBuffer) {
    if (!device || !outBuffer || req.width == 0 || req.height == 0) {
        return false;
    }

    auto *gbmDev = static_cast<gbm_device *>(device);
    const uint32_t format = req.format != 0 ? req.format : GBM_FORMAT_XRGB8888;
    uint32_t flags = 0;
    if (req.requireScanout) {
        flags |= GBM_BO_USE_SCANOUT;
    }
    if (req.requireRendering) {
        flags |= GBM_BO_USE_RENDERING;
    }

    gbm_bo *bo = nullptr;
    if (!req.preferredModifiers.isEmpty()) {
        std::vector<uint64_t> mods;
        mods.reserve(static_cast<size_t>(req.preferredModifiers.size()));
        for (uint64_t mod : req.preferredModifiers) {
            mods.push_back(mod);
        }
        bo = gbm_bo_create_with_modifiers2(
            gbmDev, req.width, req.height, format, mods.data(),
            static_cast<unsigned int>(mods.size()), flags);
    }

    if (!bo) {
        bo = gbm_bo_create(gbmDev, req.width, req.height, format, flags);
    }
    if (!bo) {
        return false;
    }

    const int dmabufFd = gbm_bo_get_fd(bo);
    if (dmabufFd < 0) {
        gbm_bo_destroy(bo);
        return false;
    }

    outBuffer->width = gbm_bo_get_width(bo);
    outBuffer->height = gbm_bo_get_height(bo);
    outBuffer->format = gbm_bo_get_format(bo);
    outBuffer->stride = gbm_bo_get_stride(bo);
    outBuffer->modifier = gbm_bo_get_modifier(bo);
    outBuffer->dmabufFd = dmabufFd;
    outBuffer->nativeHandle = bo;
    outBuffer->vulkanExternalMemoryVerified = false;
    return true;
}

void LibgbmBackend::destroyBuffer(GbmExportedBuffer *buffer) {
    if (!buffer) {
        return;
    }
    closeFdIfValid(buffer->dmabufFd);
    closeFdIfValid(buffer->acquireFenceFd);
    closeFdIfValid(buffer->releaseFenceFd);
    if (buffer->nativeHandle) {
        gbm_bo_destroy(static_cast<gbm_bo *>(buffer->nativeHandle));
        buffer->nativeHandle = nullptr;
    }
}

GbmBufferManager::GbmBufferManager(IGbmBackend &backend) noexcept
    : m_backend(backend) {}

GbmBufferManager::~GbmBufferManager() {
    shutdown();
}

bool GbmBufferManager::initializeDevice(int drmFd) {
    if (m_device) {
        return false;
    }
    if (drmFd < 0) {
        return false;
    }
    m_device = m_backend.createDevice(drmFd);
    return m_device != nullptr;
}

bool GbmBufferManager::allocateSwapchainPool(const GbmBufferAllocationRequest &req, int bufferCount) {
    if (!m_device || bufferCount <= 0 || req.width == 0 || req.height == 0) {
        return false;
    }

    releaseAllBuffers();

    for (int i = 0; i < bufferCount; ++i) {
        GbmExportedBuffer buf;
        buf.id = m_nextBufferId++;
        buf.state = GbmBufferState::Available;
        if (!m_backend.allocateBuffer(m_device, req, &buf)) {
            releaseAllBuffers();
            return false;
        }
        m_buffers.append(buf);
    }
    return true;
}

GbmExportedBuffer *GbmBufferManager::registerExternalSlot(GbmExportedBuffer slot) {
    slot.id = m_nextBufferId++;
    slot.state = GbmBufferState::Available;
    m_buffers.append(slot);
    return &m_buffers.last();
}

GbmExportedBuffer *GbmBufferManager::acquireBufferForRendering() {
    for (auto &buf : m_buffers) {
        if (buf.state == GbmBufferState::Available) {
            closeFdIfValid(buf.releaseFenceFd);
            buf.state = GbmBufferState::Rendering;
            return &buf;
        }
    }
    return nullptr;
}

bool GbmBufferManager::markBufferReadyForScanout(uint64_t bufferId, int acquireFenceFd) {
    GbmExportedBuffer *buf = findBuffer(bufferId);
    if (!buf || buf->state != GbmBufferState::Rendering) {
        return false;
    }
    closeFdIfValid(buf->acquireFenceFd);
    buf->acquireFenceFd = acquireFenceFd;
    buf->state = GbmBufferState::ReadyForScanout;
    return true;
}

bool GbmBufferManager::markBufferPendingFlip(uint64_t bufferId) {
    GbmExportedBuffer *buf = findBuffer(bufferId);
    if (!buf || buf->state != GbmBufferState::ReadyForScanout) {
        return false;
    }
    buf->state = GbmBufferState::PendingFlip;
    return true;
}

bool GbmBufferManager::markBufferReadyForFlip(uint64_t bufferId, int acquireFenceFd) {
    return markBufferReadyForScanout(bufferId, acquireFenceFd)
           && markBufferPendingFlip(bufferId);
}

bool GbmBufferManager::markBufferPresented(uint64_t bufferId, uint32_t kmsFbId) {
    GbmExportedBuffer *buf = findBuffer(bufferId);
    if (!buf || buf->state != GbmBufferState::PendingFlip) {
        return false;
    }
    for (auto &other : m_buffers) {
        if (other.id != bufferId && other.state == GbmBufferState::Presented) {
            other.state = GbmBufferState::Available;
        }
    }
    if (kmsFbId != 0) {
        buf->kmsFramebufferId = kmsFbId;
    }
    closeFdIfValid(buf->acquireFenceFd);
    buf->state = GbmBufferState::Presented;
    return true;
}

bool GbmBufferManager::markBufferTimedOut(uint64_t bufferId) {
    GbmExportedBuffer *buf = findBuffer(bufferId);
    if (!buf || buf->state != GbmBufferState::PendingFlip) {
        return false;
    }
    closeFdIfValid(buf->acquireFenceFd);
    buf->state = GbmBufferState::TimedOutInScanout;
    return true;
}

DrmModifierNegotiationResult GbmBufferManager::negotiateModifiers(
    uint32_t drmFourcc,
    const QList<uint64_t> &kmsModifiers,
    const QList<uint64_t> &vulkanModifiers) {
    DrmModifierNegotiationResult res;
    res.drmFourcc = drmFourcc;
    if (drmFourcc == 0) {
        res.failureReason = QStringLiteral("Formato DRM FourCC inválido.");
        return res;
    }
    if (kmsModifiers.isEmpty() || vulkanModifiers.isEmpty()) {
        res.failureReason = QStringLiteral("Lista de modificadores KMS ou Vulkan vazia.");
        return res;
    }

    for (uint64_t mod : vulkanModifiers) {
        if (kmsModifiers.contains(mod) && !res.negotiatedModifiers.contains(mod)) {
            res.negotiatedModifiers.append(mod);
        }
    }

    if (res.negotiatedModifiers.isEmpty()) {
        res.failureReason = QStringLiteral("Nenhum modificador DRM compatível entre KMS e Vulkan.");
        return res;
    }
    res.compatible = true;
    return res;
}

bool GbmBufferManager::releaseBufferToAvailable(uint64_t bufferId, int releaseFenceFd) {
    GbmExportedBuffer *buf = findBuffer(bufferId);
    if (!buf) {
        return false;
    }
    closeFdIfValid(buf->acquireFenceFd);
    closeFdIfValid(buf->releaseFenceFd);
    buf->releaseFenceFd = releaseFenceFd;
    buf->state = GbmBufferState::Available;
    return true;
}

void GbmBufferManager::releaseAllBuffers() {
    for (auto &buf : m_buffers) {
        m_backend.destroyBuffer(&buf);
        closeFdIfValid(buf.dmabufFd);
        closeFdIfValid(buf.acquireFenceFd);
        closeFdIfValid(buf.releaseFenceFd);
    }
    m_buffers.clear();
}

void GbmBufferManager::shutdown() {
    releaseAllBuffers();
    if (m_device) {
        m_backend.destroyDevice(m_device);
        m_device = nullptr;
    }
}

int GbmBufferManager::countInState(GbmBufferState state) const noexcept {
    int count = 0;
    for (const auto &buf : m_buffers) {
        if (buf.state == state) {
            ++count;
        }
    }
    return count;
}

GbmExportedBuffer *GbmBufferManager::findBuffer(uint64_t bufferId) noexcept {
    for (auto &buf : m_buffers) {
        if (buf.id == bufferId) {
            return &buf;
        }
    }
    return nullptr;
}

} // namespace Systalia::Platform::Drm
