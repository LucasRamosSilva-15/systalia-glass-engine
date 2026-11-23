# SYSTALIA — Etapa 03.2.4.5.4: Sincronização Explícita Vulkan/KMS, Segurança de Buffers e Auditoria Pré-Homologação

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan RADV, GBM, libdrm e QtWaylandCompositor  
**GPU de referência:** AMD Radeon Cezanne (`/dev/dri/card1`)  
**Status:** Implementado e validado em build Release (`-Werror`), 56/56 CTest, smoke tests e GPU RADV offscreen (sem scanout físico)

---

## 1. Diagnóstico Inicial

A auditoria do código anterior à Etapa 03.2.4.5.4 identificou cinco lacunas críticas no caminho de sincronização:

1. **Bloqueio de fila no fluxo normal:** `VulkanQuickRenderer::renderFrame()` chamava `vkQueueWaitIdle(m_graphicsQueue)` após `QQuickRenderControl::endFrame()`.
2. **Extensões de semáforo não habilitadas:** `VK_KHR_external_semaphore_fd` era apenas consultada em `createVulkanDevice()`, mas não habilitada em `vkCreateDevice`, e não havia consulta a `vkGetPhysicalDeviceExternalSemaphoreProperties` para `SYNC_FD`.
3. **Fechamento acidental de `acquireFenceFd`:** `GbmBufferManager::markBufferReadyForScanout(bufferId, buf->acquireFenceFd)` fechava `buf->acquireFenceFd` antes de reatribuí-lo quando o mesmo FD já estava no slot.
4. **`AtomicKmsPresenter` sem ciclo de fences completo:**
   - `submitBufferForScanout()` não verificava se `IN_FENCE_FD` existia no plano nem fechava o FD após transferência lógica para o commit.
   - `res.outFenceFd` (`OUT_FENCE_PTR`) era descartado em `submitBufferForScanout()`, e `onPageFlipCompleted()` ignorava `releaseFenceFd`.
   - `GbmBufferManager::acquireBufferForRendering()` fechava `releaseFenceFd` sem aguardar sua sinalização.
5. **Auditoria adicional pré-homologação:**
   - `querySupportedDrmModifiers()` não filtrava `drmFormatModifierPlaneCount == 1`.
   - `DrmEventDispatcher::defaultReadDrmEvents()` espera `DrmFlipDispatchBridge *` em `user_data` do `drmHandleEvent`, enquanto `LibdrmAtomicKmsBackend::commitAtomic()` passa `nullptr` para `drmModeAtomicCommit()`.
   - `src/app/main.cpp` (`SGE_USE_DRM_BACKEND=1`) ainda usa `configureOutput()` com `allocateSwapchainPool` (GBM) sem invocar `vulkanRenderer.createSharedPoolSlots()`.

---

## 2. Estratégia de Sincronização Escolhida

Pipeline implementado:

```text
ShellRoot.qml
  → QQuickRenderControl::render()
  → QRhi::setQueueSubmitParams(QRhiVulkanQueueSubmitParams)
  → QQuickRenderControl::endFrame() [vkQueueSubmit com waitSemaphores + signalSemaphores]
  → vkGetSemaphoreFdKHR (SYNC_FD → slot.acquireFenceFd)
  → AtomicKmsPresenter::submitBufferForScanout() [IN_FENCE_FD ou poll() fallback]
  → drmModeAtomicCommit(..., OUT_FENCE_PTR → m_pendingOutFenceFd)
  → onPageFlipCompleted() [anexa releaseFenceFd ao buffer Presented anterior → Available]
  → Reuso do slot: vkImportSemaphoreFdKHR(SYNC_FD, TEMPORARY) ou poll() antes de renderizar
```

`vkQueueWaitIdle` foi **removido** do fluxo normal de `renderFrame()` e `renderIntoBuffer()` (mantendo `vkDeviceWaitIdle` apenas em `resize()`, `destroySharedPoolSlots()` e `shutdown()`).

---

## 3. APIs Vulkan Utilizadas

