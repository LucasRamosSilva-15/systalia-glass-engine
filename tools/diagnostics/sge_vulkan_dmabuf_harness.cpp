#include "AtomicKmsPresenter.hpp"
#include "DrmEventDispatcher.hpp"
#include "GbmBufferManager.hpp"
#include "IndependentFrameLoop.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"

#include "AppRegistryService.hpp"
#include "InputDeviceManager.hpp"
#include "LiquidTilingService.hpp"
#include "LogindSessionService.hpp"
#include "NotificationServer.hpp"
#include "OutputManagerService.hpp"
#include "ProcessLauncherService.hpp"
#include "SystaliaCompositor.hpp"
#include "SystemControlService.hpp"
#include "SystemTelemetryService.hpp"
#include "WorkspaceManagerService.hpp"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QWaylandQuickOutput>

#include <drm_fourcc.h>
#include <fcntl.h>
#include <iostream>

using namespace Systalia::Platform::Drm;

namespace {

class HarnessMockGbm final : public IGbmBackend {
public:
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &, GbmExportedBuffer *) override { return true; }
    void destroyBuffer(GbmExportedBuffer *) override {}
};

class HarnessMockKms final : public IKmsResourceBackend {
public:
    QSize modeSize{128, 128};
    KmsDiscoverySnapshot probeResources(int drmFd) override {
        KmsDiscoverySnapshot snap;
        if (drmFd < 0) return snap;
        snap.valid = true;
        snap.atomicSupported = true;
        snap.crtcs.append(KmsCrtcInfo{55, 0, true, 0, 0, modeSize.width(), modeSize.height()});
        snap.planes.append(KmsPlaneInfo{77, 0x1, 55, KmsPlaneType::Primary, {DRM_FORMAT_XRGB8888}});
        KmsConnectorInfo hdmi;
        hdmi.connectorId = 20;
        hdmi.name = QStringLiteral("HDMI-A-1");
        hdmi.connected = true;
        hdmi.possibleCrtcs = 0x1;
        hdmi.modes.append(DrmOutputMode{QStringLiteral("HDMI-A-1"), modeSize.width(), modeSize.height(), 60, true});
        hdmi.preferredMode = hdmi.modes.first();
        snap.connectors.append(hdmi);
        return snap;
    }
};

class HarnessMockAtomic final : public IAtomicKmsBackend {
public:
    uint32_t nextFb = 800;
    QList<AtomicCommitRequest> commits;
    int lastObservedSyncFd = -1;

    ~HarnessMockAtomic() override {
        if (lastObservedSyncFd >= 0) {
            ::close(lastObservedSyncFd);
        }
    }
    KmsFenceCapabilities probeFenceCapabilities(int, uint32_t, uint32_t) override {
        return {true, true};
    }
    uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override {
        return b.stride > 0 ? nextFb++ : 0;
    }
    void removeFramebuffer(int, uint32_t) override {}
    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
        commits.append(req);
        if (lastObservedSyncFd >= 0) {
            ::close(lastObservedSyncFd);
            lastObservedSyncFd = -1;
        }
        if (req.inFenceFd >= 0) {
            lastObservedSyncFd = ::dup(req.inFenceFd);
        }
        return {true, -1};
    }
};

struct ScenarioSpec {
    QString id;
    QSize size;
    bool useDmabufPool = false;
    bool forceLinearModifier = false;
    bool loadValoShell = false;
    bool highRisk = false;
};

