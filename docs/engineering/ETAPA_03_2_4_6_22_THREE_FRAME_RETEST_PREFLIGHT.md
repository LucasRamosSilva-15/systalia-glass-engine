# SYSTALIA — Etapa 03.2.4.6.22: Revisão Final da Correção Vulkan e Pré-homologação Física de 3 Frames

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU:** AMD Radeon Renoir (`0x1002:0x1638`)  
**Baseline:** 60/60 CTest (`39 unit`, `20 integration`, `1 e2e`)  
**Estado físico:** **1 frame validado; falha anterior no Frame 3 (`tty3-homolog-3frames-20261122-161437.log`); reteste físico de 3 frames PREPARADO e BLOQUEADO aguardando autorização humana**

---

## 1. Resumo Executivo

Foi concluída a revisão técnica final da correção aplicada em `VulkanQuickRenderer::renderIntoBuffer()` e `destroySharedPoolSlots()`, bem como a auditoria de `isSyncFdSignaled()`, dos contratos de ownership/layouts Vulkan, da limitação estrita de 3 frames em `run_tty3_3frames_homolog.sh` e da instalação isolada em `$HOME/.local/systalia-homolog`. Nenhuma execução física DRM/KMS foi realizada nesta etapa.

A preparação foi classificada como **PASS** para solicitação de autorização humana do reteste físico de 3 frames na TTY3.

---

## 2. Identidade do Build

Instalação isolada atualizada em `$HOME/.local/systalia-homolog` após compilação limpa `Release` com `-Werror`:

- `repo=LucasRamosSilva-15/systalia-glass-engine`
- `branch=valo-graphics-2.0`
- `base_commit=75fa5ed`
- `dirty_worktree=1` (adição do teste `auditPollSemanticsAndFailClosedSlotReuse` em `tests/unit/tst_VulkanDmabufAudit.cpp`; `src/platform/drm/VulkanQuickRenderer.cpp` já integrado em `75fa5ed`)
- `build_type=Release`
- `cxx_flags=-Werror`
- `qt_version=6.11.2`
- `installed_at=2026-11-22T16:48:36-03:00`

---

## 3. Auditoria da Release Fence (`KMS_PAGE_FLIP_COMPLETED(2) → Slot 1 → Frame 3`)

Caminho auditado no código de produção:

1. `IndependentFrameLoop::onPageFlipCompleted()` (`src/platform/drm/IndependentFrameLoop.cpp`) valida `m_flipInFlight`, `m_activeFlipBufferId` e `commitId`, e chama `m_presenter.onPageFlipCompleted(bufferId)`.
2. `AtomicKmsPresenter::onPageFlipCompleted()` (`src/platform/drm/AtomicKmsPresenter.cpp`) transfere `m_pendingOutFenceFd` para `GbmBufferManager::markBufferPresented(2, 0, effectiveReleaseFd)`.
3. `GbmBufferManager::markBufferPresented()` (`src/platform/drm/GbmBufferManager.cpp`) anexa `releaseFenceFd` ao Slot 1 (que estava `Presented`) e o devolve a `Available`.
4. No Frame 3, `IndependentFrameLoop::processNextFrame()` adquire o Slot 1 (`Rendering`) e chama `VulkanQuickRenderer::renderIntoBuffer(slot)`.
5. `renderIntoBuffer()` (`src/platform/drm/VulkanQuickRenderer.cpp`) verifica `slot.releaseFenceFd` via `poll()` (`isSyncFdSignaled`), fecha o FD exatamente uma vez, zera `slot.releaseFenceFd = -1` e, somente se sinalizada, prossegue para `setRenderTarget()`, `beginFrame()/sync()/render()/endFrame()`.
6. `QRhiVulkanQueueSubmitParams` recebe apenas `signalSemaphores = &signalSem` (`waitSemaphoreCount = 0`), sem importar `OUT_FENCE_PTR` em `vkImportSemaphoreFdKHR`.
7. Em caso de timeout ou erro em `poll()`, `renderIntoBuffer()` retorna `false` antes de gravar comandos, `IndependentFrameLoop` ativa `m_haltedOnError = true` (quando `m_maxSubmittedFrames > 0`) e emite `frameFailed()`, impedindo sobrescrita do slot e novas submissões.

---

## 4. Revisão de `poll()` (`VulkanQuickRenderer::isSyncFdSignaled`)

Auditoria linha a linha (`src/platform/drm/VulkanQuickRenderer.cpp`):

