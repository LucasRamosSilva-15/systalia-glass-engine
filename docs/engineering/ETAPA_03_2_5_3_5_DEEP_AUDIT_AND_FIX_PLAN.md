# ETAPA 03.2.5.3.5 — Auditoria Profunda da Arquitetura Sem Waylib vs. Waylib e Plano Técnico de Correção

**Data:** 2026-11-27  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Waylib 0.6.13 (`$HOME/.cache/sge-waylib-prefix`) e wlroots 0.20.2  
**Regra desta etapa:** Auditoria **somente leitura** no código-fonte (`git diff` em `src/`, `tests/` e `cmake/` 100% vazio).

---

## 1. Resumo Executivo das Causas Raiz Confirmadas

| Sintoma Real | Fatos Confirmados (Código e Logs) | Hipóteses Residuais |
|---|---|---|
| **1. Transparência / refração inter-janelas ausente** (`Systalia Control Center` não vê `Systalia Files` atrás; janelas clientes não se veem) | Em [WaylibNestedShell.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/qml/waylib/WaylibNestedShell.qml), existem apenas duas fontes estáticas: `wallpaperOnlyBackdropSource` (captura só `valoWallpaper`) e `desktopBackdropSource` (captura `desktopContentLayer` = wallpaper + `clientLayer`). Ambos os painéis `filesWindow` e `materialWindow` recebem `desktopBackdropSource`, ignorando `internalWindowsLayer`; já cada `WaylibWindowFrame` recebe `wallpaperOnlyBackdropSource`, ignorando outras janelas clientes. Não há uso de [ValoInternalWindowBackdropV2.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/graphics/v2/qml/ValoInternalWindowBackdropV2.qml) nem cadeia ordenada por `z-order`. | — (100% confirmado no código) |
| **2. Menu do lado esquerdo da `SystaliaTopBarV2` não abre** | O símbolo esquerdo da `SystaliaTopBarV2` emite `launcherRequested()` ([SystaliaTopBarV2.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/topbar/SystaliaTopBarV2.qml#L114-L117)). Em [ShellRoot.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/core/ShellRoot.qml#L903-L916), esse sinal abre [SystaliaFlowMenu.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/overlays/SystaliaFlowMenu.qml) (`flowMenu.toggle()`, além do atalho `Meta+Space`). Em `WaylibNestedShell.qml`, `onLauncherRequested` está ligado a `cycleFocus()`, `SystaliaFlowMenu` não está instanciado, `AppRegistryService` (`AppRegistry`) não está registrado em `WaylibCompositorAdapter::registerQmlTypes()` e `DesktopIconProvider` (`image://desktopIcons/`) não está registrado em `waylib_nested_main.cpp`. | — (100% confirmado no código) |
| **3. `VS Code` fechando após abrir no `systalia_waylib_nested`** | **(a) Instância única IPC no host KDE Plasma:** `pgrep -af "/code"` confirmou PID `146242` ativo em `/home/lucasramos/.config/Code` com lock `~/.config/Code/code.lock` e socket `/run/user/1000/vscode-*-main.sock`. Ao chamar `ProcessLauncher.launch("code")` sem `--user-data-dir`, `/usr/bin/code` (`cli.js`) delega ao processo já aberto no KDE Plasma e encerra imediatamente. **(b) Protocolos Wayland faltantes em `WaylibCompositorAdapter.cpp`:** `wayland-info` confirmou ausência de **`wl_data_device_manager`** (`wlr_data_device_manager_create`), `zwp_primary_selection_device_manager_v1`, `zwp_text_input_manager_v3`, `xdg_activation_v1`, `wp_fractional_scale_manager_v1` e `wp_cursor_shape_manager_v1`. Diagnósticos ao vivo registraram `Gdk: gdk_seat_get_keyboard: assertion 'GDK_IS_SEAT (seat)' failed` e `text-input-v3 not available`. Com `--user-data-dir` isolado e sem saída antecipada do `cli.js`, o processo permaneceu vivo (`RC=124` no timeout de 4,5s) e mapeou `xdg_toplevel`. | Possível aborto/crash interativo no Electron ao acionar clipboard/drag-and-drop ou IME dentro da janela devido à ausência de `wl_data_device_manager` / `primary_selection` / `text-input-v3`, além do `cli.js` retornar `exit 0` em modo fork quando chamado sem `--wait` (fazendo `ProcessLauncher::pollProcesses` perder o PID). |

---

## 2. Análise Arquitetural da Transparência Inter-Janelas (`ShellRoot.qml` vs. `WaylibNestedShell.qml`)

### 2.1. Como funciona no `ShellRoot.qml` (sem Waylib)

Para evitar **ciclos de feedback de textura** no Qt Quick Scene Graph (`ShaderEffectSource` nunca pode capturar um ancestral ou item que contenha o próprio consumidor do backdrop), o `ShellRoot.qml` constrói um grafo acíclico dirigido (DAG):

1. `backdropGrab` (`ShellRoot.qml:523`) captura exclusivamente `wallpaperBackdropRoot` (camada base).
2. **`ValoInternalWindowBackdropV2`** (`ValoInternalWindowBackdropV2.qml`):
   - Ordena `glassChassis` (`SystaliaFilesWindowV2`) e `materialWindow` (`SystaliaMaterialWindow`) por `z`: identifica `frontWindow` e `rearWindow`.
   - Se sobrepõem no espaço da cena, compõe offscreen dentro de `composition` (`visible: false`):
     1) Um `ShaderEffect` (`valo_backdrop_copy.frag.qsb`) desenhando `wallpaperSource` em tamanho cheio do viewport;
     2) Um `ShaderEffectSource` (`rearCapture`) **do tamanho do viewport** (`anchors.fill: parent`) com `sourceItem: rearWindow` e `sourceRect: Qt.rect(-rearPosition.x, -rearPosition.y, root.width, root.height)`. O comentário em `ValoInternalWindowBackdropV2.qml:51-53` explica o motivo crítico: os shaders ópticos usam `gl_FragCoord` de viewport; uma captura do tamanho exato da janela deslocaria as coordenadas UV do backdrop dentro do passo offscreen.
   - `compositeGrab` captura `composition`. Assim, `sourceFor(frontWindow)` entrega `compositeGrab` (wallpaper + `rearWindow`) e `sourceFor(rearWindow)` entrega `wallpaperSource`.
