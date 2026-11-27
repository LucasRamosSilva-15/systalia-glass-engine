# ETAPA 03.2.5.3.3 — Auditoria de `ShellRoot.qml` e `main.cpp` Legado: Integração de `SystaliaControlCenterV2`, `SystaliaFilesWindowV2` e Lançamento de Aplicativos no Backend Waylib

**Data:** 2026-11-27  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Waylib 0.6.13 (`$HOME/.cache/sge-waylib-prefix`) e wlroots 0.20.2  
**Regra permanente:** Build incremental único `Release` em `build/waylib` com `-DSGE_ENABLE_WAYLIB_BACKEND=ON` e `-Werror`. Execução física DRM/KMS e XWayland fora do escopo desta etapa (XWayland reservado para Etapa 03.2.5.4).

---

## 1. Matriz Comparativa (`ShellRoot.qml` / `main.cpp` vs. `WaylibNestedShell.qml` / `waylib_nested_main.cpp`)

| Componente / Serviço / Sinal | `ShellRoot.qml` / `main.cpp` (Legado) | `WaylibNestedShell.qml` / `waylib_nested_main.cpp` (Antes) | Ação na Etapa 03.2.5.3.3 |
|---|---|---|---|
| `Systalia.System.SystemTelemetry` | Registrado em `main.cpp` e ligado à TopBar | Já registrado em `WaylibCompositorAdapter::registerQmlTypes()` | Mantido |
| `Systalia.System.SystemControl` | Registrado em `main.cpp` e consumido por TopBar e Control Center | Já registrado em `WaylibCompositorAdapter::registerQmlTypes()` | Mantido |
| `Systalia.System.WorkspaceManager` | Registrado em `main.cpp` e ligado à TopBar / janelas | Já registrado em `WaylibCompositorAdapter::registerQmlTypes()` | Mantido |
| `Systalia.System.ProcessLauncher` | Registrado em `main.cpp`; usava `WAYLAND_DISPLAY=wayland-systalia-0` fixo | Registrado em `WaylibCompositorAdapter`, porém desconectado da Dock/TopBar e com socket hardcoded | **Implementado:** propriedade `waylandDisplay` em `ProcessLauncherService` vinculada a `compositorAdapter.socketName` via `Binding` |
| `Systalia.System.LiquidTiling` | Registrado em `main.cpp`; usado em `SystaliaFilesWindowV2.qml` | Ausente em `WaylibCompositorAdapter::registerQmlTypes()` | **Implementado:** registrado singleton `LiquidTilingService` em `WaylibCompositorAdapter::registerQmlTypes()` |
| `SystaliaControlCenterV2` | Instanciado em `ShellRoot.qml`; aberto por `topBar.controlCenterRequested` e `topBar.powerRequested` | Ausente; `topBar.powerRequested` fechava janela ativa | **Implementado:** instanciado `waylibControlCenterV2` (`z: 115`) fora de `desktopContentLayer` com `backdropSource: desktopBackdropSource`, conectado a `controlCenterRequested`, `powerRequested`, `valoDock.onSettingsRequested` e `Esc` |
| `SystaliaFilesWindowV2` | Instanciado em `ShellRoot.qml` com `controller` (`raiseFileWindow`, `minimizeFileWindow`, `closeFileWindow`, `toggleFileWindowMaximized`) | Ausente; `valoDock.filesVisible: false` fixo e `onFilesRequested` sem handler | **Implementado:** instanciado `waylibFilesWindowV2` (`z: 85`) na workarea fora de `desktopContentLayer`, conectado a `valoDock.onFilesRequested` e `valoDock.filesVisible` |
| Sinais da `SystaliaTopBarV2` | `launcherRequested`, `controlCenterRequested`, `terminalRequested`, `powerRequested` | Apenas `launcherRequested` (`cycleFocus`) e `powerRequested` | **Implementado:** `controlCenterRequested`, `powerRequested` e `terminalRequested` (`ProcessLauncher` com fallback de terminais) |
| Sinais da `SystaliaDockV2` | `filesRequested`, `audioRequested`, `terminalRequested`, `ideRequested`, `settingsRequested` | Apenas `terminalRequested` (`cycleFocus`) | **Implementado:** todos os 5 sinais conectados (`FilesWindowV2`, `pavucontrol`/`pwvucontrol`, `alacritty`/`foot`/`konsole`/`xterm`, `code`/`codium`, `ControlCenterV2`) com log diagnóstico se binário faltar |

---

## 2. Propagação do Socket Wayland Nested (`WaylibCompositorAdapter` e `ProcessLauncherService`)

