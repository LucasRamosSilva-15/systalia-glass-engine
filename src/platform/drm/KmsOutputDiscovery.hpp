#pragma once

#include "DrmSessionTypes.hpp"

#include <QList>
#include <QString>
#include <cstdint>
#include <memory>

namespace Systalia::Platform::Drm {

enum class KmsPlaneType {
    Unknown,
    Primary,
    Overlay,
    Cursor
};

struct KmsPlaneInfo {
    uint32_t planeId = 0;
    uint32_t possibleCrtcs = 0;
    uint32_t activeCrtcId = 0;
    KmsPlaneType type = KmsPlaneType::Unknown;
    QList<uint32_t> formats;
};

struct KmsCrtcInfo {
    uint32_t crtcId = 0;
    uint32_t pipeIndex = 0;
    bool active = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct KmsConnectorInfo {
    uint32_t connectorId = 0;
    QString name;
    bool connected = false;
    uint32_t currentEncoderId = 0;
    QList<uint32_t> encoderIds;
    uint32_t possibleCrtcs = 0;
    uint32_t activeCrtcId = 0;
    QList<DrmOutputMode> modes;
    DrmOutputMode preferredMode;
};

struct KmsDiscoverySnapshot {
    bool valid = false;
    bool atomicSupported = false;
    QList<KmsConnectorInfo> connectors;
    QList<KmsCrtcInfo> crtcs;
    QList<KmsPlaneInfo> planes;

    [[nodiscard]] QList<KmsConnectorInfo> connectedConnectors() const;
    [[nodiscard]] const KmsConnectorInfo *primaryConnector() const;
    [[nodiscard]] const KmsPlaneInfo *primaryPlaneForCrtc(uint32_t crtcId) const;
};

/**
 * @brief Interface de leitura KMS sobre um FD já adquirido (ou mock determinístico).
 */
class IKmsResourceBackend {
public:
    virtual ~IKmsResourceBackend() = default;

    [[nodiscard]] virtual KmsDiscoverySnapshot probeResources(int drmFd) = 0;
};

/**
 * @brief Implementação real sobre libdrm (xf86drm / xf86drmMode) sem abrir /dev/dri/.
 */
class LibdrmResourceBackend final : public IKmsResourceBackend {
public:
    [[nodiscard]] KmsDiscoverySnapshot probeResources(int drmFd) override;
};

/**
 * @brief Descobridor de outputs KMS reutilizável para single e multi-monitor.
 */
class KmsOutputDiscovery final {
public:
    explicit KmsOutputDiscovery(IKmsResourceBackend &backend) noexcept;

    [[nodiscard]] KmsDiscoverySnapshot discover(int drmFd);
    [[nodiscard]] static DrmOutputMode selectBestMode(const QList<DrmOutputMode> &modes);
    [[nodiscard]] static uint32_t selectCompatibleCrtc(const KmsConnectorInfo &connector,
                                                         const QList<KmsCrtcInfo> &crtcs);
    [[nodiscard]] static QList<uint64_t> queryPlaneModifiers(int drmFd, uint32_t planeId, uint32_t drmFourcc);

private:
    IKmsResourceBackend &m_backend;
};

} // namespace Systalia::Platform::Drm