- **Extensões de dispositivo habilitadas em `VulkanQuickRenderer::createVulkanDevice()`:**
  - `VK_KHR_external_semaphore`
  - `VK_KHR_external_semaphore_fd`
  - (além de `VK_KHR_external_memory`, `VK_KHR_external_memory_fd`, `VK_EXT_external_memory_dma_buf`, `VK_KHR_image_format_list` e `VK_EXT_image_drm_format_modifier`)
- **Descoberta de suporte:** `vkGetPhysicalDeviceExternalSemaphoreProperties` consultando `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT` (`supportsSyncFdExport` e `supportsSyncFdImport` em `VulkanDeviceCapabilities`).
- **Objetos por slot (`GbmExportedBuffer`):**
  - `vkSignalSemaphoreHandle`: `VkSemaphore` binário criado com `VkExportSemaphoreCreateInfo` (`VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT`).
  - `vkWaitSemaphoreHandle`: `VkSemaphore` binário dedicado à importação temporária de `releaseFenceFd` via `vkImportSemaphoreFdKHR` (`VK_SEMAPHORE_IMPORT_TEMPORARY_BIT`).

---

## 4. Integração com o Qt Quick / QRhi

Em Qt 6.11.2, `QQuickWindow::rhi()` expõe `QRhi *` e `<rhi/qrhi_platform.h>` fornece `QRhiVulkanQueueSubmitParams` junto com `QRhi::setQueueSubmitParams(QRhiNativeHandles *)`:

1. Em `VulkanQuickRenderer::renderIntoBuffer(GbmExportedBuffer &slot)`:
   - Se `slot.releaseFenceFd >= 0`, importa via `vkImportSemaphoreFdKHR` para `slot.vkWaitSemaphoreHandle` (transferindo ownership do FD para o Vulkan). Caso a importação falhe ou não seja suportada, executa fallback seguro com `poll(..., POLLIN, 2000)` e fecha o FD.
   - Antes de `m_renderControl->endFrame()`, configura `QRhiVulkanQueueSubmitParams` com `waitSemaphores` (se houver `releaseFenceFd` importado) e `signalSemaphores` (`slot.vkSignalSemaphoreHandle`).
   - Chama `m_renderControl->endFrame()` e limpa `rhi->setQueueSubmitParams(nullptr)`.
2. A relação de dependência entre os comandos do `QQuickRenderControl` e o semáforo sinalizado é garantida diretamente pelo `vkQueueSubmit` interno do `QRhiVulkan::endOffscreenFrame()`.

---

## 5. Exportação de `SYNC_FD`

- Logo após `m_renderControl->endFrame()`, se `synced == true`, `supportsSyncFdExport == true` e `slot.vkSignalSemaphoreHandle != 0`, `VulkanQuickRenderer::renderIntoBuffer()` chama `vkGetSemaphoreFdKHR` com `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT`.
- O descritor retornado é salvo em `slot.acquireFenceFd`.
- Se `vkGetSemaphoreFdKHR` falhar ou retornar `fd < 0`, o método registra erro em `m_lastError` e retorna `false`, sem fabricar `SYNC_FD` fictício.

---

## 6. Contratos `IN_FENCE_FD` / `OUT_FENCE_PTR`

Implementados em `AtomicKmsPresenter` e `LibdrmAtomicKmsBackend` (`src/platform/drm/AtomicKmsPresenter.hpp` / `.cpp`):

- **Descoberta explícita (`probeFenceCapabilities`):** consulta `findPropertyId` para `"IN_FENCE_FD"` no plano primário e `"OUT_FENCE_PTR"` no CRTC durante `initializeOnFd()` e `resumePresentation()`.
- **`IN_FENCE_FD`:**
  - Se suportado no plano, o FD é passado em `AtomicCommitRequest::inFenceFd`. Após `commitAtomic()`, o FD é fechado e limpo (`target->acquireFenceFd = -1`) para nunca ser reutilizado.
  - Se **ausente** no plano, `submitBufferForScanout()` realiza fallback seguro via `poll(effectiveInFenceFd, POLLIN, 2000)` antes do commit e fecha o FD; se a fence não sinalizar, o commit é abortado com rollback para `Available`.
