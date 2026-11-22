#pragma once

#include <QList>
#include <cstdint>
#include <memory>

struct gbm_bo;
struct gbm_device;

namespace Systalia::Platform::Drm {

enum class GbmBufferState {
    Available,
    Rendering,
    ReadyForScanout,
    PendingFlip,
    Presented,
    TimedOutInScanout
};

struct GbmBufferAllocationRequest {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0; ///< FourCC DRM/GBM (ex.: GBM_FORMAT_XRGB8888 / ARGB8888)
    QList<uint64_t> preferredModifiers;
    bool requireScanout = true;
    bool requireRendering = true;
};

struct DrmModifierNegotiationResult {
    bool compatible = false;
    uint32_t drmFourcc = 0;
    QList<uint64_t> negotiatedModifiers;
    QString failureReason;
};

struct GbmExportedBuffer {
    uint64_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint32_t planeCount = 1;
    uint64_t modifier = 0;
    int dmabufFd = -1;
    uint32_t kmsFramebufferId = 0; ///< Preenchido na preparação/criação de FB KMS
    int acquireFenceFd = -1;
    int releaseFenceFd = -1;
    GbmBufferState state = GbmBufferState::Available;
    bool vulkanExternalMemoryVerified = false;
    void *nativeHandle = nullptr; ///< Ponteiro opaco para gbm_bo* no backend real
    uint64_t vkImageHandle = 0;   ///< VkImage associado quando o pool é unificado com Vulkan
    uint64_t vkMemoryHandle = 0;  ///< VkDeviceMemory associado ao slot unificado
    uint64_t vkSignalSemaphoreHandle = 0; ///< VkSemaphore exportável (SYNC_FD) do slot
    uint64_t vkWaitSemaphoreHandle = 0;   ///< VkSemaphore para importar releaseFenceFd do KMS
};

/**
 * @brief Abstração do alocador GBM associada a um único FD DRM já adquirido.
 */
class IGbmBackend {
public:
    virtual ~IGbmBackend() = default;

    [[nodiscard]] virtual void *createDevice(int drmFd) = 0;
    virtual void destroyDevice(void *device) = 0;
    [[nodiscard]] virtual bool allocateBuffer(void *device,
                                              const GbmBufferAllocationRequest &req,
                                              GbmExportedBuffer *outBuffer) = 0;
    virtual void destroyBuffer(GbmExportedBuffer *buffer) = 0;
};

/**
 * @brief Implementação real sobre libgbm (gbm.h), sem abrir /dev/dri/ diretamente.
 */
class LibgbmBackend final : public IGbmBackend {
public:
    [[nodiscard]] void *createDevice(int drmFd) override;
    void destroyDevice(void *device) override;
    [[nodiscard]] bool allocateBuffer(void *device,
                                      const GbmBufferAllocationRequest &req,
                                      GbmExportedBuffer *outBuffer) override;
    void destroyBuffer(GbmExportedBuffer *buffer) override;
};

/**
 * @brief Gerenciador determinístico de pool de buffers GBM e ciclo de flip.
 */
class GbmBufferManager final {
public:
    explicit GbmBufferManager(IGbmBackend &backend) noexcept;
    ~GbmBufferManager();

    GbmBufferManager(const GbmBufferManager &) = delete;
    GbmBufferManager &operator=(const GbmBufferManager &) = delete;

    [[nodiscard]] bool initializeDevice(int drmFd);
    [[nodiscard]] bool allocateSwapchainPool(const GbmBufferAllocationRequest &req, int bufferCount);
    [[nodiscard]] GbmExportedBuffer *registerExternalSlot(GbmExportedBuffer slot);
    [[nodiscard]] GbmExportedBuffer *acquireBufferForRendering();
    [[nodiscard]] bool markBufferReadyForScanout(uint64_t bufferId, int acquireFenceFd = -1);
    [[nodiscard]] bool markBufferReadyForFlip(uint64_t bufferId, int acquireFenceFd = -1);
    [[nodiscard]] bool markBufferPendingFlip(uint64_t bufferId);
    [[nodiscard]] bool markBufferPresented(uint64_t bufferId,
                                           uint32_t kmsFbId = 0,
                                           int previousBufferReleaseFenceFd = -1);
    [[nodiscard]] bool markBufferTimedOut(uint64_t bufferId);
    [[nodiscard]] bool releaseBufferToAvailable(uint64_t bufferId, int releaseFenceFd = -1);
    void releaseAllBuffers();
    void shutdown();

    [[nodiscard]] static DrmModifierNegotiationResult negotiateModifiers(
        uint32_t drmFourcc,
        const QList<uint64_t> &kmsModifiers,
        const QList<uint64_t> &vulkanModifiers);

    [[nodiscard]] GbmExportedBuffer *findBuffer(uint64_t bufferId) noexcept;
    [[nodiscard]] QList<GbmExportedBuffer> &buffers() noexcept { return m_buffers; }
    [[nodiscard]] const QList<GbmExportedBuffer> &buffers() const noexcept { return m_buffers; }
    [[nodiscard]] bool isDeviceReady() const noexcept { return m_device != nullptr; }
    [[nodiscard]] int bufferCount() const noexcept { return static_cast<int>(m_buffers.size()); }
    [[nodiscard]] int countInState(GbmBufferState state) const noexcept;

private:
    IGbmBackend &m_backend;
    void *m_device = nullptr;
    QList<GbmExportedBuffer> m_buffers;
    uint64_t m_nextBufferId = 1;
};

} // namespace Systalia::Platform::Drm
