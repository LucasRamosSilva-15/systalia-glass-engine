# SYSTALIA — Etapa 03.2.4.6.14: Validação Multi-frame, Release Fences e Contrato do Primeiro Modeset

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Stack:** Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline:** 59/59 testes CTest (`38 unit`, `20 integration`, `1 e2e`)  
**Estado TTY3:** **BLOQUEADA**

---

## 1. Auditoria dos Slots DMA-BUF (`GbmBufferManager`)

Estados reais em `src/platform/drm/GbmBufferManager.hpp`:
`Available` → `Rendering` → `ReadyForScanout` → `PendingFlip` → `Presented` → `Available` (e `TimedOutInScanout` em timeout/device lost).

Verificação dos 9 invariantes:
1. **Sem reuso durante renderização:** `acquireBufferForRendering()` só retorna slots em `Available` e os move imediatamente para `Rendering`.
2. **Sem duas submissões simultâneas no mesmo slot:** `markBufferReadyForScanout()` exige `Rendering`; `markBufferPendingFlip()` exige `ReadyForScanout`.
3. **Conclusão `SYNC_FD` ≠ conclusão do scanout:** `observeSyncFdCompletion()` atualiza apenas `GpuSubmissionTracker` (`Completed`), sem alterar `GbmBufferState`.
4. **Reuso somente após liberação:** um slot `Presented` só retorna a `Available` em `markBufferPresented()` quando o *próximo* framebuffer completa seu page-flip (`other.id != bufferId && other.state == GbmBufferState::Presented`).
5. **Ownership explícito de FDs:** `acquireFenceFd` e `releaseFenceFd` são fechados em `closeFdIfValid()`, transferidos com `-1` após `vkImportSemaphoreFdKHR`, e fechados em falha/shutdown.
6. **Timeout não libera prematuramente:** `markBufferTimedOut()` move o slot para `TimedOutInScanout` (nunca `Available`).
7. **Page-flip duplicado não causa double-release:** `DrmFlipContextRegistry::resolveRawUserData()` marca `ctx->completed = true` e remove o contexto na primeira entrega; eventos repetidos retornam `Status::Ignore`.
8. **Falha de commit não gera reuso inseguro:** `submitBufferForScanout()` fecha `acquireFenceFd`/`outFenceFd`, descarta `PendingFlipContext` e devolve o slot a `Available`.
9. **Device lost suspende novas submissões:** `IndependentFrameLoop::handleDeviceLost()` define `m_deviceLost = true`, bloqueia `requestFrame`/`processNextFrame`/`resume` e preserva slots incertos em `TimedOutInScanout`.

---

## 2. Ownership dos FDs

- **`dmabufFd`:** criado em `createSharedPoolSlots()` (`vkGetMemoryFdKHR`); fechado em `destroySharedPoolSlots()` / `GbmBufferManager::releaseAllBuffers()`.
- **`acquireFenceFd` (`SYNC_FD`):** exportado em `renderIntoBuffer()`, armazenado no slot em `markBufferReadyForScanout()`, passado como `IN_FENCE_FD` em `submitBufferForScanout()` e fechado imediatamente após `commitAtomic()` (ou em `releaseBufferToAvailable()` em falha).
- **`releaseFenceFd` (`OUT_FENCE_PTR`):** capturado em `m_pendingOutFenceFd` no `commitAtomic()`, transferido ao slot anteriormente `Presented` quando o novo flip completa (`markBufferPresented()`), e importado via `vkImportSemaphoreFdKHR(..., VK_SEMAPHORE_IMPORT_TEMPORARY_BIT)` (ou `poll()` + `close()`) antes de sobrescrever o slot.

---

## 3. Contrato de Conclusão GPU

- **`GPU_EXECUTION_COMPLETED`:** emitido exclusivamente quando `observeSyncFdCompletion()` confirma `POLLIN` limpo no `SYNC_FD` (pool DMA-BUF) ou `observeInternalGpuCompletion()` confirma `VK_SUCCESS` em `vkWaitForFences` (caminho interno).
- **`MOCK_PAGE_FLIP_COMPLETED` / `MOCK_BUFFER_RELEASED`:** emitidos apenas pela ponte de teste no harness (`dispatchKernelPageFlipForTest()` + loop do harness), nunca confundidos com `KMS_PAGE_FLIP_COMPLETED` / `KMS_BUFFER_RELEASED`.

