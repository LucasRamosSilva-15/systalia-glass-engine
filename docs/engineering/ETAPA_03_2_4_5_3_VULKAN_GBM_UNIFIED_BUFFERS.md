# SYSTALIA — Etapa 03.2.4.5.3: Unificação de Buffers Vulkan/GBM, DMA-BUF e Preparação do Scanout KMS

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente de referência:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm, QtWaylandCompositor  
**GPU de referência:** AMD Radeon Cezanne (`/dev/dri/card1`)  
**Status:** Implementado e validado em build Release (`-Werror`), suíte CTest (55/55) e GPU RADV offscreen (sem scanout físico)

---

## 1. Alternativa Escolhida

Foi adotada a **Alternativa B (Vulkan → DMA-BUF → KMS)**:

```text
ShellRoot.qml
  → QQuickRenderControl
  → VulkanQuickRenderer (VkImage + VkDeviceMemory exportável via DMA-BUF)
  → vkGetMemoryFdKHR (dmabufFd)
  → GbmBufferManager (GbmExportedBuffer / pool unificado)
  → AtomicKmsPresenter (drmModeAddFB2WithModifiers → drmModeAtomicCommit)
```

**Justificativa técnica:**
- Elimina a separação anterior entre uma `VkImage` interna de renderização offscreen e buffers GBM alocados independentemente para KMS.
- Utiliza as extensões padrão Vulkan no Mesa/RADV (`VK_KHR_image_format_list`, `VK_EXT_image_drm_format_modifier`, `VK_EXT_external_memory_dma_buf` e `VK_KHR_external_memory_fd`), permitindo que o driver selecione e exporte modificadores compatíveis diretamente com o plano KMS sem cópia intermediária em CPU ou blit extra.
- Mantém o QPA `sge_vulkan_offscreen`, a flag `SGE_USE_DRM_BACKEND=1`, os modos headless/nested e o contrato de propriedade única do descritor DRM.

---

## 2. Arquitetura de Memória

Cada slot do pool unificado conecta diretamente a alocação Vulkan ao descritor exportado para o KMS por meio de `GbmExportedBuffer`:

- **`VulkanQuickRenderer`** (`src/platform/drm/VulkanQuickRenderer.hpp` / `.cpp`):
  - Cria um conjunto de slots (`createSharedPoolSlots(GbmBufferManager &, QSize, int slotCount, kmsModifiers)`).
  - Cada slot possui `VkImage` (com `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` e `VkImageDrmFormatModifierListCreateInfoEXT`), `VkDeviceMemory` dedicada (`VK_MEMORY_ALLOCATE_DEDICATED_MEMORY_INFO` + `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`), `dmabufFd` obtido via `vkGetMemoryFdKHR`, `stride`, `offset`, `modifier`, `planeCount` e `vulkanExternalMemoryVerified = true`.
- **`GbmExportedBuffer`** (`src/platform/drm/GbmBufferManager.hpp`):
  - Armazena os metadados do recurso compartilhado: `bufferId`, `width`, `height`, `drmFormat`, `modifier`, `stride`, `offset`, `planeCount`, `dmabufFd`, `kmsFramebufferId`, `vkImageHandle`, `vkMemoryHandle`, `acquireFenceFd`, `releaseFenceFd` e `state`.

---

## 3. Contratos de Ownership

- **Pool unificado (`GbmBufferManager`):** detém o ciclo de vida lógico de cada `GbmExportedBuffer` (estados `Available`, `Rendering`, `ReadyForScanout`, `InScanout`, `TimedOutInScanout`, `Retired`).
- **Descritores de arquivo (`dmabufFd`, `acquireFenceFd`, `releaseFenceFd`):**
  - `registerExternalSlot()` registra o slot exportado no `GbmBufferManager`.
  - `releaseAllBuffers()` fecha deterministicamente todos os FDs válidos (`dmabufFd`, `acquireFenceFd`, `releaseFenceFd`) com helper `closeFdIfValid`, evitando vazamento de descritores.
- **Recursos Vulkan (`VkImage` e `VkDeviceMemory`):**
  - `destroySharedPoolSlots(GbmBufferManager &pool)` libera `VkImage` e `VkDeviceMemory` de cada slot após a remoção dos framebuffers KMS (`AtomicKmsPresenter::shutdown()` chama `removeFramebuffer()` antes de `m_bufferManager.shutdown()`).
  - Verificado em teste com `::fcntl(fd, F_GETFD) == -1` após destruição.

