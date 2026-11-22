#include "AtomicKmsPresenter.hpp"

#include <drm_fourcc.h>
#include <poll.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cstring>

namespace Systalia::Platform::Drm {

namespace {

uint32_t findPropertyId(int drmFd, uint32_t objectId, uint32_t objectType, const char *propName) {
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(drmFd, objectId, objectType);
    if (!props) {
        return 0;
    }
    uint32_t propId = 0;
    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr prop = drmModeGetProperty(drmFd, props->props[i]);
        if (!prop) {
            continue;
        }
        if (std::strcmp(prop->name, propName) == 0) {
            propId = prop->prop_id;
            drmModeFreeProperty(prop);
            break;
        }
        drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);
    return propId;
}

} // namespace

KmsFenceCapabilities LibdrmAtomicKmsBackend::probeFenceCapabilities(int drmFd,
                                                                       uint32_t planeId,
                                                                       uint32_t crtcId) {
    KmsFenceCapabilities caps;
    if (drmFd < 0) {
        return caps;
    }
    if (planeId != 0) {
        caps.supportsInFenceFd = findPropertyId(drmFd, planeId, DRM_MODE_OBJECT_PLANE, "IN_FENCE_FD") != 0;
    }
    if (crtcId != 0) {
        caps.supportsOutFencePtr = findPropertyId(drmFd, crtcId, DRM_MODE_OBJECT_CRTC, "OUT_FENCE_PTR") != 0;
    }
    return caps;
}

