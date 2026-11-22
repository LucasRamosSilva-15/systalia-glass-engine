#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <cstdint>
#include <functional>
#include <memory>

namespace Systalia::Platform::Drm {

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

    /// Permite simular entrega de evento em testes sem abrir /dev/dri/.
    void dispatchSimulatedPageFlip(uint64_t sequence, uint64_t userData);

    [[nodiscard]] int attachedFd() const noexcept { return m_drmFd; }
    [[nodiscard]] bool isAttached() const noexcept { return m_drmFd >= 0 && m_notifier != nullptr; }

signals:
    void pageFlipCompleted(quint64 sequence, quint64 userData);
    void pageFlipTimedOut(quint64 pendingToken);

private slots:
    void onFdReadable();

private:
    ReadEventsFn m_readEventsFn;
    int m_drmFd = -1;
    std::unique_ptr<QSocketNotifier> m_notifier;
    QTimer m_timeoutTimer;
    uint64_t m_pendingToken = 0;
};

} // namespace Systalia::Platform::Drm
