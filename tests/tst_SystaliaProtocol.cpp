#include <QElapsedTimer>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtWaylandCompositor/QWaylandClient>
#include <wayland-client.h>
#include <wayland-server-core.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <utility>

#include "SystaliaCompositor.hpp"
#include "SystaliaGlassItem.hpp"
#include "wayland-z_systalia_surface_v1-client-protocol.h"

// A real wire client connected to QtWayland's wl_client through a local socketpair.
// No private Qt headers or server request handlers are called directly.
class MockClient {
public:
    explicit MockClient(SystaliaCompositor *compositor) : compositor(compositor) {}
    ~MockClient() { disconnect(); }
    bool connectToServer() {
        int sockets[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0) return false;
        auto *client = wl_client_create(compositor->display(), sockets[0]);
        if (!client) { close(sockets[0]); close(sockets[1]); return false; }
        server = QWaylandClient::fromWlClient(compositor, client);
        display = wl_display_connect_to_fd(sockets[1]);
        if (!display) { close(sockets[1]); server->close(); return false; }
        registry = track(wl_display_get_registry(display));
        static const wl_registry_listener listener{global, globalRemoved};
        wl_registry_add_listener(registry, &listener, this);
        return roundtrip() && roundtrip() && manager && core;
    }
    bool roundtrip() {
        if (!display) return false;
        bool done = false;
        auto *callback = wl_display_sync(display);
        static const wl_callback_listener listener{[](void *data, wl_callback *, uint32_t) {
            *static_cast<bool *>(data) = true;
        }};
        wl_callback_add_listener(callback, &listener, &done);
        QElapsedTimer timer;
        timer.start();
        while (!done && timer.elapsed() < 3000 && !wl_display_get_error(display)) {
            if (wl_display_flush(display) < 0 && errno != EAGAIN) break;
            compositor->processWaylandEvents();
            wl_display_flush_clients(compositor->display());
            pollfd descriptor{wl_display_get_fd(display), POLLIN, 0};
            if (poll(&descriptor, 1, 0) > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
                if (wl_display_dispatch(display) < 0) break;
            }
            QCoreApplication::processEvents();
        }
        wl_callback_destroy(callback);
        return done && !wl_display_get_error(display);
    }
    void disconnect() {
        if (!display) return;
        for (auto *proxy : std::as_const(proxies)) wl_proxy_destroy(proxy);
        proxies.clear();
        wl_display_disconnect(std::exchange(display, nullptr));
        if (compositor) compositor->processWaylandEvents();
        QCoreApplication::processEvents();
    }
    template<class T> T *track(T *proxy) {
        proxies.append(reinterpret_cast<wl_proxy *>(proxy));
        return proxy;
    }
    template<class T> void forget(T *proxy) { proxies.removeAll(reinterpret_cast<wl_proxy *>(proxy)); }
    wl_surface *createSurface() { return track(wl_compositor_create_surface(core)); }
    z_systalia_surface_v1 *createController(wl_surface *surface) {
        return track(z_systalia_manager_v1_get_systalia_surface(manager, surface));
    }
    QWaylandSurface *serverSurface(wl_surface *surface) const {
        return QWaylandSurface::fromResource(wl_client_get_object(server->client(),
            wl_proxy_get_id(reinterpret_cast<wl_proxy *>(surface))));
    }
    SystaliaSurfaceResource *metadata(wl_surface *surface) const {
        return compositor->surfaceExtension()->resourceForSurface(serverSurface(surface));
    }
    void destroyController(z_systalia_surface_v1 *controller) {
        forget(controller);
        z_systalia_surface_v1_destroy(controller);
    }
    void destroySurface(wl_surface *surface) {
        forget(surface);
        wl_surface_destroy(surface);
    }
    uint32_t protocolError(const char *expectedInterface) const {
        const wl_interface *interface = nullptr;
        uint32_t id = 0;
        const uint32_t code = wl_display_get_protocol_error(display, &interface, &id);
        if (!interface || std::strcmp(interface->name, expectedInterface)) return UINT32_MAX;
        return code;
    }
    SystaliaCompositor *compositor;
    QPointer<QWaylandClient> server;
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_compositor *core = nullptr;
    z_systalia_manager_v1 *manager = nullptr;
    int managerGlobals = 0;
private:
    QList<wl_proxy *> proxies;
    static void global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
        auto *self = static_cast<MockClient *>(data);
        if (!std::strcmp(interface, "wl_compositor")) {
            self->core = self->track(static_cast<wl_compositor *>(
                wl_registry_bind(registry, name, &wl_compositor_interface, qMin(version, 4u))));
        } else if (!std::strcmp(interface, "z_systalia_manager_v1")) {
            ++self->managerGlobals;
            self->manager = self->track(static_cast<z_systalia_manager_v1 *>(
                wl_registry_bind(registry, name, &z_systalia_manager_v1_interface, 1)));
        }
    }
    static void globalRemoved(void *, wl_registry *, uint32_t) {}
};

