#pragma once

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformintegrationplugin.h>
#include <qpa/qplatformscreen.h>

namespace Systalia::Platform::Qpa {

class SgeOffscreenScreen final : public QPlatformScreen {
public:
    SgeOffscreenScreen(const QSize &size, int refreshHz);

    [[nodiscard]] QRect geometry() const override { return m_geometry; }
    [[nodiscard]] int depth() const override { return 32; }
    [[nodiscard]] QImage::Format format() const override { return QImage::Format_ARGB32_Premultiplied; }
    [[nodiscard]] qreal refreshRate() const override { return static_cast<qreal>(m_refreshHz); }
    [[nodiscard]] QString name() const override { return QStringLiteral("sge-vulkan-offscreen"); }

private:
    QRect m_geometry;
    int m_refreshHz = 60;
};

class SgeVulkanOffscreenIntegration final : public QPlatformIntegration {
public:
    SgeVulkanOffscreenIntegration();
    ~SgeVulkanOffscreenIntegration() override;

    void initialize() override;
    [[nodiscard]] bool hasCapability(Capability cap) const override;
    [[nodiscard]] QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    [[nodiscard]] QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    [[nodiscard]] QAbstractEventDispatcher *createEventDispatcher() const override;
    [[nodiscard]] QPlatformVulkanInstance *createPlatformVulkanInstance(QVulkanInstance *instance) const override;

private:
    SgeOffscreenScreen *m_screen = nullptr;
};

class SgeVulkanOffscreenPlugin final : public QPlatformIntegrationPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "sge_vulkan_offscreen.json")

public:
    QPlatformIntegration *create(const QString &system, const QStringList &paramList) override;
};

} // namespace Systalia::Platform::Qpa
