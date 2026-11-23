# SYSTALIA — Etapa 03.2.4: Homologação Física da Sessão Fedora Independente

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Commit base auditado:** `e77411e` (com alterações locais das Etapas 03.2.1, 03.2.2, 03.2.3 e 03.2.4 não commitadas)
- **Ambiente de referência:** Fedora Linux 44, Qt 6.11.2, `libinput-devel-1.31.3-1.fc44.x86_64`, `libudev 259`
- **Decisão técnica:** **PARCIAL** *(auditoria, correções preventivas, build Release, 48/48 CTest, smoke tests com encerramento limpo e preparação de instalação reversível concluídos; testes físicos A–D em bare-metal DRM/KMS/VT aguardam execução humana assistida).*

---

## 1. Commit e Branch Auditados

- Branch: `valo-graphics-2.0`
- Commit base: `e77411e` (`fix: abort bare-metal session on logind failure and release unused device leases`)
- Nenhum commit ou pull request foi criado automaticamente.

---

## 2. Hardware e Drivers Identificados (Somente Leitura)

- **Dispositivos DRM (`/dev/dri/`):**
  - `/dev/dri/card1` (`crw-rw----+`, `226, 1`, grupo `video`)
  - `/dev/dri/renderD128` (`crw-rw-rw-.`, `226, 128`, grupo `render`)
  - **Observação crítica:** `/dev/dri/card0` **não existe** neste host.
- **GPU (`lspci -nnk`):**
  - `04:00.0 VGA compatible controller [0300]: Advanced Micro Devices, Inc. [AMD/ATI] Cezanne [Radeon Vega Series / Radeon Vega Mobile Series] [1002:1638] (rev c9)`
  - Driver de kernel ativo: `amdgpu`
- **Vulkan (`vulkaninfo --summary`):**
  - Vulkan Instance Version: `1.4.341`
  - `GPU0`: `AMD Radeon Graphics (RADV RENOIR)` (`PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU`), driver `radv` (`Mesa 26.2.7`), `apiVersion = 1.4.354`, `vendorID = 0x1002`, `deviceID = 0x1638`
  - `GPU1`: `llvmpipe (LLVM 22.1.8, 256 bits)` (`PHYSICAL_DEVICE_TYPE_CPU`, `Mesa 26.2.7`)

---

## 3. Diagnóstico do Ambiente

- **Sessão gráfica atual (`loginctl show-session`):** `Id=2`, `VTNr=2`, `Seat=seat0`, `Service=plasmalogin`, `Type=wayland`, `Class=user`, `Active=yes`, `State=active`.
- **Display Manager ativo:** `plasmalogin.service` (`Plasma Login Manager`, ativo). `sddm` e `gdm` estão inativos.
- **`systemd-logind`:** `active`.
- **TTYs de recuperação:** `/dev/tty1` a `/dev/tty6` presentes (`tty2` em uso pela sessão principal do usuário `lucasramos`).
- **Espaço livre em disco (`df -h`):**
  - `/` e `/home` (`/dev/sda5`): `148G` total, `117G` usados, `27G` disponíveis (`82%`).
  - `/tmp` (`tmpfs`): `16G` total, `8,7G` usados, `6,8G` disponíveis (`57%`).
- **Instalação prévia em `/usr` e `/usr/local`:** ausente (`/usr/bin/systalia*`, `/usr/share/wayland-sessions/systalia.desktop`, `/usr/local/bin/systalia*` inexistentes).
- **Uso do sistema:** trata-se do **ambiente desktop principal** do usuário (`lucasramos`, sessão Plasma ativa na VT2). Por segurança, nenhum teste físico invasivo foi executado automaticamente.

---

## 4. Auditoria Final de Segurança, Build e Testes Automatizados

### 4.1 Verificações de Segurança Pré-Bare-Metal
1. Falha de `systemd-logind` em `SessionBareMetalKms` aborta com `return 1` (`LogindSessionService::shouldAbortOnLogindFailure`).
2. Falha de `acquirePrimaryDrmDevice()` chama `releaseControl()` e encerra com `return 1`.
3. Falha de `initializeHardwareContext()` (`libinput`/`udev`) não retém leases inativos, chama `releaseControl()` e encerra com `return 1`.
4. `QQuickWindow::sceneGraphError` executa `shutdownHardwareContext()`, `releaseControl()` e `QCoreApplication::exit(2)`.
5. `PauseDevice` e `ResumeDevice` respeitam `hasPausedDrmDevice()` antes de liberar a submissão GPU.
6. Em `gpuSubmissionSuspendedChanged(true)`, a janela é ocultada e `window->releaseResources()` é chamado.
7. Encerramento normal confiável via `SystemControlService::logout()` e manipulador POSIX `SIGTERM`/`SIGINT` (`socketpair` + `QSocketNotifier` → `QCoreApplication::quit()`), executando destrutores e liberando `Xwayland`, `libinput` e `logind`.
8. `SGE_ALLOW_NO_LOGIND` e `SGE_ALLOW_NO_INPUT` permanecem desativadas na homologação.