class TestSystaliaProtocol : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void globalAndDefaults();
    void cornerRadiiRoundtripAndSignal();
    void slantRoundtripAndSignal();
    void opticalPropertiesRoundtripAndSignal();
    void qmlItemTracksMetadataChanges();
    void unchangedMetadataDoesNotEmitDuplicateSignal();
    void surfacesWithinOneClientKeepIndependentMetadata();
    void separateClientsKeepIndependentMetadata();
    void destroyingControllerRemovesItsMetadata();
    void destroyingManagerLeavesLiveControllersUsable();
    void destroyingSurfaceInvalidatesMetadata();
    void disconnectingClientRemovesAllMetadata();
    void duplicateControllerIsRejected();
    void invalidValues_data();
    void invalidValues();
    void settersOnDestroyedSurfaceAreRejected();
    void compositorTeardownWithLiveResources();
private:
    std::unique_ptr<QTemporaryDir> m_runtime;
    std::unique_ptr<SystaliaCompositor> m_compositor;
};

void TestSystaliaProtocol::initTestCase() {
    m_runtime = std::make_unique<QTemporaryDir>();
    QVERIFY(m_runtime->isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime->path().toUtf8());
    qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem");
}

void TestSystaliaProtocol::init() {
    m_compositor = std::make_unique<SystaliaCompositor>();
    m_compositor->setSocketName("wayland-systalia-protocol-test");
    m_compositor->create();
    QVERIFY(m_compositor->isCreated());
}

void TestSystaliaProtocol::cleanup() { m_compositor.reset(); }

void TestSystaliaProtocol::globalAndDefaults() {
    auto *extension = m_compositor->surfaceExtension();
    QVERIFY(extension->isInitialized());
    QCOMPARE(SystaliaSurfaceExtension::findIn(m_compositor.get()), extension);
    extension->initialize(); // Idempotent: must not advertise a second global.
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    QCOMPARE(client.managerGlobals, 1);
    QCOMPARE(client.server->compositor(), m_compositor.get());
    QCOMPARE(extension->surfaceCount(), 0);
    auto *surface = client.createSurface();
    client.createController(surface);
    QVERIFY(client.roundtrip());
    auto *metadata = client.metadata(surface);
    QVERIFY(metadata);
    QCOMPARE(extension->surfaceCount(), 1);
    QCOMPARE(metadata->surface(), client.serverSurface(surface));
    QCOMPARE(metadata->radii(), QVector4D(18, 18, 18, 18));
    QCOMPARE(metadata->slant(), 0.0f);
    QCOMPARE(metadata->ior(), 1.25f);
    QCOMPARE(metadata->dispersion(), 0.040f);
    QCOMPARE(metadata->meniscusWidth(), 28.0f);
}

void TestSystaliaProtocol::cornerRadiiRoundtripAndSignal() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    auto *metadata = client.metadata(surface);
    QVERIFY(metadata);
    QSignalSpy radii(metadata, &SystaliaSurfaceResource::radiiChanged);
    QVERIFY(radii.isValid());
    const QVector4D expected(16.25f, 28.5f, 32.75f, 1.0f / 256.0f);
    z_systalia_surface_v1_set_corner_radii(controller,
        wl_fixed_from_double(16.25), wl_fixed_from_double(28.5),
        wl_fixed_from_double(32.75), 1);
    QVERIFY(client.roundtrip());
    QCOMPARE(metadata->radii(), expected);
    QCOMPARE(radii.count(), 1);
    QCOMPARE(radii.at(0).at(0).value<QVector4D>(), expected);
}

void TestSystaliaProtocol::slantRoundtripAndSignal() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    auto *metadata = client.metadata(surface);
    QVERIFY(metadata);
    QSignalSpy changed(metadata, &SystaliaSurfaceResource::slantChanged);
    QVERIFY(changed.isValid());
    z_systalia_surface_v1_set_slant(controller, wl_fixed_from_double(-0.375));
    QVERIFY(client.roundtrip());
    QCOMPARE(metadata->slant(), -0.375f);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toFloat(), -0.375f);
}

void TestSystaliaProtocol::opticalPropertiesRoundtripAndSignal() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    auto *metadata = client.metadata(surface);
    QVERIFY(metadata);
    QSignalSpy changed(metadata, &SystaliaSurfaceResource::opticalPropertiesChanged);
    QVERIFY(changed.isValid());
    z_systalia_surface_v1_set_optical_properties(controller,
        wl_fixed_from_double(1.5), wl_fixed_from_double(0.0625),
        wl_fixed_from_double(32.25));
    QVERIFY(client.roundtrip());
    QCOMPARE(metadata->ior(), 1.5f);
    QCOMPARE(metadata->dispersion(), 0.0625f);
    QCOMPARE(metadata->meniscusWidth(), 32.25f);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toFloat(), 1.5f);
    QCOMPARE(changed.at(0).at(1).toFloat(), 0.0625f);
    QCOMPARE(changed.at(0).at(2).toFloat(), 32.25f);
}

