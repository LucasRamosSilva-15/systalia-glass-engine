#include "SystaliaSurfaceExtension.hpp"

#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <wayland-server-core.h>
#include <utility>

SystaliaSurfaceResource::SystaliaSurfaceResource(QObject *parent, QWaylandSurface *surface,
                                               wl_client *client, uint32_t id, int version)
    : QObject(parent), QtWaylandServer::z_systalia_surface_v1(client, id, version),
      m_surface(surface), m_handle(resource()->handle) {
    connect(surface, &QWaylandSurface::surfaceDestroyed, this, [this, surface] { invalidate(surface); });
    connect(surface, &QObject::destroyed, this, [this, surface] { invalidate(surface); });
}

SystaliaSurfaceResource::~SystaliaSurfaceResource() {
    m_destroying = true;
    if (auto *handle = std::exchange(m_handle, nullptr)) {
        wl_resource_destroy(handle);
    }
}

void SystaliaSurfaceResource::z_systalia_surface_v1_destroy(Resource *resource) {
    wl_resource_destroy(resource->handle);
}

void SystaliaSurfaceResource::z_systalia_surface_v1_destroy_resource(Resource *) {
    m_handle = nullptr;
    if (!m_destroying) {
        delete this;
    }
}

bool SystaliaSurfaceResource::hasLiveSurface(Resource *resource) const {
    if (m_surface && !m_surface->isDestroyed()) {
        return true;
    }
    wl_resource_post_error(resource->handle, error_invalid_surface, "wl_surface is no longer alive");
    return false;
}

void SystaliaSurfaceResource::invalidate(QWaylandSurface *surface) {
    if (m_invalidated) return;
    m_invalidated = true;
    m_surface.clear();
    if (surface) {
        emit invalidated(surface);
    }
    if (m_radii != QVector4D(18, 18, 18, 18)) {
        m_radii = QVector4D(18, 18, 18, 18);
        emit radiiChanged(m_radii);
    }
    if (m_slant != 0.0f) {
        m_slant = 0.0f;
        emit slantChanged(m_slant);
    }
    updateOptics(1.25f, 0.040f, 28.0f);
    emit surfaceChanged();
}

void SystaliaSurfaceResource::z_systalia_surface_v1_set_corner_radii(
    Resource *resource, wl_fixed_t tl, wl_fixed_t tr, wl_fixed_t bl, wl_fixed_t br) {
    if (!hasLiveSurface(resource)) return;
    if (tl < 0 || tr < 0 || bl < 0 || br < 0) {
        wl_resource_post_error(resource->handle, error_invalid_value, "radii must be nonnegative");
        return;
    }
    const QVector4D radii(static_cast<float>(wl_fixed_to_double(tl)),
                          static_cast<float>(wl_fixed_to_double(tr)),
                          static_cast<float>(wl_fixed_to_double(bl)),
                          static_cast<float>(wl_fixed_to_double(br)));
    if (m_radii != radii) {
        m_radii = radii;
        emit radiiChanged(m_radii);
    }
}

void SystaliaSurfaceResource::z_systalia_surface_v1_set_slant(Resource *resource, wl_fixed_t slant) {
    if (!hasLiveSurface(resource)) return;
    const float value = static_cast<float>(wl_fixed_to_double(slant));
    if (value < -1.0f || value > 1.0f) {
        wl_resource_post_error(resource->handle, error_invalid_value, "slant must be in [-1,1]");
        return;
    }
    if (m_slant != value) {
        m_slant = value;
        emit slantChanged(value);
    }
}

void SystaliaSurfaceResource::updateOptics(float ior, float dispersion, float width) {
    if (m_ior == ior && m_dispersion == dispersion && m_meniscusWidth == width) return;
    const bool iorChangedValue = m_ior != ior;
    const bool dispersionChangedValue = m_dispersion != dispersion;
    const bool widthChangedValue = m_meniscusWidth != width;
    m_ior = ior;
    m_dispersion = dispersion;
    m_meniscusWidth = width;
    if (iorChangedValue) emit iorChanged(ior);
    if (dispersionChangedValue) emit dispersionChanged(dispersion);
    if (widthChangedValue) emit meniscusWidthChanged(width);
    emit opticalPropertiesChanged(ior, dispersion, width);
}

