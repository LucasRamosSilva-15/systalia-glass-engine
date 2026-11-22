# SYSTALIA — Etapa 03.2.4.6.16: Auditoria Final Pré-Homologação DRM/KMS, Fences Reais e Segurança do Primeiro Modeset

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Stack:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, QtWaylandCompositor, GBM, libdrm e Atomic KMS  
**GPU de referência:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline de regressão:** 59/59 testes CTest (`38 unit`, `20 integration`, `1 e2e`)  
**Estado físico TTY3:** **BLOQUEADA (pronta apenas para homologação assistida de 1 frame mediante autorização humana explícita)**

---

## 1. Resumo Executivo

Esta etapa auditou os contratos finais do backend físico DRM/KMS (`IN_FENCE_FD`, `OUT_FENCE_PTR`, liberação de framebuffers, primeiro Atomic Modeset, paridade QPA/KMS, `IN_FORMATS`, recuperação de falhas e procedimento assistido) antes de qualquer nova tentativa em TTY3.

Foram identificados e corrigidos dois gaps críticos para hardware real:
1. O primeiro Atomic Commit (`InitialModesetPending`) em `LibdrmAtomicKmsBackend::commitAtomic()` não enviava `MODE_ID` (property blob de `drmModeModeInfo`), `CRTC_ID` no conector nem `SRC_X/Y` e `CRTC_X/Y` no plano primário.
2. A geometria e taxa de atualização do QPA `sge_vulkan_offscreen` eram fixadas na criação de `QGuiApplication` antes de `KmsOutputDiscovery` descobrir o modo físico preferencial.

Todas as validações seguras passaram (`59/59` CTest, `6/6` DRM/KMS/Sync, smoke tests headless e nested Wayland).

---

## 2. Histórico e Evidências

- **Falha física original (`2026-11-22T11:02:53`, `tty3-homolog-20261122-110253.log`):** `amdgpu: ring gfx timeout, signaled seq=820468, emitted seq=820469` seguido de `Device loss detected in vkQueueSubmit()`, antes do primeiro `drmModeAtomicCommit`.
- **Experimento offscreen mais recente (`2026-11-22T14:53:58`, `offscreen-exp-H-20261122-145358.log`, commit `cd4c053`):** Cenário H (`1920×1080`, DMA-BUF `0x200000000401a01`, `ShellRoot.qml`, VALO V2, `DesktopIconProvider`), `5/5` frames concluídos (`slots 1 → 2 → 1 → 2 → 1`), 5 `GPU_EXECUTION_COMPLETED`, 4 `MOCK_BUFFER_RELEASED`, `deviceLost=false`, `rc=0`.
- **Limitação explícita:** o harness utilizou `MockAtomic` com `outFenceFd=-1` e `releaseFenceFd=-1`; fences reais de scanout KMS ainda não foram exercitadas em hardware.

---

## 3. Contrato `IN_FENCE_FD`

Sequência auditada:
`Vulkan submit (QRhi endFrame) → vkGetSemaphoreFdKHR → acquireFenceFd → IN_FENCE_FD → drmModeAtomicCommit`

- **Ownership:** `renderIntoBuffer()` exporta `acquireFenceFd` via `vkGetSemaphoreFdKHR` (fechando eventual FD anterior do slot) e o entrega a `markBufferReadyForScanout()`.
- **`submitBufferForScanout()`:** passa `effectiveInFenceFd` em `req.inFenceFd` se `IN_FENCE_FD` for suportado no plano (caso contrário, executa `poll(effectiveInFenceFd, POLLIN, 2000)` antes do commit).
- **Transferência e fechamento:** após `drmModeAtomicCommit()`, o kernel já referenciou o fence no ioctl; `submitBufferForScanout()` fecha `target->acquireFenceFd` e zera para `-1` (evitando reutilização e double-close). Em falha de commit, `releaseBufferToAvailable()` também fecha o FD.
- **Semântica:** commit aceito significa apenas que o kernel enfileirou o flip aguardando `IN_FENCE_FD`, não que a GPU já concluiu a renderização.

---

## 4. Contrato `OUT_FENCE_PTR`

