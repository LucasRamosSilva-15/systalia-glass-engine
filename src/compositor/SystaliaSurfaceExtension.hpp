#pragma once

#include <QHash>
#include <QPointer>
#include <QVector4D>
#include <QtWaylandCompositor/QWaylandCompositorExtension>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtQml/qqmlregistration.h>
#include "qwayland-server-z_systalia_surface_v1.h"

class SystaliaSurfaceResource final : public QObject,
                                     public QtWaylandServer::z_systalia_surface_v1 {
    Q_OBJECT
    QML_NAMED_ELEMENT(SystaliaSurfaceMetadata)
    QML_UNCREATABLE("Created by the Systalia Wayland protocol")
    Q_PROPERTY(QWaylandSurface *surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(QVector4D radii READ radii NOTIFY radiiChanged)
    Q_PROPERTY(float slant READ slant NOTIFY slantChanged)
    Q_PROPERTY(float ior READ ior NOTIFY iorChanged)
    Q_PROPERTY(float dispersion READ dispersion NOTIFY dispersionChanged)
    Q_PROPERTY(float meniscusWidth READ meniscusWidth NOTIFY meniscusWidthChanged)
public:
    SystaliaSurfaceResource(QObject *parent, QWaylandSurface *surface,
                            wl_client *client, uint32_t id, int version);
    ~SystaliaSurfaceResource() override;
    QWaylandSurface *surface() const { return m_surface; }
    QVector4D radii() const { return m_radii; }
    float slant() const { return m_slant; }
    float ior() const { return m_ior; }
    float dispersion() const { return m_dispersion; }
    float meniscusWidth() const { return m_meniscusWidth; }
Q_SIGNALS:
    void surfaceChanged();
    void radiiChanged(const QVector4D &radii);
    void slantChanged(float slant);
    void iorChanged(float ior);
    void dispersionChanged(float dispersion);
    void meniscusWidthChanged(float meniscusWidth);
    void opticalPropertiesChanged(float ior, float dispersion, float meniscusWidth);
    void invalidated(QWaylandSurface *surface);
protected:
    void z_systalia_surface_v1_destroy(Resource *resource) override;
    void z_systalia_surface_v1_destroy_resource(Resource *resource) override;
    void z_systalia_surface_v1_set_corner_radii(Resource *resource, wl_fixed_t tl,
                                              wl_fixed_t tr, wl_fixed_t bl, wl_fixed_t br) override;
    void z_systalia_surface_v1_set_slant(Resource *resource, wl_fixed_t slant) override;
    void z_systalia_surface_v1_set_optical_properties(Resource *resource, wl_fixed_t ior,
                                                    wl_fixed_t dispersion, wl_fixed_t width) override;
private:
    bool hasLiveSurface(Resource *resource) const;
    void invalidate(QWaylandSurface *surface);
    void updateOptics(float ior, float dispersion, float width);
    QPointer<QWaylandSurface> m_surface;
    QVector4D m_radii{18, 18, 18, 18};
    float m_slant = 0.0f;
    float m_ior = 1.25f;
    float m_dispersion = 0.040f;
    float m_meniscusWidth = 28.0f;
    wl_resource *m_handle = nullptr;
    bool m_destroying = false;
    bool m_invalidated = false;
};

class SystaliaSurfaceExtension final
    : public QWaylandCompositorExtensionTemplate<SystaliaSurfaceExtension>,
      public QtWaylandServer::z_systalia_manager_v1 {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int surfaceCount READ surfaceCount NOTIFY surfacesChanged)
public:
    explicit SystaliaSurfaceExtension(QWaylandCompositor *compositor);
    ~SystaliaSurfaceExtension() override;
    void initialize() override;
    static const wl_interface *interface() { return QtWaylandServer::z_systalia_manager_v1::interface(); }
    static QByteArray interfaceName() { return QByteArrayLiteral("z_systalia_manager_v1"); }
    int surfaceCount() const { return static_cast<int>(m_surfaces.size()); }
    Q_INVOKABLE SystaliaSurfaceResource *resourceForSurface(QWaylandSurface *surface) const;
Q_SIGNALS:
    void surfacesChanged();
protected:
    void z_systalia_manager_v1_destroy(Resource *resource) override;
    void z_systalia_manager_v1_get_systalia_surface(Resource *resource, uint32_t id,
                                                   wl_resource *surface) override;
private:
    void removeSurface(QWaylandSurface *surface, SystaliaSurfaceResource *resource);
    QHash<QWaylandSurface *, SystaliaSurfaceResource *> m_surfaces;
};
