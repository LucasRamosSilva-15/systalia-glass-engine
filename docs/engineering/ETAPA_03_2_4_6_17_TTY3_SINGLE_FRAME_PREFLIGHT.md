# SYSTALIA — Etapa 03.2.4.6.17: Preparação Final da Homologação Física, Instalação Isolada e Limite Estrito de Um Frame

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU de referência:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline de regressão:** 59/59 testes CTest (`38 unit`, `20 integration`, `1 e2e`)  
**Estado físico TTY3:** **BLOQUEADA até autorização humana específica**

---

## 1. Resumo Executivo

Esta etapa preparou tecnicamente uma única execução física controlada de **1 frame** do SYSTALIA na TTY3, sem realizá-la:

- Implementado o limite experimental opt-in **`SGE_HOMOLOG_MAX_FRAMES=1`** em `IndependentFrameLoop` e `src/app/main.cpp`, contabilizando apenas submissões reais do renderer, bloqueando novos pedidos (`renderRequested`, `sceneChanged`, timers/callbacks) após atingir o limite, entrando em falha controlada em erro e encerrando com segurança somente após o page-flip real (ou falha controlada).
- Atualizada a instalação isolada em **`$HOME/.local/systalia-homolog`** (com backup de rollback em `$HOME/.local/systalia-homolog.prev`), validada via `verify_homolog_env.sh`, `ldd`, `BUILD_IDENTITY.txt` e smoke tests headless e nested Wayland.
- A causa definitiva do `ring gfx timeout` original (`11:02:53`) permanece desconhecida; o GPU hang **não** é declarado resolvido.

---

## 2. Auditoria do Frame Loop

Em `src/platform/drm/IndependentFrameLoop.hpp` / `.cpp` e `src/app/main.cpp`:
- Anteriormente, `QQuickRenderControl::renderRequested` e `sceneChanged` estavam conectados diretamente a `IndependentFrameLoop::requestFrame()` sem limite de frames submetidos, dependendo apenas de `SIGTERM`/`timeout` externo.
- Agora, `IndependentFrameLoop` lê `SGE_HOMOLOG_MAX_FRAMES` no construtor (e aceita `setMaxSubmittedFrames()`), preservando comportamento ilimitado (`m_maxSubmittedFrames == 0`) quando a variável está ausente.

---

## 3. Limite Estrito de Um Frame (`SGE_HOMOLOG_MAX_FRAMES=1`)

Requisitos atendidos:
1. **Contagem de submissões reais:** `m_submittedFrameCount` só incrementa após `m_renderSource.renderIntoBuffer(*buf)` retornar `true` (falhas anteriores à submissão não incrementam o contador).
2. **Bloqueio após limite:** ao atingir `m_maxSubmittedFrames`, define `m_frameLimitReached = true`, limpa `m_renderPending`/`m_scheduled`, emite `frameLimitReached()` e registra `[SGE-DIAG] HOMOLOG_FRAME_LIMIT_REACHED`.
3. **Bloqueio de callbacks adicionais:** `requestFrame()`, `scheduleQueuedProcess()`, `processNextFrame()`, `onPageFlipCompleted()` e `onPageFlipTimedOut()` verificam `m_frameLimitReached` e `m_haltedOnError`, impedindo qualquer segundo agendamento.
4. **Sem reuso de slots após o limite:** nenhum novo `acquireBufferForRendering()` ocorre após `m_frameLimitReached`.
5. **Encerramento sincronizado:** em `main.cpp`, `framePresented` encerra `QCoreApplication::exit(0)` somente após o page-flip real completar; `frameFailed` encerra com `QCoreApplication::exit(1)` em falha controlada (`m_haltedOnError`).
6. **Limitação explícita:** `SGE_HOMOLOG_MAX_FRAMES=1` impede um segundo frame no espaço de usuário, mas **não impede** um eventual travamento do kernel AMDGPU durante a execução do primeiro frame.

---

## 4. Primeiro Atomic Commit

Revalidado em `AtomicKmsPresenter` (`Unconfigured → InitialModesetPending → Active → Suspended`) e `tst_IndependentFrameLoop` + `tst_VulkanDmabufAudit`:
- `MODE_ID` (blob de `drmModeModeInfo`), `ACTIVE=1`, `CRTC_ID` no conector e plano, `FB_ID`, `SRC_X/Y/W/H`, `CRTC_X/Y/W/H`, `IN_FENCE_FD`, `OUT_FENCE_PTR` e `DRM_MODE_ATOMIC_ALLOW_MODESET` apenas no primeiro commit (`InitialModesetPending`).
- Confirmado por teste determinístico que nenhum segundo commit é disparado após `SGE_HOMOLOG_MAX_FRAMES=1` ou durante o shutdown.

---

## 5. Instalação Experimental Isolada (`$HOME/.local/systalia-homolog`)

- Prefixo anterior preservado em `$HOME/.local/systalia-homolog.prev` para rollback imediato.
- Instalação limpa executada via `cmake --install` (build Release `-Werror`), contendo:
  - `$HOME/.local/systalia-homolog/bin/systalia_glass_engine`
  - `$HOME/.local/systalia-homolog/bin/systalia-session`
  - `$HOME/.local/systalia-homolog/bin/platforms/libsge_vulkan_offscreen.so`
  - `$HOME/.local/systalia-homolog/share/wayland-sessions/systalia.desktop`
  - `$HOME/.local/systalia-homolog/share/systalia/eglfs_kms.json`
  - `$HOME/.local/systalia-homolog/share/systalia/BUILD_IDENTITY.txt`