---

## 4. Contrato de Liberação KMS

- Em hardware real, `drmHandleEvent` → `DrmEventDispatcher::onFdReadable()` → `KMS_PAGE_FLIP_COMPLETED` → `IndependentFrameLoop::onPageFlipCompleted()` → `AtomicKmsPresenter::onPageFlipCompleted()` → `GbmBufferManager::markBufferPresented()`:
  - O slot que acabou de virar ativo vai para `Presented` (NÃO é liberado).
  - Apenas o slot *anteriormente* `Presented` retorna a `Available`, recebendo `m_pendingOutFenceFd` como `releaseFenceFd`.

---

## 5. Auditoria Multi-frame (Preparação do Cenário H — 5 frames)

Em `tools/diagnostics/sge_vulkan_dmabuf_harness.cpp`:
- Cada iteração `0..frames-1` chama `frameLoop.requestFrame()` → `frameLoop.processNextFrame()` (que adquire o próximo slot `Available`, executa `VulkanQuickRenderer::renderIntoBuffer()`, grava comandos e submete na GPU com exportação real de `SYNC_FD`) → `observeSyncFdCompletion()` → `dispatchKernelPageFlipForTest()`.
- Adicionado log `[SGE-DIAG] MOCK_BUFFER_RELEASED` identificando o slot anteriormente `Presented` que voltou a `Available` e seu `releaseFenceFd`.
- O Cenário H **não** foi executado nesta etapa.

---

## 6. Máquina de Estados do Primeiro Modeset (`KmsModesetState`)

Implementada em `AtomicKmsPresenter` (`src/platform/drm/AtomicKmsPresenter.hpp` / `.cpp`):

`Unconfigured` → `InitialModesetPending` → `Active` → `Suspended`

- `initializeOnFd()` / `configureOutput()` → `InitialModesetPending`.
- `submitBufferForScanout()`: define `req.allowModeset = (m_modesetState == KmsModesetState::InitialModesetPending)`.
  - Em `LibdrmAtomicKmsBackend::commitAtomic()`, quando `req.allowModeset == true`, adiciona `DRM_MODE_ATOMIC_ALLOW_MODESET` e a propriedade `ACTIVE = 1` no CRTC.
  - Em sucesso de `commitAtomic()`, `InitialModesetPending` transiciona para `Active`; commits subsequentes usam `allowModeset = false`.
  - Em falha de `commitAtomic()` (ex.: perda de DRM master), permanece em `InitialModesetPending`.
- `suspendPresentation()` → `Suspended`.
- `resumePresentation()` → `InitialModesetPending`.
- `shutdown()` → `Unconfigured`.

---

## 7. Validação `IN_FORMATS`

Implementado `KmsOutputDiscovery::parseInFormatsBlob(const void *data, size_t length, uint32_t drmFourcc)` em `src/platform/drm/KmsOutputDiscovery.cpp`:
- Valida `data != nullptr`, `length >= sizeof(drm_format_modifier_blob)`, `count_formats > 0`, `count_modifiers > 0`.
- Valida `formats_offset` e `modifiers_offset` e seus tamanhos multiplicados contra `length` sem overflow inteiro, utilizando `std::memcpy`.
- Extrai exclusivamente os modificadores cujo bitmask `entry.formats` inclui o índice do `drmFourcc` (`DRM_FORMAT_XRGB8888`).
- Sem interseção em `GbmBufferManager::negotiateModifiers()`, retorna `compatible = false` sem fallback silencioso.

---

## 8. Paridade XWayland

- O harness mantém `comp->stopXWayland()` por padrão para isolamento determinístico.
- Adicionada flag opt-in `--keep-xwayland` em `sge_vulkan_dmabuf_harness.cpp` para futura investigação controlada de paridade com `main.cpp` (não executada nesta etapa).

---

## 9. Correções Implementadas

