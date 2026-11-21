#pragma once

#include <QString>
#include <cstdint>
#include <optional>
#include <utility>

namespace Systalia::Platform::Drm {

/**
 * @brief Estado do ciclo de vida do descritor DRM único.
 */
enum class DrmOwnershipState {
    Uninitialized,
    Acquired,
    Suspended,
    Released,
    Failed
};

/**
 * @brief Representa a concessão (lease) exclusiva de um único nó DRM obtido via sessão.
 *
 * Move-only para garantir propriedade única do descritor sem cópias acidentais
 * nem abertura paralela do dispositivo físico.
 */
struct DrmDeviceDescriptor {
    int fd = -1;
    uint32_t major = 0;
    uint32_t minor = 0;
    QString devicePath;

    DrmDeviceDescriptor() = default;
    DrmDeviceDescriptor(int rawFd, uint32_t devMajor, uint32_t devMinor, QString path)
        : fd(rawFd), major(devMajor), minor(devMinor), devicePath(std::move(path)) {}

    DrmDeviceDescriptor(const DrmDeviceDescriptor &) = delete;
    DrmDeviceDescriptor &operator=(const DrmDeviceDescriptor &) = delete;

    DrmDeviceDescriptor(DrmDeviceDescriptor &&other) noexcept
        : fd(std::exchange(other.fd, -1)),
          major(std::exchange(other.major, 0)),
          minor(std::exchange(other.minor, 0)),
          devicePath(std::move(other.devicePath)) {}

    DrmDeviceDescriptor &operator=(DrmDeviceDescriptor &&other) noexcept {
        if (this != &other) {
            fd = std::exchange(other.fd, -1);
            major = std::exchange(other.major, 0);
            minor = std::exchange(other.minor, 0);
            devicePath = std::move(other.devicePath);
        }
        return *this;
    }

    [[nodiscard]] bool isValid() const noexcept { return fd >= 0; }
};

/**
 * @brief Modo de saída KMS selecionado para apresentação.
 */
struct DrmOutputMode {
    QString connectorName;
    int width = 0;
    int height = 0;
    int refreshHz = 60;
    bool preferred = false;

    [[nodiscard]] bool isValid() const noexcept {
        return !connectorName.isEmpty() && width > 0 && height > 0 && refreshHz > 0;
    }
};

/**
 * @brief Abstração do provedor de sessão (ex.: systemd-logind ou libseat).
 *
 * Responsável exclusivamente por adquirir e liberar o descritor DRM único.
 */
class ISessionDrmProvider {
public:
    virtual ~ISessionDrmProvider() = default;

    [[nodiscard]] virtual bool isSessionActive() const = 0;
    [[nodiscard]] virtual std::optional<DrmDeviceDescriptor> acquireDrmDevice(const QString &devicePath) = 0;
    virtual bool releaseDrmDevice(uint32_t major, uint32_t minor, int fd) = 0;
};

/**
 * @brief Abstração de apresentação gráfica/KMS sobre o FD único cedido pela sessão.
 *
 * Na implementação de produção, encapsulará GBM + Vulkan (QQuickRenderControl) + Atomic KMS
 * sem nunca chamar open("/dev/dri/card*") diretamente.
 */
class IDrmPresenter {
public:
    virtual ~IDrmPresenter() = default;

    [[nodiscard]] virtual bool initializeOnFd(int drmFd) = 0;
    [[nodiscard]] virtual bool configureOutput(const DrmOutputMode &mode) = 0;
    [[nodiscard]] virtual bool presentFrame(uint64_t frameIndex, int drmFd) = 0;
    virtual void suspendPresentation() = 0;
    [[nodiscard]] virtual bool resumePresentation(int drmFd) = 0;
    virtual void shutdown() = 0;
};

} // namespace Systalia::Platform::Drm