uint32_t LibdrmAtomicKmsBackend::addFramebuffer(int drmFd, const GbmExportedBuffer &buffer) {
    if (drmFd < 0 || buffer.dmabufFd < 0 || buffer.width == 0 || buffer.height == 0) {
        return 0;
    }
    uint32_t gemHandle = 0;
    if (drmPrimeFDToHandle(drmFd, buffer.dmabufFd, &gemHandle) != 0) {
        return 0;
    }

    uint32_t handles[4] = {gemHandle, 0, 0, 0};
    uint32_t pitches[4] = {buffer.stride, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    uint64_t modifiers[4] = {buffer.modifier, 0, 0, 0};
    uint32_t fbId = 0;

    const uint32_t flags = (buffer.modifier != DRM_FORMAT_MOD_INVALID && buffer.modifier != 0)
                               ? DRM_MODE_FB_MODIFIERS
                               : 0;
    const uint32_t format = buffer.format != 0 ? buffer.format : DRM_FORMAT_XRGB8888;
    const int rc = drmModeAddFB2WithModifiers(
        drmFd, buffer.width, buffer.height, format, handles, pitches, offsets, modifiers, &fbId, flags);

    drm_gem_close closeReq{};
    closeReq.handle = gemHandle;
    Q_UNUSED(drmIoctl(drmFd, DRM_IOCTL_GEM_CLOSE, &closeReq));

    return rc == 0 ? fbId : 0;
}

void LibdrmAtomicKmsBackend::removeFramebuffer(int drmFd, uint32_t fbId) {
    if (drmFd >= 0 && fbId != 0) {
        Q_UNUSED(drmModeRmFB(drmFd, fbId));
    }
}

AtomicCommitResult LibdrmAtomicKmsBackend::commitAtomic(int drmFd, const AtomicCommitRequest &req) {
    AtomicCommitResult result;
    if (drmFd < 0 || req.planeId == 0 || req.crtcId == 0 || req.framebufferId == 0) {
        return result;
    }

    drmModeAtomicReqPtr atomicReq = drmModeAtomicAlloc();
    if (!atomicReq) {
        return result;
    }

    const uint32_t fbProp = findPropertyId(drmFd, req.planeId, DRM_MODE_OBJECT_PLANE, "FB_ID");
    const uint32_t crtcProp = findPropertyId(drmFd, req.planeId, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
    const uint32_t srcWProp = findPropertyId(drmFd, req.planeId, DRM_MODE_OBJECT_PLANE, "SRC_W");
    const uint32_t srcHProp = findPropertyId(drmFd, req.planeId, DRM_MODE_OBJECT_PLANE, "SRC_H");
    const uint32_t crtcWProp = findPropertyId(drmFd, req.planeId, DRM_MODE_OBJECT_PLANE, "CRTC_W");
    const uint32_t crtcHProp = findPropertyId(drmFd, req.planeId, DRM_MODE_OBJECT_PLANE, "CRTC_H");

    if (!fbProp || !crtcProp || !srcWProp || !srcHProp || !crtcWProp || !crtcHProp) {
        drmModeAtomicFree(atomicReq);
        return result;
    }

    drmModeAtomicAddProperty(atomicReq, req.planeId, fbProp, req.framebufferId);
    drmModeAtomicAddProperty(atomicReq, req.planeId, crtcProp, req.crtcId);
    drmModeAtomicAddProperty(atomicReq, req.planeId, srcWProp, static_cast<uint64_t>(req.srcWidth) << 16);
    drmModeAtomicAddProperty(atomicReq, req.planeId, srcHProp, static_cast<uint64_t>(req.srcHeight) << 16);
    drmModeAtomicAddProperty(atomicReq, req.planeId, crtcWProp, req.srcWidth);
    drmModeAtomicAddProperty(atomicReq, req.planeId, crtcHProp, req.srcHeight);

    if (req.inFenceFd >= 0) {
        if (const uint32_t inFenceProp = findPropertyId(drmFd, req.planeId, DRM_MODE_OBJECT_PLANE, "IN_FENCE_FD")) {
            drmModeAtomicAddProperty(atomicReq, req.planeId, inFenceProp, static_cast<uint64_t>(req.inFenceFd));
        }
    }

    int outFenceFd = -1;
    if (!req.testOnly) {
        if (const uint32_t outFenceProp = findPropertyId(drmFd, req.crtcId, DRM_MODE_OBJECT_CRTC, "OUT_FENCE_PTR")) {
            drmModeAtomicAddProperty(atomicReq, req.crtcId, outFenceProp, reinterpret_cast<uint64_t>(&outFenceFd));
        }
    }

    uint32_t flags = req.testOnly ? DRM_MODE_ATOMIC_TEST_ONLY : (DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT);
    if (req.allowModeset) {
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    }

    result.succeeded = (drmModeAtomicCommit(drmFd, atomicReq, flags, nullptr) == 0);
    result.outFenceFd = result.succeeded ? outFenceFd : -1;
    drmModeAtomicFree(atomicReq);
    return result;
}

AtomicKmsPresenter::AtomicKmsPresenter(IKmsResourceBackend &kmsBackend,
                                       IGbmBackend &gbmBackend,
                                       IAtomicKmsBackend &atomicBackend) noexcept
    : m_discovery(kmsBackend),
      m_bufferManager(gbmBackend),
      m_atomicBackend(atomicBackend) {}

AtomicKmsPresenter::~AtomicKmsPresenter() {
    shutdown();
}

bool AtomicKmsPresenter::initializeOnFd(int drmFd) {
    if (m_initialized || drmFd < 0) {
        return false;
    }
    m_snapshot = m_discovery.discover(drmFd);
    if (!m_snapshot.valid) {
        return false;
    }

    const KmsConnectorInfo *primary = m_snapshot.primaryConnector();
    if (!primary) {
        return false;
    }
    const uint32_t crtcId = KmsOutputDiscovery::selectCompatibleCrtc(*primary, m_snapshot.crtcs);
    const KmsPlaneInfo *plane = m_snapshot.primaryPlaneForCrtc(crtcId);
    if (crtcId == 0 || !plane) {
        return false;
    }

    if (!m_bufferManager.initializeDevice(drmFd)) {
        return false;
    }

    m_drmFd = drmFd;
    m_connectorId = primary->connectorId;
    m_crtcId = crtcId;
    m_planeId = plane->planeId;
    m_fenceCaps = m_atomicBackend.probeFenceCapabilities(m_drmFd, m_planeId, m_crtcId);
    m_initialized = true;
    m_suspended = false;
    return true;
}

bool AtomicKmsPresenter::configureOutput(const DrmOutputMode &mode) {
    if (!m_initialized || !mode.isValid()) {
        return false;
    }
    m_activeMode = mode;
    const GbmBufferAllocationRequest req{
        static_cast<uint32_t>(mode.width),
        static_cast<uint32_t>(mode.height),
        DRM_FORMAT_XRGB8888,
        {},
        true,
        true};
    return m_bufferManager.allocateSwapchainPool(req, 2);
}

bool AtomicKmsPresenter::presentFrame(uint64_t, int drmFd) {
    if (!m_initialized || m_suspended || drmFd != m_drmFd) {
        return false;
    }
    GbmExportedBuffer *buf = m_bufferManager.acquireBufferForRendering();
    if (!buf) {
        return false;
    }
    if (!m_bufferManager.markBufferReadyForScanout(buf->id)) {
        Q_UNUSED(m_bufferManager.releaseBufferToAvailable(buf->id));
        return false;
    }
    return submitBufferForScanout(buf->id);
}

bool AtomicKmsPresenter::submitBufferForScanout(uint64_t bufferId, int acquireFenceFd) {
    if (!m_initialized || m_suspended) {
        return false;
    }
    GbmExportedBuffer *target = m_bufferManager.findBuffer(bufferId);
    if (!target) {
        return false;
    }
    if (!m_bufferManager.markBufferPendingFlip(bufferId)) {
        return false;
    }

    if (target->kmsFramebufferId == 0) {
        const uint32_t fbId = m_atomicBackend.addFramebuffer(m_drmFd, *target);
        if (fbId == 0) {
            Q_UNUSED(m_bufferManager.releaseBufferToAvailable(bufferId));
            return false;
        }
        target->kmsFramebufferId = fbId;
    }

    int effectiveInFenceFd = (acquireFenceFd >= 0) ? acquireFenceFd : target->acquireFenceFd;
    const bool inFenceFromTarget = (acquireFenceFd < 0 && target->acquireFenceFd >= 0);

    AtomicCommitRequest req;
    req.connectorId = m_connectorId;
    req.crtcId = m_crtcId;
    req.planeId = m_planeId;
    req.framebufferId = target->kmsFramebufferId;
    req.srcWidth = static_cast<uint32_t>(m_activeMode.width);
    req.srcHeight = static_cast<uint32_t>(m_activeMode.height);
    req.inFenceFd = -1;

    if (effectiveInFenceFd >= 0) {
        if (m_fenceCaps.supportsInFenceFd) {
            req.inFenceFd = effectiveInFenceFd;
        } else {
            // Fallback seguro quando IN_FENCE_FD não existe no plano: aguarda sinalização antes do commit.
            pollfd pfd{effectiveInFenceFd, POLLIN, 0};
            const int pollRc = ::poll(&pfd, 1, 2000);
            if (inFenceFromTarget) {
                ::close(target->acquireFenceFd);
                target->acquireFenceFd = -1;
            } else {
                ::close(effectiveInFenceFd);
            }
            effectiveInFenceFd = -1;
            if (pollRc <= 0 || (pfd.revents & POLLIN) == 0) {
                Q_UNUSED(m_bufferManager.releaseBufferToAvailable(bufferId));
                return false;
            }
        }
    }

    const AtomicCommitResult res = m_atomicBackend.commitAtomic(m_drmFd, req);

    // Após commitAtomic, o FD de entrada nunca deve ser reutilizado em commits futuros.
    if (effectiveInFenceFd >= 0) {
        if (inFenceFromTarget && target->acquireFenceFd >= 0) {
            ::close(target->acquireFenceFd);
            target->acquireFenceFd = -1;
        } else if (!inFenceFromTarget && res.succeeded) {
            ::close(effectiveInFenceFd);
        }
    }

    if (!res.succeeded) {
        if (res.outFenceFd >= 0) {
            ::close(res.outFenceFd);
        }
        Q_UNUSED(m_bufferManager.releaseBufferToAvailable(bufferId));
        return false;
    }

    if (m_pendingOutFenceFd >= 0) {
        ::close(m_pendingOutFenceFd);
        m_pendingOutFenceFd = -1;
    }
    m_pendingFlipBufferId = bufferId;
    m_pendingOutFenceFd = res.outFenceFd;
    return true;
}

bool AtomicKmsPresenter::onPageFlipCompleted(uint64_t bufferId, int releaseFenceFd) {
    int effectiveReleaseFd = releaseFenceFd;
    if (bufferId == m_pendingFlipBufferId) {
        if (effectiveReleaseFd < 0) {
            effectiveReleaseFd = m_pendingOutFenceFd;
            m_pendingOutFenceFd = -1;
        } else if (m_pendingOutFenceFd >= 0) {
            ::close(m_pendingOutFenceFd);
            m_pendingOutFenceFd = -1;
        }
        m_pendingFlipBufferId = 0;
    }
    return m_bufferManager.markBufferPresented(bufferId, 0, effectiveReleaseFd);
}

void AtomicKmsPresenter::suspendPresentation() {
    m_suspended = true;
    if (m_pendingOutFenceFd >= 0) {
        ::close(m_pendingOutFenceFd);
        m_pendingOutFenceFd = -1;
    }
    m_pendingFlipBufferId = 0;
}

bool AtomicKmsPresenter::resumePresentation(int drmFd) {
    if (!m_initialized || drmFd < 0) {
        return false;
    }
    if (drmFd != m_drmFd) {
        for (auto &buf : m_bufferManager.buffers()) {
            buf.kmsFramebufferId = 0;
        }
    }
    m_drmFd = drmFd;
    m_fenceCaps = m_atomicBackend.probeFenceCapabilities(m_drmFd, m_planeId, m_crtcId);
    m_suspended = false;
    return true;
}

void AtomicKmsPresenter::shutdown() {
    if (m_pendingOutFenceFd >= 0) {
        ::close(m_pendingOutFenceFd);
        m_pendingOutFenceFd = -1;
    }
    m_pendingFlipBufferId = 0;
    if (m_drmFd >= 0) {
        for (auto &buf : m_bufferManager.buffers()) {
            if (buf.kmsFramebufferId != 0) {
                m_atomicBackend.removeFramebuffer(m_drmFd, buf.kmsFramebufferId);
                buf.kmsFramebufferId = 0;
            }
        }
    }
    m_bufferManager.shutdown();
    m_drmFd = -1;
    m_connectorId = 0;
    m_crtcId = 0;
    m_planeId = 0;
    m_initialized = false;
    m_suspended = false;
}

} // namespace Systalia::Platform::Drm