void TestSystaliaProtocol::qmlItemTracksMetadataChanges() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    auto *metadata = client.metadata(surface);
    QVERIFY(metadata);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import Systalia.Engine
        SystaliaGlassItem {
            required property var metadata
            radii: metadata.radii
            slant: metadata.slant
            ior: metadata.ior
            chromaticDispersion: metadata.dispersion
        })", QUrl());
    std::unique_ptr<QObject> item(component.createWithInitialProperties({
        {QStringLiteral("metadata"), QVariant::fromValue(metadata)}}));
    QVERIFY2(item, qPrintable(component.errorString()));
    z_systalia_surface_v1_set_corner_radii(controller,
        wl_fixed_from_double(16.25), wl_fixed_from_double(28.5),
        wl_fixed_from_double(32.75), 1);
    z_systalia_surface_v1_set_slant(controller, wl_fixed_from_double(-0.375));
    z_systalia_surface_v1_set_optical_properties(controller,
        wl_fixed_from_double(1.5), wl_fixed_from_double(0.0625),
        wl_fixed_from_double(32.25));
    QVERIFY(client.roundtrip());
    QCOMPARE(item->property("radii").value<QVector4D>(),
             QVector4D(16.25f, 28.5f, 32.75f, 1.0f / 256.0f));
    QCOMPARE(item->property("slant").toFloat(), -0.375f);
    QCOMPARE(item->property("ior").toFloat(), 1.5f);
    QCOMPARE(item->property("chromaticDispersion").toFloat(), 0.0625f);
}

void TestSystaliaProtocol::unchangedMetadataDoesNotEmitDuplicateSignal() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    auto *metadata = client.metadata(surface);
    QVERIFY(metadata);
    QSignalSpy changed(metadata, &SystaliaSurfaceResource::radiiChanged);
    QVERIFY(changed.isValid());
    const auto sendRadii = [controller] {
        z_systalia_surface_v1_set_corner_radii(controller,
            wl_fixed_from_double(16.25), wl_fixed_from_double(28.5),
            wl_fixed_from_double(32.75), 1);
    };
    sendRadii();
    QVERIFY(client.roundtrip());
    sendRadii();
    QVERIFY(client.roundtrip());
    QCOMPARE(changed.count(), 1);
}

void TestSystaliaProtocol::surfacesWithinOneClientKeepIndependentMetadata() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *firstSurface = client.createSurface();
    auto *secondSurface = client.createSurface();
    auto *controller = client.createController(firstSurface);
    client.createController(secondSurface);
    QVERIFY(client.roundtrip());
    z_systalia_surface_v1_set_slant(controller, wl_fixed_from_double(0.5));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.metadata(firstSurface)->slant(), 0.5f);
    QCOMPARE(client.metadata(secondSurface)->slant(), 0.0f);
}

void TestSystaliaProtocol::separateClientsKeepIndependentMetadata() {
    MockClient first(m_compositor.get());
    MockClient second(m_compositor.get());
    QVERIFY(first.connectToServer());
    QVERIFY(second.connectToServer());
    auto *firstSurface = first.createSurface();
    auto *secondSurface = second.createSurface();
    auto *controller = first.createController(firstSurface);
    second.createController(secondSurface);
    QVERIFY(first.roundtrip());
    QVERIFY(second.roundtrip());
    z_systalia_surface_v1_set_slant(controller, wl_fixed_from_double(0.5));
    QVERIFY(first.roundtrip());
    QCOMPARE(first.metadata(firstSurface)->slant(), 0.5f);
    QCOMPARE(second.metadata(secondSurface)->slant(), 0.0f);
}

void TestSystaliaProtocol::destroyingControllerRemovesItsMetadata() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    QPointer<SystaliaSurfaceResource> metadata = client.metadata(surface);
    client.destroyController(controller);
    QVERIFY(client.roundtrip());
    QVERIFY(metadata.isNull());
    QCOMPARE(m_compositor->surfaceExtension()->surfaceCount(), 0);
}

void TestSystaliaProtocol::destroyingManagerLeavesLiveControllersUsable() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    QVERIFY(client.metadata(surface));
    client.forget(client.manager);
    z_systalia_manager_v1_destroy(client.manager);
    client.manager = nullptr;
    z_systalia_surface_v1_set_slant(controller, wl_fixed_from_double(0.25));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.metadata(surface)->slant(), 0.25f);
}

