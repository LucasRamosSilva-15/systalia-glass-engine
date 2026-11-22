#include "KmsOutputDiscovery.hpp"

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cstring>

namespace Systalia::Platform::Drm {

namespace {

QString connectorTypeName(uint32_t type) {
    switch (type) {
    case DRM_MODE_CONNECTOR_VGA:
        return QStringLiteral("VGA");
    case DRM_MODE_CONNECTOR_DVII:
    case DRM_MODE_CONNECTOR_DVID:
    case DRM_MODE_CONNECTOR_DVIA:
        return QStringLiteral("DVI");
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
        return QStringLiteral("HDMI-A");
    case DRM_MODE_CONNECTOR_DisplayPort:
        return QStringLiteral("DP");
    case DRM_MODE_CONNECTOR_eDP:
        return QStringLiteral("eDP");
    case DRM_MODE_CONNECTOR_VIRTUAL:
        return QStringLiteral("Virtual");
    case DRM_MODE_CONNECTOR_DSI:
        return QStringLiteral("DSI");
    default:
        return QStringLiteral("Unknown");
    }
}

int modeRefreshHz(const drmModeModeInfo &mode) {
    if (mode.htotal == 0 || mode.vtotal == 0) {
        return 60;
    }
    const double vrefresh = (static_cast<double>(mode.clock) * 1000.0)
                            / (static_cast<double>(mode.htotal) * static_cast<double>(mode.vtotal));
    const int hz = static_cast<int>(vrefresh + 0.5);
    return hz > 0 ? hz : 60;
}

KmsPlaneType queryPlaneType(int drmFd, uint32_t planeId) {
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(drmFd, planeId, DRM_MODE_OBJECT_PLANE);
    if (!props) {
        return KmsPlaneType::Unknown;
    }

    KmsPlaneType result = KmsPlaneType::Unknown;
    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr prop = drmModeGetProperty(drmFd, props->props[i]);
        if (!prop) {
            continue;
        }
        if (std::strcmp(prop->name, "type") == 0) {
            switch (props->prop_values[i]) {
            case DRM_PLANE_TYPE_PRIMARY:
                result = KmsPlaneType::Primary;
                break;
            case DRM_PLANE_TYPE_OVERLAY:
                result = KmsPlaneType::Overlay;
                break;
            case DRM_PLANE_TYPE_CURSOR:
                result = KmsPlaneType::Cursor;
                break;
            default:
                break;
            }
            drmModeFreeProperty(prop);
            break;
        }
        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    return result;
}

QList<uint64_t> queryPlaneModifiersForFourcc(int drmFd, uint32_t planeId, uint32_t drmFourcc) {
    QList<uint64_t> mods;
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(drmFd, planeId, DRM_MODE_OBJECT_PLANE);
    if (!props) {
        return mods;
    }
    uint32_t blobId = 0;
    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr prop = drmModeGetProperty(drmFd, props->props[i]);
        if (!prop) {
            continue;
        }
        if (std::strcmp(prop->name, "IN_FORMATS") == 0 && (prop->flags & DRM_MODE_PROP_BLOB) != 0) {
            blobId = static_cast<uint32_t>(props->prop_values[i]);
            drmModeFreeProperty(prop);
            break;
        }
        drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);

    if (blobId == 0) {
        return mods;
    }
    drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(drmFd, blobId);
    if (!blob || !blob->data || blob->length < sizeof(drm_format_modifier_blob)) {
        if (blob) {
            drmModeFreePropertyBlob(blob);
        }
        return mods;
    }
    const auto *hdr = static_cast<const drm_format_modifier_blob *>(blob->data);
    const auto *formats = reinterpret_cast<const uint32_t *>(
        static_cast<const uint8_t *>(blob->data) + hdr->formats_offset);
    const auto *modEntries = reinterpret_cast<const drm_format_modifier *>(
        static_cast<const uint8_t *>(blob->data) + hdr->modifiers_offset);