Sequência auditada:
`drmModeAtomicCommit (OUT_FENCE_PTR) → m_pendingOutFenceFd → onPageFlipCompleted → releaseFenceFd no slot anterior → vkImportSemaphoreFdKHR / poll`

- **Significado real:** `OUT_FENCE_PTR` no CRTC sinaliza a conclusão do commit correspondente (momento em que o CRTC adotou o novo framebuffer e liberou o framebuffer *anterior*).
- **Associação correta:** `m_pendingOutFenceFd` é guardado no presenter durante o commit `N` e só é anexado ao buffer *anteriormente* `Presented` em `GbmBufferManager::markBufferPresented()` quando o page-flip do commit `N` completa.
- **Erros e casos extremos:**
  - Commit falho: `res.outFenceFd` (se `>= 0`) é fechado imediatamente em `submitBufferForScanout()`, sem sobrescrever `m_pendingOutFenceFd`.
  - Fence ainda não sinalizada ao reutilizar o slot: `renderIntoBuffer()` importa via `vkImportSemaphoreFdKHR(VK_SEMAPHORE_IMPORT_TEMPORARY_BIT)` para a GPU aguardar na fila, ou valida via `isSyncFdSignaled()` no fallback.
  - Flip atrasado/duplicado: `DrmFlipContextRegistry` ignora duplicatas e emite `latePageFlipCompleted` para flips pós-timeout sem liberar buffers ativos.

---

## 5. Ciclo Real dos Slots

- Um slot torna-se seguro para reutilização **somente** quando:
  1. Outro framebuffer completou `drmHandleEvent` (`page_flip_handler`/`page_flip_handler2`), movendo o slot anterior de `Presented` → `Available`; **e**
  2. `releaseFenceFd` anexado ao slot foi importado em `vkWaitSemaphoreHandle` (ou sinalizado via `poll()`).
- Timeout de flip move o slot para `TimedOutInScanout` (nunca `Available`).

---

## 6. Primeiro Atomic Modeset (`KmsModesetState`)

Máquina auditada: `Unconfigured → InitialModesetPending → Active → Suspended`

Em `LibdrmAtomicKmsBackend::commitAtomic()` quando `req.allowModeset == true`:
- **Plano primário (`req.planeId`):** `FB_ID`, `CRTC_ID`, `SRC_X=0`, `SRC_Y=0`, `SRC_W`, `SRC_H`, `CRTC_X=0`, `CRTC_Y=0`, `CRTC_W`, `CRTC_H`, `IN_FENCE_FD` (se `>= 0`).
- **Conector (`req.connectorId`):** `CRTC_ID = req.crtcId`.
- **CRTC (`req.crtcId`):** `MODE_ID = modeBlobId` (criado com `drmModeCreatePropertyBlob` a partir de `req.modeInfoBlob` capturado em `KmsOutputDiscovery` e destruído após o commit com `drmModeDestroyPropertyBlob`), `ACTIVE = 1`, `OUT_FENCE_PTR`.
- **Flags:** `DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_ALLOW_MODESET` apenas enquanto em `InitialModesetPending` (transiciona para `Active` somente após `commitAtomic` retornar sucesso).

---

## 7. Geometria QPA (`sge_vulkan_offscreen` vs. KMS)

- `SgeVulkanOffscreenIntegration` instala `SgeQpaModeSyncFilter` em `QCoreApplication`.
- Quando `main.cpp` descobre `activeMode` via `DrmBackendController` / `KmsOutputDiscovery`, define `app.setProperty("sgeKmsOutputRefreshHz", activeMode.refreshHz)` e `app.setProperty("sgeKmsOutputSize", targetSize)` **antes** de `VulkanQuickRenderer::initialize(targetSize)`.
- `SgeOffscreenScreen::updateOutputMode()` atualiza `m_geometry` e `m_refreshHz` via `QWindowSystemInterface::handleScreenGeometryChange()` e `handleScreenRefreshRateChange()`, sem recriar `QGuiApplication`, `QVulkanInstance` ou `VkDevice` e sem forçar `1920×1080` se o monitor preferir outro modo.

---

## 8. Framebuffers e Modificadores