3. **`ValoSingleWindowBackdropV2` / `ValoLimitedBackdropV2`** (`ShellRoot.qml:584-653`): aplicam o mesmo padrão para `topBar` e `systemDock` sobre janelas internas (quando não há clientes externos interpostos no produtor).

### 2.2. Por que falha atualmente no `WaylibNestedShell.qml`

1. **Entre janelas internas (`filesWindow` e `materialWindow`):** ambas recebem `backdropSource: desktopBackdropSource`. Como `desktopBackdropSource` captura apenas `desktopContentLayer` (`z: 10`), ele nunca contém `internalWindowsLayer` (`z: 40`). Logo, `materialWindow` (`z: 32`) refrata o wallpaper e os clientes Wayland, mas fica completamente cega para `filesWindow` (`z: 31`) atrás dela (e vice-versa).
2. **Entre janelas clientes (`WaylibWindowFrame.qml`):** todos os `WaylibWindowFrame` dentro de `clientLayer` recebem `backdropSource: wallpaperOnlyBackdropSource` (que contém apenas `valoWallpaper`). Nenhum cliente sobreposto enxerga outro cliente abaixo dele.
3. **Nos overlays (`topBar`, `valoDock`, `controlCenter`, `flowMenu`):** recebem `desktopBackdropSource` (`desktopContentLayer`), de modo que janelas internas abertas (`filesWindow` / `materialWindow`) atrás da Dock, do `controlCenter` ou do `flowMenu` também ficam invisíveis através do vidro do overlay.

