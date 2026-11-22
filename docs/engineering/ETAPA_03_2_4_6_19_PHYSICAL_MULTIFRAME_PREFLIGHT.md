# SYSTALIA — Etapa 03.2.4.6.19: Homologação Física Multi-frame, Alternância de Buffers e Sincronização DRM/KMS

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU de referência:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline:** 60/60 CTest (`39 unit`, `20 integration`, `1 e2e`)  
**Estado físico:** **1 frame validado; multi-frame (3 frames) preparado e BLOQUEADO aguardando autorização humana**

---

## 1. Resultado Físico do Primeiro Frame (`tty3-homolog-20261122-154428.log`)

- **Data/Sessão:** `2026-11-22T15:44:28-03:00`, sessão `id=4`, `tty=tty3`, `vtnr=3`, `seat=seat0`, `base_commit=f2310e2`.
- **Pipeline validado em hardware real:**
  - `KMS_MODE_DISCOVERED`: `HDMI-A-1`, `1920×1080@60Hz`, `hasModeBlob=true`.
  - `VULKAN_DEVICE_SELECTED`: `AMD Radeon Graphics (RADV RENOIR)` (`0x1002:0x1638`).
  - `KMS_MODIFIER_NEGOTIATED`: 3 modificadores compatíveis, preferido `0x200000000401901`.
  - 2 slots DMA-BUF criados (`dmabufFd=63` e `62`, `stride=7680`, `allocBytes=8912896`).
  - 5 blurs VALO V2 executados em `1920×1080` (`format=1`).
  - `CPU_COMMAND_RECORDED` → `GPU_SUBMITTED` → `SYNC_FD exported (acquireFenceFd=80)` → `HOMOLOG_FRAME_LIMIT_REACHED (1/1)`.
  - `KMS_INITIAL_MODESET_REQUESTED` → `KMS_COMMIT_SUBMITTED (bufferId=1, fbId=153, inFenceFd=80, outFenceFd=82, succeeded=true)`.
  - `KMS_PAGE_FLIP_COMPLETED (seq=1664109, commitId=1, bufferId=1)` → `KMS_BUFFER_RELEASED`.
  - Encerramento controlado (`deviceLost=false`, `rc=0`).

---

## 2. Auditoria dos Slots (`GbmBufferManager`)

Estados: `Available → Rendering → ReadyForScanout → PendingFlip → Presented → Available` (ou `TimedOutInScanout`).

| Frame | Slot | Estado após flip | Liberação |
|---|---|---|---|
| 1 | 1 | `Presented` | Nenhum buffer anterior liberado |
| 2 | 2 | `Presented` | Slot 1 → `Available` (recebe `releaseFenceFd` do commit 2) |
| 3 | 1 | `Presented` | Slot 2 → `Available` (recebe `releaseFenceFd` do commit 3) |

- Frame 1 não libera seu próprio buffer; slots pendentes não são destruídos prematuramente; reuso ocorre apenas após liberação pelo próximo flip e espera/importação da release fence.

---

## 3. Contrato de Release Fences

- **Criação:** `LibdrmAtomicKmsBackend::commitAtomic()` recebe `outFenceFd` via `OUT_FENCE_PTR` no CRTC (`outFenceFd=82` observado no teste físico de 1 frame).
- **Transferência:** `AtomicKmsPresenter` guarda `m_pendingOutFenceFd` e o entrega a `markBufferPresented()` como `releaseFenceFd` do slot *anteriormente* `Presented` quando o novo page-flip completa.
- **Importação/Espera:** ao reutilizar o slot (Frame 3 no slot 1), `VulkanQuickRenderer::renderIntoBuffer()` importa `releaseFenceFd` via `vkImportSemaphoreFdKHR(..., VK_SEMAPHORE_IMPORT_TEMPORARY_BIT)` (ou aguarda via `isSyncFdSignaled()` no fallback) antes de sobrescrever a `VkImage`.
- **Fechamento e falhas:** FD transferido ao Vulkan recebe `-1`; em timeout, `deviceLost` ou `shutdown`, `closeFdIfValid` / `::close` fecha o FD sem vazamento nem double-close.

---

## 4. Limite de Três Frames (`SGE_HOMOLOG_MAX_FRAMES`)

- `IndependentFrameLoop` aceita apenas valores controlados `1` ou `3` via `SGE_HOMOLOG_MAX_FRAMES` (qualquer outro valor positivo/inválido quando a variável está presente recua para `1`; ausência da variável preserva modo normal ilimitado).
- Contabiliza apenas submissões reais (`renderIntoBuffer()`), bloqueia a 4ª submissão, mantém o processamento dos 3 page-flips pendentes, encerra `app.exec()` somente após `framePresented(3)` (ou `frameFailed` em falha controlada) e não repete frames após erro.

---

## 5. Modos KMS (`InitialModesetPending` → `Active`)

- Commit 1 executa com `allowModeset=true` (`MODE_ID`, `ACTIVE=1`, conector `CRTC_ID`, plano `CRTC_ID`, `FB_ID`, geometria completa, `IN_FENCE_FD`, `OUT_FENCE_PTR`, `DRM_MODE_ATOMIC_ALLOW_MODESET`).
- Commits 2 e 3 executam em `KmsModesetState::Active` com `allowModeset=false`, reutilizando o modo ativo.
- Rejeição no commit 2 ou 3, flip tardio/duplicado, perda de DRM master e suspensão de sessão foram validados deterministicamente sem retries inseguros.

---

## 6. Instrumentação (`SGE_GRAPHICS_DIAGNOSTICS=1`)

