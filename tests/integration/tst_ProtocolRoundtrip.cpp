#include <QElapsedTimer>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtWaylandCompositor/QWaylandClient>
#include <wayland-client.h>
#include <wayland-server-core.h>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

#include "SystaliaCompositor.hpp"
#include "wayland-z_systalia_surface_v1-client-protocol.h"

class WireClient final {
public:
    explicit WireClient(SystaliaCompositor *compositor) : m_compositor(compositor) {}
    ~WireClient() { disconnect(); }

    bool connectToServer() {
        int sockets[2]{};
        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0)
            return false;

        wl_client *wlClient = wl_client_create(m_compositor->display(), sockets[0]);
        if (!wlClient) {
            ::close(sockets[0]);
            ::close(sockets[1]);
            return false;
        }
        m_serverClient = QWaylandClient::fromWlClient(m_compositor, wlClient);
        m_display = wl_display_connect_to_fd(sockets[1]);
        if (!m_display) {
            ::close(sockets[1]);
            m_serverClient->close();
            return false;
        }

        m_registry = track(wl_display_get_registry(m_display));
        static const wl_registry_listener listener{handleGlobal, handleGlobalRemoved};
        wl_registry_add_listener(m_registry, &listener, this);
        return roundtrip() && roundtrip() && m_core && m_manager;
    }

    bool roundtrip() {
        if (!m_display)
            return false;

        bool done = false;
        wl_callback *callback = wl_display_sync(m_display);
        static const wl_callback_listener listener{
            [](void *data, wl_callback *, uint32_t) { *static_cast<bool *>(data) = true; }
        };
        wl_callback_add_listener(callback, &listener, &done);

        QElapsedTimer timer;
        timer.start();
        while (!done && timer.elapsed() < 3000 && !wl_display_get_error(m_display)) {
            if (wl_display_flush(m_display) < 0 && errno != EAGAIN)
                break;
            m_compositor->processWaylandEvents();
            wl_display_flush_clients(m_compositor->display());
            pollfd descriptor{wl_display_get_fd(m_display), POLLIN, 0};
            if (::poll(&descriptor, 1, 0) > 0
                && (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
                if (wl_display_dispatch(m_display) < 0)
                    break;
            }
            QCoreApplication::processEvents();
        }

        wl_callback_destroy(callback);
        return done && wl_display_get_error(m_display) == 0;
    }

    wl_surface *createSurface() { return track(wl_compositor_create_surface(m_core)); }

    bool hasManagerGlobal() const { return m_manager != nullptr; }
    int managerGlobalCount() const { return m_managerGlobals; }

    z_systalia_surface_v1 *createSurfaceMetadata(wl_surface *surface) {
        return track(z_systalia_manager_v1_get_systalia_surface(m_manager, surface));
    }

    SystaliaSurfaceResource *serverMetadata(wl_surface *surface) const {
        const uint32_t id = wl_proxy_get_id(reinterpret_cast<wl_proxy *>(surface));
        QWaylandSurface *serverSurface = QWaylandSurface::fromResource(
            wl_client_get_object(m_serverClient->client(), id));
        return m_compositor->surfaceExtension()->resourceForSurface(serverSurface);
    }

private:
    template<typename Proxy>
    Proxy *track(Proxy *proxy) {
        m_proxies.append(reinterpret_cast<wl_proxy *>(proxy));
        return proxy;
    }

    void disconnect() {
        if (!m_display)
            return;
        for (wl_proxy *proxy : std::as_const(m_proxies))
            wl_proxy_destroy(proxy);
        m_proxies.clear();
        wl_display_disconnect(std::exchange(m_display, nullptr));
        m_compositor->processWaylandEvents();
        QCoreApplication::processEvents();
    }

    static void handleGlobal(void *data, wl_registry *registry, uint32_t name,
                             const char *interface, uint32_t version) {
        auto *self = static_cast<WireClient *>(data);
        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            self->m_core = self->track(static_cast<wl_compositor *>(
                wl_registry_bind(registry, name, &wl_compositor_interface, qMin(version, 4u))));
        } else if (std::strcmp(interface, z_systalia_manager_v1_interface.name) == 0) {
            ++self->m_managerGlobals;
            self->m_manager = self->track(static_cast<z_systalia_manager_v1 *>(
                wl_registry_bind(registry, name, &z_systalia_manager_v1_interface, 1)));
        }
    }

    static void handleGlobalRemoved(void *, wl_registry *, uint32_t) {}

    SystaliaCompositor *m_compositor = nullptr;
    QPointer<QWaylandClient> m_serverClient;
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_compositor *m_core = nullptr;
    z_systalia_manager_v1 *m_manager = nullptr;
    int m_managerGlobals = 0;
    QList<wl_proxy *> m_proxies;
};

class TestProtocolRoundtrip final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void advertisesOneManagerGlobal();
    void cornerRadiiRoundtripEmitsItsSignal();
    void slantRoundtripEmitsItsSignal();
    void opticalTupleRoundtripEmitsOneSignal();
    void repeatedRadiiDoNotEmitDuplicateChanges();

private:
    QTemporaryDir m_runtime;
    std::unique_ptr<SystaliaCompositor> m_compositor;
};

