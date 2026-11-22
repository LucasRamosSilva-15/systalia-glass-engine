#pragma once

#include "WaylandFrameCoordinator.hpp"

#include <QObject>
#include <cstdint>

namespace Systalia::Platform::Drm {

class AtomicKmsPresenter;
class DrmEventDispatcher;
struct GbmExportedBuffer;

/**
 * @brief Abstração do estágio de renderização Qt Quick/Vulkan para o loop independente.
 */
class IControlledRenderSource {
public:
    virtual ~IControlledRenderSource() = default;

    [[nodiscard]] virtual bool renderControlledFrame() = 0;
    [[nodiscard]] virtual bool renderIntoBuffer(GbmExportedBuffer &) {
        return renderControlledFrame();
    }
};

/**
 * @brief Coordenador do loop contínuo sob demanda com backpressure e page-flip KMS.
 *
 * Evita temporizadores descontrolados: só renderiza quando solicitado e quando
 * não há flip KMS pendente; acumula no máximo 1 solicitação pendente (backpressure).
 */
class IndependentFrameLoop final : public QObject {
    Q_OBJECT

public:
    IndependentFrameLoop(IControlledRenderSource &renderSource,
                         AtomicKmsPresenter &presenter,
                         WaylandFrameCoordinator &waylandCoordinator,
                         DrmEventDispatcher &eventDispatcher,
                         QObject *parent = nullptr);
    ~IndependentFrameLoop() override;

    IndependentFrameLoop(const IndependentFrameLoop &) = delete;
    IndependentFrameLoop &operator=(const IndependentFrameLoop &) = delete;

    void setFlipTimeoutMs(int timeoutMs) noexcept { m_flipTimeoutMs = timeoutMs; }

    void requestFrame();
    [[nodiscard]] bool processNextFrame();
    void suspend();
    void resume(int drmFd);
    void shutdown();

    [[nodiscard]] bool isFlipInFlight() const noexcept { return m_flipInFlight; }
    [[nodiscard]] bool hasPendingRender() const noexcept { return m_renderPending; }
    [[nodiscard]] bool isSuspended() const noexcept { return m_suspended; }
    [[nodiscard]] uint64_t activeFlipBufferId() const noexcept { return m_activeFlipBufferId; }
    [[nodiscard]] uint64_t presentedCount() const noexcept { return m_presentedCount; }
    [[nodiscard]] uint64_t failedRenderCount() const noexcept { return m_failedRenderCount; }
    [[nodiscard]] uint64_t failedCommitCount() const noexcept { return m_failedCommitCount; }
    [[nodiscard]] uint64_t timedOutCount() const noexcept { return m_timedOutCount; }

signals:
    void framePresented(quint64 totalPresented);
    void frameFailed();

private slots:
    void onPageFlipCompleted(quint64 sequence, quint64 userData);
    void onLatePageFlipCompleted(quint64 commitId, quint64 bufferId);
    void onPageFlipTimedOut(quint64 pendingToken);

private:
    void scheduleQueuedProcess();

    IControlledRenderSource &m_renderSource;
    AtomicKmsPresenter &m_presenter;
    WaylandFrameCoordinator &m_waylandCoordinator;
    DrmEventDispatcher &m_eventDispatcher;

    int m_flipTimeoutMs = 250;
    bool m_suspended = false;
    bool m_shuttingDown = false;
    bool m_flipInFlight = false;
    bool m_renderPending = false;
    bool m_scheduled = false;
    uint64_t m_activeFlipBufferId = 0;
    uint64_t m_activeFlipCommitId = 0;
    uint64_t m_presentedCount = 0;
    uint64_t m_failedRenderCount = 0;
    uint64_t m_failedCommitCount = 0;
    uint64_t m_timedOutCount = 0;
};

} // namespace Systalia::Platform::Drm