- **`OUT_FENCE_PTR`:**
  - Quando `commitAtomic()` retorna `res.outFenceFd >= 0`, `AtomicKmsPresenter` o armazena em `m_pendingOutFenceFd` vinculado a `m_pendingFlipBufferId`.
  - Se `commitAtomic()` falhar, `res.outFenceFd` é fechado imediatamente.
  - Em `onPageFlipCompleted(bufferId, releaseFenceFd)`, o `m_pendingOutFenceFd` (ou `releaseFenceFd` explícito) é anexado ao buffer **`Presented` anterior** ao transicioná-lo para `Available`. Se não houver buffer anterior (primeiro frame) ou se o flip for duplicado/inválido, o FD é fechado sem vazamento.

---

## 7. Ciclo de Vida dos Buffers

Estados em `GbmBufferState`:

`Available → Rendering → ReadyForScanout → PendingFlip → Presented → Available` (ou `TimedOutInScanout` em timeout)

Garantias verificadas:
- `acquireBufferForRendering()` só seleciona `Available` e **preserva** `buf.releaseFenceFd` para consumo pelo `VulkanQuickRenderer`.
- `markBufferReadyForScanout()` protege contra auto-fechamento quando `buf->acquireFenceFd == acquireFenceFd`.
- `markBufferTimedOut()` move buffers em timeout para `TimedOutInScanout`, bloqueando reutilização.
- `suspendPresentation()` fecha `m_pendingOutFenceFd` pendente; `resumePresentation(newFd)` invalida `kmsFramebufferId = 0` quando o FD muda e re-consulta `probeFenceCapabilities()`.
- `shutdown()` fecha FDs pendentes, remove FBs KMS via `removeFramebuffer()` antes de `GbmBufferManager::shutdown()` e destrói semáforos/imagens Vulkan após `vkDeviceWaitIdle`.

---

## 8. Matriz de Formatos e Modificadores

| Camada | Formato / Parâmetro | Auditoria |
|---|---|---|
| Vulkan (`VulkanQuickRenderer`) | `VK_FORMAT_B8G8R8A8_UNORM` | Layout little-endian `[B, G, R, A]`; `querySupportedDrmModifiers()` agora exige `VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT` **e** `drmFormatModifierPlaneCount == 1`. |
| KMS (`GbmExportedBuffer` / `LibdrmAtomicKmsBackend`) | `DRM_FORMAT_XRGB8888` (`0x34325258`) / `DRM_FORMAT_ARGB8888` | Canal alpha ignorado em `XRGB8888` (plano primário opaco) ou preservado em `ARGB8888`. |
| Layout / Memória | `stride`, `offset`, `planeCount = 1`, `modifier` | Obtidos via `vkGetImageSubresourceLayout(..., MEMORY_PLANE_0_BIT_EXT)` e `vkGetImageDrmFormatModifierPropertiesEXT`. |
| Verificação pendente | `drmModeAddFB2WithModifiers` e capacidades do plano físico | Aceitação real do modificador pelo hardware de display depende de execução física em `/dev/dri/card1`. |

---

## 9. Testes Executados

Todos executados após a última edição em build `Release` com `-DSGE_ENABLE_WARNINGS_AS_ERRORS=ON`:

1. **Testes específicos dos componentes modificados (8/8 aprovados, `1.40 s`):**
   - `tst_drm_backend_poc`: `0.04 s`
   - `tst_drm_gbm_integration`: `0.04 s`
   - `tst_vulkan_quick_renderer`: `0.09 s`
   - `tst_independent_vulkan_session`: `0.09 s`
   - `tst_independent_frame_loop`: `0.14 s`
   - `tst_shell_root_integration`: `0.70 s`
   - `tst_unified_vulkan_gbm_pool`: `0.16 s`
   - `tst_vulkan_kms_sync`: `0.14 s`
2. **Suíte CTest completa:** **56/56 aprovados** (`100% tests passed, 0 tests failed out of 56`, `50.47 s`: 38 `unit`, 17 `integration`, 1 `e2e`).
3. **Smoke tests (`systalia-session` instalado):**
   - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
   - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 10. Evidências em GPU RADV

