# SYSTALIA — Etapa 03.2.4.6: Primeira Homologação Física Bare-Metal Assistida — Fedora 44

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Commit base inspecionado:** `e6a1579` (com scripts de homologação em `tools/homologation/`)  
**Estado da homologação:** **PREPARADO — EXECUÇÃO FÍSICA PENDENTE**

---

## 1. Ambiente Físico

- **Sistema operacional:** Fedora Linux 44 (KDE Plasma Desktop Edition)
- **Qt:** `qt6-qtbase-6.11.2-3.fc44.x86_64`, `qt6-qtdeclarative-6.11.2-3.fc44.x86_64`, `qt6-qtwayland-6.11.2-1.fc44.x86_64`
- **GPU e nós DRM:** AMD Radeon Cezanne (`amdgpu` / Vulkan RADV) — `/dev/dri/card1` (`226:1`) e `/dev/dri/renderD128` (`226:128`)
- **Conectores físicos (leitura sysfs não invasiva):**
  - `card1-DP-1`: `disconnected`
  - `card1-HDMI-A-1`: `connected`, modo inicial `1920x1080`
- **Sessão ativa principal:** `loginctl` sessão `2`, usuário `lucasramos` (`UID=1000`), `seat0`, `tty2` (KDE Plasma).
- **Sessão alvo da homologação:** `TTY3` (execução assistida mediante autorização humana).

---

## 2. Commit e Build Testados

- **Branch / Commit:** `valo-graphics-2.0` @ `e6a1579`
- **Build isolada (Fase B):** `cmake -S . -B "$HOME/.cache/sge-build-homolog" -DCMAKE_BUILD_TYPE=Release`
- **Observação de CMake:** A flag `-DSGE_ENABLE_WARNINGS_AS_ERRORS=ON` não é uma variável CMake consumida pelo `CMakeLists.txt` (aviso `Manually-specified variables were not used by the project`), mantendo as flags padrão de Release do projeto. Compilação concluída com código de saída `0`.

---

## 3. Dependências Verificadas

Verificado via `./tools/homologation/verify_homolog_env.sh`:
- Todas as bibliotecas compartilhadas de `$HOME/.local/systalia-homolog/bin/systalia_glass_engine` e `$HOME/.local/systalia-homolog/bin/platforms/libsge_vulkan_offscreen.so` resolvidas sem `not found` via `ldd`.
- `libdrm` (`2.4.134`), `gbm` (`26.2.7`), `Wayland` (`1.26.0`), `Vulkan` (`1.4.341`), `libinput`, `udev` e `Qt6WaylandCompositor` presentes.
- Recursos QML e shaders `.qsb` compilados e embutidos no binário (`sge_core`, `sge_shaders`).

---

## 4. Resultado dos 57+ Testes (Fase C)

Executados na build `$HOME/.cache/sge-build-homolog`:

- **Suíte CTest completa:** **57/57 aprovados** (`100% tests passed, 0 tests failed out of 57`, `50.61 s`: `38 unit`, `18 integration`, `1 e2e`).
  - Inclui `tst_logind_integration`, `tst_input_hardware_backend`, `tst_drm_backend_poc`, `tst_drm_gbm_integration`, `tst_vulkan_quick_renderer`, `tst_independent_vulkan_session`, `tst_independent_frame_loop`, `tst_shell_root_integration`, `tst_unified_vulkan_gbm_pool`, `tst_vulkan_kms_sync` e `tst_page_flip_and_unified_startup`.
- **Smoke tests no binário instalado (`$HOME/.local/systalia-homolog/bin/systalia-session`):**
  - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
  - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 5. Instalação Isolada

Instalado exclusivamente no diretório do usuário (`$HOME/.local/systalia-homolog`, zero alterações em `/usr`):
- `$HOME/.local/systalia-homolog/bin/systalia_glass_engine`
- `$HOME/.local/systalia-homolog/bin/systalia-session`
- `$HOME/.local/systalia-homolog/bin/platforms/libsge_vulkan_offscreen.so`
- `$HOME/.local/systalia-homolog/share/wayland-sessions/systalia.desktop`
- `$HOME/.local/systalia-homolog/share/systalia/eglfs_kms.json`
- Logs reservados em: `$HOME/systalia-homolog-logs`

---

## 6. Auditoria Final de Segurança (Fase D)

- **Sessão:**
  - `run_tty3_homolog.sh` recusa execução como `root`, recusa `WAYLAND_DISPLAY`/`DISPLAY` ativos, bloqueia `tty2` (`XDG_VTNR=2`) e exige sessão `logind` `Type=tty` e `State=active`.
  - Sem bypass de `logind` (`SGE_ALLOW_NO_LOGIND` não definido); sem `SGE_USE_DRM_BACKEND=1` ativo por padrão.
- **DRM:**
  - `SgeVulkanOffscreenIntegration` (`src/platform/qpa/SgeVulkanOffscreenQpa.cpp`) não abre `/dev/dri/card*` e retorna `supportsPresent() = false`.
  - `LogindDrmProvider` adquire o único FD DRM via `LogindSessionService::takeDevice()` e aborta em caso de falha ou lease inativo.
