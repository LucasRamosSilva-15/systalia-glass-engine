#pragma once

#include <QPointer>

#include <cstdint>

class QWaylandQuickOutput;

namespace Systalia::Platform::Drm {

/**
 * @brief Abstração do destino de callbacks wl_surface.frame para testabilidade sem display.
 */
class IWaylandOutputFrameSink {
public:
    virtual ~IWaylandOutputFrameSink() = default;

    virtual void setAutomaticCallbacks(bool automatic) = 0;
    virtual void notifyFrameStarted() = 0;
    virtual void notifySendFrameCallbacks() = 0;
};

/**
 * @brief Adaptador real sobre QWaylandQuickOutput.
 */
class QWaylandOutputFrameSink final : public IWaylandOutputFrameSink {
public:
    explicit QWaylandOutputFrameSink(QWaylandQuickOutput *output) noexcept;

    void setAutomaticCallbacks(bool automatic) override;
    void notifyFrameStarted() override;
    void notifySendFrameCallbacks() override;

private:
    QPointer<QWaylandQuickOutput> m_output;
};

/**
 * @brief Coordena callbacks de frame do QtWaylandCompositor com renderização controlada
 *        e apresentação Atomic KMS, evitando callbacks duplicados ou prematuros.
 */
class WaylandFrameCoordinator final {
public:
    explicit WaylandFrameCoordinator(IWaylandOutputFrameSink &sink) noexcept;

    [[nodiscard]] bool beginFrame();
    [[nodiscard]] bool completeFrame(bool presentedSuccessfully);
    void setSuspended(bool suspended) noexcept;

    [[nodiscard]] bool isFrameInFlight() const noexcept { return m_frameInFlight; }
    [[nodiscard]] bool isSuspended() const noexcept { return m_suspended; }
    [[nodiscard]] uint64_t completedFrames() const noexcept { return m_completedFrames; }
    [[nodiscard]] uint64_t droppedFrames() const noexcept { return m_droppedFrames; }

private:
    IWaylandOutputFrameSink &m_sink;
    bool m_frameInFlight = false;
    bool m_suspended = false;
    uint64_t m_completedFrames = 0;
    uint64_t m_droppedFrames = 0;
};

} // namespace Systalia::Platform::Drm
