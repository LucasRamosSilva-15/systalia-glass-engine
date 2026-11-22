#include "DrmEventDispatcher.hpp"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cerrno>
#include <cstring>

namespace Systalia::Platform::Drm {

namespace {

struct DrmFlipDispatchBridge {
    const DrmEventDispatcher::PageFlipCallback *callback = nullptr;
};

void onLibdrmPageFlipEvent(int,
                           unsigned int sequence,
                           unsigned int,
                           unsigned int,
                           void *userData) {
    auto *bridge = static_cast<DrmFlipDispatchBridge *>(userData);
    if (bridge && bridge->callback && *bridge->callback) {
        (*bridge->callback)(static_cast<uint64_t>(sequence), 0);
    }
}

bool defaultReadDrmEvents(int drmFd, const DrmEventDispatcher::PageFlipCallback &onFlip) {
    if (drmFd < 0) {
        return false;
    }
    DrmFlipDispatchBridge bridge{&onFlip};
    drmEventContext evCtx{};
    evCtx.version = DRM_EVENT_CONTEXT_VERSION;
    evCtx.page_flip_handler = onLibdrmPageFlipEvent;

    //Nota: drmHandleEvent lê eventos do FD DRM sem abrir novo descritor.
    Q_UNUSED(bridge);
    const int rc = drmHandleEvent(drmFd, &evCtx);
    return rc == 0 || errno == EAGAIN;
}

} // namespace

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

void DrmEventDispatcher::dispatchSimulatedPageFlip(uint64_t sequence, uint64_t userData) {
    clearFlipTimeout();
    emit pageFlipCompleted(static_cast<quint64>(sequence), static_cast<quint64>(userData));
}

void DrmEventDispatcher::onFdReadable() {
    if (m_drmFd < 0 || !m_readEventsFn) {
        return;
    }
    if (m_notifier) {
        m_notifier->setEnabled(false);
    }
    Q_UNUSED(m_readEventsFn(m_drmFd, [this](uint64_t seq, uint64_t userData) {
        clearFlipTimeout();
        emit pageFlipCompleted(static_cast<quint64>(seq), static_cast<quint64>(userData));
    }));
    if (m_notifier && m_drmFd >= 0) {
        m_notifier->setEnabled(true);
    }
}

} // namespace Systalia::Platform::Drm
