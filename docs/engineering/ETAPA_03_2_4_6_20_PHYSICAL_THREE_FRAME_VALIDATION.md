# SYSTALIA — Etapa 03.2.4.6.20: Homologação Física Assistida de 3 Frames — Vulkan, DMA-BUF e DRM/KMS

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`, `vendor=0x1002`, `device=0x1638`)  
**Baseline:** 60/60 CTest (`39 unit`, `20 integration`, `1 e2e`)  
**Estado físico:** **1 frame validado (`tty3-homolog-20261122-154428.log`); experimento físico de 3 frames NÃO EXECUTADO (aguardando autorização humana explícita)**

---

## 1. Identidade do Build

Instalação isolada em `$HOME/.local/systalia-homolog` atualizada a partir de compilação limpa `Release` com `-Werror`:

- `repo=LucasRamosSilva-15/systalia-glass-engine`
- `branch=valo-graphics-2.0`
- `base_commit=546cb1c`
- `dirty_worktree=0`
- `build_type=Release`
- `cxx_flags=-Werror`
- `qt_version=6.11.2`
- `installed_at=2026-11-22T16:07:44-03:00`

---

## 2. Pré-verificações

Executado `./tools/homologation/verify_homolog_env.sh` sem iniciar DRM master (`VERIFY_RC=0`):

- **Artefatos instalados:** `bin/systalia_glass_engine`, `bin/systalia-session`, `bin/platforms/libsge_vulkan_offscreen.so`, `share/wayland-sessions/systalia.desktop`, `share/systalia/eglfs_kms.json` e `share/systalia/BUILD_IDENTITY.txt` todos `[OK]`.
- **Bibliotecas (`ldd`):** todas as dependências compartilhadas de `systalia_glass_engine` e `libsge_vulkan_offscreen.so` resolvidas (`[OK]`).
- **GPU e nós DRM:** `/dev/dri/card1` e `/dev/dri/renderD128` presentes (`vendor=0x1002`, `device=0x1638`).
- **Conector físico (sysfs):** `card1-HDMI-A-1: status=connected preferred_or_first_mode=1920x1080`; `card1-DP-1: status=disconnected`.
- **Estado `systemd-logind`:** sessão `2` em `tty2` (`seat0`) e sessão `4` em `tty3` (`seat0`); `journalctl` disponível.
- **Diretório de logs:** `/home/lucasramos/systalia-homolog-logs` (`27G` livres).
- **Limite de frames:** mecanismo `SGE_HOMOLOG_MAX_FRAMES` presente no binário instalado; `SGE_USE_DRM_BACKEND` desativado por padrão.
- **Execução concorrente:** nenhum processo `systalia_glass_engine` ou `run_tty3*` ativo; arquivo de lock `/run/user/1000/sge-homolog-1000.lock` livre.
- **Suíte automatizada (`ctest --output-on-failure` em `Release`):** **60/60 aprovados** (`100% tests passed, 0 tests failed out of 60` em `51.82 s`: `39 unit`, `20 integration`, `1 e2e`), além de `bash -n` aprovado nos 3 scripts de `tools/homologation/`.
- **Procedimento de recuperação:** retornar da TTY3 à TTY2 (`Ctrl+Alt+F2`) após conclusão ou falha; se necessário, inspecionar `journalctl -k` para eventos `amdgpu`/`drm`.

---

## 3. Proteções do Executor (`tools/homologation/run_tty3_3frames_homolog.sh`)

Inspeção integral e comparação com `tools/homologation/run_tty3_homolog.sh` confirmam ausência de regressões e paridade completa de hardening:

| Proteção | `run_tty3_homolog.sh` (1 frame) | `run_tty3_3frames_homolog.sh` (3 frames) |
|---|---|---|
| Flag explícita obrigatória | N/A (1 frame fixo) | Exige `--authorize-3frames` (`exit 3` `[BLOCKED]` sem flag; ignora `SGE_HOMOLOG_MAX_FRAMES=3` herdado) |
| Recusa `root` (`EUID -eq 0`) | Sim (`exit 1`) | Sim (`exit 1`) |
| Recusa SSH (`SSH_CONNECTION/CLIENT/TTY`) | Sim (`exit 1`) | Sim (`exit 1`) |
| Recusa sessão gráfica (`WAYLAND_DISPLAY/DISPLAY/MIR_SOCKET`) | Sim (`exit 1`) | Sim (`exit 1`) |
| Exige terminal controlador `/dev/tty3` | Sim (`exit 1`) | Sim (`exit 1`) |
| Exige `XDG_VTNR=3` | Sim (`exit 1`) | Sim (`exit 1`) |
| Exige `XDG_SESSION_ID` e sessão `logind` ativa em `tty3` | Sim (`exit 1`, sem fallback `list-sessions`) | Sim (`exit 1`, sem fallback `list-sessions`) |
| Valida `BUILD_IDENTITY.txt` e `SGE_HOMOLOG_MAX_FRAMES` | Sim (`exit 1`) | Sim (`exit 1`) |
| Lock exclusivo `flock -n 9` | Sim (`$XDG_RUNTIME_DIR/sge-homolog-$UID.lock`) | Sim (`$XDG_RUNTIME_DIR/sge-homolog-$UID.lock`) |
| Força `SGE_HOMOLOG_MAX_FRAMES` e `SGE_GRAPHICS_DIAGNOSTICS` | `SGE_HOMOLOG_MAX_FRAMES=1`, `SGE_GRAPHICS_DIAGNOSTICS=1` | `SGE_HOMOLOG_MAX_FRAMES=3`, `SGE_GRAPHICS_DIAGNOSTICS=1` |
| Sem reinicialização automática | Sim | Sim |
| Preserva logs (`stdbuf -oL -eL` + `sync`) e propaga `RC` | Sim (`tty3-homolog-*.log`) | Sim (`tty3-homolog-3frames-*.log`) |

---

## 4. Execução Física

**NÃO EXECUTADO.**  
Nenhuma execução física com `SGE_USE_DRM_BACKEND=1`, `TakeDevice`, troca de VT ou Atomic Commit real foi realizada nesta etapa, conforme restrição explícita.

---

## 5. Submissões Vulkan

**NÃO EXECUTADO** (aguardando execução física autorizada na TTY3).

Contrato verificado no código (`IndependentFrameLoop.cpp` + `VulkanQuickRenderer.cpp`):
- `IndependentFrameLoop` contabiliza exclusivamente submissões reais de `renderIntoBuffer(*buf)` até `m_maxSubmittedFrames = 3`.
- Ao atingir `3/3`, ativa `m_frameLimitReached = true`, emite `HOMOLOG_FRAME_LIMIT_REACHED submittedFrames=3 maxFrames=3` e bloqueia qualquer 4ª submissão em `requestFrame()`, `scheduleQueuedProcess()` e `processNextFrame()`.

---

## 6. Atomic Commits

**NÃO EXECUTADO** (aguardando execução física autorizada na TTY3).

Contrato esperado na execução:
- Commit 1 (`allowModeset=true` em `InitialModesetPending`), seguido de Commits 2 e 3 (`allowModeset=false` em `Active`), registrando `KMS_COMMIT_SUBMITTED` com `bufferId`, `fbId`, `commitId`, `inFenceFd`, `outFenceFd` e `succeeded=true`.

---

## 7. Page-flips

**NÃO EXECUTADO** (aguardando execução física autorizada na TTY3).

Contrato esperado na execução:
- Três eventos reais `KMS_PAGE_FLIP_COMPLETED` emitidos por `DrmEventDispatcher` após retorno de eventos DRM reais (nunca por mocks), mantendo os 3 page-flips pendentes em voo normalmente mesmo após `m_frameLimitReached = true` no 3º frame.

---

## 8. Alternância de Slots

**NÃO EXECUTADO** (aguardando execução física autorizada na TTY3).

Sequência contratual esperada para 2 slots (`GbmBufferManager`):

| Frame | Slot | Apresentação esperada | Regra de liberação e reuso |
|---|---|---|---|
| 1 | 1 | Primeira apresentação | Nenhum slot anterior liberado |
| 2 | 2 | Substitui slot 1 | Slot 1 retorna a `Available` após `KMS_PAGE_FLIP_COMPLETED` do Frame 2 com `releaseFenceFd` do commit 2 |
| 3 | 1 | Reutiliza slot 1 | Slot 1 só é sobrescrito após `RELEASE_FENCE_IMPORTED` ou `RELEASE_FENCE_SIGNALED` (não basta `KMS_BUFFER_RELEASED` isolado); após flip 3, slot 2 retorna a `Available` |

---

## 9. Release Fences

**NÃO EXECUTADO** (aguardando execução física autorizada na TTY3).

Instrumentação ativa em `AtomicKmsPresenter.cpp`, `VulkanQuickRenderer.cpp` e `DrmEventDispatcher.cpp` sob `SGE_GRAPHICS_DIAGNOSTICS=1`:
- `FRAME_START`: registra `frameId`, `slotId`, `mode=dmabufPool`, `vkFormat`, `modifier`.
- `GPU_SUBMITTED`: registra `frameId` e `slotId`.
- `KMS_COMMIT_SUBMITTED` e `RELEASE_FENCE_CREATED`: correlacionam `commitId`, `bufferId` e `outFenceFd` (mantendo distinção explícita quando `outFenceFd=-1`).
- `KMS_PAGE_FLIP_COMPLETED` e `KMS_BUFFER_RELEASED`: correlacionam `seq`, `commitId` e `bufferId`.
- `RELEASE_FENCE_IMPORTED` / `RELEASE_FENCE_SIGNALED`: registram `frameId`, `slotId` e `releaseFenceFd` no reuso do slot.
- `RELEASE_FENCE_CLOSED`: registra fechamento de FD (`slotId`, `releaseFenceFd`), distinto de sinalização efetiva.

---

## 10. Encerramento

**NÃO EXECUTADO** (aguardando execução física autorizada na TTY3).

Expectativa contratual: encerramento controlado após `framePresented(3)` com `rc=0` (ou parada imediata sem repetição de frames via `m_haltedOnError` em caso de falha).

---

## 11. Logs do Kernel

**NÃO EXECUTADO** (aguardando execução física autorizada na TTY3).

Após a execução futura autorizada na TTY3 e retorno à TTY2, inspecionar `journalctl -k` para o intervalo do teste (verificando ausência de `amdgpu: ring gfx timeout`, erros de modeset ou resets de GPU).

---

## 12. Resultado

**NÃO EXECUTADO — PRONTO PARA SOLICITAÇÃO DE AUTORIZAÇÃO HUMANA.**

Critérios para declarar **PASS** após execução física autorizada:
- 3 submissões Vulkan (`FRAME_START` / `GPU_SUBMITTED`), 3 `KMS_COMMIT_SUBMITTED` (`succeeded=true`), 3 `KMS_PAGE_FLIP_COMPLETED` reais.
- Alternância de slots `1 → 2 → 1` com reuso do slot 1 somente após `RELEASE_FENCE_IMPORTED` ou `RELEASE_FENCE_SIGNALED`.
- Nenhuma 4ª submissão (`HOMOLOG_FRAME_LIMIT_REACHED submittedFrames=3 maxFrames=3`).
- Ausência de `VK_ERROR_DEVICE_LOST`, `GPU_COMPLETION_TIMED_OUT`, `KMS_COMMIT_REJECTED`, timeout de flip/fence ou `ring gfx timeout`, e saída com `rc=0`.
- Ausência de qualquer evidência necessária implicará classificação **inconclusivo**.

---

## 13. Riscos Restantes

- A causa histórica do `amdgpu: ring gfx timeout` da primeira homologação física permanece sem diagnóstico definitivo fechado; 3 frames bem-sucedidos validarão alternância e release fences, mas **não comprovam estabilidade prolongada**.
- `/dev/dri/card1` permanece compartilhado com a sessão KDE Plasma ativa em TTY2.

---

## 14. Recomendação da Próxima Etapa

1. Solicitar autorização humana explícita para executar na TTY3:
   ```bash
   ./tools/homologation/run_tty3_3frames_homolog.sh --authorize-3frames
   ```
2. Salvar todo o trabalho aberto antes do teste, capturar o log em `$HOME/systalia-homolog-logs/tty3-homolog-3frames-*.log` e inspecionar `journalctl -k` após o retorno.
3. Somente após análise do log real de 3 frames (e classificação PASS/FAIL/inconclusivo), decidir os próximos passos de duração estendida.