- `KmsOutputDiscovery::queryPlaneModifiers()` extrai modificadores do blob `IN_FORMATS` para `DRM_FORMAT_XRGB8888`.
- `main.cpp` repassa `atomicPresenter->primaryPlaneModifiers()` a `createSharedPoolSlots()`.
- `GbmBufferManager::negotiateModifiers()` calcula a interseção explícita entre KMS e Vulkan; se vazia, retorna `compatible = false` sem fallback silencioso.
- `LibdrmAtomicKmsBackend::addFramebuffer()` usa `drmPrimeFDToHandle` + `drmModeAddFB2WithModifiers(..., DRM_MODE_FB_MODIFIERS)` e fecha o `gemHandle` imediatamente com `DRM_IOCTL_GEM_CLOSE`.

---

## 9. Recuperação de Falhas

- Falhas pré-Vulkan, pós-Vulkan, em `createSharedPoolSlots`, `addFramebuffer`, primeiro modeset ou commit subsequente executam rollback ordenado (`frameLoop.shutdown()` → `releaseKmsFramebuffers()` → `destroySharedPoolSlots()` → `renderer.shutdown()` → `drmController->shutdown()` → `logindSessionService.releaseControl()`), sem vazar FDs.
- `VK_ERROR_DEVICE_LOST` aciona `IndependentFrameLoop::handleDeviceLost()` e `QCoreApplication::exit(2)`, omitindo `vkDeviceWaitIdle()`.
- Perda de DRM master (`EACCES` em `commitAtomic`) rejeita o commit sem liberar slots pendentes e mantém `InitialModesetPending`.

---

## 10. Correções Aplicadas

1. `src/platform/qpa/SgeVulkanOffscreenQpa.hpp` / `.cpp`: atualização dinâmica de geometria/refresh rate via `QDynamicPropertyChangeEvent` (`QPA_GEOMETRY_APPLIED`).
2. `src/app/main.cpp`: propagação de `activeMode` descoberto ao QPA antes de `VulkanQuickRenderer::initialize()`.
3. `src/platform/drm/KmsOutputDiscovery.hpp` / `.cpp`: captura de `rawModeInfos` (`drmModeModeInfo`) nos modos descobertos.
4. `src/platform/drm/AtomicKmsPresenter.hpp` / `.cpp`: `modeInfoBlob` em `AtomicCommitRequest`, `MODE_ID`, `SRC_X/Y`, `CRTC_X/Y`, `CRTC_ID` no conector em `allowModeset == true`, e logs estruturados `KMS_MODE_DISCOVERED`, `KMS_INITIAL_MODESET_REQUESTED`, `KMS_COMMIT_REJECTED`.
5. `src/platform/drm/VulkanQuickRenderer.cpp`: log `KMS_MODIFIER_NEGOTIATED`.
6. `src/platform/drm/DrmEventDispatcher.cpp`: log `KMS_BUFFER_RELEASED` exclusivo de eventos reais do kernel (`t_activeKernelFlipHandler`).
7. `tools/homologation/verify_homolog_env.sh`: checagem adicional não invasiva de `loginctl` e `journalctl`.
8. `tests/unit/tst_VulkanDmabufAudit.cpp`: teste `firstModesetModeBlobOutFenceReleaseAndQpaSync()`.

---

## 11. Testes Automatizados (após edições finais)

- **Build Release (`-Werror`):** limpa, zero warnings/erros.
- **Testes específicos (`6/6` em `0.91 s`):** `tst_drm_gbm_integration`, `tst_independent_vulkan_session`, `tst_unified_vulkan_gbm_pool`, `tst_vulkan_kms_sync`, `tst_page_flip_and_unified_startup`, `tst_vulkan_dmabuf_audit` (`0.33 s`, 12/12 subtestes aprovados).
- **CTest completo:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`) em `51.78 s` (`38 unit`, `20 integration`, `1 e2e`).
- **Smoke tests `systalia-session` (offscreen e nested Wayland):** `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 12. Instrumentação (`SGE_GRAPHICS_DIAGNOSTICS=1`)

