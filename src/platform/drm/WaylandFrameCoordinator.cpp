#include "WaylandFrameCoordinator.hpp"

#include <QWaylandQuickOutput>

namespace Systalia::Platform::Drm {

QWaylandOutputFrameSink::QWaylandOutputFrameSink(QWaylandQuickOutput *output) noexcept
    : m_output(output) {
    if (m_output) {
        m_output->setAutomaticFrameCallback(false);
    }
}

void QWaylandOutputFrameSink::setAutomaticCallbacks(bool automatic) {
    if (m_output) {
        m_output->setAutomaticFrameCallback(automatic);
    }
}

void QWaylandOutputFrameSink::notifyFrameStarted() {
    if (m_output) {
        m_output->frameStarted();
    }
}

void QWaylandOutputFrameSink::notifySendFrameCallbacks() {
    if (m_output) {
        m_output->sendFrameCallbacks();
    }
}

WaylandFrameCoordinator::WaylandFrameCoordinator(IWaylandOutputFrameSink &sink) noexcept
    : m_sink(sink) {
    m_sink.setAutomaticCallbacks(false);
}

bool WaylandFrameCoordinator::beginFrame() {
    if (m_suspended || m_frameInFlight) {
        return false;
    }
    m_frameInFlight = true;
    m_sink.notifyFrameStarted();
    return true;
}

bool WaylandFrameCoordinator::completeFrame(bool presentedSuccessfully) {
    if (!m_frameInFlight) {
        return false;
    }
    m_frameInFlight = false;

    if (!presentedSuccessfully || m_suspended) {
        ++m_droppedFrames;
        return false;
    }

    m_sink.notifySendFrameCallbacks();
    ++m_completedFrames;
    return true;
}

void WaylandFrameCoordinator::setSuspended(bool suspended) noexcept {
    m_suspended = suspended;
    if (m_suspended) {
        m_frameInFlight = false;
    }
}

} // namespace Systalia::Platform::Drm