### 4.2 Build Release e CTest (após a última edição)
- **Configuração CMake Release:** `cmake -B /tmp/sge-build-0324 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/sge-install-0324`
  - Flags confirmadas em `build.ninja`: `-DSGE_HAVE_LIBINPUT=1 -DSGE_HAVE_LIBUDEV=1 -DSGE_HAVE_XCB=1` (com `-Werror`).
- **Ordem obrigatória de execução de testes (`QT_QPA_PLATFORM=offscreen`):**
  1. **Específicos primeiro:** `tst_input_hardware_backend` (**Passed**, `0.04 s`) e `tst_logind_integration` (**Passed**, `0.04 s`).
  2. **Suíte completa:** primeira execução teve falha pontual em `tst_valo_optical_contract_v2` (linha 148); reexecução isolada (`20.91 s`, Passed) e nova execução completa da suíte obtiveram **48/48 aprovados** (`100% tests passed, 0 tests failed out of 48`, `51.39 s`: 33 `unit`, 14 `integration`, 1 `e2e`).
- **Vínculo dinâmico (`ldd`):** `libwayland-server.so.0`, `libudev.so.1`, `libinput.so.10`.
- **Smoke tests (`systalia-session`, cwd `/tmp`, encerramento via `SIGTERM`):**
  - **Headless (`QT_QPA_PLATFORM=offscreen`):** `early_poll=None`, `sigterm_rc=0`, `Servidor finalizado no socket: wayland-systalia-0`, `Processo Xwayland finalizado. Código: 0 | Status: NormalExit`.
  - **Nested Wayland (`QT_QPA_PLATFORM=wayland`):** `early_poll=None`, `sigterm_rc=0`, `SGE-BACKDROP-V2` (`1662x975`, `Blur GPU executado com sucesso!`), encerramento limpo do servidor Wayland e do `Xwayland` (`Código: 0`).
- **Instalação e remoção isoladas:** artefatos verificados via `install_manifest.txt` em `/tmp/sge-install-0324` e removidos limpamente (`UNINSTALL_OK`).

---

## 5. Instalação Reversível e Procedimento de Remoção (Para Execução Humana)

Não utilize `sudo cmake --install` em `/usr`. Utilize um prefixo isolado no diretório do usuário, preservando `plasmalogin.service`, `/usr` e as configurações atuais do desktop:

```bash
# 1. Compilar em Release (SGE_HAVE_LIBINPUT=1) e instalar em prefixo isolado
cmake -B "$HOME/.cache/sge-build-homolog" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local/systalia-homolog"
cmake --build "$HOME/.cache/sge-build-homolog" -j"$(nproc)"
cmake --install "$HOME/.cache/sge-build-homolog"

# 2. Verificar arquivos instalados (4 arquivos, zero impacto em /usr)
cat "$HOME/.cache/sge-build-homolog/install_manifest.txt"
```

### Execução com Logs Persistentes (na TTY dedicada)

```bash
mkdir -p "$HOME/.local/state/systalia"
LOG_FILE="$HOME/.local/state/systalia/homolog-$(date +%Y%m%d-%H%M%S).log"
env -u WAYLAND_DISPLAY -u DISPLAY -u QT_QPA_PLATFORM \
  QT_LOGGING_RULES="qt.qpa.eglfs.*=true;qt.qpa.input.*=true" \
  "$HOME/.local/systalia-homolog/bin/systalia-session" 2>&1 | tee "$LOG_FILE"
```

### Procedimento Explícito de Desinstalação

```bash
rm -rf "$HOME/.local/systalia-homolog" "$HOME/.cache/sge-build-homolog"
# Opcional: remover logs após análise
# rm -rf "$HOME/.local/state/systalia"
```

---

## 6. Matriz dos Testes Físicos (Roteiro Assistido)

| Teste | Escopo | Passos Principais | Status Atual |
| :--- | :--- | :--- | :--- |
| **Teste A — Inicialização independente** | TTY dedicada, `logind`, `eglfs_kms`, Vulkan, shell VALO | Alternar para `Ctrl+Alt+F3`, autenticar, executar `systalia-session` com log persistente (sem `WAYLAND_DISPLAY`/`DISPLAY`/`SGE_ALLOW_NO_*`). | **PENDENTE (humano)** |
| **Teste B — Entrada física** | Teclado, ponteiro, cliques, atalhos, Dock, TopBar, janelas | Mover mouse, clicar na Dock/TopBar, usar atalhos (`Alt+Tab`, `Super+*`), abrir terminal/app. | **PENDENTE (humano)** |
| **Teste C — Ciclo de vida gráfico** | Clientes Wayland, troca de VT (`PauseDevice`/`ResumeDevice`), retomada Vulkan | Abrir/fechar cliente Wayland; alternar `Ctrl+Alt+F4` e voltar para `Ctrl+Alt+F3`; confirmar ausência de tela preta e retomada de input. | **PENDENTE (humano)** |
| **Teste D — Encerramento e recuperação** | Saída normal, liberação de dispositivos, retorno à TTY e ao Plasma | Encerrar `systalia-session` (`Ctrl+C` na TTY3 ou `SIGTERM` de outra TTY); conferir log (`sigterm_rc=0`); voltar para `Ctrl+Alt+F2`. | **PENDENTE (humano)** |

