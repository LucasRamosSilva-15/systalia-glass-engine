#include "SgeVulkanOffscreenQpa.hpp"

#include <QImage>
#include <QWindow>
#include <qpa/qplatformbackingstore.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>
#include <private/qbasicvulkanplatforminstance_p.h>
#include <private/qgenericunixeventdispatcher_p.h>

namespace Systalia::Platform::Qpa {

namespace {

class SgeOffscreenBackingStore final : public QPlatformBackingStore {
public:
    explicit SgeOffscreenBackingStore(QWindow *window)
        : QPlatformBackingStore(window) {}

    QPaintDevice *paintDevice() override { return &m_image; }
    void flush(QWindow *, const QRegion &, const QPoint &) override {}
    void resize(const QSize &size, const QRegion &) override {
        if (m_image.size() != size) {
            m_image = QImage(size, QImage::Format_ARGB32_Premultiplied);
        }
    }

private:
    QImage m_image;
};

class SgeOffscreenVulkanInstance final : public QBasicPlatformVulkanInstance {
public:
    explicit SgeOffscreenVulkanInstance(QVulkanInstance *instance)
        : m_instance(instance) {
        loadVulkanLibrary(QStringLiteral("vulkan"), 1);
    }

    void createOrAdoptInstance() override {
        initInstance(m_instance, {});
    }

    bool supportsPresent(VkPhysicalDevice, uint32_t, QWindow *) override {
        // Apresentação física ocorre via AtomicKmsPresenter, nunca via VkSurfaceKHR.
        return false;
    }

private:
    QVulkanInstance *m_instance = nullptr;
};

int readEnvPositiveInt(const char *name, int fallback) {
    bool ok = false;
    const int val = qEnvironmentVariableIntValue(name, &ok);
    return (ok && val > 0) ? val : fallback;
}

} // namespace

SgeOffscreenScreen::SgeOffscreenScreen(const QSize &size, int refreshHz)
    : m_geometry(QPoint(0, 0), size),
      m_refreshHz(refreshHz) {}

SgeVulkanOffscreenIntegration::SgeVulkanOffscreenIntegration() = default;

SgeVulkanOffscreenIntegration::~SgeVulkanOffscreenIntegration() {
    if (m_screen) {
        QWindowSystemInterface::handleScreenRemoved(m_screen);
        m_screen = nullptr;
    }
}

void SgeVulkanOffscreenIntegration::initialize() {
    const int width = readEnvPositiveInt("SGE_QPA_WIDTH", 1920);
    const int height = readEnvPositiveInt("SGE_QPA_HEIGHT", 1080);
    const int refreshHz = readEnvPositiveInt("SGE_QPA_REFRESH_HZ", 60);
    m_screen = new SgeOffscreenScreen(QSize(width, height), refreshHz);
    QWindowSystemInterface::handleScreenAdded(m_screen);
}

bool SgeVulkanOffscreenIntegration::hasCapability(Capability cap) const {
    switch (cap) {
    case ThreadedPixmaps:
    case WindowManagement:
        return true;
    case OpenGL:
    case ThreadedOpenGL:
        return false;
    default:
        return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *SgeVulkanOffscreenIntegration::createPlatformWindow(QWindow *window) const {
    return new QPlatformWindow(window);
}

QPlatformBackingStore *SgeVulkanOffscreenIntegration::createPlatformBackingStore(QWindow *window) const {
    return new SgeOffscreenBackingStore(window);
}

QAbstractEventDispatcher *SgeVulkanOffscreenIntegration::createEventDispatcher() const {
    return createUnixEventDispatcher();
}

QPlatformVulkanInstance *SgeVulkanOffscreenIntegration::createPlatformVulkanInstance(
    QVulkanInstance *instance) const {
    return new SgeOffscreenVulkanInstance(instance);
}

QPlatformIntegration *SgeVulkanOffscreenPlugin::create(const QString &system,
                                                       const QStringList &) {
    if (system.compare(QStringLiteral("sge_vulkan_offscreen"), Qt::CaseInsensitive) == 0) {
        return new SgeVulkanOffscreenIntegration();
    }
    return nullptr;
}

} // namespace Systalia::Platform::Qpa
