# SYSTALIA — Etapa 03.2.4.6.4: Validação de Conclusão GPU, Lifetime dos Semáforos e Preparação de Experimentos Offscreen

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU:** AMD Radeon Cezanne/Renoir (`AMD Radeon Graphics (RADV RENOIR)`)  
**Prioridade:** P0 — estabilidade gráfica

---

## 1. Auditoria Inicial

Na Etapa 03.2.4.6.3, identificou-se duas falhas de contrato no rastreamento de conclusão e lifetime:
1. `IndependentFrameLoop::onPageFlipCompleted()` emitia `[SGE-DIAG] GPU_COMPLETED` ao receber callbacks simulados de page-flip, confundindo apresentação simulada com conclusão efetiva da submissão Vulkan.
2. `VulkanQuickRenderer::renderIntoBuffer()` chamava `vkDestroySemaphore()` imediatamente quando `vkGetSemaphoreFdKHR()` falhava ou quando `!synced`, o que violaria o lifetime Vulkan se a GPU ainda referenciar o semáforo em uma submissão pendente.

---

## 2. Semântica de Conclusão da GPU

Eventos separados explicitamente sob `SGE_GRAPHICS_DIAGNOSTICS=1`:
- **`CPU_COMMAND_RECORDED`:** comandos Qt Quick / VALO V2 gravados (`m_renderControl->render()`).
- **`GPU_SUBMITTED`:** `m_renderControl->endFrame()` retornou sem `QRhi::isDeviceLost()` e o `SYNC_FD` foi exportado (ou submissão registrada).
- **`GPU_EXECUTION_COMPLETED`:** emitido **exclusivamente** por `VulkanQuickRenderer::observeSyncFdCompletion()` quando `poll(syncFd, POLLIN, timeoutMs)` confirma sinalização real da GPU.
- **`MOCK_PAGE_FLIP_COMPLETED`:** emitido por `DrmEventDispatcher::dispatchSimulatedPageFlip()` e `dispatchKernelPageFlipForTest()`.
- **`KMS_PAGE_FLIP_COMPLETED`:** emitido por `DrmEventDispatcher::handleKernelPageFlip()` apenas quando invocado via `onFdReadable()` (`drmHandleEvent` real do kernel).

---

## 3. Contratos dos Semáforos Vulkan

- **`vkSignalSemaphoreHandle`:** criado com `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT`; sinalizado pelo `vkQueueSubmit` do `QRhiVulkan::endOffscreenFrame()`; exportado uma única vez via `vkGetSemaphoreFdKHR` (que reseta o payload por transferência temporária).
- **`vkWaitSemaphoreHandle`:** importa `releaseFenceFd` via `vkImportSemaphoreFdKHR(..., VK_SEMAPHORE_IMPORT_TEMPORARY_BIT)` e é aguardado pelo `QRhiVulkanQueueSubmitParams::waitSemaphores`.
- Sucesso de `vkGetSemaphoreFdKHR` **não** é tratado como `GPU_EXECUTION_COMPLETED`, e conclusão de renderização (`SYNC_FD`) **não** libera o buffer do display (isso continua exclusivo da confirmação de page-flip no `GbmBufferManager`).

---

## 4. Lifetime e Quarentena

Implementados `quarantineSemaphore(VkSemaphore)` e `flushQuarantinedSemaphores()` em `VulkanQuickRenderer` (`src/platform/drm/VulkanQuickRenderer.hpp` / `.cpp`):
- Se `!synced && hasImportedWaitSemaphore`, ou se `vkGetSemaphoreFdKHR` falhar (sem `DEVICE_LOST`), o `VkSemaphore` antigo **nunca** é destruído inline durante `renderIntoBuffer()` — ele é movido para `m_quarantinedSemaphores` e o slot recebe um novo `VkSemaphore` limpo.
- `flushQuarantinedSemaphores()` destrói os semáforos em quarentena apenas em `destroySharedPoolSlots()` / `shutdown()` (após `vkDeviceWaitIdle`, omitido se `m_deviceLost`).

---

## 5. Observador de Conclusão Real

Implementado `VulkanQuickRenderer::observeSyncFdCompletion(uint64_t frameId, int syncFd, int timeoutMs)`:
- Verifica `m_deviceLost` antes de aguardar (`DeviceLost`).
- Executa `::poll(&pfd, 1, timeoutMs)` com timeout finito sobre o `SYNC_FD` (sem introduzir segunda submissão Vulkan nem `vkQueueWaitIdle()`).
- Em `POLLIN`: transiciona `Submitted → Completed` (`GPU_EXECUTION_COMPLETED`).
- Em timeout (`rc == 0`): transiciona `Submitted → CompletionTimedOut` sem assumir conclusão nem liberar o slot.