- **`src/services/applications/ProcessLauncherService.hpp` / `.cpp`:**
  - Adicionada `Q_PROPERTY(QString waylandDisplay READ waylandDisplay WRITE setWaylandDisplay NOTIFY waylandDisplayChanged)`, mantendo `"wayland-systalia-0"` como padrão retrocompatível.
  - `waylandProcessEnvironment(const QString &waylandDisplay = QStringLiteral("wayland-systalia-0"))` injeta `WAYLAND_DISPLAY=<waylandDisplay>`, `QT_QPA_PLATFORM=wayland` e `GDK_BACKEND=wayland` exclusivamente no `QProcessEnvironment` do processo filho (`QProcess::startDetached`), sem alterar o ambiente pai nem a sessão KDE Plasma.
  - Em `launch()`, falhas de execução (como binário ausente) registram aviso diagnóstico claro (`[SGE-LAUNCHER] Falha ao iniciar aplicativo: ...`) e retornam `false` sem derrubar a shell.
- **`src/compositor/waylib/WaylibCompositorAdapter.cpp`:**
  - Adicionado registro de `LiquidTilingService` (`Systalia.System` 1.0 `LiquidTiling`) em `registerQmlTypes()`, necessário para `SystaliaFilesWindowV2.qml`.
- **`src/qml/waylib/WaylibNestedShell.qml`:**
  - Adicionado `Binding` vinculando `ProcessLauncher.waylandDisplay` a `root.compositorAdapter.socketName`.

---

## 3. Integração de `SystaliaControlCenterV2`, `SystaliaFilesWindowV2` e Sinais da TopBar/Dock

- **`SystaliaFilesWindowV2` (`objectName: "waylibFilesWindowV2"`):**
  - Instanciada em `shellChromeLayer` (`z: 85`), **fora de `desktopContentLayer`**, recebendo `backdropSource: desktopBackdropSource` real (sem feedback loop) e dimensão clamped à *workarea* (`clientLayer`).
  - Controlada por `raiseFileWindow()`, `minimizeFileWindow()`, `closeFileWindow()` e `toggleFileWindowMaximized()` no objeto raiz `root`, sincronizada com `valoDock.filesVisible`.
- **`SystaliaControlCenterV2` (`objectName: "waylibControlCenterV2"`):**
  - Instanciada em `shellChromeLayer` (`z: 115`, abaixo de `waylibPopupLayer` em `z: 120` e `waylibCursorOverlayLayer` em `z: 10000`), recebendo `backdropSource: desktopBackdropSource` real.
  - Alternada por `valoTopBar.onControlCenterRequested`, `valoTopBar.onPowerRequested` (`open()`) e `valoDock.onSettingsRequested`; fecha por clique fora (`TapHandler` interno), novo clique no gatilho ou tecla `Escape` (`Shortcut`).

---

## 4. Evidências de Compilação, Testes e Smoke Test

- **Build incremental (`Release`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, `-Werror`):**
  - `cmake --build build/waylib -j "$(nproc)"` após ajustes finais em `tst_WaylibBackend.cpp`: **7.47s** (`real 7.47`, `user 6.36`, `sys 1.03`).
- **Novos casos adicionados em `tests/compositor/tst_WaylibBackend.cpp`:**
  - `SystaliaControlCenterV2`: disparo de `controlCenterRequested`, verificação de `opened == true`, `visible == true`, `backdropSource == desktopBackdropSource` e fechamento.
  - `SystaliaFilesWindowV2`: disparo de `valoDock.filesRequested`, verificação de abertura dentro da *workarea* (`1280x760`), `filesVisible == true`, `backdropSource == desktopBackdropSource` e minimização.
  - `ProcessLauncher`: validação automática de `launcher->waylandDisplay() == adapter.socketName()`, lançamento de `tst_waylib_test_client` sem definir `WAYLAND_DISPLAY` no pai e comprovação da conexão `xdg_toplevel` dentro do compositor Waylib.
- **Testes direcionados (`tst_process_launcher`, `tst_valo_shell_v2`, `tst_waylib_backend`):**
  - **3/3 Passed** (`tst_process_launcher` 0.53s, `tst_valo_shell_v2` 0.52s, `tst_waylib_backend` 3.34s; total 4.39s).
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):**
  - **61/61 Passed** (`100% tests passed, 0 tests failed out of 61`, tempo total `38.62 sec`).
- **Smoke test ao vivo nested (`./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500`):**
  - Encerrado com código `0`, zero erros QML, `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760` e `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!`.

---

## 5. Prontidão para a Etapa 03.2.5.4 (XWayland)

A shell interativa Waylib agora possui paridade funcional para os painéis internos V2 (`SystaliaControlCenterV2` e `SystaliaFilesWindowV2`), lançamento isolado de clientes nativos Wayland via `ProcessLauncher` com `WAYLAND_DISPLAY` do compositor nested, popups XDG, `WXdgDecorationManager::Server`, `Alt+Tab` e pipeline óptico VALO Glass V2 real, estando pronta para a integração de **XWayland** na **Etapa 03.2.5.4**.