---

## 4. Formatos e Modificadores Suportados

- **Negociação explícita (`GbmBufferManager::negotiateModifiers`):**
  - Recebe a lista de modificadores suportados pelo plano KMS e a lista consultada no Vulkan (`VulkanQuickRenderer::querySupportedDrmModifiers()`).
  - Calcula a interseção ordenada e retorna `DrmModifierNegotiationResult` contendo o modificador selecionado ou diagnóstico de incompatibilidade.
- **Consulta Vulkan (`VK_EXT_image_drm_format_modifier`):**
  - `querySupportedDrmModifiers()` consulta `vkGetPhysicalDeviceFormatProperties2` com `VkDrmFormatModifierPropertiesListEXT`, filtrando modificadores compatíveis com uso de color attachment / transfer para `VK_FORMAT_B8G8R8A8_UNORM` (`DRM_FORMAT_XRGB8888` / `DRM_FORMAT_ARGB8888`).

---

## 5. Integração com Qt Quick

- **`IControlledRenderSource::renderIntoBuffer(GbmExportedBuffer &)`** (`src/platform/drm/IndependentFrameLoop.hpp` / `.cpp`):
  - Substitui a renderização cega para uma única imagem interna pela renderização direcionada ao slot adquirido pelo `IndependentFrameLoop`.
- **`VulkanQuickRenderer::renderIntoBuffer(GbmExportedBuffer &slot)`:**
  - Vincula a `VkImage` do slot atual em `QQuickRenderTarget::fromVulkanImage(...)`, alternando o alvo de renderização do `QQuickWindow` / `QQuickRenderControl` conforme o slot ativo (double ou triple buffering).
  - `IndependentFrameLoop::processNextFrame()` adquire o próximo slot livre, invoca `m_renderSource.renderIntoBuffer(*buf)` e marca o slot como `ReadyForScanout` com `markBufferReadyForScanout(bufferId, buf->acquireFenceFd)`.

---

## 6. Integração com GBM

- `GbmBufferManager` suporta tanto slots alocados via GBM quanto slots externos registrados via `registerExternalSlot()` (caso do pool unificado Vulkan → DMA-BUF).
- **Double e Triple Buffering:**
  - `createSharedPoolSlots()` aceita `slotCount` configurável (testado explicitamente com 2 e 3 slots).
  - O `GbmBufferManager` alterna os `bufferId`s entre frames consecutivos sem sobrescrever o buffer que permanece em `InScanout`.

---

## 7. Preparação KMS

- **`AtomicKmsPresenter::submitBufferForScanout()`** (`src/platform/drm/AtomicKmsPresenter.cpp`):
  - Localiza o `GbmExportedBuffer *target = m_bufferManager.findBuffer(bufferId)`.
  - Quando `target->kmsFramebufferId == 0`, chama `m_atomicBackend.addFramebuffer(m_drmFd, *target)` (preparado para `drmModeAddFB2WithModifiers` usando `drmFormat`, `modifier`, `stride`, `offset`, `planeCount` e `dmabufFd`).
  - Passa `target->kmsFramebufferId` e `inFenceFd` no `AtomicCommitRequest`.
- **`AtomicKmsPresenter::shutdown()`:**
  - Itera pelos buffers do pool e chama `m_atomicBackend.removeFramebuffer(m_drmFd, buf.kmsFramebufferId)` antes de `m_bufferManager.shutdown()`.

---

## 8. Sincronização

- **Proteção contra timeout de page-flip:**
  - `GbmBufferState::TimedOutInScanout` foi adicionado em `GbmBufferManager.hpp`.
  - Em `IndependentFrameLoop::onPageFlipTimedOut()`, o buffer afetado transita para `TimedOutInScanout` via `markBufferTimedOut()` em vez de retornar diretamente para `Available`, impedindo que a GPU reutilize um buffer ainda potencialmente lido pelo hardware de display.
- **Sincronização GPU → Display:**
  - No caminho controlado atual (`sge_vulkan_offscreen`), `VulkanQuickRenderer::renderFrame()` aguarda a conclusão da fila via `vkQueueWaitIdle` antes da conclusão do slot.
  - **Contrato pendente para scanout físico:** exportação explícita de `SYNC_FD` por frame via `VK_KHR_external_semaphore_fd` para alimentar `IN_FENCE_FD` no `drmModeAtomicCommit`, em conjunto com `OUT_FENCE_PTR` do KMS.

---

## 9. Resultados dos Testes

