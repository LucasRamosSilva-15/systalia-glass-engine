#include "DrmBackendController.hpp"

#include <utility>

namespace Systalia::Platform::Drm {

DrmBackendController::DrmBackendController(ISessionDrmProvider &sessionProvider,
                                           IDrmPresenter &presenter) noexcept
    : m_sessionProvider(sessionProvider),
      m_presenter(presenter) {}

DrmBackendController::~DrmBackendController() {
    shutdown();
}

bool DrmBackendController::initialize(const QString &devicePath, const DrmOutputMode &initialMode) {
    if (m_state == DrmOwnershipState::Acquired || m_state == DrmOwnershipState::Suspended) {
        return false;
    }

    if (devicePath.trimmed().isEmpty() || !initialMode.isValid() || !m_sessionProvider.isSessionActive()) {
        m_state = DrmOwnershipState::Failed;
        return false;
    }

    auto acquired = m_sessionProvider.acquireDrmDevice(devicePath);
    if (!acquired.has_value() || !acquired->isValid()) {
        m_state = DrmOwnershipState::Failed;
        return false;
    }

    m_descriptor = std::move(*acquired);

    if (!m_presenter.initializeOnFd(m_descriptor.fd) || !m_presenter.configureOutput(initialMode)) {
        m_presenter.shutdown();
        releaseDescriptorIfHeld();
        m_state = DrmOwnershipState::Failed;
        return false;
    }

    m_presenterInitialized = true;
    m_state = DrmOwnershipState::Acquired;
    return true;
}

bool DrmBackendController::renderAndPresentFrame() {
    if (m_state != DrmOwnershipState::Acquired || !m_presenterInitialized || !m_descriptor.isValid()) {
        return false;
    }

    const uint64_t nextFrame = m_presentedFrames + 1;
    if (!m_presenter.presentFrame(nextFrame, m_descriptor.fd)) {
        return false;
    }

    m_presentedFrames = nextFrame;
    return true;
}

void DrmBackendController::onDevicePaused(uint32_t major, uint32_t minor) {
    if (!matchesActiveDevice(major, minor) || m_state != DrmOwnershipState::Acquired) {
        return;
    }

    m_presenter.suspendPresentation();
    m_state = DrmOwnershipState::Suspended;
}

bool DrmBackendController::onDeviceResumed(uint32_t major, uint32_t minor, int resumedFd) {
    if (!matchesActiveDevice(major, minor) || m_state != DrmOwnershipState::Suspended) {
        return false;
    }

    if (resumedFd >= 0) {
        m_descriptor.fd = resumedFd;
    }

    if (!m_descriptor.isValid() || !m_presenter.resumePresentation(m_descriptor.fd)) {
        return false;
    }

    m_state = DrmOwnershipState::Acquired;
    return true;
}

void DrmBackendController::shutdown() {
    if (m_presenterInitialized) {
        m_presenter.shutdown();
        m_presenterInitialized = false;
    }

    if (m_descriptor.isValid()) {
        releaseDescriptorIfHeld();
        m_state = DrmOwnershipState::Released;
    } else if (m_state == DrmOwnershipState::Acquired || m_state == DrmOwnershipState::Suspended) {
        m_state = DrmOwnershipState::Released;
    }
}

bool DrmBackendController::matchesActiveDevice(uint32_t major, uint32_t minor) const noexcept {
    return m_descriptor.isValid()
           && m_descriptor.majorNumber == major
           && m_descriptor.minorNumber == minor;
}

void DrmBackendController::releaseDescriptorIfHeld() {
    if (!m_descriptor.isValid()) {
        return;
    }

    const uint32_t devMajor = m_descriptor.majorNumber;
    const uint32_t devMinor = m_descriptor.minorNumber;
    const int fd = m_descriptor.fd;
    m_descriptor = DrmDeviceDescriptor{};
    m_sessionProvider.releaseDrmDevice(devMajor, devMinor, fd);
}

} // namespace Systalia::Platform::Drm
