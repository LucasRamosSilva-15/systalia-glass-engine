#include "DrmEventDispatcher.hpp"
#include "VulkanQuickRenderer.hpp"

#include <QDebug>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cerrno>
#include <cstring>

namespace Systalia::Platform::Drm {

namespace {

thread_local std::function<void(uint64_t, uint32_t, void *)> *t_activeKernelFlipHandler = nullptr;

void onLibdrmPageFlipEvent(int,
                           unsigned int sequence,
                           unsigned int,
                           unsigned int,
                           void *userData) {
    if (t_activeKernelFlipHandler && *t_activeKernelFlipHandler) {
        (*t_activeKernelFlipHandler)(static_cast<uint64_t>(sequence), 0U, userData);
    }
}

void onLibdrmPageFlipEvent2(int,
                            unsigned int sequence,
                            unsigned int,
                            unsigned int,
                            unsigned int crtcId,
                            void *userData) {
    if (t_activeKernelFlipHandler && *t_activeKernelFlipHandler) {
        (*t_activeKernelFlipHandler)(static_cast<uint64_t>(sequence), crtcId, userData);
    }
}

bool defaultReadDrmEvents(int drmFd, const DrmEventDispatcher::PageFlipCallback &) {
    if (drmFd < 0) {
        return false;
    }
    drmEventContext evCtx{};
    evCtx.version = DRM_EVENT_CONTEXT_VERSION;
    evCtx.page_flip_handler = onLibdrmPageFlipEvent;
    evCtx.page_flip_handler2 = onLibdrmPageFlipEvent2;

    const int rc = drmHandleEvent(drmFd, &evCtx);
    return rc == 0 || errno == EAGAIN;
}

} // namespace

PendingFlipContext *DrmFlipContextRegistry::createContext(uint64_t bufferId, uint32_t crtcId) {
    auto ctx = std::make_unique<PendingFlipContext>();
    ctx->commitId = m_nextCommitId++;
    ctx->generation = m_generation;
    ctx->bufferId = bufferId;
    ctx->crtcId = crtcId;
    PendingFlipContext *raw = ctx.get();
    m_contexts.emplace(raw, std::move(ctx));
    return raw;
}

void DrmFlipContextRegistry::discardContext(uint64_t commitId) {
    for (auto it = m_contexts.begin(); it != m_contexts.end(); ++it) {
        if (it->second->commitId == commitId) {
            m_contexts.erase(it);
            return;
        }
    }
}

bool DrmFlipContextRegistry::markTimedOut(uint64_t commitId) {
    for (auto &kv : m_contexts) {
        if (kv.second->commitId == commitId) {
            kv.second->timedOut = true;
            return true;
        }
    }
    return false;
}

void DrmFlipContextRegistry::advanceGeneration() noexcept {
    ++m_generation;
}

void DrmFlipContextRegistry::clearAll() noexcept {
    m_contexts.clear();
}

bool DrmFlipContextRegistry::hasPendingContext(uint64_t commitId) const noexcept {
    for (const auto &kv : m_contexts) {
        if (kv.second->commitId == commitId) {
            return true;
        }
    }
    return false;
}

DrmFlipContextRegistry::Resolution DrmFlipContextRegistry::resolveRawUserData(void *userData) {
    auto it = m_contexts.find(static_cast<PendingFlipContext *>(userData));
    if (it == m_contexts.end()) {
        return Resolution{};
    }
    PendingFlipContext *ctx = it->second.get();
    if (ctx->completed) {
        return Resolution{};
    }
    ctx->completed = true;

    Resolution res;
    res.commitId = ctx->commitId;
    res.bufferId = ctx->bufferId;
    if (ctx->generation != m_generation) {
        res.status = Resolution::Status::Ignore;
    } else if (ctx->timedOut) {
        res.status = Resolution::Status::LateTimedOutFlip;
    } else {
        res.status = Resolution::Status::NormalFlip;
    }

    m_contexts.erase(it);
    return res;
}

DrmEventDispatcher::DrmEventDispatcher(ReadEventsFn readEventsFn, QObject *parent)
    : QObject(parent),
      m_readEventsFn(std::move(readEventsFn)) {
    if (!m_readEventsFn) {
        m_readEventsFn = defaultReadDrmEvents;
    }

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
        emit pageFlipTimedOut(static_cast<quint64>(m_pendingToken));
    });
}

