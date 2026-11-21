#pragma once

#include "DrmSessionTypes.hpp"

#include <QString>
#include <cstdint>

namespace Systalia::Platform::Drm {

/**
 * @brief Controlador isolado de ciclo de vida DRM com proprietário único.
 *
 * Coordena aquisição via ISessionDrmProvider e apresentação via IDrmPresenter,
 * garantindo que nenhum componente gráfico abra /dev/dri/card* por conta própria
 * e que nenhuma renderização ocorra com descritor suspenso ou liberado.
 */
class DrmBackendController final {
public:
    DrmBackendController(ISessionDrmProvider &sessionProvider, IDrmPresenter &presenter) noexcept;
    ~DrmBackendController();

    DrmBackendController(const DrmBackendController &) = delete;
    DrmBackendController &operator=(const DrmBackendController &) = delete;

    [[nodiscard]] bool initialize(const QString &devicePath, const DrmOutputMode &initialMode);
    [[nodiscard]] bool renderAndPresentFrame();
    void onDevicePaused(uint32_t major, uint32_t minor);
    [[nodiscard]] bool onDeviceResumed(uint32_t major, uint32_t minor, int resumedFd = -1);
    void shutdown();

    [[nodiscard]] DrmOwnershipState state() const noexcept { return m_state; }
    [[nodiscard]] int activeFd() const noexcept { return m_descriptor.fd; }
    [[nodiscard]] uint32_t activeMajor() const noexcept { return m_descriptor.major; }
    [[nodiscard]] uint32_t activeMinor() const noexcept { return m_descriptor.minor; }
    [[nodiscard]] uint64_t presentedFrames() const noexcept { return m_presentedFrames; }
    [[nodiscard]] bool isPresentationActive() const noexcept {
        return m_state == DrmOwnershipState::Acquired && m_presenterInitialized;
    }

private:
    bool matchesActiveDevice(uint32_t major, uint32_t minor) const noexcept;
    void releaseDescriptorIfHeld();

    ISessionDrmProvider &m_sessionProvider;
    IDrmPresenter &m_presenter;
    DrmDeviceDescriptor m_descriptor;
    DrmOwnershipState m_state = DrmOwnershipState::Uninitialized;
    bool m_presenterInitialized = false;
    uint64_t m_presentedFrames = 0;
};

} // namespace Systalia::Platform::Drm
