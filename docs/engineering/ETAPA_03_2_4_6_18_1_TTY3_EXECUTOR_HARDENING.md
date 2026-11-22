# SYSTALIA — Etapa 03.2.4.6.18.1: Hardening Final do Executor TTY3

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Baseline:** 60/60 testes CTest (`39 unit`, `20 integration`, `1 e2e`)  
**Estado físico TTY3 (Fase B):** **BLOQUEADA — não executada nesta etapa**

---

## 1. Correções Aplicadas

### 1.1 `tools/homologation/run_tty3_homolog.sh`
- **Recusa estrita de ambiente não-TTY3:**
  - Bloqueia `root` (`$EUID -eq 0`).
  - Bloqueia execução via SSH (`SSH_CONNECTION`, `SSH_CLIENT`, `SSH_TTY`).
  - Bloqueia sessão gráfica (`WAYLAND_DISPLAY`, `DISPLAY`, `MIR_SOCKET`).
  - Exige terminal controlador exato `/dev/tty3` (`tty`).
  - Exige `XDG_VTNR == 3` e `XDG_SESSION_ID` não vazio, **removendo** qualquer fallback para `loginctl list-sessions`.
  - Valida via `loginctl show-session` que a sessão está vinculada exatamente a `TTY=tty3`, `Type=tty`, `State=active`, `Active=yes` e `VTNr=3`.
- **Validação prévia do binário e identidade:**
  - Verifica presença e validade de `$HOMOLOG_DIR/share/systalia/BUILD_IDENTITY.txt` (`branch=valo-graphics-2.0`, `build_type=Release`, `base_commit` hexadecimal >= 7 dígitos).
  - Verifica presença do mecanismo `SGE_HOMOLOG_MAX_FRAMES` no binário instalado (`grep -aq "SGE_HOMOLOG_MAX_FRAMES"`).
- **Sobrescrita obrigatória de variáveis de segurança:**
  - Força `export SGE_HOMOLOG_MAX_FRAMES=1` e `export SGE_GRAPHICS_DIAGNOSTICS=1` (além de `SGE_USE_DRM_BACKEND=1`), ignorando qualquer valor herdado (ex.: `SGE_HOMOLOG_MAX_FRAMES=0`).
- **Logs, flushing e monitoramento externo opcional:**
  - Utiliza `stdbuf -oL -eL` quando disponível e `sync` no arquivo de log antes/após execução.
  - Suporta monitoramento auxiliar opt-in do kernel (`SGE_ENABLE_KERNEL_WATCH=1` → `journalctl -k -f` em `$LOG_DIR/tty3-kernel-<timestamp>.log` com limpeza em `trap EXIT`), explicitando que o monitoramento externo **não** garante recuperação nem persistência integral em caso de GPU hang severo no kernel.
- **Preservação de isolamento e lock:**
  - Mantém `flock -n 9`, ausência de reinicialização automática e propagação exata do código de saída (`$RC`).

### 1.2 `tools/homologation/verify_homolog_env.sh`
- Passa a exigir `BUILD_IDENTITY.txt` válido (`branch=valo-graphics-2.0`, `build_type=Release`, `base_commit`) e presença de `SGE_HOMOLOG_MAX_FRAMES` no binário instalado, mantendo leitura estritamente não invasiva (sem DRM master, modeset ou troca de VT).

---

## 2. Testes Automatizados

Criada a suíte determinística `tests/unit/tst_Tty3HomologScripts.py` (registrada no CTest como `tst_tty3_homolog_scripts`), utilizando mocks isolados de `tty`, `loginctl` e binário em diretório temporário:

1. `test_refuses_tty2_and_graphical_session` — recusa `tty2` e `WAYLAND_DISPLAY`.
2. `test_refuses_tty4` — recusa `tty4` / `XDG_VTNR=4`.
3. `test_refuses_ssh` — recusa `SSH_CONNECTION`, `SSH_CLIENT` e `SSH_TTY`.
4. `test_refuses_inconsistent_logind_session` — recusa `XDG_SESSION_ID` ausente, `Active=no` e `XDG_VTNR` divergente.
5. `test_refuses_missing_or_invalid_build_identity` — recusa `BUILD_IDENTITY.txt` ausente ou inválido.
6. `test_forces_one_frame_and_diagnostics_ignoring_inherited_overrides` — confirma que `SGE_HOMOLOG_MAX_FRAMES=0` e `SGE_GRAPHICS_DIAGNOSTICS=0` herdados são sobrescritos para `MAX=1 DIAG=1 DRM=1`.
7. `test_blocks_concurrent_execution_with_flock` — bloqueia instância concorrente via `flock` antes de invocar o compositor.
8. `test_handles_pre_launch_failure_without_invoking_compositor` — trata falha de criação de diretório de logs antes de executar o compositor.

### Resultados executados (após edições finais)
- `bash -n tools/homologation/run_tty3_homolog.sh tools/homologation/verify_homolog_env.sh`: aprovado (`rc=0`).
- `python3 -m unittest -v tests/unit/tst_Tty3HomologScripts.py`: **8/8 aprovados** em `0.321 s`.
- `./tools/homologation/verify_homolog_env.sh`: aprovado (`[OK]` em todas as verificações).
- **Build Release (`-Werror`) + CTest completo:** **60/60 aprovados** (`100% tests passed, 0 tests failed out of 60`).

---

## 3. Riscos Remanescentes

- O executor endurecido garante pré-condições de sessão, lock e limite `SGE_HOMOLOG_MAX_FRAMES=1`, mas não impede um eventual `ring gfx timeout` dentro do kernel AMDGPU durante o primeiro frame.
- `/dev/dri/card1` permanece compartilhada com a sessão KDE Plasma (TTY2).

---

## 4. Decisão de Segurança

- **Executor TTY3 (`run_tty3_homolog.sh`) e pré-verificação (`verify_homolog_env.sh`):** **ENDURECIDOS e VALIDADOS** (`60/60` CTest).
- **Fase B (homologação física TTY3):** **BLOQUEADA** — não executada; aguarda autorização humana explícita.