DrmEventDispatcher::~DrmEventDispatcher() {
    detachFd();
}

void DrmEventDispatcher::attachFd(int drmFd) {
    detachFd();
    if (drmFd < 0) {
        return;
    }
    m_drmFd = drmFd;
    m_notifier = std::make_unique<QSocketNotifier>(m_drmFd, QSocketNotifier::Read, this);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &DrmEventDispatcher::onFdReadable);
}

void DrmEventDispatcher::detachFd() {
    clearFlipTimeout();
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier.reset();
    }
    m_drmFd = -1;
}

void DrmEventDispatcher::armFlipTimeout(int timeoutMs, uint64_t pendingToken) {
    m_pendingToken = pendingToken;
    m_timeoutTimer.start(timeoutMs > 0 ? timeoutMs : 100);
}

void DrmEventDispatcher::clearFlipTimeout() {
    m_timeoutTimer.stop();
}

void DrmEventDispatcher::setFlipContextRegistry(std::shared_ptr<DrmFlipContextRegistry> registry) noexcept {
    m_registry = std::move(registry);
}

void DrmEventDispatcher::dispatchSimulatedPageFlip(uint64_t sequence, uint64_t userData) {
    clearFlipTimeout();
    if (VulkanQuickRenderer::graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] MOCK_PAGE_FLIP_COMPLETED seq=" << sequence << "userData=" << userData;
    }
    emit pageFlipCompleted(static_cast<quint64>(sequence), static_cast<quint64>(userData));
}

void DrmEventDispatcher::dispatchKernelPageFlipForTest(uint64_t sequence, uint32_t crtcId, void *userData) {
    if (VulkanQuickRenderer::graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] MOCK_PAGE_FLIP_COMPLETED (test bridge) seq=" << sequence << "crtc=" << crtcId;
    }
    handleKernelPageFlip(sequence, crtcId, userData);
}

void DrmEventDispatcher::handleKernelPageFlip(uint64_t sequence, uint32_t, void *userData) {
    if (!m_registry) {
        clearFlipTimeout();
        emit pageFlipCompleted(static_cast<quint64>(sequence), 0ULL);
        return;
    }

    const auto resolution = m_registry->resolveRawUserData(userData);
    if (resolution.status == DrmFlipContextRegistry::Resolution::Status::Ignore) {
        return;
    }
    if (resolution.status == DrmFlipContextRegistry::Resolution::Status::LateTimedOutFlip) {
        emit latePageFlipCompleted(static_cast<quint64>(resolution.commitId),
                                   static_cast<quint64>(resolution.bufferId));
        return;
    }

    clearFlipTimeout();
    if (t_activeKernelFlipHandler && VulkanQuickRenderer::graphicsDiagnosticsEnabled()) {
        qInfo() << "[SGE-DIAG] KMS_PAGE_FLIP_COMPLETED seq=" << sequence
                << "commitId=" << resolution.commitId
                << "bufferId=" << resolution.bufferId;
    }
    emit pageFlipCompleted(static_cast<quint64>(sequence), static_cast<quint64>(resolution.commitId));
}

void DrmEventDispatcher::onFdReadable() {
    if (m_drmFd < 0 || !m_readEventsFn) {
        return;
    }
    if (m_notifier) {
        m_notifier->setEnabled(false);
    }

    std::function<void(uint64_t, uint32_t, void *)> kernelHandler =
        [this](uint64_t seq, uint32_t crtcId, void *userData) {
            handleKernelPageFlip(seq, crtcId, userData);
        };
    auto *prevHandler = t_activeKernelFlipHandler;
    t_activeKernelFlipHandler = &kernelHandler;

    Q_UNUSED(m_readEventsFn(m_drmFd, [this](uint64_t seq, uint64_t userData) {
        clearFlipTimeout();
        emit pageFlipCompleted(static_cast<quint64>(seq), static_cast<quint64>(userData));
    }));

    t_activeKernelFlipHandler = prevHandler;
    if (m_notifier && m_drmFd >= 0) {
        m_notifier->setEnabled(true);
    }
}

} // namespace Systalia::Platform::Drm