ScenarioSpec specForId(const QString &id) {
    const QString u = id.trimmed().toUpper();
    if (u == QLatin1String("A")) return {u, QSize(128, 128), false, false, false, false};
    if (u == QLatin1String("B")) return {u, QSize(128, 128), true, false, false, false};
    if (u == QLatin1String("C")) return {u, QSize(128, 128), true, false, true, false};
    if (u == QLatin1String("D")) return {u, QSize(1920, 1080), false, false, false, true};
    if (u == QLatin1String("E")) return {u, QSize(1920, 1080), true, true, false, true};
    if (u == QLatin1String("F")) return {u, QSize(1920, 1080), true, false, false, true};
    if (u == QLatin1String("G")) return {u, QSize(1920, 1080), true, true, true, true};
    if (u == QLatin1String("H")) return {u, QSize(1920, 1080), true, false, true, true};
    return {};
}

} // namespace

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "sge_vulkan_offscreen");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("sge_vulkan_dmabuf_harness"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption optScenario(QStringLiteral("scenario"),
                                   QStringLiteral("Cenário da matriz (A..H)."),
                                   QStringLiteral("id"),
                                   QStringLiteral("A"));
    QCommandLineOption optFrames(QStringLiteral("frames"),
                                 QStringLiteral("Quantidade de frames a renderizar."),
                                 QStringLiteral("count"),
                                 QStringLiteral("2"));
    QCommandLineOption optProbeMods(QStringLiteral("probe-modifiers"),
                                    QStringLiteral("Lista capacidades de modificadores DRM sem renderizar."));
    QCommandLineOption optAllowRisk(QStringLiteral("allow-gpu-risk"),
                                    QStringLiteral("Autoriza explicitamente cenários D..H (1920x1080)."));
    parser.addOption(optScenario);
    parser.addOption(optFrames);
    parser.addOption(optProbeMods);
    parser.addOption(optAllowRisk);
    parser.process(app);

    if (parser.isSet(optProbeMods)) {
        VulkanQuickRenderer probe;
        if (!probe.initialize(QSize(128, 128))) {
            std::cerr << "[FAIL] initialize: " << probe.lastError().toStdString() << "\n";
            return 1;
        }
        const auto caps = probe.capabilities();
        std::cout << "Device: " << caps.deviceName.toStdString()
                  << " extMemFd=" << caps.supportsExternalMemoryFd
                  << " dmabuf=" << caps.supportsExternalMemoryDmabuf
                  << " drmMod=" << caps.supportsDrmFormatModifier
                  << " syncFdExport=" << caps.supportsSyncFdExport
                  << " syncFdImport=" << caps.supportsSyncFdImport << "\n";
        for (const auto &m : probe.queryDrmModifierCapabilities()) {
            std::cout << "  mod=0x" << std::hex << m.modifier << std::dec
                      << " planes=" << m.planeCount
                      << " features=0x" << std::hex << m.tilingFeatures << std::dec
                      << " exportable=" << m.dmabufExportable
                      << " importable=" << m.dmabufImportable << "\n";
        }
        probe.shutdown();
        return 0;
    }

    const ScenarioSpec spec = specForId(parser.value(optScenario));
    if (spec.id.isEmpty()) {
        std::cerr << "[FAIL] Cenário inválido. Use A..H.\n";
        return 2;
    }

    if (spec.highRisk) {
        const bool envAllowed = qEnvironmentVariableIntValue("SGE_ALLOW_HIGH_RISK_GPU_SCENARIO") == 1;
        if (!parser.isSet(optAllowRisk) || !envAllowed) {
            std::cout << "[BLOCKED] Cenário " << spec.id.toStdString()
                      << " (" << spec.size.width() << "x" << spec.size.height()
                      << ") bloqueado por segurança. Requer --allow-gpu-risk e SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1.\n";
            return 3;
        }
    }

    QTemporaryDir runtimeDir;
    if (runtimeDir.isValid()) {
        qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8());
    }

    VulkanQuickRenderer renderer;
    if (!renderer.initialize(spec.size)) {
        std::cerr << "[FAIL] initialize: " << renderer.lastError().toStdString() << "\n";
        return 1;
    }

    HarnessMockKms mockKms;
    mockKms.modeSize = spec.size;
    HarnessMockGbm mockGbm;
    HarnessMockAtomic mockAtomic;
    AtomicKmsPresenter presenter(mockKms, mockGbm, mockAtomic);
    presenter.setUseExternalVulkanPool(true);
    if (!presenter.initializeOnFd(10)
        || !presenter.configureOutput(DrmOutputMode{QStringLiteral("HDMI-A-1"), spec.size.width(), spec.size.height(), 60, true})) {
        std::cerr << "[FAIL] presenter init/configure\n";
        renderer.shutdown();
        return 1;
    }

    QList<int> trackedFds;
    if (spec.useDmabufPool) {
        QList<uint64_t> mods;
        if (spec.forceLinearModifier) {
            mods.append(DRM_FORMAT_MOD_LINEAR);
        }
        if (!renderer.createSharedPoolSlots(presenter.bufferManager(), spec.size, 2, mods)) {
            std::cerr << "[FAIL] createSharedPoolSlots: " << renderer.lastError().toStdString() << "\n";
            presenter.shutdown();
            renderer.shutdown();
            return 1;
        }
        for (const auto &s : presenter.bufferManager().buffers()) {
            trackedFds.append(s.dmabufFd);
        }
    }

    NotificationServer notif;
    SystemControlService control;
    WorkspaceManagerService workspaces;
    LiquidTilingService tiling;
    OutputManagerService outputs;
    LogindSessionService session;
    InputDeviceManager input;
    qmlRegisterType<SystaliaCompositor>("Systalia.Compositor", 1, 0, "SystaliaCompositor");
    qmlRegisterSingletonType<SystemTelemetryService>(
        "Systalia.System", 1, 0, "SystemTelemetry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemTelemetryService(); });
    qmlRegisterSingletonType<ProcessLauncherService>(
        "Systalia.System", 1, 0, "ProcessLauncher",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new ProcessLauncherService(); });
    qmlRegisterSingletonType<AppRegistryService>(
        "Systalia.System", 1, 0, "AppRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new AppRegistryService(); });
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "Notifications", &notif);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "SystemControl", &control);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "WorkspaceManager", &workspaces);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LiquidTiling", &tiling);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "OutputManager", &outputs);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LogindSession", &session);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "InputDeviceManager", &input);

    QQmlEngine qmlEngine;
    if (spec.loadValoShell) {
        if (!renderer.loadQmlUrl(QUrl(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/ShellRoot.qml")),
                                 &qmlEngine)) {
            std::cerr << "[FAIL] loadQmlUrl(ShellRoot.qml): " << renderer.lastError().toStdString() << "\n";
            presenter.releaseKmsFramebuffers();
            renderer.destroySharedPoolSlots(presenter.bufferManager());
            presenter.shutdown();
            renderer.shutdown();
            return 1;
        }
        if (auto *comp = renderer.rootItem()->findChild<SystaliaCompositor *>()) {
            comp->stopXWayland();
        }
    } else {
        if (!renderer.loadQmlSource(
                "import QtQuick 2.15\nRectangle { anchors.fill: parent; color: '#0b1e36' }\n")) {
            std::cerr << "[FAIL] loadQmlSource: " << renderer.lastError().toStdString() << "\n";
            presenter.releaseKmsFramebuffers();
            renderer.destroySharedPoolSlots(presenter.bufferManager());
            presenter.shutdown();
            renderer.shutdown();
            return 1;
        }
    }

    auto *waylandOutput = renderer.rootItem()
                              ? renderer.rootItem()->findChild<QWaylandQuickOutput *>()
                              : nullptr;
    QWaylandOutputFrameSink frameSink(waylandOutput);
    WaylandFrameCoordinator waylandCoord(frameSink);
    DrmEventDispatcher drmEvents;
    IndependentFrameLoop frameLoop(renderer, presenter, waylandCoord, drmEvents);

    const int frames = qMax(1, parser.value(optFrames).toInt());
    for (int i = 0; i < frames; ++i) {
        bool ok = false;
        if (spec.useDmabufPool) {
            frameLoop.requestFrame();
            ok = frameLoop.processNextFrame();
            if (ok && mockAtomic.lastObservedSyncFd >= 0) {
                ok = renderer.observeSyncFdCompletion(
                    renderer.lastSubmittedFrameId(), mockAtomic.lastObservedSyncFd, 2000);
                ::close(mockAtomic.lastObservedSyncFd);
                mockAtomic.lastObservedSyncFd = -1;
            }
            if (ok && !mockAtomic.commits.isEmpty()) {
                drmEvents.dispatchKernelPageFlipForTest(
                    static_cast<uint64_t>(i + 1), 55, mockAtomic.commits.last().userData);
            }
        } else {
            ok = renderer.renderFrame();
        }
        if (!ok || renderer.isDeviceLost()) {
            std::cerr << "[FAIL] Render frame " << i << " deviceLost=" << renderer.isDeviceLost()
                      << " err=" << renderer.lastError().toStdString() << "\n";
            frameLoop.shutdown();
            presenter.releaseKmsFramebuffers();
            renderer.destroySharedPoolSlots(presenter.bufferManager());
            presenter.shutdown();
            renderer.shutdown();
            return 1;
        }
    }

    frameLoop.shutdown();
    presenter.releaseKmsFramebuffers();
    renderer.destroySharedPoolSlots(presenter.bufferManager());
    presenter.shutdown();
    renderer.shutdown();

    for (int fd : trackedFds) {
        if (::fcntl(fd, F_GETFD) != -1) {
            std::cerr << "[FAIL] FD vazado após shutdown: " << fd << "\n";
            return 1;
        }
    }

    std::cout << "[PASS] Cenário " << spec.id.toStdString() << " concluiu " << frames << " frames.\n";
    return 0;
}