Eventos estruturados completos disponíveis com correlação por `frameId`, `slotId` e `commitId`:
- `FRAME_START`, `CPU_COMMAND_RECORDED`, `GPU_SUBMITTED`, `HOMOLOG_FRAME_LIMIT_REACHED`, `KMS_COMMIT_SUBMITTED`, `KMS_PAGE_FLIP_COMPLETED`, `KMS_BUFFER_RELEASED`, `RELEASE_FENCE_CREATED`, `RELEASE_FENCE_IMPORTED`, `RELEASE_FENCE_SIGNALED`, `RELEASE_FENCE_CLOSED`, `RENDERER_SHUTDOWN_COMPLETE`.
- Ausência de fence (`outFenceFd=-1`) é registrada explicitamente sem inventar sinalização; eventos `KMS_PAGE_FLIP_COMPLETED` e `KMS_BUFFER_RELEASED` não são emitidos por mocks.

---

## 7. Diagnóstico XWayland

- **Causa do log `Código: 9 | Status: CrashExit` no teste físico:** em `XWaylandManager::stop()` (`src/compositor/xwayland/XWaylandManager.cpp`), chamado durante `~SystaliaCompositor()`, o compositor envia `SIGTERM` (`terminate()`) e, após `1500 ms` sem saída, envia `SIGKILL` (`kill()` → código `9` / `QProcess::CrashExit`).
- **Conclusão:** trata-se de **encerramento forçado intencional durante o shutdown do compositor**, e não de um crash inesperado em runtime.
- **Correção:** `XWaylandManager::onProcessFinished()` agora verifica `m_stopping`, registrando `encerrado intencionalmente durante shutdown (Status: ForcedSignal / NormalExit)` quando intencional e mantendo `CrashExit` apenas para quedas reais com `!m_stopping`.

---

## 8. Correções Aplicadas

1. `src/compositor/xwayland/XWaylandManager.cpp`: distinção entre encerramento intencional (`m_stopping`) e crash real em `onProcessFinished()`.
2. `src/platform/drm/IndependentFrameLoop.cpp`: validação restrita de `SGE_HOMOLOG_MAX_FRAMES` (`1` ou `3`, fallback fail-safe `1`).
3. `src/platform/drm/VulkanQuickRenderer.cpp` e `src/platform/drm/AtomicKmsPresenter.cpp`: logs `FRAME_START` (DMA-BUF pool), `RELEASE_FENCE_CREATED`, `RELEASE_FENCE_IMPORTED`, `RELEASE_FENCE_SIGNALED` e `RELEASE_FENCE_CLOSED`.
4. `tools/homologation/run_tty3_3frames_homolog.sh`: executor separado para 3 frames exigindo `--authorize-3frames` explícito (não aceita `SGE_HOMOLOG_MAX_FRAMES=3` herdado).
5. `tests/unit/tst_IndependentFrameLoop.cpp` e `tests/unit/tst_Tty3HomologScripts.py`: cobertura determinística de 3 frames, bloqueio da 4ª submissão, rejeição nos commits 2/3, fallback de limite inválido e bloqueio sem `--authorize-3frames`.

---

## 9. Testes Automatizados (após edições finais)

- **Build Release (`-Werror`):** limpa.
- **`bash -n` nos 3 scripts (`run_tty3_homolog.sh`, `run_tty3_3frames_homolog.sh`, `verify_homolog_env.sh`):** aprovado.
- **Testes específicos (`6/6` em `1.81 s`):** `tst_xwayland_integration`, `tst_tty3_homolog_scripts`, `tst_independent_frame_loop`, `tst_page_flip_and_unified_startup`, `tst_device_lost_and_compositor_lifecycle`, `tst_vulkan_dmabuf_audit`.
- **CTest completo:** **60/60 aprovados** (`100% tests passed, 0 tests failed out of 60`) em `52.14 s` (`39 unit`, `20 integration`, `1 e2e`).
- **Smoke tests (`$HOME/.local/systalia-homolog/bin/systalia-session`, offscreen e nested Wayland) e pré-verificação (`verify_homolog_env.sh`):** executados e aprovados no prefixo instalado atualizado.

---

## 10. Riscos Residuais

- Três frames bem-sucedidos **não comprovam estabilidade prolongada** nem encerram a investigação histórica do GPU hang original.
- A GPU `/dev/dri/card1` permanece compartilhada com a sessão KDE Plasma em TTY2.

---

## 11. Checklist de Execução Física (3 Frames — Não Executada)

1. Autorização humana explícita e separada para 3 frames.
2. Trabalho aberto salvo.
3. Prefixo `$HOME/.local/systalia-homolog` atualizado e validado com `./tools/homologation/verify_homolog_env.sh`.
4. Executar exclusivamente na TTY3: `./tools/homologation/run_tty3_3frames_homolog.sh --authorize-3frames`.
5. Capturar `journalctl -k` após retorno e abortar diante de qualquer erro.

---

## 12. Critérios de PASS/FAIL

- **PASS:** 3 frames submetidos, 3 `KMS_COMMIT_SUBMITTED` (`succeeded=true`), 3 `KMS_PAGE_FLIP_COMPLETED`, alternância de slots `1 → 2 → 1`, tratamento válido das release fences (`RELEASE_FENCE_CREATED` + `IMPORTED`/`SIGNALED`/`CLOSED`), nenhum 4º frame, `deviceLost=false`, nenhum `ring gfx timeout` e `rc=0`.
- **FAIL:** qualquer `VK_ERROR_DEVICE_LOST`, `KMS_COMMIT_REJECTED`, timeout de flip/fence, 4º frame ou `ring gfx timeout`.

---

## 13. Decisão de Segurança

- **Preparação de 3 frames:** **CONCLUÍDA e VALIDADA** por testes determinísticos (`60/60` CTest).
- **Execução física de 3 frames:** **BLOQUEADA aguardando autorização humana explícita**.