- Nenhuma instalação global, sessão KDE Plasma, display manager ou parâmetro AMDGPU foi modificado.
- **Rollback da instalação:**
  `rm -rf "$HOME/.local/systalia-homolog" && mv "$HOME/.local/systalia-homolog.prev" "$HOME/.local/systalia-homolog"`

---

## 6. Identidade do Build

Registrada em `$HOME/.local/systalia-homolog/share/systalia/BUILD_IDENTITY.txt` (branch `valo-graphics-2.0`, `build_type=Release`, `cxx_flags=-Werror`, `qt_version=6.11.2`, timestamp de instalação e contagem de modificações locais).

---

## 7. Pré-verificação Não Invasiva (`tools/homologation/verify_homolog_env.sh`)

Ampliada e executada sem abrir DRM master, modeset ou sessão física:
- Valida usuário não-root, presença de `/dev/dri/card1` e `renderD128`, GPU `vendor=0x1002 device=0x1638`, conectores sysfs, `ldd` sem bibliotecas faltantes, `BUILD_IDENTITY.txt`, espaço livre em `$HOME/systalia-homolog-logs`, sessões `loginctl`, disponibilidade de `journalctl`, e defaults `SGE_HOMOLOG_MAX_FRAMES=1` / `SGE_GRAPHICS_DIAGNOSTICS=1`.

---

## 8. Captura Persistente de Logs

- `tools/homologation/run_tty3_homolog.sh` grava em `$HOME/systalia-homolog-logs/tty3-homolog-<timestamp>.log` (fora do repositório), executando `sync` no arquivo de log ao encerrar.
- Captura pós-teste recomendada fora do repositório:
  `journalctl -k -b --since "<timestamp>" > "$HOME/systalia-homolog-logs/kernel-<timestamp>.log"`
- **Limitação:** em caso de GPU hang severo ou reboot forçado, buffers em memória do kernel/filesystem podem não ser gravados completamente antes da perda do sistema; quando possível, usar máquina separada ou sessão SSH auxiliar observando `dmesg -w`.

---

## 9. Procedimento de Recuperação

- **Encerramento normal:** com `SGE_HOMOLOG_MAX_FRAMES=1`, o processo encerra sozinho após o primeiro page-flip (`rc=0`) ou falha controlada (`rc=1` / `rc=2` em `deviceLost`).
- **Falha de submissão / ausência de flip:** verificar `HOMOLOG_FRAME_LIMIT_REACHED`, `KMS_COMMIT_REJECTED`, `DEVICE_LOST` ou ausência de `KMS_PAGE_FLIP_COMPLETED` no log em `$HOME/systalia-homolog-logs/`.
- **Retorno e contingência:** `Ctrl+Alt+F2` para retornar à TTY2 ou acesso via SSH; verificar que `run_tty3_homolog.sh` nunca reinicia automaticamente (e usa `flock`).
- **Restauração da instalação:** usar o comando de rollback da Seção 5.
- **Aviso crítico:** `Ctrl+Alt+F2`, SSH e `SIGKILL` podem deixar de responder se um GPU hang travar o subsistema gráfico/kernel.

---

## 10. Testes Automatizados (após edições finais)

- **Build Release (`-Werror`):** compilação limpa.
- **Testes específicos (`5/5` em `2.36 s`):** `tst_independent_frame_loop` (`strictOneFrameHomologLimitBlocksSecondFrameAndHaltsOnFailure`), `tst_shell_root_integration`, `tst_page_flip_and_unified_startup`, `tst_device_lost_and_compositor_lifecycle`, `tst_vulkan_dmabuf_audit`.
- **CTest completo:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`) em `51.43 s` (`38 unit`, `20 integration`, `1 e2e`).
- **Pré-verificação (`verify_homolog_env.sh`):** `[OK]` em todas as checagens.
- **Smoke tests (`$HOME/.local/systalia-homolog/bin/systalia-session`):**
  - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
  - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 11. Riscos Residuais

- O GPU hang original (`amdgpu: ring gfx timeout`) em hardware físico durante o Frame 1 permanece sem causa definitiva comprovada.
- A GPU `/dev/dri/card1` é compartilhada com a sessão KDE Plasma em TTY2; recomenda-se salvar todo o trabalho e, idealmente, testar em ambiente/máquina isolada.

---

## 12. Critérios para Autorização Física (Execução Assistida de 1 Frame)

1. Autorização humana específica para o experimento físico na TTY3.
2. Backup e salvamento integral do trabalho aberto.
3. Confirmação de `BUILD_IDENTITY.txt` em `$HOME/.local/systalia-homolog`.
4. Execução prévia de `./tools/homologation/verify_homolog_env.sh`.
5. Confirmação de `SGE_HOMOLOG_MAX_FRAMES=1` e `SGE_GRAPHICS_DIAGNOSTICS=1`.
6. Captura persistente de logs (`tty3-homolog-*.log` + `journalctl -k`).
7. Plano de recuperação pronto (`Ctrl+Alt+F2` / SSH / rollback).
8. Execução estritamente sem `root`.
9. Interrupção imediata diante de qualquer falha.
10. Proibição de reinicialização automática.

---

## 13. Decisão Final de Segurança

- **Execução automática nesta etapa:** **BLOQUEADA (nenhum teste físico executado)**.
- **Prontidão técnica para solicitar autorização:** **SIM — tecnicamente preparado** para solicitar uma autorização humana separada e específica para o teste físico assistido de **1 frame** na TTY3 (`./tools/homologation/run_tty3_homolog.sh`).