Em `tst_vulkan_kms_sync` (`tests/unit/tst_VulkanKmsSync.cpp`, `QT_QPA_PLATFORM=sge_vulkan_offscreen`, sem DRM master):

- Confirmado suporte real no driver AMD RADV para `VK_KHR_external_semaphore_fd`, `supportsSyncFdExport = true` e `supportsSyncFdImport = true`.
- Exportação real de `SYNC_FD` após `QQuickRenderControl::endFrame()` sem `vkQueueWaitIdle`, com confirmação de sinalização (`VulkanQuickRenderer::isSyncFdSignaled(..., 2000) == true`) e readback de pixels `#33dd99`.
- Fechamento determinístico de `inFenceFd` após `submitBufferForScanout()` (`fcntl(fd, F_GETFD) == -1`) e consumo de `releaseFenceFd` (`OUT_FENCE_PTR`) via `vkImportSemaphoreFdKHR` ao reutilizar o Slot 0 no Frame 3.

---

## 11. Limitações Físicas

- Não foi executado `TakeDevice`, aquisição de DRM master, `drmModeAddFB2WithModifiers`, `drmModeAtomicCommit`, modeset ou troca de VT sobre `/dev/dri/card1`.
- Aceitação de modificadores pelo plano primário físico, sinalização real de `OUT_FENCE_PTR` pelo kernel `amdgpu` e latência de vblank só podem ser comprovadas no hardware físico.

---

## 12. Riscos de Integração Identificados na Auditoria

1. **Bloqueador de entrega de page-flip em `DrmEventDispatcher` + `LibdrmAtomicKmsBackend`:**
   - `DrmEventDispatcher::defaultReadDrmEvents()` (`src/platform/drm/DrmEventDispatcher.cpp:32-39`) cria um `DrmFlipDispatchBridge` local não passado ao kernel, e `LibdrmAtomicKmsBackend::commitAtomic()` (`src/platform/drm/AtomicKmsPresenter.cpp:120`) passa `user_data = nullptr` para `drmModeAtomicCommit()`. Em execução física real, `onLibdrmPageFlipEvent` receberá `userData == nullptr` e não invocará o callback de conclusão de flip, causando timeout contínuo no `IndependentFrameLoop`.
2. **Conexão pendente em `src/app/main.cpp`:**
   - O caminho `SGE_USE_DRM_BACKEND=1` ainda chama `atomicPresenter->configureOutput(initialMode)` (pool GBM) com resolução fixa `1920x1080` sem chamar `vulkanRenderer.createSharedPoolSlots(atomicPresenter->bufferManager(), ...)` para popular o pool unificado Vulkan → DMA-BUF na inicialização da sessão.

---

## 13. Checklist para Homologação Física

- [ ] Corrigir o bridge `user_data` entre `LibdrmAtomicKmsBackend::commitAtomic()` e `DrmEventDispatcher::defaultReadDrmEvents()`.
- [ ] Conectar `vulkanRenderer.createSharedPoolSlots(atomicPresenter->bufferManager(), ...)` e resolução dinâmica do `preferredMode` no caminho `SGE_USE_DRM_BACKEND=1` de `src/app/main.cpp`.
- [ ] Executar reteste físico assistido em TTY3 (`SGE_USE_DRM_BACKEND=1`) verificando `TakeDevice`, `drmModeAddFB2WithModifiers`, `IN_FENCE_FD`, `OUT_FENCE_PTR` e troca de VT (`PauseDevice`/`ResumeDevice`).

---

## 14. Decisão Explícita

**NÃO APTO** para o primeiro reteste físico assistido completo da shell neste momento.

**Justificativa:** Embora a sincronização explícita Vulkan/KMS (`SYNC_FD`, `IN_FENCE_FD`, `OUT_FENCE_PTR`, pool unificado e segurança de buffers) esteja implementada e validada na GPU RADV e em 56/56 testes CTest, os dois pontos de integração identificados na Seção 12 (`user_data` no `drmHandleEvent`/`drmModeAtomicCommit` e ligação de `createSharedPoolSlots` em `src/app/main.cpp`) impedem o funcionamento completo do flip físico na sessão real até serem corrigidos na próxima etapa preparatória.