void TestProtocolRoundtrip::initTestCase() {
    QVERIFY(m_runtime.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
}

void TestProtocolRoundtrip::init() {
    m_compositor = std::make_unique<SystaliaCompositor>();
    m_compositor->setSocketName(QByteArrayLiteral("wayland-protocol-roundtrip"));
    m_compositor->create();
    QVERIFY(m_compositor->isCreated());
}

void TestProtocolRoundtrip::cleanup() {
    m_compositor.reset();
}

void TestProtocolRoundtrip::advertisesOneManagerGlobal() {
    WireClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    QVERIFY(client.hasManagerGlobal());
    QCOMPARE(client.managerGlobalCount(), 1);
}

void TestProtocolRoundtrip::cornerRadiiRoundtripEmitsItsSignal() {
    WireClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    wl_surface *surface = client.createSurface();
    z_systalia_surface_v1 *metadataProxy = client.createSurfaceMetadata(surface);
    QVERIFY(surface);
    QVERIFY(metadataProxy);
    QVERIFY(client.roundtrip());

    SystaliaSurfaceResource *metadata = client.serverMetadata(surface);
    QVERIFY(metadata);
    QSignalSpy radiiChanged(metadata, &SystaliaSurfaceResource::radiiChanged);
    QVERIFY(radiiChanged.isValid());

    const wl_fixed_t tl = wl_fixed_from_double(28.0);
    const wl_fixed_t tr = wl_fixed_from_double(24.0);
    const wl_fixed_t bl = wl_fixed_from_double(20.0);
    const wl_fixed_t br = wl_fixed_from_double(16.0);
    z_systalia_surface_v1_set_corner_radii(metadataProxy, tl, tr, bl, br);
    QVERIFY(client.roundtrip());

    QCOMPARE(metadata->radii(), QVector4D(28.0f, 24.0f, 20.0f, 16.0f));
    QCOMPARE(radiiChanged.count(), 1);
    QCOMPARE(radiiChanged.at(0).at(0).value<QVector4D>(), metadata->radii());
}

void TestProtocolRoundtrip::slantRoundtripEmitsItsSignal() {
    WireClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    wl_surface *surface = client.createSurface();
    z_systalia_surface_v1 *metadataProxy = client.createSurfaceMetadata(surface);
    QVERIFY(client.roundtrip());
    SystaliaSurfaceResource *metadata = client.serverMetadata(surface);
    QVERIFY(metadata);
    QSignalSpy slantChanged(metadata, &SystaliaSurfaceResource::slantChanged);
    QVERIFY(slantChanged.isValid());

    const wl_fixed_t requested = wl_fixed_from_double(0.25);
    z_systalia_surface_v1_set_slant(metadataProxy, requested);
    QVERIFY(client.roundtrip());
    const float expected = static_cast<float>(wl_fixed_to_double(requested));
    QCOMPARE(metadata->slant(), expected);
    QCOMPARE(slantChanged.count(), 1);
    QCOMPARE(slantChanged.at(0).at(0).toFloat(), expected);
}

void TestProtocolRoundtrip::opticalTupleRoundtripEmitsOneSignal() {
    WireClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    wl_surface *surface = client.createSurface();
    z_systalia_surface_v1 *metadataProxy = client.createSurfaceMetadata(surface);
    QVERIFY(client.roundtrip());
    SystaliaSurfaceResource *metadata = client.serverMetadata(surface);
    QVERIFY(metadata);
    QSignalSpy opticalChanged(metadata, &SystaliaSurfaceResource::opticalPropertiesChanged);
    QVERIFY(opticalChanged.isValid());

    const wl_fixed_t ior = wl_fixed_from_double(1.33);
    const wl_fixed_t dispersion = wl_fixed_from_double(0.04);
    const wl_fixed_t meniscusWidth = wl_fixed_from_double(30.0);
    z_systalia_surface_v1_set_optical_properties(metadataProxy, ior, dispersion, meniscusWidth);
    QVERIFY(client.roundtrip());
    const float expectedIor = static_cast<float>(wl_fixed_to_double(ior));
    const float expectedDispersion = static_cast<float>(wl_fixed_to_double(dispersion));
    const float expectedWidth = static_cast<float>(wl_fixed_to_double(meniscusWidth));
    QCOMPARE(metadata->ior(), expectedIor);
    QCOMPARE(metadata->dispersion(), expectedDispersion);
    QCOMPARE(metadata->meniscusWidth(), expectedWidth);
    QCOMPARE(opticalChanged.count(), 1);
    QCOMPARE(opticalChanged.at(0).at(0).toFloat(), expectedIor);
    QCOMPARE(opticalChanged.at(0).at(1).toFloat(), expectedDispersion);
    QCOMPARE(opticalChanged.at(0).at(2).toFloat(), expectedWidth);
}

void TestProtocolRoundtrip::repeatedRadiiDoNotEmitDuplicateChanges() {
    WireClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    wl_surface *surface = client.createSurface();
    z_systalia_surface_v1 *metadataProxy = client.createSurfaceMetadata(surface);
    QVERIFY(client.roundtrip());
    SystaliaSurfaceResource *metadata = client.serverMetadata(surface);
    QVERIFY(metadata);
    QSignalSpy radiiChanged(metadata, &SystaliaSurfaceResource::radiiChanged);
    QVERIFY(radiiChanged.isValid());

    z_systalia_surface_v1_set_corner_radii(metadataProxy,
        wl_fixed_from_double(28.0), wl_fixed_from_double(24.0),
        wl_fixed_from_double(20.0), wl_fixed_from_double(16.0));
    QVERIFY(client.roundtrip());
    z_systalia_surface_v1_set_corner_radii(metadataProxy,
        wl_fixed_from_double(28.0), wl_fixed_from_double(24.0),
        wl_fixed_from_double(20.0), wl_fixed_from_double(16.0));
    QVERIFY(client.roundtrip());
    QCOMPARE(radiiChanged.count(), 1);
}

QTEST_GUILESS_MAIN(TestProtocolRoundtrip)
#include "tst_ProtocolRoundtrip.moc"
