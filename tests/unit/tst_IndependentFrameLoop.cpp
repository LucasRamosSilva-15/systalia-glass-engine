#include <QtTest>

#include "AtomicKmsPresenter.hpp"
#include "DrmBackendController.hpp"
#include "DrmEventDispatcher.hpp"
#include "IndependentFrameLoop.hpp"
#include "LogindDrmProvider.hpp"
#include "LogindSessionService.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"

#include <QColor>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <fcntl.h>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

int makePipeReadFd() {
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return -1;
    }
    ::close(fds[1]);
    return fds[0];
}

class MockRenderSource final : public IControlledRenderSource {
public:
    bool failRender = false;
    int renderCalls = 0;
    std::function<void()> onRenderHook;

    bool renderControlledFrame() override {
        ++renderCalls;
        if (onRenderHook) {
            onRenderHook();
        }
        return !failRender;
    }
};

class MockKms final : public IKmsResourceBackend {
public:
    KmsDiscoverySnapshot probeResources(int drmFd) override {
        KmsDiscoverySnapshot snap;
        if (drmFd < 0) {
            return snap;
        }
        snap.valid = true;
        snap.atomicSupported = true;
        snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, 1920, 1080});
        snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {0x34325258}});
        KmsConnectorInfo dp;
        dp.connectorId = 20;
        dp.name = QStringLiteral("DP-1");
        dp.connected = true;
        dp.possibleCrtcs = 0x1;
        dp.modes.append(DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true});
        dp.preferredMode = dp.modes.first();
        snap.connectors.append(dp);
        return snap;
    }
};

class MockGbm final : public IGbmBackend {
public:
    int activeBuffers = 0;
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &req, GbmExportedBuffer *out) override {
        ++activeBuffers;
        out->width = req.width;
        out->height = req.height;
        out->format = req.format;
        out->stride = req.width * 4;
        out->nativeHandle = reinterpret_cast<void *>(0x2);
        return true;
    }
    void destroyBuffer(GbmExportedBuffer *buf) override {
        if (buf && buf->nativeHandle) {
            --activeBuffers;
            buf->nativeHandle = nullptr;
        }
    }
};

class MockAtomic final : public IAtomicKmsBackend {
public:
    bool failCommit = false;
    int commitCalls = 0;
    uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override { return static_cast<uint32_t>(b.id); }
    void removeFramebuffer(int, uint32_t) override {}
    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &) override {
        ++commitCalls;
        return AtomicCommitResult{!failCommit, -1};
    }
};

class MockSink final : public IWaylandOutputFrameSink {
public:
    int started = 0;
    int sent = 0;
    void setAutomaticCallbacks(bool) override {}
    void notifyFrameStarted() override { ++started; }
    void notifySendFrameCallbacks() override { ++sent; }
};

struct LoopHarness {
    MockRenderSource render;
    MockKms kms;
    MockGbm gbm;
    MockAtomic atomic;
    AtomicKmsPresenter presenter{kms, gbm, atomic};
    MockSink sink;
    WaylandFrameCoordinator wayland{sink};
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop{render, presenter, wayland, dispatcher};

    bool init(int drmFd = 10) {
        return presenter.initializeOnFd(drmFd)
               && presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true});
    }
};

} // namespace

class tst_IndependentFrameLoop final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void singleAndMultipleFramesWithBackpressureAndDuplicateFlip();
    void renderAndCommitFailuresRollbackWithoutSendingWaylandCallbacks();
    void pageFlipTimeoutRecoversCleanly();
    void suspendDuringRenderAndCommitAndResumeWithNewFd();
    void shutdownWithPendingFrameLeavesZeroHeldBuffers();
    void reorderedLogindEventsControlFrameLoop();
    void rendersRealOffscreenVulkanSceneWithReadback();
};

