# SYSTALIA — Etapa 03.2.3: Homologação Assistida da Sessão Independente

- **Branch:** `valo-graphics-2.0`
- **Commit base:** `e77411e` (com alterações locais das Etapas 03.2.1, 03.2.2 e 03.2.3 ainda não commitadas)
- **Ambiente de referência provisório:** Fedora Linux 44, Qt 6.11.2, `libinput-devel-1.31.3-1.fc44.x86_64`, `libudev 259`
- **Sistema hospedeiro atual:** filesystem raiz `btrfs /dev/sda5[/root]` (não Live), sessão gráfica ativa `SESSION_TYPE=wayland`, `WAYLAND_DISPLAY=wayland-1`, `DISPLAY=:0`, `systemd-detect-virt` = `none`, unidades `sshd`, `sddm` e `gdm` inativas (`inactive`), e artefatos de sessão ainda **não** instalados em `/usr` (`/usr/share/wayland-sessions/systalia.desktop` e `/usr/bin/systalia-session` ausentes).

---

## 1. Auditoria Preventiva e Diagnóstico de Riscos

### Risco A — Inicialização sem `systemd-logind`
- Em modo `SessionBareMetalKms`, iniciar o compositor sem sessão `systemd-logind` válida e sem fallback explícito pode resultar em falha silenciosa de permissão ao abrir dispositivos DRM/input ou em execução inconsistente.
- O predicado `LogindSessionService::shouldAbortOnLogindFailure(bool isBareMetalKms, quint32 effectiveUid, bool allowNoLogind)` centraliza a decisão considerando sessão bare-metal KMS, UID efetivo e a variável de escape `SGE_ALLOW_NO_LOGIND`.

### Risco B — Falhas na inicialização de dispositivos e contexto gráfico
- Se `acquirePrimaryDrmDevice()` ou `initializeHardwareContext()` falhar em `SessionBareMetalKms`, o compositor não deve prosseguir em estado parcialmente inicializado sem liberação do controle de sessão.
- Da mesma forma, uma falha no backend gráfico durante a inicialização da janela (`QQuickWindow::sceneGraphError`) exige desligamento ordenado do contexto de entrada e liberação do controle do `logind`.

### Risco C — Suspensão e retomada de VT (`PauseDevice` / `ResumeDevice` / `Active`)
- Durante troca de VT, o `systemd-logind` pode emitir `PropertiesChanged("Active": true)` antes de entregar o sinal `ResumeDevice` para o dispositivo DRM (`major = 226`). Se a submissão GPU fosse retomada apenas pela mudança de `Active`, haveria janela de corrida com o FD DRM ainda pausado.
- No subsistema `libinput`, ao retomar a entrada após suspensão de hardware, eventos pendentes no descritor precisam ser drenados de forma assíncrona, e o encerramento do contexto precisa limpar dispositivos alugados (`m_leasedDevices`) e estado de botões pressionados (`m_pressedButtons`).

---

## 2. Correções Implementadas

1. **Rastreamento de dispositivo DRM pausado (`src/services/session/LogindSessionService.hpp`, `src/services/session/LogindSessionService.cpp`):**
   - Adicionado `bool hasPausedDrmDevice() const noexcept;` (`kDrmMajorNumber = 226u`).
   - Em `onPropertiesChanged()`, quando `Active` transiciona para `true`, a suspensão de submissão GPU só é encerrada se `!hasPausedDrmDevice()`, evitando retomada prematura antes de `ResumeDevice(DRM)`.

2. **Drenagem pós-retomada e limpeza de estado em `InputDeviceManager` (`src/services/input/InputDeviceManager.cpp`):**
   - Em `setHardwareSuspended(false)`, após `libinput_resume()` bem-sucedido, agenda `pollHardwareEvents` via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
   - Em `shutdownHardwareContext()`, limpa `m_leasedDevices.clear()`, redefine `m_pressedButtons = Qt::NoButton` e `m_hardwareSuspended = false`.

3. **Guardas fatais bare-metal e tratamento de `sceneGraphError` (`src/app/main.cpp`):**
   - Em `SessionBareMetalKms`, falha em `acquirePrimaryDrmDevice()` sem `SGE_ALLOW_NO_LOGIND` executa `logindSessionService.releaseControl()` e encerra com código `1`.
   - Falha em `initializeHardwareContext()` sem `SGE_ALLOW_NO_INPUT` nem `SGE_ALLOW_NO_LOGIND` executa `logindSessionService.releaseControl()` e encerra com código `1`.
   - Conexão de `QQuickWindow::sceneGraphError` para chamar `inputDeviceManager.shutdownHardwareContext()`, `logindSessionService.releaseControl()` e `QCoreApplication::exit(2)`.

4. **Cobertura automatizada (`tests/unit/tst_LogindIntegration.cpp`):**
   - Adicionados cenários cobrindo `PropertiesChanged("Active": true)` antes de `ResumeDevice` para DRM, garantindo que `isGpuSubmissionSuspended()` permaneça `true` até a restauração do descritor DRM por `onResumeDevice`.

---

## 3. Validação Automatizada e de Smoke Test

### 3.1 Build Release (`-Werror`) e Definições de Compilação
- Configuração limpa via `cmake -B /tmp/sge-build-0323 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/sge-install-0323`.
- Flags verificadas em `build.ninja`: `-DSGE_HAVE_LIBINPUT=1 -DSGE_HAVE_LIBUDEV=1 -DSGE_HAVE_XCB=1`.
- Compilação Release completa sem erros nem avisos tratados como erro (`-Werror`).