### 2.3. Nova Arquitetura Acíclica Proposta para `WaylibNestedShell.qml`

```
[Stage 0] valoWallpaper ──► wallpaperOnlyBackdropSource (wallpaper puro)
                │
                ▼
[Stage 1..N] WaylibWindowFrame ordenados por windowZOrder ascendente (slice_0 .. slice_{k-1})
   - Frame k recebe backdropSource = clientStageSource[k]
     (para k = 0 -> wallpaperOnlyBackdropSource;
      para k > 0 -> composição offscreen [clientStageSource[k-1]] + [frame_{k-1} em sourceRect(-pos.x, -pos.y, W, H)])
   - Zero ciclo: frame k nunca está dentro de clientStageSource[k]!
                │
                ▼
[desktopBackdropSource] captura desktopContentLayer (wallpaper + todos os clientes Wayland)
                │
                ▼
[internalWindowBackdrop] = ValoInternalWindowBackdropV2 {
   firstWindow: filesWindow
   secondWindow: materialWindow
   wallpaperSource: desktopBackdropSource
}
   - Janela interna traseira recebe desktopBackdropSource (wallpaper + clientes Wayland)
   - Janela interna frontal recebe compositeGrab (wallpaper + clientes Wayland + janela interna traseira)
                │
                ▼
[overlayCompositeBackdropSource] captura Item offscreen {
   ShaderEffect(desktopBackdropSource) + ShaderEffectSource(internalWindowsLayer, tamanho viewport)
}
   - TopBar, Dock, SystaliaControlCenterV2 e SystaliaFlowMenu recebem overlayCompositeBackdropSource
     (enxergam wallpaper + clientes Wayland + Systalia Files + Systalia Control Center, sem feedback loop!)
```

**Observação de escala/performance:** Para a cadeia de clientes Wayland em `clientLayer`, manter um pool fixo/limitado (ex.: até `kMaxStackedClientBackdrops = 6` níveis ativos ordenados por `zOrder` usando `Repeater`, com fallback para o estágio anterior caso existam mais de 6 clientes simultâneos) evita recriação de FBOs a cada mudança de foco e mantém custo previsível em GPU.

---

## 3. Análise do Menu Esquerdo da `SystaliaTopBarV2`

- **Componente da versão sem Waylib:** [SystaliaFlowMenu.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/overlays/SystaliaFlowMenu.qml), instanciado em `ShellRoot.qml:903-912` com `id: flowMenu` (`z: 120`, acima da TopBar e Dock) e acionado por:
  - `topBar.onLauncherRequested` -> `flowMenu.toggle()` (`ShellRoot.qml:916`) — emitido pelo `TapHandler` no ícone SVG oficial da SYSTALIA à esquerda da TopBar (`SystaliaTopBarV2.qml:104-117`);
  - Atalho global `Meta+Space` (`ShellRoot.qml:987-991`);
  - Fechamento ao clicar fora (`MouseArea` no fundo `#40020a18`), clicar em um aplicativo lançado ou pressionar `Escape`.