    int xrgbIdx = -1;
    for (uint32_t i = 0; i < hdr->count_formats; ++i) {
        if (formats[i] == drmFourcc) {
            xrgbIdx = static_cast<int>(i);
            break;
        }
    }
    if (xrgbIdx >= 0) {
        for (uint32_t m = 0; m < hdr->count_modifiers; ++m) {
            const auto &entry = modEntries[m];
            if (xrgbIdx >= static_cast<int>(entry.offset)
                && xrgbIdx < static_cast<int>(entry.offset) + 64) {
                const uint64_t bit = 1ULL << static_cast<uint32_t>(xrgbIdx - static_cast<int>(entry.offset));
                if ((entry.formats & bit) != 0 && !mods.contains(entry.modifier)) {
                    mods.append(entry.modifier);
                }
            }
        }
    }
    drmModeFreePropertyBlob(blob);
    return mods;
}

} // namespace

QList<KmsConnectorInfo> KmsDiscoverySnapshot::connectedConnectors() const {
    QList<KmsConnectorInfo> out;
    for (const auto &conn : connectors) {
        if (conn.connected) {
            out.append(conn);
        }
    }
    return out;
}

const KmsConnectorInfo *KmsDiscoverySnapshot::primaryConnector() const {
    for (const auto &conn : connectors) {
        if (conn.connected) {
            return &conn;
        }
    }
    return nullptr;
}

const KmsPlaneInfo *KmsDiscoverySnapshot::primaryPlaneForCrtc(uint32_t crtcId) const {
    if (crtcId == 0) {
        return nullptr;
    }
    int crtcIndex = -1;
    for (int i = 0; i < crtcs.size(); ++i) {
        if (crtcs.at(i).crtcId == crtcId) {
            crtcIndex = i;
            break;
        }
    }
    if (crtcIndex < 0) {
        return nullptr;
    }
    const uint32_t bit = 1u << static_cast<uint32_t>(crtcIndex);
    for (const auto &plane : planes) {
        if (plane.type == KmsPlaneType::Primary && (plane.possibleCrtcs & bit) != 0) {
            return &plane;
        }
    }
    return nullptr;
}