- **FD inválido (`fenceFd < 0`):** retorna `false` imediatamente.
- **Não bloqueante primeiro (`timeoutMs = 0`):** `isSyncFdSignaled(observedFd, 0)` verifica imediatamente quando `KMS_PAGE_FLIP_COMPLETED` já sinalizou a `OUT_FENCE_PTR` (sem espera adicional).
- **Fallback limitado (`timeoutMs = 2000`):** acionado exclusivamente se `isSyncFdSignaled(observedFd, 0)` retornar `false`; bloqueia na fila de espera do kernel via `::poll(&pfd, 1, remaining)` sem busy-wait e sem espera ilimitada.
- **Tratamento de `EINTR`:** recalcula `remaining = qMax(0, boundedTimeoutMs - elapsed)` e continua somente enquanto `timer.elapsed() < boundedTimeoutMs`.
- **Critério de retorno positivo:** `rc > 0 && (pfd.revents & (POLLERR | POLLNVAL | POLLHUP)) == 0 && (pfd.revents & POLLIN) != 0` — qualquer erro (`POLLERR`, `POLLNVAL`, `POLLHUP`) ou timeout (`rc == 0`) retorna `false` (fail-closed).
- **Nota arquitetural:** a espera bloqueante limitada de até `2000 ms` no fallback é aceitável apenas como estratégia experimental controlada para homologação de 3 frames, não representando a arquitetura definitiva de sincronização assíncrona para uma sessão desktop contínua.

---

## 5. Ownership dos FDs

- `slot.releaseFenceFd` permanece sob ownership de `GbmExportedBuffer` até `VulkanQuickRenderer::renderIntoBuffer()`, que o fecha com `::close(slot.releaseFenceFd)` exatamente uma vez e atribui `-1` tanto no caminho de sucesso quanto no de timeout/erro.
- Em falha de `renderIntoBuffer()`, `IndependentFrameLoop` chama `releaseBufferToAvailable()` (que usa `closeFdIfValid`, seguro para `-1` sem double-close).
- `acquireFenceFd` continua sendo fechado após `commitAtomic()` ou em `markBufferPresented()`/`releaseBufferToAvailable()`.
- `dmabufFd` permanece aberto até `destroySharedPoolSlots()`.

---

## 6. Layouts Vulkan

- Imagens do pool criadas com `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT`, `initialLayout = VK_IMAGE_LAYOUT_UNDEFINED` e uso `COLOR_ATTACHMENT_BIT | TRANSFER_SRC_BIT | SAMPLED_BIT`.
- Na reutilização (`Frame 3`), `QQuickRenderTarget::fromVulkanImage(vkImg, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1)` orienta o `QRhi` a descartar o conteúdo anterior na transição `UNDEFINED → COLOR_ATTACHMENT_OPTIMAL`, sem barreiras redundantes manuais.
- Negociação de modificadores KMS/Vulkan (`IN_FORMATS`) preservada.

---

## 7. Ciclo de Vida das Imagens e Teardown

- `destroySharedPoolSlots()` executa `m_window->setRenderTarget(QQuickRenderTarget{})` antes de `flushQuarantinedSemaphores()`, `vkDeviceWaitIdle(m_device)` (quando `!m_deviceLost`) e destruição de semáforos, `VkImage` e `VkDeviceMemory`.
- `shutdown()` destrói `m_rootItem`, limpa `renderTarget`, invalida `m_renderControl`, sincroniza GPU (`vkDeviceWaitIdle`) e destrói dispositivo e instância Vulkan em ordem segura.

---

## 8. Verificação do Limite de Três Frames

Auditoria de `tools/homologation/run_tty3_3frames_homolog.sh` e `IndependentFrameLoop.cpp`:

- Exige `--authorize-3frames` (`exit 3` `[BLOCKED]` sem flag; ignora `SGE_HOMOLOG_MAX_FRAMES=3` herdado).
- Recusa `root`, SSH e sessões gráficas; exige `/dev/tty3`, `XDG_VTNR=3` e sessão `systemd-logind` ativa em `tty3` (sem fallback `list-sessions`).
- Força `SGE_HOMOLOG_MAX_FRAMES=3` e `SGE_GRAPHICS_DIAGNOSTICS=1`.
- Usa `flock -n 9`, não reinicia automaticamente, preserva logs e propaga `RC`.
- `IndependentFrameLoop` bloqueia a 4ª submissão em `requestFrame()`, `scheduleQueuedProcess()` e `processNextFrame()` via `m_frameLimitReached`, encerrando apenas após `framePresented(3)` ou `frameFailed()`.