- **KMS:**
  - Pool unificado `VulkanQuickRenderer::createSharedPoolSlots()` + `AtomicKmsPresenter::setUseExternalVulkanPool(true)` sem pool GBM concorrente.
  - Negociação de modificadores single-plane (`drmFormatModifierPlaneCount == 1`), `IN_FENCE_FD` / `OUT_FENCE_PTR` com fallback seguro via `poll()`, contextos `PendingFlipContext` com `generation` e `commitId`, e `TimedOutInScanout` preservado em timeout.
  - **Observação técnica de risco no primeiro commit:** `AtomicKmsPresenter::submitBufferForScanout()` envia `req.allowModeset = false` (sem `MODE_ID`/`ACTIVE`), assumindo que o CRTC da TTY3 (`fbcon`) já está ativo no modo preferencial `1920x1080` em `HDMI-A-1`. Caso o kernel retorne `-EINVAL` no primeiro `drmModeAtomicCommit`, o erro deve ser registrado nos logs sem improvisação em campo.
- **Recuperação:**
  - `SIGTERM`/`SIGINT` tratados via `socketpair` + `QCoreApplication::quit()` em `src/app/main.cpp`, executando shutdown em ordem reversa (`frameLoop` → `releaseKmsFramebuffers` → `destroySharedPoolSlots` → `vulkanRenderer` → `drmController` → `inputDeviceManager` → `logindSessionService`).

---

## 7. Comandos de Execução (SOMENTE após autorização humana)

1. **Verificação não invasiva (pode ser executada antes de ir para a TTY3):**
   ```bash
   ./tools/homologation/verify_homolog_env.sh
   ```
2. **Execução física assistida (EXCLUSIVAMENTE na TTY3 após `Ctrl+Alt+F3` e login como `lucasramos`):**
   ```bash
   cd ~/Documentos/systalia-glass-engine
   ./tools/homologation/run_tty3_homolog.sh
   ```

---

## 8. Procedimentos de Recuperação

- **Retorno imediato ao KDE Plasma:** pressionar `Ctrl+Alt+F2` para voltar à TTY2 (KDE Plasma intacto).
- **Encerramento limpo na TTY3:** pressionar `Ctrl+C` (`SIGINT`) no terminal da TTY3 ou executar a partir de outro terminal/TTY:
  ```bash
  pkill -TERM -u "$USER" systalia_glass_engine
  ```
- **Inspeção de logs após o teste:** verificar o arquivo gerado em `$HOME/systalia-homolog-logs/tty3-homolog-*.log`.

---

## 9. Resultados Físicos A–D e Classificação por Subsistema

*(Execução física ainda não realizada; nenhum teste físico foi simulado como aprovado.)*

| Subsistema | Estado | Observação |
|---|---|---|
| QPA (`sge_vulkan_offscreen`) | **NOT TESTED (físico)** | Compilado, `ldd` OK, validado em RADV offscreen |
| `systemd-logind` | **NOT TESTED (físico)** | Sessão `tty2` verificada; `TakeDevice` físico pendente |
| DRM master | **NOT TESTED (físico)** | Aguardando execução na TTY3 |
| KMS (`HDMI-A-1` 1920×1080) | **NOT TESTED (físico)** | Conector e modo detectados em sysfs; commit físico pendente |
| Vulkan (RADV) | **NOT TESTED (físico)** | Validado offscreen; apresentação física pendente |
| DMA-BUF | **NOT TESTED (físico)** | Exportação validada em CTest; `drmModeAddFB2WithModifiers` físico pendente |
| Sincronização (`SYNC_FD`/`IN_FENCE_FD`/`OUT_FENCE_PTR`) | **NOT TESTED (físico)** | Validada em RADV + mock KMS; kernel `amdgpu` físico pendente |
| Page-flip (`drmHandleEvent`) | **NOT TESTED (físico)** | Contexto e ponte validados em CTest; evento real pendente |
| `QtWaylandCompositor` | **NOT TESTED (físico)** | Validado em CTest/nested; bare-metal pendente |
| VALO V2 (`ShellRoot.qml`) | **NOT TESTED (físico)** | Validado em `tst_shell_root_integration` e nested; TTY3 pendente |
| Input (`libinput`) | **NOT TESTED (físico)** | Compilado e validado em unit tests; físico pendente |
| Troca de VT (TTY3 ↔ TTY2) | **NOT TESTED (físico)** | Teste D pendente |
| Shutdown | **NOT TESTED (físico)** | Validado em headless/nested (`sigterm_rc=0`); bare-metal pendente |

---

## 10. Logs Relevantes

- Logs da execução física serão gravados exclusivamente em `$HOME/systalia-homolog-logs/tty3-homolog-<timestamp>.log`.
- Nenhum log extenso ou dump temporário foi gravado em `docs/` ou `artifacts/`.

---

## 11. Falhas e Limitações

- Nenhuma falha nos 57 testes CTest nem nos smoke tests headless/nested.
- Limitação conhecida pré-teste: o primeiro `drmModeAtomicCommit` não força `DRM_MODE_ATOMIC_ALLOW_MODESET` (`MODE_ID`/`ACTIVE`), contando com o CRTC já ativo na TTY3 em `1920x1080`.

---

## 12. Estado da Homologação

**PREPARADO — EXECUÇÃO FÍSICA PENDENTE**

Todos os critérios de **Preparação concluída** foram atendidos (build aprovada, 57/57 CTest, smoke tests headless/nested aprovados, scripts validados, procedimento de recuperação documentado, nenhum bloqueador crítico de preparação). A **Homologação física concluída** aguarda autorização humana e execução dos Testes A–D na TTY3.

---

## 13. Próximas Ações

1. Usuário revisar este relatório e autorizar a execução assistida.
2. Na TTY3 (`Ctrl+Alt+F3`), executar `./tools/homologation/run_tty3_homolog.sh` e observar progressivamente os Testes Físicos A, B, C e D.
3. Registrar os resultados reais e os logs de `$HOME/systalia-homolog-logs/` para encerrar ou corrigir eventuais falhas físicas antes de qualquer nova tentativa.
