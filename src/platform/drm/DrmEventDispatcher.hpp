#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Systalia::Platform::Drm {

class DrmFlipContextRegistry;

struct PendingFlipContext {
    uint64_t commitId = 0;
    uint64_t generation = 0;
    uint64_t bufferId = 0;
    uint32_t crtcId = 0;
    bool completed = false;
    bool timedOut = false;
};

/**
 * @brief Registro seguro de contextos de page-flip por geração e commitId.
 *
 * Mantém contextos pendentes/timed-out vivos até a entrega do evento ou
 * encerramento seguro, evitando use-after-free e rejeitando eventos duplicados,
 * fora de ordem ou de gerações anteriores.
 */
class DrmFlipContextRegistry final {
public:
    struct Resolution {
        enum class Status {
            Ignore,
            NormalFlip,
            LateTimedOutFlip
        };
        Status status = Status::Ignore;
        uint64_t commitId = 0;
        uint64_t bufferId = 0;
    };

    PendingFlipContext *createContext(uint64_t bufferId, uint32_t crtcId);
    void discardContext(uint64_t commitId);
    bool markTimedOut(uint64_t commitId);
    void advanceGeneration() noexcept;
    void clearAll() noexcept;

    [[nodiscard]] Resolution resolveRawUserData(void *userData);
    [[nodiscard]] uint64_t currentGeneration() const noexcept { return m_generation; }
    [[nodiscard]] bool hasPendingContext(uint64_t commitId) const noexcept;
    [[nodiscard]] size_t liveContextCount() const noexcept { return m_contexts.size(); }

private:
    uint64_t m_generation = 1;
    uint64_t m_nextCommitId = 1;
    std::unordered_map<PendingFlipContext *, std::unique_ptr<PendingFlipContext>> m_contexts;
};

/**
 * @brief Despachante não-bloqueante de eventos DRM (page-flip) integrado ao event loop Qt.
 *
 * Opera exclusivamente sobre um descritor DRM já adquirido (não-proprietário),
 * suportando pausa/retomada com substituição de FD e detecção de timeout.
 */
class DrmEventDispatcher final : public QObject {
    Q_OBJECT

public:
    using PageFlipCallback = std::function<void(uint64_t sequence, uint64_t userData)>;
    using ReadEventsFn = std::function<bool(int drmFd, const PageFlipCallback &onFlip)>;

    explicit DrmEventDispatcher(ReadEventsFn readEventsFn = {}, QObject *parent = nullptr);
    ~DrmEventDispatcher() override;

    DrmEventDispatcher(const DrmEventDispatcher &) = delete;
    DrmEventDispatcher &operator=(const DrmEventDispatcher &) = delete;

    void attachFd(int drmFd);
    void detachFd();

    void armFlipTimeout(int timeoutMs, uint64_t pendingToken);
    void clearFlipTimeout();

    void setFlipContextRegistry(std::shared_ptr<DrmFlipContextRegistry> registry) noexcept;

    /// Permite simular entrega de evento em testes sem abrir /dev/dri/.
    void dispatchSimulatedPageFlip(uint64_t sequence, uint64_t userData);
    /// Simula entrega direta de page_flip_handler2 do libdrm com user_data real.
    void dispatchKernelPageFlipForTest(uint64_t sequence, uint32_t crtcId, void *userData);

    [[nodiscard]] int attachedFd() const noexcept { return m_drmFd; }
    [[nodiscard]] bool isAttached() const noexcept { return m_drmFd >= 0 && m_notifier != nullptr; }

signals:
    void pageFlipCompleted(quint64 sequence, quint64 userData);
    void latePageFlipCompleted(quint64 commitId, quint64 bufferId);
    void pageFlipTimedOut(quint64 pendingToken);

private slots:
    void onFdReadable();

private:
    void handleKernelPageFlip(uint64_t sequence, uint32_t crtcId, void *userData);

    ReadEventsFn m_readEventsFn;
    std::shared_ptr<DrmFlipContextRegistry> m_registry;
    int m_drmFd = -1;
    std::unique_ptr<QSocketNotifier> m_notifier;
    QTimer m_timeoutTimer;
    uint64_t m_pendingToken = 0;
};

} // namespace Systalia::Platform::Drm