KmsDiscoverySnapshot LibdrmResourceBackend::probeResources(int drmFd) {
    KmsDiscoverySnapshot snapshot;
    if (drmFd < 0) {
        return snapshot;
    }

    drmModeResPtr res = drmModeGetResources(drmFd);
    if (!res) {
        return snapshot;
    }

    snapshot.valid = true;
    snapshot.atomicSupported = (drmSetClientCap(drmFd, DRM_CLIENT_CAP_ATOMIC, 1) == 0);
    Q_UNUSED(drmSetClientCap(drmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1));

    for (int i = 0; i < res->count_crtcs; ++i) {
        const uint32_t crtcId = res->crtcs[i];
        KmsCrtcInfo crtcInfo;
        crtcInfo.crtcId = crtcId;
        crtcInfo.pipeIndex = static_cast<uint32_t>(i);
        if (drmModeCrtcPtr crtc = drmModeGetCrtc(drmFd, crtcId)) {
            crtcInfo.active = crtc->mode_valid != 0;
            crtcInfo.x = static_cast<int>(crtc->x);
            crtcInfo.y = static_cast<int>(crtc->y);
            crtcInfo.width = static_cast<int>(crtc->width);
            crtcInfo.height = static_cast<int>(crtc->height);
            drmModeFreeCrtc(crtc);
        }
        snapshot.crtcs.append(crtcInfo);
    }

    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnectorPtr conn = drmModeGetConnector(drmFd, res->connectors[i]);
        if (!conn) {
            continue;
        }

        KmsConnectorInfo info;
        info.connectorId = conn->connector_id;
        info.name = QStringLiteral("%1-%2")
                        .arg(connectorTypeName(conn->connector_type))
                        .arg(conn->connector_type_id);
        info.connected = (conn->connection == DRM_MODE_CONNECTED);
        info.currentEncoderId = conn->encoder_id;

        for (int e = 0; e < conn->count_encoders; ++e) {
            info.encoderIds.append(conn->encoders[e]);
            if (drmModeEncoderPtr enc = drmModeGetEncoder(drmFd, conn->encoders[e])) {
                info.possibleCrtcs |= enc->possible_crtcs;
                if (enc->encoder_id == conn->encoder_id) {
                    info.activeCrtcId = enc->crtc_id;
                }
                drmModeFreeEncoder(enc);
            }
        }

        for (int m = 0; m < conn->count_modes; ++m) {
            const drmModeModeInfo &rawMode = conn->modes[m];
            DrmOutputMode mode;
            mode.connectorName = info.name;
            mode.width = static_cast<int>(rawMode.hdisplay);
            mode.height = static_cast<int>(rawMode.vdisplay);
            mode.refreshHz = modeRefreshHz(rawMode);
            mode.preferred = (rawMode.type & DRM_MODE_TYPE_PREFERRED) != 0;
            info.modes.append(mode);
        }

        info.preferredMode = KmsOutputDiscovery::selectBestMode(info.modes);
        snapshot.connectors.append(info);
        drmModeFreeConnector(conn);
    }

    if (drmModePlaneResPtr planeRes = drmModeGetPlaneResources(drmFd)) {
        for (uint32_t i = 0; i < planeRes->count_planes; ++i) {
            drmModePlanePtr plane = drmModeGetPlane(drmFd, planeRes->planes[i]);
            if (!plane) {
                continue;
            }
            KmsPlaneInfo pInfo;
            pInfo.planeId = plane->plane_id;
            pInfo.possibleCrtcs = plane->possible_crtcs;
            pInfo.activeCrtcId = plane->crtc_id;
            pInfo.type = queryPlaneType(drmFd, plane->plane_id);
            for (uint32_t f = 0; f < plane->count_formats; ++f) {
                pInfo.formats.append(plane->formats[f]);
            }
            snapshot.planes.append(pInfo);
            drmModeFreePlane(plane);
        }
        drmModeFreePlaneResources(planeRes);
    }

    drmModeFreeResources(res);
    return snapshot;
}

KmsOutputDiscovery::KmsOutputDiscovery(IKmsResourceBackend &backend) noexcept
    : m_backend(backend) {}

KmsDiscoverySnapshot KmsOutputDiscovery::discover(int drmFd) {
    if (drmFd < 0) {
        return KmsDiscoverySnapshot{};
    }
    return m_backend.probeResources(drmFd);
}

QList<uint64_t> KmsOutputDiscovery::queryPlaneModifiers(int drmFd, uint32_t planeId, uint32_t drmFourcc) {
    if (drmFd < 0 || planeId == 0 || drmFourcc == 0) {
        return {};
    }
    return queryPlaneModifiersForFourcc(drmFd, planeId, drmFourcc);
}

DrmOutputMode KmsOutputDiscovery::selectBestMode(const QList<DrmOutputMode> &modes) {
    DrmOutputMode best;
    qint64 bestPixels = -1;

    for (const auto &mode : modes) {
        if (!mode.isValid()) {
            continue;
        }
        if (mode.preferred) {
            return mode;
        }
        const qint64 pixels = static_cast<qint64>(mode.width) * static_cast<qint64>(mode.height);
        if (pixels > bestPixels || (pixels == bestPixels && mode.refreshHz > best.refreshHz)) {
            best = mode;
            bestPixels = pixels;
        }
    }
    return best;
}

uint32_t KmsOutputDiscovery::selectCompatibleCrtc(const KmsConnectorInfo &connector,
                                                  const QList<KmsCrtcInfo> &crtcs) {
    if (connector.activeCrtcId != 0) {
        return connector.activeCrtcId;
    }
    for (int i = 0; i < crtcs.size(); ++i) {
        const uint32_t bit = 1u << static_cast<uint32_t>(i);
        if ((connector.possibleCrtcs & bit) != 0) {
            return crtcs.at(i).crtcId;
        }
    }
    return 0;
}

} // namespace Systalia::Platform::Drm
