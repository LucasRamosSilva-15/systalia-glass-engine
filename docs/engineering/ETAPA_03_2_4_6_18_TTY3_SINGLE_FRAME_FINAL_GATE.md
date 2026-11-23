# SYSTALIA — Etapa 03.2.4.6.18: Homologação Física Assistida de Um Frame — Validação Final e Execução Condicional

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, QtWaylandCompositor, GBM e DRM/KMS Atomic  
**GPU de referência:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline de regressão:** 59/59 testes CTest (`38 unit`, `20 integration`, `1 e2e`)  
**Situação da Fase B (homologação física):** **BLOQUEADA — aguardando autorização humana específica**

---

## 1. Auditoria Inicial (Fase A)

Inspecionados integralmente `docs/engineering/ETAPA_03_2_4_6_17_TTY3_SINGLE_FRAME_PREFLIGHT.md` e os módulos:
- `src/platform/drm/IndependentFrameLoop.hpp` / `.cpp`
- `src/platform/drm/AtomicKmsPresenter.hpp` / `.cpp`
- `src/platform/drm/DrmEventDispatcher.hpp` / `.cpp`
- `src/platform/drm/VulkanQuickRenderer.hpp` / `.cpp`
- `src/app/main.cpp`
- `tools/homologation/run_tty3_homolog.sh`
- `tools/homologation/verify_homolog_env.sh`

---

## 2. Fluxo do Primeiro Frame (`SGE_HOMOLOG_MAX_FRAMES=1`)

Sequência validada:
`renderIntoBuffer → HOMOLOG_FRAME_LIMIT_REACHED → Atomic Commit (MODE_ID + ACTIVE=1 + ALLOW_MODESET + IN_FENCE_FD + OUT_FENCE_PTR) → KMS_PAGE_FLIP_COMPLETED → framePresented → QCoreApplication::exit(0)`

- `IndependentFrameLoop::onPageFlipCompleted()` **não** possui early return sobre `m_frameLimitReached`: mesmo com `m_frameLimitReached == true`, o evento de page-flip do primeiro frame conclui `m_presenter.onPageFlipCompleted(bufferId)`, `m_waylandCoordinator.completeFrame(presented)`, incrementa `m_presentedCount` para `1` e emite `framePresented(1)`.
- Ao final de `onPageFlipCompleted()`, `if (m_renderPending && ... && !m_frameLimitReached && !m_haltedOnError)` impede qualquer segundo agendamento.
- Em `src/app/main.cpp`, `framePresented` verifica `frameLoop.isFrameLimitReached()` e chama `QCoreApplication::exit(0)`.

---

## 3. Contrato de `frameLimitReached`

- Contabilizado apenas após `m_renderSource.renderIntoBuffer(*buf)` retornar `true`.
- Define `m_frameLimitReached = true`, limpa `m_renderPending` e `m_scheduled`, emite `frameLimitReached(1)` e registra `[SGE-DIAG] HOMOLOG_FRAME_LIMIT_REACHED`.
- Falhas anteriores à submissão (render, `markBufferReadyForScanout`, `submitBufferForScanout`, timeout ou `deviceLost`) não incrementam `m_submittedFrameCount`, ativam `m_haltedOnError = true` (ou `m_deviceLost = true`) e emitem `frameFailed()` / `deviceLost()`, encerrando `app.exec()` sem espera infinita nem retry automático.

---

## 4. Contrato de `framePresented`

- Emitido **exclusivamente** em `IndependentFrameLoop::onPageFlipCompleted()` após `m_presenter.onPageFlipCompleted(bufferId)` retornar `true`.
- Validado por `QSignalSpy` em `tst_IndependentFrameLoop::strictOneFrameHomologLimitBlocksSecondFrameAndHaltsOnFailure()`:
  - `presentedSpy.count() == 1` (`1ULL`) após o primeiro flip.
  - `requestFrame()` adicional após o flip resulta em `0` novas submissões (`renderCalls == 1`, `commitCalls == 1`).

---

## 5. Correções Aplicadas (Fase A)

- `tests/unit/tst_IndependentFrameLoop.cpp`: adicionada verificação explícita via `QSignalSpy` (`presentedSpy`) em `strictOneFrameHomologLimitBlocksSecondFrameAndHaltsOnFailure()` comprovando emissão única de `framePresented(1)` após o primeiro page-flip e bloqueio total de segundo frame.
- Nenhuma alteração estrutural foi necessária no código de produção nesta etapa; todos os 12 eventos esperados de diagnóstico já estão presentes em seus pontos reais de emissão.