---

## 6. Estados de Submissão (`GpuSubmissionTracker`)

Implementado em `VulkanQuickRenderer.hpp` / `.cpp`:
- Estados: `Recorded`, `Submitted`, `Completed`, `SubmissionFailed`, `CompletionTimedOut`, `DeviceLost`, `Unknown`.
- Cada `GpuSubmissionRecord` armazena `frameId`, `slotId`, `generation`, `semaphoreHandle`, `syncFd`, `submittedAtNs`, `completedAtNs`, `state` e `failureReason`.
- Transições inválidas (como `markCompleted()` sem `Submitted` prévio, ou `markTimedOut()` após `Completed`) são rejeitadas deterministicamente.

---

## 7. Mudanças no Harness (`sge_vulkan_dmabuf_harness.cpp`)

- `HarnessMockAtomic::commitAtomic()` duplica `req.inFenceFd` para `lastObservedSyncFd` antes de o `AtomicKmsPresenter` fechar o FD original.
- Após `frameLoop.processNextFrame()`, o harness chama `renderer.observeSyncFdCompletion(renderer.lastSubmittedFrameId(), mockAtomic.lastObservedSyncFd, 2000)` para comprovar `GPU_EXECUTION_COMPLETED` real no RADV antes de despachar `MOCK_PAGE_FLIP_COMPLETED`.
- Cenários `D–H` continuam bloqueados (`[BLOCKED]`, exit `3`) sem `--allow-gpu-risk` e `SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1`.

---

## 8. Testes Adicionados

Em `tests/unit/tst_VulkanDmabufAudit.cpp`:
- `submissionTrackerDistinguishesSubmittedTimedOutAndRealGpuCompletion()`:
  - Rejeição de transições impossíveis (`Recorded → Completed` bloqueado).
  - Observação real de `SYNC_FD` em GPU RADV (`Submitted → Completed`).
  - Fence exportada/presente ainda não sinalizada → `CompletionTimedOut` (não vira `Completed`).

---

## 9. Resultados das Regressões (após a última edição)

- **Build Release (`-DCMAKE_CXX_FLAGS="-Werror"`):** limpa.
- **Harness seguro (`SGE_GRAPHICS_DIAGNOSTICS=1`):**
  - `--scenario A --frames 2`: `PASS`.
  - `--scenario B --frames 2`: `PASS`, sequência observada: `CPU_COMMAND_RECORDED → GPU_SUBMITTED → SYNC_FD exported → commit submitted → GPU_EXECUTION_COMPLETED → MOCK_PAGE_FLIP_COMPLETED`.
  - `--scenario D` (sem flags de risco): `[BLOCKED]`, exit `3`.
- **Testes específicos (11/11 aprovados, `2.20 s`):** incluindo `tst_vulkan_dmabuf_audit` (`0.33 s`).
- **Suíte CTest completa:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`, `50.87 s`: `38 unit`, `20 integration`, `1 e2e`).
- **Smoke tests (`systalia-session` instalado):**
  - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
  - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 10. Limitações Restantes

- Cenários `C` e `D–H` não foram executados automaticamente conforme política de segurança.
- A conclusão real do `ring gfx` em `1920×1080` com `ShellRoot.qml` e `VALO V2` permanece pendente de experimento offscreen autorizado.

---

## 11. Condições para Experimento Offscreen

1. Executar progressivamente 1 cenário por vez (`--frames 1` e depois `--frames 3`), com `SGE_GRAPHICS_DIAGNOSTICS=1`:
   - Primeiro: `--scenario C --frames 2` (`128×128`, DMA-BUF, `ShellRoot.qml`).
   - Em seguida (com `--allow-gpu-risk` e `SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1`): `D → E → F → G → H`.
2. Interromper imediatamente caso `GPU_COMPLETION_TIMED_OUT` ou `VK_ERROR_DEVICE_LOST` ocorra.

---

## 12. Decisão de Segurança

- **Testes automatizados:** **APTO** (59/59 CTest + A–B aprovados).
- **Experimento offscreen C (`128×128`, `ShellRoot.qml`):** **APTO**, aguardando autorização humana.
- **Experimentos offscreen D–H (`1920×1080`):** **APTO para execução progressiva controlada**, aguardando autorização humana explícita.
- **Homologação física TTY3:** **NÃO APTO** até que os experimentos offscreen `1920×1080` (`D–H`) concluam sem timeout nem `VK_ERROR_DEVICE_LOST`.