void tst_IndependentFrameLoop::singleAndMultipleFramesWithBackpressureAndDuplicateFlip() {
    LoopHarness h;
    QVERIFY(h.init(10));

    // 1. Frame único sob demanda
    h.loop.requestFrame();
    QTRY_VERIFY(h.loop.isFlipInFlight());
    QCOMPARE(h.render.renderCalls, 1);
    QCOMPARE(h.atomic.commitCalls, 1);
    QCOMPARE(h.sink.started, 1);
    QCOMPARE(h.sink.sent, 0); // Ainda não apresentou flip

    // 2. Backpressure: múltiplos requestFrame enquanto flip está em voo colapsam em 1 pendência
    h.loop.requestFrame();
    h.loop.requestFrame();
    h.loop.requestFrame();
    QCoreApplication::processEvents();
    QCOMPARE(h.render.renderCalls, 1);
    QCOMPARE(h.atomic.commitCalls, 1);
    QVERIFY(h.loop.hasPendingRender());

    // 3. Conclusão do flip libera callback Wayland e dispara exatamente o 2º frame pendente
    h.dispatcher.dispatchSimulatedPageFlip(1, 0);
    QCOMPARE(h.sink.sent, 1);
    QCOMPARE(h.loop.presentedCount(), 1ULL);

    // Flip duplicado ignorado antes do 2º commit? Só processa após fila
    QTRY_VERIFY(h.loop.isFlipInFlight());
    QCOMPARE(h.render.renderCalls, 2);
    QCOMPARE(h.atomic.commitCalls, 2);

    h.dispatcher.dispatchSimulatedPageFlip(2, 0);
    // Flip duplicado extra não deve incrementar presentedCount nem sent
    h.dispatcher.dispatchSimulatedPageFlip(2, 0);
    QCOMPARE(h.loop.presentedCount(), 2ULL);
    QCOMPARE(h.sink.sent, 2);
}

void tst_IndependentFrameLoop::renderAndCommitFailuresRollbackWithoutSendingWaylandCallbacks() {
    LoopHarness h;
    QVERIFY(h.init(10));

    // 1. Falha de renderização
    h.render.failRender = true;
    h.loop.requestFrame();
    QTRY_COMPARE(h.loop.failedRenderCount(), 1ULL);
    QVERIFY(!h.loop.isFlipInFlight());
    QCOMPARE(h.atomic.commitCalls, 0);
    QCOMPARE(h.sink.sent, 0);
    QCOMPARE(h.presenter.bufferManager().countInState(GbmBufferState::Available), 2);

    // 2. Falha de commit KMS
    h.render.failRender = false;
    h.atomic.failCommit = true;
    h.loop.requestFrame();
    QTRY_COMPARE(h.loop.failedCommitCount(), 1ULL);
    QVERIFY(!h.loop.isFlipInFlight());
    QCOMPARE(h.sink.sent, 0);
    QCOMPARE(h.presenter.bufferManager().countInState(GbmBufferState::Available), 2);
}

void tst_IndependentFrameLoop::pageFlipTimeoutRecoversCleanly() {
    LoopHarness h;
    QVERIFY(h.init(10));
    h.loop.setFlipTimeoutMs(25);

    h.loop.requestFrame();
    QTRY_VERIFY(h.loop.isFlipInFlight());
    QTRY_COMPARE(h.loop.timedOutCount(), 1ULL);
    QVERIFY(!h.loop.isFlipInFlight());
    QCOMPARE(h.sink.sent, 0);
    QCOMPARE(h.presenter.bufferManager().countInState(GbmBufferState::Available), 2);
}

void tst_IndependentFrameLoop::suspendDuringRenderAndCommitAndResumeWithNewFd() {
    LoopHarness h;
    QVERIFY(h.init(10));

    // 1. Suspensão durante renderização
    h.render.onRenderHook = [&h]() { h.loop.suspend(); };
    h.loop.requestFrame();
    QCoreApplication::processEvents();
    QVERIFY(h.loop.isSuspended());
    QVERIFY(!h.loop.isFlipInFlight());
    QCOMPARE(h.atomic.commitCalls, 0);
    QCOMPARE(h.sink.sent, 0);
    h.render.onRenderHook = {};

    // 2. Retomada com novo FD
    const int newFd = makePipeReadFd();
    QVERIFY(newFd >= 0);
    h.loop.resume(newFd);
    QVERIFY(!h.loop.isSuspended());
    QCOMPARE(h.dispatcher.attachedFd(), newFd);
    QTRY_VERIFY(h.loop.isFlipInFlight());

    // 3. Suspensão durante commit em voo libera buffer sem enviar callback
    h.loop.suspend();
    QVERIFY(!h.loop.isFlipInFlight());
    QCOMPARE(h.sink.sent, 0);
    QCOMPARE(h.presenter.bufferManager().countInState(GbmBufferState::Available), 2);
    ::close(newFd);
}

void tst_IndependentFrameLoop::shutdownWithPendingFrameLeavesZeroHeldBuffers() {
    LoopHarness h;
    QVERIFY(h.init(10));

    h.loop.requestFrame();
    QTRY_VERIFY(h.loop.isFlipInFlight());
    h.loop.shutdown();
    h.loop.shutdown();
    h.presenter.shutdown();
    QCOMPARE(h.gbm.activeBuffers, 0);
    QVERIFY(!h.dispatcher.isAttached());
}

