#include "AtomicKmsPresenter.hpp"
#include "VulkanQuickRenderer.hpp"

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <iostream>
#include <unistd.h>

using namespace Systalia::Platform::Drm;

namespace {

class SimKmsBackend final : public IKmsResourceBackend {
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

class SimGbmBackend final : public IGbmBackend {
public:
    void *createDevice(int drmFd) override { return drmFd >= 0 ? reinterpret_cast<void *>(0x1) : nullptr; }
    void destroyDevice(void *) override {}
    bool allocateBuffer(void *, const GbmBufferAllocationRequest &req, GbmExportedBuffer *out) override {
        out->width = req.width;
        out->height = req.height;
        out->format = req.format;
        out->stride = req.width * 4;
        return true;
    }
    void destroyBuffer(GbmExportedBuffer *) override {}
};

class SimAtomicBackend final : public IAtomicKmsBackend {
public:
    uint32_t addFramebuffer(int, const GbmExportedBuffer &b) override { return static_cast<uint32_t>(b.id); }
    void removeFramebuffer(int, uint32_t) override {}
    AtomicCommitResult commitAtomic(int, const AtomicCommitRequest &req) override {
        return AtomicCommitResult{req.planeId != 0 && req.crtcId != 0 && req.framebufferId != 0, -1};
    }
};

} // namespace

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
        && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        && qEnvironmentVariableIsEmpty("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QGuiApplication app(argc, argv);

    VulkanQuickRenderer renderer;

    // Fase A — Vulkan
    const bool initOk = renderer.initialize(QSize(128, 128));
    if (!initOk) {
        std::cout << "[PHASE_A] FAIL: " << renderer.lastError().toStdString() << std::endl;
        return 1;
    }
    const auto caps = renderer.capabilities();
    std::cout << "[PHASE_A] PASS: device=" << caps.deviceName.toStdString()
              << " software=" << (caps.isSoftware ? "true" : "false")
              << " ext_mem_fd=" << caps.supportsExternalMemoryFd
              << " dmabuf=" << caps.supportsExternalMemoryDmabuf
              << " drm_mod=" << caps.supportsDrmFormatModifier << std::endl;

    // Fase B — Qt Quick
    const QByteArray qml =
        "import QtQuick 2.15\n"
        "Rectangle {\n"
        "  width: 128; height: 128; color: '#002244'\n"
        "  Rectangle { x: 16; y: 16; width: 96; height: 96; color: '#00ff88' }\n"
        "}\n";
    if (!renderer.loadQmlSource(qml) || !renderer.renderFrame()) {
        std::cout << "[PHASE_B] FAIL: " << renderer.lastError().toStdString() << std::endl;
        return 2;
    }
    std::cout << "[PHASE_B] PASS: QQuickRenderControl frame rendered." << std::endl;

    // Fase C — Inspeção de framebuffer
    const QImage fb = renderer.readbackImage();
    if (fb.isNull()) {
        std::cout << "[PHASE_C] FAIL: readback returned null image." << std::endl;
        return 3;
    }
    const QColor corner = fb.pixelColor(4, 4);
    const QColor center = fb.pixelColor(64, 64);
    std::cout << "[PHASE_C] PASS: size=" << fb.width() << "x" << fb.height()
              << " corner=" << corner.name().toStdString()
              << " center=" << center.name().toStdString() << std::endl;

    // Fase D — Compartilhamento de memória DMA-BUF
    const int dmabufFd = renderer.exportDmabufFd();
    if (dmabufFd >= 0) {
        std::cout << "[PHASE_D] PASS: exported dmabufFd=" << dmabufFd << std::endl;
        ::close(dmabufFd);
    } else {
        std::cout << "[PHASE_D] SKIP: DMA-BUF export not supported on active Vulkan device." << std::endl;
    }

    // Fase E — Preparação Atomic KMS simulada (sem modeset físico)
    SimKmsBackend simKms;
    SimGbmBackend simGbm;
    SimAtomicBackend simAtomic;
    AtomicKmsPresenter presenter(simKms, simGbm, simAtomic);
    const bool kmsOk = presenter.initializeOnFd(10)
                       && presenter.configureOutput(DrmOutputMode{QStringLiteral("DP-1"), 1920, 1080, 60, true})
                       && presenter.presentFrame(1, 10);
    std::cout << "[PHASE_E] " << (kmsOk ? "PASS" : "FAIL")
              << ": simulated Atomic KMS commit prepared (connector=" << presenter.activeConnectorId()
              << ", crtc=" << presenter.activeCrtcId()
              << ", plane=" << presenter.activePlaneId() << ")" << std::endl;

    presenter.shutdown();
    renderer.shutdown();
    return kmsOk ? 0 : 4;
}