Todos os builds e testes abaixo foram executados após a última edição de código em build `Release` com `-DSGE_ENABLE_WARNINGS_AS_ERRORS=ON`:

1. **Testes específicos dos componentes modificados (6/6 aprovados, 1.20 s total):**
   - `tst_drm_backend_poc`: passou (`0.04 s`)
   - `tst_drm_gbm_integration`: passou (`0.04 s`)
   - `tst_vulkan_quick_renderer`: passou (`0.09 s`)
   - `tst_independent_frame_loop`: passou (`0.14 s`)
   - `tst_shell_root_integration`: passou (`0.70 s`)
   - `tst_unified_vulkan_gbm_pool`: passou (`0.17 s`)
2. **Suíte CTest completa (55/55 aprovados, `50.32 s`):**
   - `100% tests passed, 0 tests failed out of 55` (38 `unit`, 16 `integration`, 1 `e2e`).
3. **Smoke tests com binário instalado (`systalia-session`):**
   - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
   - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 10. Evidências da GPU RADV

O teste `tst_unified_vulkan_gbm_pool` (`tests/unit/tst_UnifiedVulkanGbmPool.cpp`, executado com `QT_QPA_PLATFORM=sge_vulkan_offscreen`) validou em GPU AMD RADV real (sem DRM master / sem `TakeDevice`):

- Negociação de modificadores DRM entre lista KMS simulada e modificadores consultados via `VK_EXT_image_drm_format_modifier`.
- Criação de pool unificado com **double buffering (2 slots)** e **triple buffering (3 slots)**, com exportação real de `dmabufFd` via `vkGetMemoryFdKHR`.
- Renderização alternada por slot e readback de pixels confirmando a cor `#33dd99` diretamente na `VkImage` exportada de cada slot.
- Rollback seguro quando `addFramebuffer` falha no backend simulado.
- Fechamento determinístico de todos os FDs ao encerrar (`::fcntl(fd, F_GETFD) == -1`).

---

## 11. Falhas Encontradas e Correções Aplicadas

1. **Conflito com macro `slots` do Qt:**
   - Variáveis nomeadas `slots` em código C++ colidem com `#define slots Q_SLOTS` dos headers Qt. Renomeado para `poolSlots` em `VulkanQuickRenderer`.
2. **Estado de timeout em `IndependentFrameLoop`:**
   - Ajustado `onPageFlipTimedOut()` e os testes correspondentes em `tst_IndependentFrameLoop.cpp` para validar a transição para `GbmBufferState::TimedOutInScanout` em vez de retorno imediato a `Available`.
3. **Ordem de destruição e fechamento de FDs:**
   - Garantido que `AtomicKmsPresenter::shutdown()` remova os framebuffers KMS antes de `m_bufferManager.shutdown()`, e que `GbmBufferManager::releaseAllBuffers()` feche todos os FDs válidos (`dmabufFd`, `acquireFenceFd`, `releaseFenceFd`) antes da destruição das imagens Vulkan.

---

## 12. Limitações

- **Sem execução física de DRM/KMS:** não foram executados `drmModeAddFB2WithModifiers`, `drmModeAtomicCommit`, aquisição real via `systemd-logind` (`TakeDevice`) ou troca de VT sobre `/dev/dri/card1` nesta etapa.
- **Fences explícitas:** a exportação por frame de `SYNC_FD` via `VK_KHR_external_semaphore_fd` permanece como contrato a ser ativado na integração física com `IN_FENCE_FD` / `OUT_FENCE_PTR`.

---

## 13. Critérios para Homologação Física

Para avançar à validação física assistida em sessão independente sobre `/dev/dri/card1`:

1. Validar a aquisição única do descritor DRM via `LogindDrmProvider` sem interferência de `eglfs_kms`.
2. Verificar `drmModeAddFB2WithModifiers` real com os `dmabufFd`, `stride`, `offset` e `modifier` exportados pelo `VulkanQuickRenderer`.
3. Ativar commit atômico real (`drmModeAtomicCommit` com `DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT`) e tratamento de eventos `vblank`/`page_flip` via `DrmEventDispatcher`.
4. Implementar exportação de `SYNC_FD` (`VK_KHR_external_semaphore_fd`) para `IN_FENCE_FD` e consumo de `OUT_FENCE_PTR`.
5. Testar ciclo completo de suspensão/retomada em troca de VT (`chvt`), garantindo ausência de vazamento de FDs, FBs ou deadlocks no `IndependentFrameLoop`.
