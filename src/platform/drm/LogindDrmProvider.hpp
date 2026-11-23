#pragma once

#include "DrmSessionTypes.hpp"

#include <QObject>
#include <functional>

class LogindSessionService;

namespace Systalia::Platform::Drm {

class DrmBackendController;

/**
 * @brief Adaptador real de ISessionDrmProvider sobre LogindSessionService.
 *
 * Nunca chama open("/dev/dri/card*") diretamente: toda aquisição passa por
 * LogindSessionService::takeDevice(), preservando propriedade única do FD.
 */
class LogindDrmProvider final : public QObject, public ISessionDrmProvider {
    Q_OBJECT

public:
    using DeviceStatFn = std::function<bool(const QString &path, uint32_t *majorOut, uint32_t *minorOut)>;

    explicit LogindDrmProvider(LogindSessionService &logind,
                               DeviceStatFn statFn = {},
                               QObject *parent = nullptr);
    ~LogindDrmProvider() override = default;

    void bindController(DrmBackendController *controller) noexcept;

    [[nodiscard]] bool isSessionActive() const override;
    [[nodiscard]] std::optional<DrmDeviceDescriptor> acquireDrmDevice(const QString &devicePath) override;
    bool releaseDrmDevice(uint32_t major, uint32_t minor, int fd) override;

    [[nodiscard]] int pausedSignalCount() const noexcept { return m_pausedSignalCount; }
    [[nodiscard]] int resumedSignalCount() const noexcept { return m_resumedSignalCount; }

private:
    LogindSessionService &m_logind;
    DeviceStatFn m_statFn;
    DrmBackendController *m_controller = nullptr;
    int m_pausedSignalCount = 0;
    int m_resumedSignalCount = 0;
};

} // namespace Systalia::Platform::Drm