void TestSystaliaProtocol::destroyingSurfaceInvalidatesMetadata() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    QPointer<SystaliaSurfaceResource> metadata = client.metadata(surface);
    z_systalia_surface_v1_set_slant(controller, wl_fixed_from_double(0.75));
    client.destroySurface(surface);
    QVERIFY(client.roundtrip());
    QCOMPARE(m_compositor->surfaceExtension()->surfaceCount(), 0);
    QVERIFY(metadata);
    QVERIFY(!metadata->surface());
    QCOMPARE(metadata->slant(), 0.0f);
    client.destroyController(controller);
    QVERIFY(client.roundtrip());
    QVERIFY(metadata.isNull());
}

void TestSystaliaProtocol::disconnectingClientRemovesAllMetadata() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    client.createController(surface);
    QVERIFY(client.roundtrip());
    QPointer<SystaliaSurfaceResource> metadata = client.metadata(surface);
    QVERIFY(metadata);
    client.disconnect();
    QTRY_VERIFY(metadata.isNull());
    QCOMPARE(m_compositor->surfaceExtension()->surfaceCount(), 0);
    QCOMPARE(m_compositor->surfaceExtension()->findChildren<SystaliaSurfaceResource *>().size(), 0);
}

void TestSystaliaProtocol::duplicateControllerIsRejected() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    client.createController(surface);
    QVERIFY(client.roundtrip());
    client.createController(surface);
    QVERIFY(!client.roundtrip());
    QCOMPARE(client.protocolError("z_systalia_manager_v1"), uint32_t(Z_SYSTALIA_MANAGER_V1_ERROR_ALREADY_EXISTS));
    client.disconnect();
    QCOMPARE(m_compositor->surfaceExtension()->surfaceCount(), 0);
}

void TestSystaliaProtocol::invalidValues_data() {
    QTest::addColumn<int>("kind");
    QTest::newRow("negative-radius") << 0;
    QTest::newRow("slant-too-large") << 1;
    QTest::newRow("ior-below-one") << 2;
    QTest::newRow("negative-dispersion") << 3;
    QTest::newRow("zero-meniscus") << 4;
}

void TestSystaliaProtocol::invalidValues() {
    QFETCH(int, kind);
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    auto *metadata = client.metadata(surface);
    QSignalSpy radii(metadata, &SystaliaSurfaceResource::radiiChanged);
    QSignalSpy slant(metadata, &SystaliaSurfaceResource::slantChanged);
    QSignalSpy optics(metadata, &SystaliaSurfaceResource::opticalPropertiesChanged);
    if (kind == 0) z_systalia_surface_v1_set_corner_radii(controller, -1, 0, 0, 0);
    else if (kind == 1) z_systalia_surface_v1_set_slant(controller, wl_fixed_from_double(1.5));
    else z_systalia_surface_v1_set_optical_properties(controller,
        wl_fixed_from_double(kind == 2 ? 0.5 : 1.5),
        wl_fixed_from_double(kind == 3 ? -0.1 : 0.125),
        wl_fixed_from_double(kind == 4 ? 0 : 32));
    QVERIFY(!client.roundtrip());
    QCOMPARE(client.protocolError("z_systalia_surface_v1"), uint32_t(Z_SYSTALIA_SURFACE_V1_ERROR_INVALID_VALUE));
    QCOMPARE(radii.count(), 0);
    QCOMPARE(slant.count(), 0);
    QCOMPARE(optics.count(), 0);
}

void TestSystaliaProtocol::settersOnDestroyedSurfaceAreRejected() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    auto *controller = client.createController(surface);
    QVERIFY(client.roundtrip());
    client.destroySurface(surface);
    QVERIFY(client.roundtrip());
    z_systalia_surface_v1_set_slant(controller, 0);
    QVERIFY(!client.roundtrip());
    QCOMPARE(client.protocolError("z_systalia_surface_v1"), uint32_t(Z_SYSTALIA_SURFACE_V1_ERROR_INVALID_SURFACE));
}

void TestSystaliaProtocol::compositorTeardownWithLiveResources() {
    MockClient client(m_compositor.get());
    QVERIFY(client.connectToServer());
    auto *surface = client.createSurface();
    client.createController(surface);
    QVERIFY(client.roundtrip());
    QPointer<SystaliaSurfaceResource> metadata = client.metadata(surface);
    // Compositor ownership must release resources even if the client never sends destroy.
    m_compositor.reset();
    client.compositor = nullptr;
    QVERIFY(metadata.isNull());
    QVERIFY(client.server.isNull());
    client.disconnect();
}

QTEST_MAIN(TestSystaliaProtocol)
#include "tst_SystaliaProtocol.moc"