### 3.2 Execução de Testes (Regra Obrigatória: Específicos Primeiro, Suíte Completa Depois)
1. **Testes específicos dos componentes modificados (`QT_QPA_PLATFORM=offscreen`):**
   - `tst_input_hardware_backend`: **Passed** (`0.04 sec`)
   - `tst_logind_integration`: **Passed** (`0.04 sec`)
2. **Suíte completa do CTest (`QT_QPA_PLATFORM=offscreen`):**
   - **48/48 aprovados** (`100% tests passed, 0 tests failed out of 48`, tempo total `49.52 sec`: 33 `unit`, 14 `integration`, 1 `e2e`).

### 3.3 Instalação Temporária e Dependências Dinâmicas
- `cmake --install /tmp/sge-build-0323` instalou:
  - `/tmp/sge-install-0323/bin/systalia_glass_engine`
  - `/tmp/sge-install-0323/bin/systalia-session`
  - `/tmp/sge-install-0323/share/systalia/eglfs_kms.json`
  - `/tmp/sge-install-0323/share/wayland-sessions/systalia.desktop`
- `ldd /tmp/sge-install-0323/bin/systalia_glass_engine` confirmou vínculo dinâmico com:
  - `libwayland-server.so.0`
  - `libudev.so.1`
  - `libinput.so.10`

### 3.4 Smoke Tests Seguros (Headless e Nested Wayland)
- **Headless (`QT_QPA_PLATFORM=offscreen`, cwd `/tmp`):** inicialização de XDG Shell, XDG Decoration Manager (SSD), Viewporter (`wp_viewporter`), Seat (teclado/ponteiro) e XWayland (`:1` + `WM_S0`); encerramento limpo ao receber `SIGTERM` (código `-15`).
- **Nested Wayland (`QT_QPA_PLATFORM=wayland` sobre `wayland-1`, cwd `/tmp`):** inicialização completa com execução do pipeline GPU `SGE-BACKDROP-V2` (`1662x975`, `Blur GPU executado com sucesso!`) e XWayland (`:1` + `WM_S0`); encerramento limpo ao receber `SIGTERM` (código `-15`).
- Diretórios temporários `/tmp/sge-build-0323` e `/tmp/sge-install-0323` removidos após validação.

---

## 4. Matriz de Falhas e Comportamento Esperado

| Cenário de Falha | Condição | Comportamento Esperado |
| :--- | :--- | :--- |
| Ausência/falha de `systemd-logind` | `SessionBareMetalKms` sem `SGE_ALLOW_NO_LOGIND` | Aborta inicialização com código `1` |
| Falha ao adquirir dispositivo DRM primário | `acquirePrimaryDrmDevice() == false` em `SessionBareMetalKms` sem `SGE_ALLOW_NO_LOGIND` | Chama `releaseControl()` e encerra com código `1` |
| Falha no contexto de entrada hardware | `initializeHardwareContext() == false` em `SessionBareMetalKms` sem `SGE_ALLOW_NO_INPUT`/`SGE_ALLOW_NO_LOGIND` | Chama `releaseControl()` e encerra com código `1` |
| Erro de Scene Graph / backend gráfico | Sinal `QQuickWindow::sceneGraphError` | Chama `shutdownHardwareContext()`, `releaseControl()` e `QCoreApplication::exit(2)` |
| `Active=true` antes de `ResumeDevice(DRM)` | Troca de VT com FD DRM ainda pausado | `isGpuSubmissionSuspended()` permanece `true` até `onResumeDevice` restaurar o FD DRM |
| Retomada de entrada `libinput` | `setHardwareSuspended(false)` com sucesso | Agenda `pollHardwareEvents` enfileirado para drenar eventos pendentes |

---

## 5. Checklist Humano Seguro para Homologação Física (Ambiente Dedicado)

> **Nota de segurança:** Nenhum teste físico de DRM/KMS, troca de VT ou reinício de display manager foi executado nesta máquina. No host atual, `/usr` não possui `systalia.desktop` nem `systalia-session` instalados e `sshd`/`sddm`/`gdm` estão `inactive`.

1. **Instalação no sistema alvo (fora desta sessão assistida):**
   - Instalar com prefixo `/usr` (`cmake --install <build_dir> --prefix /usr` com privilégios administrativos) e confirmar presença de `/usr/bin/systalia_glass_engine`, `/usr/bin/systalia-session`, `/usr/share/systalia/eglfs_kms.json` e `/usr/share/wayland-sessions/systalia.desktop`.
2. **Acesso de recuperação:**
   - Garantir acesso SSH ativo (`systemctl start sshd`) ou TTY secundário livre antes de iniciar sessão bare-metal.
3. **Execução em TTY dedicado (sem compositor pai):**
   - Alternar para um TTY dedicado (`Ctrl+Alt+F3`), autenticar e executar `systalia-session` (ou selecionar a sessão **Systalia** no SDDM/GDM quando habilitado).
4. **Verificações presenciais obrigatórias:**
   - Abertura do shell VALO V2 em DRM/KMS sem falhas de `sceneGraphError`.
   - Resposta de teclado e ponteiro via `libinput` (inclusive atalhos de teclado e cliques).
   - Troca de VT (`Ctrl+Alt+F3` -> `Ctrl+Alt+F4` -> retorno ao VT da sessão): validar pausa limpa de entrada/GPU na saída e retomada sem travamento visual ou perda de eventos na volta.
   - Encerramento limpo da sessão retornando o terminal/display manager ao estado normal.

---

## 6. Pendências para a Etapa 03.2.4

- Execução presencial do checklist físico acima em ambiente dedicado com GPU DRM/KMS real.
- Registro dos logs de `systemd-logind`, DRM/KMS e `libinput` obtidos em hardware físico.
- Decisão de empacotamento/instalação definitiva em `/usr` (RPM/Spec ou instalação direta no ambiente de homologação).
