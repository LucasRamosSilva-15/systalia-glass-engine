#include "IndependentFrameLoop.hpp"

#include "AtomicKmsPresenter.hpp"
#include "DrmEventDispatcher.hpp"
#include "GbmBufferManager.hpp"

#include <QMetaObject>

namespace Systalia::Platform::Drm {

IndependentFrameLoop::IndependentFrameLoop(IControlledRenderSource &renderSource,
                                           AtomicKmsPresenter &presenter,
                                           WaylandFrameCoordinator &waylandCoordinator,
                                           DrmEventDispatcher &eventDispatcher,
                                           QObject *parent)
    : QObject(parent),
      m_renderSource(renderSource),
      m_presenter(presenter),
      m_waylandCoordinator(waylandCoordinator),
      m_eventDispatcher(eventDispatcher) {
    connect(&m_eventDispatcher, &DrmEventDispatcher::pageFlipCompleted,
            this, &IndependentFrameLoop::onPageFlipCompleted);
    connect(&m_eventDispatcher, &DrmEventDispatcher::pageFlipTimedOut,
            this, &IndependentFrameLoop::onPageFlipTimedOut);
}

IndependentFrameLoop::~IndependentFrameLoop() {
    shutdown();
}

void IndependentFrameLoop::requestFrame() {
    if (m_shuttingDown || m_suspended) {
        return;
    }
    m_renderPending = true;
    if (!m_flipInFlight) {
        scheduleQueuedProcess();
    }
}

void IndependentFrameLoop::scheduleQueuedProcess() {
    if (m_scheduled || m_shuttingDown || m_suspended || m_flipInFlight) {
        return;
    }
    m_scheduled = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_scheduled = false;
        Q_UNUSED(processNextFrame());
    }, Qt::QueuedConnection);
}

bool IndependentFrameLoop::processNextFrame() {
    if (m_shuttingDown || m_suspended || m_flipInFlight || !m_renderPending) {
        return false;
    }
    m_renderPending = false;

    auto &gbm = m_presenter.bufferManager();
    GbmExportedBuffer *buf = gbm.acquireBufferForRendering();
    if (!buf) {
        // Sem buffer disponível: mantém pendência para tentar após próximo flip
        m_renderPending = true;
        return false;
    }
    const uint64_t bufferId = buf->id;

    if (!m_waylandCoordinator.beginFrame()) {
        Q_UNUSED(gbm.releaseBufferToAvailable(bufferId));
        return false;
    }

    if (!m_renderSource.renderIntoBuffer(*buf)) {
        Q_UNUSED(gbm.releaseBufferToAvailable(bufferId));
        Q_UNUSED(m_waylandCoordinator.completeFrame(false));
        ++m_failedRenderCount;
        emit frameFailed();
        return false;
    }

    if (!gbm.markBufferReadyForScanout(bufferId, buf->acquireFenceFd)) {
        Q_UNUSED(gbm.releaseBufferToAvailable(bufferId));
        Q_UNUSED(m_waylandCoordinator.completeFrame(false));
        ++m_failedRenderCount;
        emit frameFailed();
        return false;
    }

    if (!m_presenter.submitBufferForScanout(bufferId)) {
        Q_UNUSED(gbm.releaseBufferToAvailable(bufferId));
        Q_UNUSED(m_waylandCoordinator.completeFrame(false));
        ++m_failedCommitCount;
        emit frameFailed();
        return false;
    }

    m_flipInFlight = true;
    m_activeFlipBufferId = bufferId;
    m_eventDispatcher.armFlipTimeout(m_flipTimeoutMs, bufferId);
    return true;
}

void IndependentFrameLoop::onPageFlipCompleted(quint64, quint64) {
    if (!m_flipInFlight || m_activeFlipBufferId == 0) {
        // Evento duplicado ou fora de ordem: ignorar com segurança
        return;
    }

    const uint64_t bufferId = m_activeFlipBufferId;
    m_activeFlipBufferId = 0;
    m_flipInFlight = false;
    m_eventDispatcher.clearFlipTimeout();

    if (m_shuttingDown || m_suspended) {
        Q_UNUSED(m_presenter.bufferManager().releaseBufferToAvailable(bufferId));
        Q_UNUSED(m_waylandCoordinator.completeFrame(false));
        return;
    }

    const bool presented = m_presenter.onPageFlipCompleted(bufferId);
    Q_UNUSED(m_waylandCoordinator.completeFrame(presented));
    if (presented) {
        ++m_presentedCount;
        emit framePresented(static_cast<quint64>(m_presentedCount));
    } else {
        Q_UNUSED(m_presenter.bufferManager().releaseBufferToAvailable(bufferId));
        emit frameFailed();
    }

    if (m_renderPending && !m_suspended && !m_shuttingDown) {
        scheduleQueuedProcess();
    }
}

void IndependentFrameLoop::onPageFlipTimedOut(quint64 pendingToken) {
    if (!m_flipInFlight || m_activeFlipBufferId == 0 || pendingToken != m_activeFlipBufferId) {
        return;
    }

    const uint64_t bufferId = m_activeFlipBufferId;
    m_activeFlipBufferId = 0;
    m_flipInFlight = false;
    ++m_timedOutCount;

    Q_UNUSED(m_presenter.bufferManager().markBufferTimedOut(bufferId));
    Q_UNUSED(m_waylandCoordinator.completeFrame(false));
    emit frameFailed();

    if (m_renderPending && !m_suspended && !m_shuttingDown) {
        scheduleQueuedProcess();
    }
}

void IndependentFrameLoop::suspend() {
    m_suspended = true;
    m_renderPending = false;
    m_scheduled = false;
    m_eventDispatcher.detachFd();

    if (m_flipInFlight && m_activeFlipBufferId != 0) {
        Q_UNUSED(m_presenter.bufferManager().releaseBufferToAvailable(m_activeFlipBufferId));
        m_activeFlipBufferId = 0;
        m_flipInFlight = false;
    }

    m_waylandCoordinator.setSuspended(true);
    m_presenter.suspendPresentation();
}

void IndependentFrameLoop::resume(int drmFd) {
    if (m_shuttingDown || drmFd < 0) {
        return;
    }
    if (!m_presenter.resumePresentation(drmFd)) {
        return;
    }
    m_waylandCoordinator.setSuspended(false);
    m_eventDispatcher.attachFd(drmFd);
    m_suspended = false;
    requestFrame();
}

void IndependentFrameLoop::shutdown() {
    if (m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;
    suspend();
}

} // namespace Systalia::Platform::Drm