| Arquivo | Correção |
| --- | --- |
| `src/platform/drm/KmsOutputDiscovery.hpp` / `.cpp` | Extraído `parseInFormatsBlob()` com verificação estrita de limites e `std::memcpy`. |
| `src/platform/drm/AtomicKmsPresenter.hpp` / `.cpp` | Adicionada máquina de estados `KmsModesetState` (`Unconfigured → InitialModesetPending → Active → Suspended`) e `ACTIVE = 1` quando `allowModeset == true`. |
| `tools/diagnostics/sge_vulkan_dmabuf_harness.cpp` | Adicionada flag opt-in `--keep-xwayland` e diagnóstico `[SGE-DIAG] MOCK_BUFFER_RELEASED` por frame. |
| `tests/unit/tst_VulkanDmabufAudit.cpp` | Adicionados testes determinísticos multi-frame (5 frames + flip duplicado + shutdown pendente), máquina de estados do primeiro modeset e parsing `IN_FORMATS` (válido, vazio, malformado, sem interseção). |

---

## 10. Testes de Regressão (após edições finais)

- **Build Release (`-Werror`):** limpa, zero warnings/erros.
- **Testes específicos DRM/KMS/Sync (`6/6` em `0.95 s`):** `tst_drm_gbm_integration`, `tst_independent_frame_loop`, `tst_unified_vulkan_gbm_pool`, `tst_vulkan_kms_sync`, `tst_page_flip_and_unified_startup`, `tst_vulkan_dmabuf_audit` (`0.32 s`).
- **CTest completo:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`) em `51.56 s` (`38 unit`, `20 integration`, `1 e2e`).
- **Harness seguro B (3 frames, `SGE_GRAPHICS_DIAGNOSTICS=1`):** `[PASS]`, alternância de slots e `MOCK_BUFFER_RELEASED` observados.
- **Bloqueio de H sem autorização:** `sge_vulkan_dmabuf_harness --scenario H` e `./tools/homologation/run_offscreen_experiment.sh --scenario H --frames 5` retornaram saída `3` (`[BLOCKED]`).
- **Smoke tests `systalia-session` (offscreen e nested Wayland):** ambos encerraram com `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 11. Riscos Restantes

- **Cenário H (5 frames) na sessão KDE Plasma:** a GPU `/dev/dri/card1` é compartilhada com o compositor da sessão ativa; um eventual `ring gfx timeout` em multi-frame pode congelar a sessão KDE Plasma e não é recuperável por `timeout`/`SIGKILL` no espaço de usuário.
- **Autorização independente:** a autorização prévia de H(1 frame) **não** autoriza H(5 frames).
- **Cache do executor:** o diretório `$HOME/.cache/sge-build-homolog` precisa estar sincronizado com o build atual antes da execução assistida.

---

## 12. Decisão de Segurança

- **Testes automatizados e contratos simulados:** **APTO** (`59/59` CTest).
- **Cenário H multi-frame (`5 frames`, `1920×1080`):** **PREPARADO / BLOQUEADO aguardando autorização humana explícita**.
- **Homologação física TTY3 (`SGE_USE_DRM_BACKEND=1`):** **NÃO APTO — BLOQUEADA** (GPU hang original não declarado resolvido).

---

## 13. Requisitos para Autorizar H Multi-frame

1. Atualizar o build em `$HOME/.cache/sge-build-homolog` a partir do `HEAD` validado.
2. Autorização humana explícita ciente do risco de compartilhamento de GPU com KDE Plasma.
3. Executar isoladamente:
   `./tools/homologation/run_offscreen_experiment.sh --scenario H --frames 5 --authorize`
4. Interromper imediatamente em caso de `VK_ERROR_DEVICE_LOST`, `GPU_COMPLETION_TIMED_OUT` ou travamento.

---

## 14. Requisitos Ainda Pendentes para TTY3

1. Conclusão sem falhas de **H(5 frames)** offscreen.
2. Validação do alinhamento de resolução `SGE_QPA_WIDTH`/`HEIGHT` quando o modo preferencial diferir de `1920×1080@60`.
3. Execução física assistida inicial de **1 frame** na TTY3 com captura persistente de logs e plano de recuperação.