- **Contratos exigidos por `SystaliaFlowMenu.qml`:**
  1. **Singleton `AppRegistry` (`Systalia.System 1.0`):** [AppRegistryService](file:///home/lucasramos/Documentos/systalia-glass-engine/src/services/applications/AppRegistryService.hpp) fornece `applicationCount`, `filterText` e o modelo de entradas `.desktop` (`name`, `terminal`, `command`, `arguments`, `icon`, `categories`). **Ausente** em `WaylibCompositorAdapter::registerQmlTypes()`.
  2. **Image Provider `image://desktopIcons/`:** [DesktopIconProvider](file:///home/lucasramos/Documentos/systalia-glass-engine/src/services/applications/AppRegistryService.hpp#L68-L74) registrado em `main.cpp:194,328` (`engine.addImageProvider("desktopIcons", new DesktopIconProvider())`). **Ausente** em `waylib_nested_main.cpp` (e no `QQmlApplicationEngine` de `tst_WaylibBackend.cpp`).
  3. **`ProcessLauncher` (`Systalia.System 1.0`):** `ProcessLauncher.launch` e `ProcessLauncher.launchInTerminal` — já conectado ao `socketName` do Waylib desde a Etapa 03.2.5.3.3.
  4. **Propriedades ópticas:** `backdropSource`, `blurProfile`, `userHighContrast`, `userPrefersReducedMotion`, `screenResolution`, `animationTime`.

---

## 4. Análise Completa do Comportamento do `VS Code` e Protocolos Wayland

### 4.1. Evidências das Execuções Diagnósticas Controladas

1. **Host KDE Plasma com VS Code ativo (`~/.config/Code/code.lock`):**
   - `pgrep -af "/code"` mostrou PID `146242 /usr/share/code/code` rodando na sessão host (`~/.config/Code`), e `ls -la ~/.config/Code/code.lock` + `/run/user/1000/vscode-a895ac24-1.14-main.sock` confirmaram socket IPC ativo.
   - O script `/usr/bin/code` executa `ELECTRON_RUN_AS_NODE=1 /usr/share/code/code /usr/share/code/resources/app/out/cli.js "$@"`. Sem `--user-data-dir` distinto, `cli.js` conecta ao IPC existente do host KDE Plasma e encerra o processo recém-lançado sem abrir janela no Waylib.
   - Além disso, sem `--wait`, `cli.js` faz spawn destacado e encerra imediatamente (`ProcessLauncher` vê o PID do wrapper finalizar antes da janela mapear).
2. **Execução com `--user-data-dir` isolado em `systalia_waylib_nested` (nested `WL-1` e `--headless`):**
   - Com `--user-data-dir=/tmp/sge-diag-wl1` e `--wait`, `/usr/bin/code` (tanto com quanto sem `--ozone-platform=wayland`) permaneceu aberto por toda a janela de diagnóstico (`WL1_CODE_RC=124` no timeout de 4,5s), mapeando `xdg_toplevel` (`com.microsoft.VSCode`).
   - Entretanto, o cliente emitiu erros/avisos críticos relacionados a protocolos faltantes no compositor:
     - `ERROR:content/browser/browser_main_loop.cc:276] Gdk: gdk_seat_get_keyboard: assertion 'GDK_IS_SEAT (seat)' failed`
     - `WARNING:ui/ozone/platform/wayland/host/wayland_connection.cc:408] text-input-v3 not available.`
     - Ausência de `wl_data_device_manager` (confirmada via `wayland-info`), deixando clipboard/drag-and-drop/GDK sem dispositivo de dados no `wl_seat`.
   - Em execuções headless sem `WL-1` ou após desconexão de socket, o Chromium aborta com `ERROR:ui/aura/env.cc:246] The platform failed to initialize. Exiting.`.

### 4.2. Comparativo de Protocolos Wayland e Serviços (`Sem Waylib` vs. `WaylibCompositorAdapter`)

| Protocolo / Serviço | Versão Sem Waylib (`SystaliaCompositor` / `main.cpp`) | Waylib Atual (`WaylibCompositorAdapter`) | Status / Impacto |
|---|---|---|---|
| `xdg_wm_base` | `QWaylandXdgShell` | `WXdgShell` v5 | Paridade |
| `zxdg_decoration_manager_v1` | `QWaylandXdgDecorationManagerV1` (`ServerSideDecoration`) | `WXdgDecorationManager` (`Server`) | Paridade |
| `wp_viewporter` | `QWaylandViewporter` | `wlr_viewporter_create` | Paridade |
| `wl_compositor` / `wl_subcompositor` | Nativo `QtWaylandCompositor` | `wlr_compositor_create` + `wlr_subcompositor_create` | Paridade |
| `wl_data_device_manager` | Nativo `QtWaylandCompositor` (`QWaylandSeat`) | **Ausente** (`wlr_data_device_manager_create` existe em `wlr/types/wlr_data_device.h`) | **Crítico:** clipboard, drag-and-drop e GDK/Electron |
| `zwp_primary_selection_device_manager_v1` | — | **Ausente** (`wlr_primary_selection_v1_device_manager_create` disponível) | Recomendado para seleção primária X11/Wayland no Electron |
| `xdg_activation_v1` | — | **Ausente** (`wlr_xdg_activation_v1_create` disponível) | Recomendado para ativação/foco seguro de apps Electron/GTK |
| `wp_cursor_shape_manager_v1` | Nativo Qt 6.8+ | **Ausente** (`WCursorShapeManagerV1` em `wcursorshapemanagerv1.h`) | Cursores nativos do cliente sem fallback |
| `wp_fractional_scale_manager_v1` | — | **Ausente** (`wlr_fractional_scale_manager_v1_create` disponível) | Escala fracionária no Chromium/Qt |
| `zwp_text_input_manager_v3` | — | **Ausente** (`wlr_text_input_manager_v3_create` disponível) | IME / entrada de texto no Electron (warning explícito) |
| `AppRegistry` (`Systalia.System`) + `DesktopIconProvider` | Registrados em `main.cpp:171-194,328` | **Ausentes** | **Obrigatório** para `SystaliaFlowMenu` |
| `Notifications` / `OutputManager` (`Systalia.System`) | Registrados em `main.cpp:176-185` | **Ausentes** | **Necessário** para `SystaliaNotificationHost` |

---

## 5. Matriz de Lacunas Remanescentes (`ShellRoot.qml` / `main.cpp` vs. Waylib)

| Recurso em `ShellRoot.qml` / `main.cpp` | Estado Atual no `WaylibNestedShell.qml` / `waylib_nested_main.cpp` | Prioridade de Correção |
|---|---|---|
| `SystaliaFlowMenu` (`flowMenu` + `Meta+Space` + `onLauncherRequested`) | Ausente | **P0 (Etapa 03.2.5.3.6)** |
| Transparência real inter-janelas (`ValoInternalWindowBackdropV2` + cadeia clientes + `overlayCompositeBackdropSource`) | Ausente (apenas 2 fontes estáticas) | **P0 (Etapa 03.2.5.3.6)** |
| `wl_data_device_manager`, `primary_selection_v1`, `xdg_activation_v1`, `cursor_shape_v1`, `fractional_scale_v1`, `text_input_v3` | Ausentes em `WaylibCompositorAdapter.cpp` | **P0 (Etapa 03.2.5.3.6)** |
| Lançamento isolado de Electron/VS Code quando há sessão host rodando (`--ozone-platform=wayland`, fallback de perfil nested opcional ou `ELECTRON_OZONE_PLATFORM_HINT=wayland` + flags seguras) | Herda `DISPLAY` e colide com `~/.config/Code/code.lock` do KDE Plasma | **P0 (Etapa 03.2.5.3.6)** |
| `SystaliaNotificationHost` + `Notifications` (`Systalia.System`) | Ausente em `WaylibNestedShell.qml` / `registerQmlTypes()` | **P1 (Etapa 03.2.5.3.6)** |
| `softwareBrightnessDimmer` (`SystemControl.softwareBrightness`) | Ausente em `WaylibNestedShell.qml` | **P1 (Etapa 03.2.5.3.6)** |
| `SystaliaTaskSwitcher` (visual `Alt+Tab` com thumbnails ao vivo) | Apenas `cycleFocus()` direto sem overlay visual | **P2 (pós-estabilização)** |
| Multi-workspace visual (`WorkspaceViewport`) / Liquid Tiling (`SnapPreviewOverlay` + `TileSplitter`) | Apenas indicador na TopBar sem páginas horizontais/tiling | **P2 (pós-estabilização)** |
| `XWaylandManager` (`WXWayland`) | Reservado explicitamente para **Etapa 03.2.5.4** | **Etapa 03.2.5.4** |

---

## 6. Plano Técnico de Correção Passo a Passo (Para Execução na Etapa 03.2.5.3.6)

### Sub-etapa A — Protocolos Wayland, Singletons e Icon Provider C++
- **Arquivos:**
  - `src/compositor/waylib/WaylibCompositorAdapter.cpp`
  - `src/app/waylib_nested_main.cpp`
  - `tests/compositor/tst_WaylibBackend.cpp`
- **Alterações detalhadas:**
  1. Em `WaylibCompositorAdapter::initialize()`, registrar:
     - `wlr_data_device_manager_create(m_server->handle());`
     - `wlr_primary_selection_v1_device_manager_create(m_server->handle());`
     - `wlr_xdg_activation_v1_create(m_server->handle());`
     - `wlr_fractional_scale_manager_v1_create(m_server->handle(), 1);`
     - `wlr_text_input_manager_v3_create(m_server->handle());`
     - `m_server->attach<WCursorShapeManagerV1>();`
     (Incluir `<wcursorshapemanagerv1.h>` e verificar headers em `wlr_all.h` / `<wlr/types/...>` no build `-Werror`).
  2. Em `WaylibCompositorAdapter::registerQmlTypes()`, registrar `AppRegistryService` (`AppRegistry`) e `NotificationServer` (`Notifications`).
  3. Em `waylib_nested_main.cpp` (e na criação de `engine` em `tst_WaylibBackend.cpp`), registrar `engine.addImageProvider(QStringLiteral("desktopIcons"), new DesktopIconProvider());`.

### Sub-etapa B — Lançamento de Apps Wayland/Electron no `ProcessLauncherService` e `WaylibNestedShell.qml`
- **Arquivos:**
  - `src/services/applications/ProcessLauncherService.hpp` / `.cpp`
  - `src/qml/waylib/WaylibNestedShell.qml`
- **Alterações detalhadas:**
  1. Em `ProcessLauncherService::waylandProcessEnvironment()`, adicionar `ELECTRON_OZONE_PLATFORM_HINT=wayland` (mantendo `QT_QPA_PLATFORM=wayland` e `GDK_BACKEND=wayland`) e, quando em modo nested isolado (via propriedade opcional `isolatedNestedSession` / `waylandDisplay != "wayland-systalia-0"`), remover `DISPLAY` se ainda não houver XWayland ativo no Waylib (`WAYLAND_DISPLAY` já aponta para o socket nested, evitando fallback X11 no KDE Plasma antes da Etapa 03.2.5.4).
  2. Para abertura do `VS Code` pela Dock (`onIdeRequested`) no ambiente nested quando `~/.config/Code/code.lock` pertencer a outro compositor host, documentar/suportar `--user-data-dir` por sessão nested quando configurado (`SGE_NESTED_ISOLATED_APP_PROFILE=1` ou arg `--new-window` / perfil dedicado no launcher nested para Electron) — sem quebrar execução em sessão bare-metal/única onde `~/.config/Code` já roda dentro do próprio SYSTALIA.

### Sub-etapa C — Pipeline Acíclico de Transparência Inter-Janelas em `WaylibNestedShell.qml`
- **Arquivos:**
  - `src/qml/waylib/WaylibNestedShell.qml`
  - `src/qml/waylib/WaylibWindowFrame.qml`
- **Alterações detalhadas:**
  1. **Clientes Wayland encadeados (`clientBackdropChain`):**
     - Ordenar os frames de `surfaceRepeater` por `windowZOrder` (ou manter estágios por índice ordenado `0 .. 5`):
     - Construir `Repeater` de estágios offscreen (`Item` oculto com `ShaderEffect(prevSource)` + `ShaderEffectSource(frame_{k-1}, anchors.fill: parent, sourceRect: Qt.rect(-pos.x, -pos.y, width, height))`) + `ShaderEffectSource` de saída `stageGrab[k]`.
     - Cada `WaylibWindowFrame` recebe `backdropSource: orderedStageSourceFor(windowZOrder)` -> enxerga wallpaper + todas as janelas clientes abaixo dela, sem ciclo de feedback.
  2. **Janelas internas (`ValoInternalWindowBackdropV2`):**
     - Instanciar `ValoInternalWindowBackdropV2` com `firstWindow: filesWindow`, `secondWindow: materialWindow`, `wallpaperSource: desktopBackdropSource`.
     - Conectar `filesWindow.backdropSource: internalWindowBackdrop.sourceFor(filesWindow)` e `materialWindow.backdropSource: internalWindowBackdrop.sourceFor(materialWindow)`.
     - Assim, quando `Systalia Control Center` (`materialWindow`) sobrepuser `Systalia Files` (`filesWindow`), a refração e o blur Dual-Kawase exibirão `filesWindow` + clientes Wayland + wallpaper através do vidro.
  3. **Overlays (`topBar`, `valoDock`, `controlCenter`, `flowMenu`, `notificationHost`):**
     - Criar `overlayCompositeBackdropSource` capturando um `Item` offscreen com `ShaderEffect(desktopBackdropSource)` + `ShaderEffectSource(internalWindowsLayer)` (excluindo `shellChromeLayer`), fornecendo blur real sobre wallpaper + clientes + janelas internas sem loop.

### Sub-etapa D — Integração de `SystaliaFlowMenu`, `SystaliaNotificationHost` e `softwareBrightnessDimmer`
- **Arquivos:**
  - `src/qml/waylib/WaylibNestedShell.qml`
- **Alterações detalhadas:**
  1. Instanciar `SystaliaFlowMenu` (`objectName: "waylibFlowMenu"`, `z: 120`, `backdropSource: overlayCompositeBackdropSource`, `blurProfile`, `screenResolution`, `animationTime`).
  2. Conectar `valoTopBar.onLauncherRequested` -> `flowMenu.toggle()` e adicionar `Shortcut { sequence: "Meta+Space"; context: Qt.ApplicationShortcut; onActivated: flowMenu.toggle() }`.
  3. Instanciar `SystaliaNotificationHost` (`z: 118`) e `softwareBrightnessDimmer` (`Rectangle` vinculado a `SystemControl.softwareBrightness` / `SystemControl.brightness`).

### Sub-etapa E — Expansão de Testes Automatizados e Validação (Etapa 03.2.5.3.6)
- **Casos a adicionar em `tests/compositor/tst_WaylibBackend.cpp`:**
  1. **Protocolos Wayland no servidor:** verificar via `wl_registry` (ou cliente/inspeção `WServer`) que `wl_data_device_manager`, `zwp_primary_selection_device_manager_v1`, `xdg_activation_v1`, `wp_fractional_scale_manager_v1`, `wp_cursor_shape_manager_v1` e `zwp_text_input_manager_v3` estão publicados no socket Waylib.
  2. **Menu esquerdo da TopBar (`SystaliaFlowMenu`):** acionar `topBar.launcherRequested()` -> verificar `waylibFlowMenu` visível (`opened == true`), `AppRegistry` disponível, `desktopIcons` registrado, e fechamento via `flowMenu.close()`.
  3. **Transparência inter-janelas (`SystaliaMaterialWindow` sobre `SystaliaFilesWindowV2`):** abrir ambas (`filesRequested` + `settingsRequested`), posicioná-las sobrepostas com `materialWindow.z > filesWindow.z`, verificar `internalWindowBackdrop.compositeAvailable == true` e `materialWindow.backdropSource !== desktopBackdropSource` (`internalPairBackdropCaptureV2` ativo), e para 2 clientes Wayland sobrepostos verificar que o `backdropSource` do cliente frontal difere de `wallpaperOnlyBackdropSource`.
- **Critério final da Etapa 03.2.5.3.6:** Build incremental Release em `build/waylib` (`-DSGE_ENABLE_WAYLIB_BACKEND=ON`, `-Werror`), **61/61+ CTest aprovados** e smoke test ao vivo nested em `wayland-1` com `RC=0` e `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA`.