---

## 6. Testes de Regressão (após edição final)

- **Build Release (`-Werror`):** compilação limpa sem erros/warnings.
- **Testes específicos (`5/5` em `2.03 s`):** `tst_independent_frame_loop`, `tst_shell_root_integration`, `tst_page_flip_and_unified_startup`, `tst_device_lost_and_compositor_lifecycle`, `tst_vulkan_dmabuf_audit`.
- **CTest completo:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`) em `51.28 s` (`38 unit`, `20 integration`, `1 e2e`).
- **Smoke tests (`$HOME/.local/systalia-homolog/bin/systalia-session`):**
  - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
  - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 7. Verificação da Instalação (`$HOME/.local/systalia-homolog`)

Validada via `./tools/homologation/verify_homolog_env.sh`:
- Binários (`systalia_glass_engine`, `systalia-session`), QPA (`libsge_vulkan_offscreen.so`), `systalia.desktop`, `eglfs_kms.json` e `BUILD_IDENTITY.txt` presentes e atualizados.
- `ldd` sem bibliotecas ausentes.
- GPU `/dev/dri/card1` identificada (`vendor=0x1002 device=0x1638`), conector `card1-HDMI-A-1` (`1920x1080`).
- `SGE_HOMOLOG_MAX_FRAMES=1`, `SGE_GRAPHICS_DIAGNOSTICS=1`, `SGE_USE_DRM_BACKEND` desativado por padrão.

---

## 8. Procedimento de Captura (Fase B — Não Executada)

Quando autorizada separadamente, a execução ocorrerá exclusivamente em TTY3 puro via:
`./tools/homologation/run_tty3_homolog.sh`
- Grava log persistente em `$HOME/systalia-homolog-logs/tty3-homolog-<timestamp>.log` com `sync` ao final.
- Após o retorno, coletar logs do kernel:
  `journalctl -k -b --since "<timestamp>" > "$HOME/systalia-homolog-logs/kernel-<timestamp>.log"`
- Eventos monitorados: `VULKAN_DEVICE_SELECTED`, `KMS_MODE_DISCOVERED`, `QPA_GEOMETRY_APPLIED`, `KMS_MODIFIER_NEGOTIATED`, `CPU_COMMAND_RECORDED`, `GPU_SUBMITTED`, `SYNC_FD exported`, `HOMOLOG_FRAME_LIMIT_REACHED`, `KMS_INITIAL_MODESET_REQUESTED`, `KMS_COMMIT_SUBMITTED`, `KMS_PAGE_FLIP_COMPLETED`, `RENDERER_SHUTDOWN_COMPLETE`.

---

## 9. Condições de Recuperação

- Encerramento automático em `rc=0` após o primeiro page-flip, `rc=1` em falha controlada ou `rc=2` em `VK_ERROR_DEVICE_LOST`.
- Retorno via `Ctrl+Alt+F2` ou contingência SSH; rollback imediato da instalação via `$HOME/.local/systalia-homolog.prev`.
- Sem reinicialização automática; `Ctrl+Alt+F2`, SSH e `SIGKILL` podem falhar se ocorrer novo hang no kernel AMDGPU.

---

## 10. Riscos Residuais

- A causa definitiva do `amdgpu: ring gfx timeout` original permanece desconhecida; o GPU hang **não** é declarado resolvido.
- `/dev/dri/card1` é compartilhada com a sessão KDE Plasma em TTY2.

---

## 11. Checklist de Autorização Física (Fase B)

- [x] Fase A concluída (`59/59` CTest, instalação `$HOME/.local/systalia-homolog` sincronizada, `verify_homolog_env.sh` `[OK]`).
- [ ] Trabalho do usuário salvo e backup concluído.
- [ ] Autorização humana explícita para executar a Fase B na TTY3.
- [ ] Plano de coleta `journalctl -k` e contingência (`Ctrl+Alt+F2` / SSH) prontos.

---

## 12. Decisão Final de Segurança

- **Fase A:** **APROVADA e CONCLUÍDA**.
- **Fase B (homologação física TTY3 de 1 frame):** **BLOQUEADA aguardando autorização humana explícita**.
- **Critério de PASS físico futuro:** requer comprovação real de `KMS_PAGE_FLIP_COMPLETED` sem `VK_ERROR_DEVICE_LOST` nem `ring gfx timeout`; saída `rc=0` sem comprovação do page-flip físico deve ser classificada como **inconclusiva**.