---

## 9. Correções Adicionais

Nenhuma correção adicional de produção foi necessária na revisão 03.2.4.6.22; foi adicionado o teste direcionado `auditPollSemanticsAndFailClosedSlotReuse()` em `tests/unit/tst_VulkanDmabufAudit.cpp` cobrindo `POLLIN` imediato/após espera, timeout, `POLLERR`, `POLLHUP`, `POLLNVAL`, FD inválido (`-1`) e estado indisponível após timeout/dupla tentativa.

---

## 10. Testes Realizados (após edições finais, em `Release` `-Werror`)

- **Testes direcionados (`4/4` em `1.16 sec`):** `tst_tty3_homolog_scripts`, `tst_independent_frame_loop`, `tst_vulkan_kms_sync`, `tst_vulkan_dmabuf_audit`.
- **CTest completo:** **`60/60` aprovados** (`100% tests passed, 0 tests failed out of 60` em `52.03 sec`: `39 unit`, `20 integration`, `1 e2e`).
- **`bash -n` nos 3 scripts (`run_tty3_homolog.sh`, `run_tty3_3frames_homolog.sh`, `verify_homolog_env.sh`):** aprovado.
- **Smoke tests seguros no prefixo instalado (`$HOME/.local/systalia-homolog/bin/systalia-session`):** `offscreen` (`poll_before_sigterm=None`, `sigterm_rc=0`) e `wayland` (`poll_before_sigterm=None`, `sigterm_rc=0`).
- **Diretório temporário de build (`/tmp/sge-build-0324622`):** removido após instalação e testes.

---

## 11. Instalação Isolada

- Prefixo `$HOME/.local/systalia-homolog` atualizado e validado via `./tools/homologation/verify_homolog_env.sh` (`VERIFY_RC=0`).
- Presença da correção de `poll()` confirmada no binário instalado (`skipped_sync_file_poll` verificado em `$HOME/.local/systalia-homolog/bin/systalia_glass_engine`).
- `BUILD_IDENTITY.txt` atualizado (`base_commit=75fa5ed`, `build_type=Release`, `cxx_flags=-Werror`, `qt_version=6.11.2`).

---

## 12. Riscos Residuais

- A correção ainda **não foi validada fisicamente** na GPU real com `OUT_FENCE_PTR` do DRM KMS; portanto, a causa do `VK_ERROR_DEVICE_LOST` no Frame 3 não pode ser declarada definitivamente resolvida até o reteste físico.
- O diagnóstico histórico do `ring gfx timeout` original permanece em aberto até validação física multi-frame.
- `/dev/dri/card1` permanece compartilhado com a sessão KDE Plasma na TTY2.

---

## 13. Checklist para Homologação Física (Não Executada)

1. Autorização humana explícita para o reteste de 3 frames.
2. Trabalho aberto salvo na TTY2.
3. Executar exclusivamente na TTY3: `./tools/homologation/run_tty3_3frames_homolog.sh --authorize-3frames`.
4. Retornar à TTY2 após conclusão ou falha e inspecionar `$HOME/systalia-homolog-logs/tty3-homolog-3frames-*.log` e `journalctl -k`.

---

## 14. Critérios de PASS/FAIL do Reteste Físico

- **PASS:** 3 `FRAME_START`/`GPU_SUBMITTED`, `RELEASE_FENCE_SIGNALED` com `importResult=skipped_sync_file_poll` no Frame 3, 3 `KMS_COMMIT_SUBMITTED` (`succeeded=true`), 3 `KMS_PAGE_FLIP_COMPLETED`, alternância `1 → 2 → 1`, ausência de 4ª submissão, `deviceLost=false`, ausência de `ring gfx timeout` e `rc=0`.
- **FAIL:** qualquer `VK_ERROR_DEVICE_LOST`, `radv/amdgpu: The CS has been rejected`, timeout de `poll()`/flip, `KMS_COMMIT_REJECTED`, 4ª submissão ou `ring gfx timeout`.

---

## 15. Decisão de Segurança

- **Revisão e pré-homologação (Etapa 03.2.4.6.22):** **PASS**.
- **Execução física na TTY3:** **BLOQUEADA aguardando autorização humana explícita**.