void SystaliaSurfaceResource::z_systalia_surface_v1_set_optical_properties(
    Resource *resource, wl_fixed_t ior, wl_fixed_t dispersion, wl_fixed_t width) {
    if (!hasLiveSurface(resource)) return;
    const float iorValue = static_cast<float>(wl_fixed_to_double(ior));
    const float dispersionValue = static_cast<float>(wl_fixed_to_double(dispersion));
    const float widthValue = static_cast<float>(wl_fixed_to_double(width));
    if (iorValue < 1.0f || iorValue > 3.0f || dispersionValue < 0.0f
        || dispersionValue > 1.0f || widthValue <= 0.0f) {
        wl_resource_post_error(resource->handle, error_invalid_value, "invalid optical properties");
        return;
    }
    updateOptics(iorValue, dispersionValue, widthValue);
}

SystaliaSurfaceExtension::SystaliaSurfaceExtension(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate<SystaliaSurfaceExtension>(compositor) {
    setParent(compositor);
}

SystaliaSurfaceExtension::~SystaliaSurfaceExtension() {
    // Tear down protocol objects while the registry and its signal handlers still exist.
    const auto resources = findChildren<SystaliaSurfaceResource *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *resource : resources) {
        disconnect(resource, nullptr, this, nullptr);
        delete resource;
    }
    m_surfaces.clear();
    const auto managers = resourceMap();
    for (auto *resource : managers) {
        wl_resource_destroy(resource->handle);
    }
}

void SystaliaSurfaceExtension::initialize() {
    if (isInitialized()) return;
    auto *compositor = qobject_cast<QWaylandCompositor *>(extensionContainer());
    if (!compositor || !compositor->isCreated()) return;
    QWaylandCompositorExtensionTemplate<SystaliaSurfaceExtension>::initialize();
    QtWaylandServer::z_systalia_manager_v1::init(compositor->display(), 1);
}

SystaliaSurfaceResource *SystaliaSurfaceExtension::resourceForSurface(QWaylandSurface *surface) const {
    return m_surfaces.value(surface, nullptr);
}

void SystaliaSurfaceExtension::removeSurface(QWaylandSurface *surface, SystaliaSurfaceResource *resource) {
    if (m_surfaces.value(surface) == resource) {
        m_surfaces.remove(surface);
        emit surfacesChanged();
    }
}

void SystaliaSurfaceExtension::z_systalia_manager_v1_destroy(Resource *resource) {
    wl_resource_destroy(resource->handle);
}

void SystaliaSurfaceExtension::z_systalia_manager_v1_get_systalia_surface(
    Resource *resource, uint32_t id, wl_resource *surfaceResource) {
    auto *surface = QWaylandSurface::fromResource(surfaceResource);
    auto *compositor = qobject_cast<QWaylandCompositor *>(extensionContainer());
    if (!surface || surface->isDestroyed() || surface->compositor() != compositor
        || surface->waylandClient() != resource->client()) {
        wl_resource_post_error(resource->handle, error_invalid_surface, "invalid wl_surface");
        return;
    }
    if (m_surfaces.contains(surface)) {
        wl_resource_post_error(resource->handle, error_already_exists, "surface already has optical metadata");
        return;
    }
    auto *metadata = new SystaliaSurfaceResource(this, surface, resource->client(), id, resource->version());
    m_surfaces.insert(surface, metadata);
    connect(metadata, &SystaliaSurfaceResource::invalidated, this,
            [this, metadata](QWaylandSurface *removed) { removeSurface(removed, metadata); });
    connect(metadata, &QObject::destroyed, this,
            [this, surface, metadata] { removeSurface(surface, metadata); });
    emit surfacesChanged();
}