---

## 7. Evidências Reais Coletadas vs. Pendentes

- **Coletadas no host (leitura/automatizadas):** detecção de `/dev/dri/card1` (`226,1`), `amdgpu`, Mesa `26.2.7` `radv`, `plasmalogin.service` na `VTNr=2`, `systemd-logind` ativo, vínculo `libinput.so.10`/`libudev.so.1`, build Release com `SGE_HAVE_LIBINPUT=1`, 48/48 CTest e smoke tests headless/nested com `sigterm_rc=0`.
- **Pendentes (exigem execução humana em TTY dedicada):** logs reais de `logind TakeControl`/`TakeDevice` em `seat0`, criação de swapchain Vulkan em `eglfs_kms` sobre `/dev/dri/card1`, eventos `libinput` físicos e ciclo `PauseDevice`/`ResumeDevice` em troca de VT.

---

## 8. Problemas Encontrados e Correções Aplicadas Nesta Etapa

1. **Hardcoding de `/dev/dri/card0` em sistema com `/dev/dri/card1`:**
   - **Problema:** neste host AMD Cezanne Fedora 44, `/dev/dri/card0` não existe (apenas `/dev/dri/card1`). `InputDeviceManager::m_activeDrmDevice` e `config/eglfs_kms.json` apontavam fixo para `/dev/dri/card0`, o que causaria falha imediata em `acquirePrimaryDrmDevice()` e `eglfs_kms`.
   - **Correção:** adicionado `InputDeviceManager::discoverPrimaryDrmDevice()` ([InputDeviceManager.hpp](../../src/services/input/InputDeviceManager.hpp), [InputDeviceManager.cpp](../../src/services/input/InputDeviceManager.cpp)) com suporte a `SGE_DRM_DEVICE` e varredura numérica de `/dev/dri/card*`; removido `"device": "/dev/dri/card0"` de [`config/eglfs_kms.json`](../../config/eglfs_kms.json) para permitir autodetecção udev do Qt `eglfs_kms`; adicionado teste unitário `primaryDrmDeviceDiscovery()` em [`tests/unit/tst_InputHardwareBackend.cpp`](../../tests/unit/tst_InputHardwareBackend.cpp).
2. **Encerramento limpo via `SIGTERM`/`SIGINT`:**
   - **Problema:** `SIGTERM`/`SIGINT` não acionavam `QCoreApplication::quit()`, podendo encerrar o processo sem executar os destrutores de `InputDeviceManager`, `LogindSessionService` e `XWaylandManager`.
   - **Correção:** adicionado manipulador seguro de `SIGTERM`/`SIGINT` via `socketpair` + `QSocketNotifier` em [`src/app/main.cpp`](../../src/app/main.cpp), comprovado pelos smoke tests com saída `0`, encerramento do socket Wayland e `Xwayland` `NormalExit` (`0`).

---

## 9. Procedimento de Recuperação em Caso de Falha Física

1. **Voltar ao desktop principal:** pressionar `Ctrl+Alt+F2` (VT2 onde a sessão Plasma ativa permanece intacta).
2. **Se a tela da VT3 travar:** alternar para `Ctrl+Alt+F4` (ou usar SSH caso tenha habilitado `sshd` previamente), autenticar e executar:
   ```bash
   pkill -TERM -x systalia_glass_engine || true
   sleep 2
   pkill -KILL -x systalia_glass_engine || true
   ```
3. **Inspecionar o log persistente:** verificar `$HOME/.local/state/systalia/homolog-*.log` e `journalctl --user -b --no-pager | tail -n 100`.
4. **Remover instalação de teste** se desejar restaurar estado limpo (Seção 5).

---

## 10. Decisão Técnica

**Decisão: PARCIAL.**

A fundação de build (`SGE_HAVE_LIBINPUT=1`, `-Werror`), os 48/48 testes automatizados, a correção crítica de descoberta de `/dev/dri/card1`, o encerramento limpo por `SIGTERM`/`SIGINT` e a preparação de instalação reversível estão validados. Contudo, conforme o critério da Etapa 03.2, a homologação **não pode** ser declarada concluída até a execução humana assistida dos **Testes A–D** em TTY dedicada sobre DRM/KMS, Vulkan, `logind` e `libinput` reais.
