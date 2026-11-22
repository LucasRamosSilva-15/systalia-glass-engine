#include "LogindDrmProvider.hpp"

#include "DrmBackendController.hpp"
#include "LogindSessionService.hpp"

namespace Systalia::Platform::Drm {

LogindDrmProvider::LogindDrmProvider(LogindSessionService &logind,
                                     DeviceStatFn statFn,
                                     QObject *parent)
    : QObject(parent),
      m_logind(logind),
      m_statFn(std::move(statFn)) {
    if (!m_statFn) {
        m_statFn = [](const QString &path, uint32_t *majorOut, uint32_t *minorOut) {
            quint32 maj = 0;
            quint32 min = 0;
            if (!LogindSessionService::statDeviceMajorMinor(path, &maj, &min)) {
                return false;
            }
            if (majorOut) {
                *majorOut = maj;
            }
            if (minorOut) {
                *minorOut = min;
            }
            return true;
        };
    }

    connect(&m_logind, &LogindSessionService::devicePaused, this,
            [this](quint32 major, quint32 minor, const QString &) {
        ++m_pausedSignalCount;
        if (m_controller) {
            m_controller->onDevicePaused(major, minor);
        }
    });

    connect(&m_logind, &LogindSessionService::deviceResumed, this,
            [this](quint32 major, quint32 minor, int fd) {
        ++m_resumedSignalCount;
        if (m_controller) {
            Q_UNUSED(m_controller->onDeviceResumed(major, minor, fd));
        }
    });
}

void LogindDrmProvider::bindController(DrmBackendController *controller) noexcept {
    m_controller = controller;
}

bool LogindDrmProvider::isSessionActive() const {
    return m_logind.hasSessionControl() && m_logind.isSessionActive();
}

std::optional<DrmDeviceDescriptor> LogindDrmProvider::acquireDrmDevice(const QString &devicePath) {
    if (!isSessionActive()) {
        return std::nullopt;
    }

    uint32_t major = 0;
    uint32_t minor = 0;
    if (!m_statFn(devicePath, &major, &minor)) {
        return std::nullopt;
    }

    bool inactive = false;
    const int fd = m_logind.takeDevice(major, minor, &inactive);
    if (fd < 0) {
        return std::nullopt;
    }

    if (inactive) {
        // Rollback imediato: não reter lease inativo nem fechar o FD manualmente
        m_logind.releaseDevice(major, minor);
        return std::nullopt;
    }

    return DrmDeviceDescriptor(fd, major, minor, devicePath, FdOwnership::BorrowedFromSession);
}

bool LogindDrmProvider::releaseDrmDevice(uint32_t major, uint32_t minor, int fd) {
    Q_UNUSED(fd);
    // LogindSessionService::releaseDevice fecha o descritor em m_devices de forma segura.
    return m_logind.releaseDevice(major, minor);
}

} // namespace Systalia::Platform::Drm