void tst_IndependentFrameLoop::reorderedLogindEventsControlFrameLoop() {
    LogindSessionService logind;
    logind.setCustomCallHandler([](const QDBusMessage &msg) -> QDBusMessage {
        const QString m = msg.member();
        if (m == QStringLiteral("GetSessionByPID")) {
            auto r = msg.createReply();
            r << QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/login1/session/_39")));
            return r;
        }
        if (m == QStringLiteral("TakeDevice")) {
            const int rawFd = makePipeReadFd();
            QDBusUnixFileDescriptor fd(rawFd);
            ::close(rawFd);
            auto r = msg.createReply();
            r << QVariant::fromValue(fd) << false;
            return r;
        }
        return msg.createReply();
    });
    QVERIFY(logind.initializeSession(false));

    LogindDrmProvider provider(logind, [](const QString &, uint32_t *maj, uint32_t *min) {
        *maj = 226;
        *min = 1;
        return true;
    });
    MockRenderSource render;
    MockKms kms;
    MockGbm gbm;
    MockAtomic atomic;
    AtomicKmsPresenter presenter(kms, gbm, atomic);
    DrmBackendController controller(provider, presenter);
    provider.bindController(&controller);

    QVERIFY(controller.initialize(QStringLiteral("/dev/dri/card1"),
                                  DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true}));

    MockSink sink;
    WaylandFrameCoordinator wayland(sink);
    DrmEventDispatcher dispatcher;
    IndependentFrameLoop loop(render, presenter, wayland, dispatcher);

    QObject::connect(&logind, &LogindSessionService::devicePaused, &loop,
                     [&loop](quint32 majorNum, quint32) {
        if (majorNum == 226u) {
            loop.suspend();
        }
    });
    QObject::connect(&logind, &LogindSessionService::deviceResumed, &loop,
                     [&loop](quint32 majorNum, quint32, int fd) {
        if (majorNum == 226u) {
            loop.resume(fd);
        }
    });

    logind.onPauseDevice(226, 1, QStringLiteral("pause"));
    QVERIFY(loop.isSuspended());

    // Active=true antes de ResumeDevice(DRM) não deve retomar o loop
    logind.onPropertiesChanged(QStringLiteral("org.freedesktop.login1.Session"),
                               {{QStringLiteral("Active"), true}}, {});
    QVERIFY(loop.isSuspended());

    // ResumeDevice(DRM) com novo FD retoma o loop com segurança
    const int resumedRaw = makePipeReadFd();
    QDBusUnixFileDescriptor resumedDbus(resumedRaw);
    ::close(resumedRaw);
    logind.onResumeDevice(226, 1, resumedDbus);
    QVERIFY(!loop.isSuspended());
    QTRY_VERIFY(loop.isFlipInFlight());
    loop.shutdown();
    controller.shutdown();
}

void tst_IndependentFrameLoop::rendersRealOffscreenVulkanSceneWithReadback() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(128, 128)), qPrintable(renderer.lastError()));

    const QByteArray shellPreviewQml =
        "import QtQuick 2.15\n"
        "Rectangle {\n"
        "  width: 128; height: 128; color: '#07111f'\n"
        "  Rectangle { x: 0; y: 0; width: 128; height: 24; color: '#14304d' }\n"
        "  Rectangle { x: 24; y: 92; width: 80; height: 28; radius: 8; color: '#00d4ff' }\n"
        "}\n";
    QVERIFY2(renderer.loadQmlSource(shellPreviewQml), qPrintable(renderer.lastError()));
    QVERIFY(renderer.renderFrame());

    const QImage fb = renderer.readbackImage();
    QVERIFY(!fb.isNull());
    QCOMPARE(fb.size(), QSize(128, 128));
    const QColor topBarPixel = fb.pixelColor(64, 12);
    const QColor dockPixel = fb.pixelColor(64, 106);
    QVERIFY(qAbs(topBarPixel.red() - 0x14) <= 4 && qAbs(topBarPixel.blue() - 0x4d) <= 4);
    QVERIFY(dockPixel.blue() >= 250 && qAbs(dockPixel.green() - 0xd4) <= 4);
    renderer.shutdown();
}

QTEST_MAIN(tst_IndependentFrameLoop)
#include "tst_IndependentFrameLoop.moc"
