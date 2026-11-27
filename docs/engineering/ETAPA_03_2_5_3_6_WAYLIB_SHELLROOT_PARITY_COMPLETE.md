# SYSTALIA — ETAPA 03.2.5.3.6
## Execução do Plano de Paridade `ShellRoot.qml`: Protocolos Wayland, `SystaliaFlowMenu`, Transparência Inter-Janelas Acíclica e Isolamento de Apps Electron/Wayland

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Documento de referência:** [`docs/engineering/ETAPA_03_2_5_3_5_DEEP_AUDIT_AND_FIX_PLAN.md`](ETAPA_03_2_5_3_5_DEEP_AUDIT_AND_FIX_PLAN.md)  
**Data:** 2026-11-27  
**Status:** Concluída (`61/61` CTest com `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, smoke test nested ao vivo com `RC=0`)

---

## 1. Resumo Executivo

A Etapa 03.2.5.3.6 executou integralmente o plano de paridade com `ShellRoot.qml` definido na Etapa 03.2.5.3.5 sobre o backend Waylib (`build/waylib`, `Release`, `-Werror`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`), sem movimentação em massa de diretórios V2, sem XWayland (reservado para a Etapa 03.2.5.4) e sem execução física DRM/KMS.

Principais entregas:
1. **Protocolos Wayland essenciais e singletons/providers QML** registrados em `WaylibCompositorAdapter` e `waylib_nested_main.cpp`.
2. **Isolamento de sessão nested e lançamento Electron/Chromium Wayland** em `ProcessLauncherService` + `WaylibNestedShell.qml`.
3. **Pipeline acíclico de transparência inter-janelas** (`clientBackdropPool` + `ValoInternalWindowBackdropV2` + `overlayCompositeBackdropSource`).
4. **Paridade funcional de overlays** com `SystaliaFlowMenu` (`Meta+Space` e botão esquerdo da TopBar), `SystaliaNotificationHost` e `softwareBrightnessDimmer`.
5. **Validação completa**: `tst_waylib_backend`, `tst_valo_shell_v2`, `tst_process_launcher`, `tst_telemetry`, suíte completa `61/61` no CTest e smoke test ao vivo em `wayland-1`.

---

## 2. Protocolos Wayland Registrados e Singletons/Providers

### 2.1 Protocolos Wayland (`src/compositor/waylib/WaylibCompositorAdapter.cpp`)
- `WCursorShapeManagerV1` (`wp_cursor_shape_manager_v1`) anexado ao `WServer` antes de `m_server->start()`.
- Criados após `m_server->start()` sobre `m_server->handle()`:
  - `wlr_data_device_manager_create` (`wl_data_device_manager`)
  - `wlr_primary_selection_v1_device_manager_create` (`zwp_primary_selection_device_manager_v1`)
  - `wlr_xdg_activation_v1_create` (`xdg_activation_v1`)
  - `wlr_fractional_scale_manager_v1_create(..., 1)` (`wp_fractional_scale_manager_v1`)
  - `wlr_text_input_manager_v3_create` (`zwp_text_input_manager_v3`)

### 2.2 Singletons QML e ImageProvider (`WaylibCompositorAdapter.cpp` e `src/app/waylib_nested_main.cpp`)
- `AppRegistry` (`Systalia.System` → `AppRegistryService`) registrado via `qmlRegisterSingletonType`.
- `Notifications` (`Systalia.System` → `NotificationServer`) registrado via `qmlRegisterSingletonType`.
- `DesktopIconProvider` registrado em `QQmlEngine` com o prefixo `image://desktopIcons/` tanto no binário `systalia_waylib_nested` quanto na suíte `tst_WaylibBackend`.

---

## 3. Isolamento Electron/VS Code e Paridade de Lançamento

Em `src/services/applications/ProcessLauncherService.hpp` e `ProcessLauncherService.cpp`:
- Adicionada a propriedade `Q_PROPERTY(bool isolatedNestedSession ...)` (ativada por `WaylibNestedShell.qml` via `ProcessLauncher.isolatedNestedSession = true`).
- `waylandProcessEnvironment(waylandDisplay, isolatedNestedSession)`:
  - Define `WAYLAND_DISPLAY`, `QT_QPA_PLATFORM=wayland`, `GDK_BACKEND=wayland`, `SDL_VIDEODRIVER=wayland`, `MOZ_ENABLE_WAYLAND=1` e `ELECTRON_OZONE_PLATFORM_HINT=wayland`.
  - Quando `isolatedNestedSession == true`, remove explicitamente `DISPLAY` do ambiente filho para impedir fallback X11/XWayland acidental para o compositor host durante testes nested.