Eventos estruturados presentes:
- `KMS_MODE_DISCOVERED`, `QPA_GEOMETRY_APPLIED`, `KMS_MODIFIER_NEGOTIATED`, `KMS_INITIAL_MODESET_REQUESTED`, `KMS_COMMIT_SUBMITTED`, `KMS_COMMIT_REJECTED`, `KMS_PAGE_FLIP_COMPLETED`, `KMS_BUFFER_RELEASED`, `GPU_EXECUTION_COMPLETED`, `GPU_COMPLETION_TIMED_OUT`, `DEVICE_LOST` (além de `MOCK_PAGE_FLIP_COMPLETED` / `MOCK_BUFFER_RELEASED` restritos ao harness).

---

## 13. Checklist e Procedimento de Homologação Assistida (1 Frame em TTY3)

1. **Pré-verificação não invasiva:** executar `./tools/homologation/verify_homolog_env.sh` (valida usuário não-root, binários instalados em `$HOME/.local/systalia-homolog`, `/dev/dri/card1`, conectores sysfs, `ldd`, `loginctl` e `journalctl`).
2. **Ambiente e build:** instalar build `HEAD` em `$HOME/.local/systalia-homolog`.
3. **Execução em TTY3 puro (fora do KDE Plasma / TTY2):**
   - Definir `SGE_GRAPHICS_DIAGNOSTICS=1` e limite/encerramento controlado para 1 frame (ou `SIGTERM` imediato após registro do primeiro flip).
   - Executar `./tools/homologation/run_tty3_homolog.sh` (grava log em `$HOME/systalia-homolog-logs/tty3-homolog-<timestamp>.log`).
4. **Captura pós-execução:** coletar `journalctl -k -b --since "<timestamp>"` em arquivo separado fora do repositório.
5. **Critérios de aborto imediato:** qualquer `VK_ERROR_DEVICE_LOST`, `KMS_COMMIT_REJECTED`, `GPU_COMPLETION_TIMED_OUT`, ausência de page-flip ou freeze visual.
6. **Plano de recuperação:** retorno à TTY2 (`Ctrl+Alt+F2`), acesso SSH de contingência e, se possível, validação em máquina/GPU dedicada para não arriscar a sessão principal KDE Plasma.

---

## 14. Riscos Residuais

- **Classificação separada:**
  - *Contratos comprovados por testes determinísticos:* `IN_FENCE_FD` ownership/close, `OUT_FENCE_PTR` associação ao framebuffer anterior, `KmsModesetState` + `modeInfoBlob`, parsing `IN_FORMATS`, atualização dinâmica `QPA_GEOMETRY_APPLIED`, shutdown idempotente e fail-closed `DEVICE_LOST`.
  - *Contratos ainda sem validação em hardware:* `drmModeAtomicCommit` real com `MODE_ID`/`ACTIVE=1`/`IN_FENCE_FD`/`OUT_FENCE_PTR` no driver `amdgpu` em `/dev/dri/card1`, e recebimento real de `drmHandleEvent` (`page_flip_handler2`).
  - *Hipóteses sobre o GPU hang original (`11:02:53`):* ausência histórica de `VkMemoryDedicatedAllocateInfo` (já corrigida) e/ou concorrência inicial com `Xwayland :1` durante o Frame 1 (hipótese ainda não validada em hardware).
  - *Riscos de travamento do host:* `/dev/dri/card1` é compartilhada com a sessão KDE Plasma; um novo `ring gfx timeout` no kernel AMDGPU pode congelar a sessão do host.

---

## 15. Decisão Final de Segurança

- **Homologação física TTY3 (`SGE_USE_DRM_BACKEND=1`):** **NÃO APTO para execução automática — BLOQUEADA aguardando autorização humana explícita**.
- **Prontidão técnica do código:** os contratos KMS pré-homologação estão implementados, auditados e cobertos por `59/59` testes determinísticos; a próxima etapa recomendada é a **atualização do prefixo instalado em `$HOME/.local/systalia-homolog` e execução assistida de 1 frame na TTY3** (preferencialmente com sessão/host preparado para captura de `journalctl -k` e plano de contingência via SSH/`Ctrl+Alt+F2`).
