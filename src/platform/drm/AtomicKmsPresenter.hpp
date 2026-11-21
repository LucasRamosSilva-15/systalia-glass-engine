#pragma once

#include "DrmSessionTypes.hpp"
#include "GbmBufferManager.hpp"
#include "KmsOutputDiscovery.hpp"

#include <cstdint>

namespace Systalia::Platform::Drm {

struct AtomicCommitRequest {
    uint32_t connectorId = 0;
    uint32_t crtcId = 0;
    uint32_t planeId = 0;
    uint32_t framebufferId = 0;
    uint32_t srcWidth = 0;
    uint32_t srcHeight = 0;
    int inFenceFd = -1;
    bool testOnly = false;
    bool allowModeset = false;
};

struct AtomicCommitResult {
    bool succeeded = false;
    int outFenceFd = -1;
};

/**
 * @brief Interface de commit Atomic KMS e registro de framebuffers (mockável para CTest).
 */
class IAtomicKmsBackend {
public:
    virtual ~IAtomicKmsBackend() = default;

    [[nodiscard]] virtual uint32_t addFramebuffer(int drmFd, const GbmExportedBuffer &buffer) = 0;
    virtual void removeFramebuffer(int drmFd, uint32_t fbId) = 0;
    [[nodiscard]] virtual AtomicCommitResult commitAtomic(int drmFd, const AtomicCommitRequest &req) = 0;
};

/**
 * @brief Implementação libdrm real (drmModeAddFB2WithModifiers / drmModeAtomicCommit).
 *
 * Não executada automaticamente sobre GPU física durante testes automatizados.
 */
class LibdrmAtomicKmsBackend final : public IAtomicKmsBackend {
public:
    [[nodiscard]] uint32_t addFramebuffer(int drmFd, const GbmExportedBuffer &buffer) override;
    void removeFramebuffer(int drmFd, uint32_t fbId) override;
    [[nodiscard]] AtomicCommitResult commitAtomic(int drmFd, const AtomicCommitRequest &req) override;
};

/**
 * @brief Apresentador Atomic KMS integrando KmsOutputDiscovery, GbmBufferManager e IDrmPresenter.
 */
class AtomicKmsPresenter final : public IDrmPresenter {
public:
    AtomicKmsPresenter(IKmsResourceBackend &kmsBackend,
                       IGbmBackend &gbmBackend,
                       IAtomicKmsBackend &atomicBackend) noexcept;
    ~AtomicKmsPresenter() override;

    [[nodiscard]] bool initializeOnFd(int drmFd) override;
    [[nodiscard]] bool configureOutput(const DrmOutputMode &mode) override;
    [[nodiscard]] bool presentFrame(uint64_t frameIndex, int drmFd) override;
    void suspendPresentation() override;
    [[nodiscard]] bool resumePresentation(int drmFd) override;
    void shutdown() override;

    [[nodiscard]] bool submitBufferForScanout(uint64_t bufferId, int acquireFenceFd = -1);
    [[nodiscard]] bool onPageFlipCompleted(uint64_t bufferId, int releaseFenceFd = -1);

    [[nodiscard]] GbmBufferManager &bufferManager() noexcept { return m_bufferManager; }
    [[nodiscard]] const KmsDiscoverySnapshot &snapshot() const noexcept { return m_snapshot; }
    [[nodiscard]] uint32_t activeConnectorId() const noexcept { return m_connectorId; }
    [[nodiscard]] uint32_t activeCrtcId() const noexcept { return m_crtcId; }
    [[nodiscard]] uint32_t activePlaneId() const noexcept { return m_planeId; }
    [[nodiscard]] bool isSuspended() const noexcept { return m_suspended; }

private:
    KmsOutputDiscovery m_discovery;
    GbmBufferManager m_bufferManager;
    IAtomicKmsBackend &m_atomicBackend;

    int m_drmFd = -1;
    KmsDiscoverySnapshot m_snapshot;
    uint32_t m_connectorId = 0;
    uint32_t m_crtcId = 0;
    uint32_t m_planeId = 0;
    DrmOutputMode m_activeMode;
    bool m_initialized = false;
    bool m_suspended = false;
};

} // namespace Systalia::Platform::Drm