- `effectiveArgumentsForLaunch(program, arguments, waylandDisplay, isolatedNestedSession)`:
  - Para executáveis `code` / `codium` em sessão nested isolada, injeta automaticamente `--ozone-platform=wayland`, `--new-window` e `--user-data-dir=<writableTemp>/systalia-vscode-nested-<socket>` (caso não fornecidos), evitando colisão de instância única via IPC/D-Bus com o VS Code do host.

---

## 4. Pipeline Acíclico de Transparência Inter-Janelas e Overlays

Em `src/qml/waylib/WaylibNestedShell.qml`:
1. **`clientBackdropPool` + `clientStageRepeater` (clientes Wayland):**
   - Calcula `orderedClientFrames` ordenados por `windowZOrder`.
   - O cliente na base recebe `desktopBackdropSource`.
   - Cada janela subsequente recebe o estágio composto (`ShaderEffectSource` com `sourceItem` contendo o estágio anterior e a janela imediatamente abaixo), garantindo refração e blur reais entre clientes Wayland sobrepostos sem ciclo de dependência QML.
2. **`ValoInternalWindowBackdropV2` (`waylibInternalWindowBackdropV2`):**
   - Gerencia a composição acíclica entre `filesWindow` e `materialWindow` (`Systalia Control Center`) sobre `desktopBackdropSource`, atribuindo `internalWindowBackdrop.sourceFor(...)` a cada janela interna conforme o z-order dinâmico.
3. **`overlayCompositeBackdropSource` (`waylibOverlayCompositeBackdropSource`):**
   - Compõe `desktopBackdropSource` + `clientLayer` + `internalWindowsLayer` (fora de `shellChromeLayer`), servindo de backdrop para `valoTopBar`, `valoDock`, `controlCenter`, `waylibFlowMenu` e `waylibNotificationHost`.
4. **Overlays e dimmer de brilho:**
   - `SystaliaFlowMenu` (`waylibFlowMenu`, `z: 120`) conectado a `topBar.onLauncherRequested` e ao atalho `Meta+Space`.
   - `SystaliaNotificationHost` (`waylibNotificationHost`, `z: 118`) conectado ao singleton `Notifications`.
   - `softwareBrightnessDimmer` (`waylibSoftwareBrightnessDimmer`, `z: 9990`) vinculado a `SystemControl.brightness`.

---

## 5. Validação (Build Incremental, CTest e Smoke Test Nested)

- **Build incremental (`Release`, `-Werror`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`):**
  - `cmake --build build/waylib -j "$(nproc)"` concluído com sucesso e 0 warnings/erros.
- **Testes direcionados (`ctest --test-dir build/waylib -R "^(tst_waylib_backend|tst_valo_shell_v2|tst_process_launcher|tst_telemetry)$" --output-on-failure`):**
  - `4/4` aprovados (`tst_telemetry`, `tst_process_launcher`, `tst_valo_shell_v2`, `tst_waylib_backend`).
  - `tst_WaylibBackend` validou: os 6 protocolos Wayland via `wayland-info`, `AppRegistry`, `DesktopIconProvider`, `SystaliaFlowMenu` (`launcherRequested` + `close()`), `SystaliaNotificationHost`, `softwareBrightnessDimmer`, `ValoInternalWindowBackdropV2` (`compositeAvailable == true`), transparência inter-janelas entre 2 clientes Wayland (`frameBInit->backdropSource != backdropSource`) e o ambiente/argumentos isolados do `ProcessLauncherService`.
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):**
  - `61/61` testes aprovados (`100% tests passed, 0 tests failed out of 61`, `40.08 sec`).
- **Smoke test nested ao vivo (`./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500` em `WAYLAND_DISPLAY=wayland-1`):**
  - Encerrado com `RC=0`.
  - Logs observados:
    - `[SGE-WAYLIB] Compositor nested/headless iniciado no socket: "sge-nested-poc"`
    - `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760`
    - `[SGE-BACKDROP-V2] Blur GPU executado com sucesso! ... format=1, size=1280x760`
    - `waylib.output.buffer: Selected primary buffer format: "XR24" "(0x34325258)" for output: "WL-1"`
  - Zero erros críticos de QML.

---

## 6. Arquivos Modificados e Próximo Passo

### Arquivos alterados nesta etapa
- `src/compositor/waylib/WaylibCompositorAdapter.cpp`
- `src/app/waylib_nested_main.cpp`
- `src/services/applications/ProcessLauncherService.hpp`
- `src/services/applications/ProcessLauncherService.cpp`
- `src/qml/waylib/WaylibNestedShell.qml`
- `tests/compositor/tst_WaylibBackend.cpp`
- `docs/engineering/ETAPA_03_2_5_3_6_WAYLIB_SHELLROOT_PARITY_COMPLETE.md` (novo)

### Próximo passo
- **Etapa 03.2.5.4:** Integração de XWayland isolado sobre o backend Waylib (`WXWayland` / `WServer`).
