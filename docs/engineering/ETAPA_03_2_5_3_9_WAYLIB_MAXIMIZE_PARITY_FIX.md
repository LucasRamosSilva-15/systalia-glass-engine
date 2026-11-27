# SYSTALIA — ETAPA 03.2.5.3.9
## Correção Visual e Funcional da Maximização de Janelas no Waylib (Redimensionamento Real da Superfície, Auto-Hide da Dock, Moldura e Paridade com `ShellRoot.qml`)

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Data:** 2026-11-27  
**Status:** Concluída (`61/61` CTest com `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, build incremental `13.11 s`, smoke test nested ao vivo com `RC=0`)

---

## 1. Causa Raiz da Divergência entre Moldura e Superfície Cliente ao Maximizar

1. **Disparo de tamanho desatualizado em `WaylibWindowFrame.qml`:**
   - Em `WaylibWindowFrame.qml`, `syncMaximizedSurface()` lia `frame.width` / `frame.height` dentro de `onIsMaximizedChanged`, antes de os bindings dependentes de `isMaximized` (ou da expansão de `clientLayer` pelo auto-hide da Dock) terem estabilizado, enviando frequentemente as dimensões antigas em vez da área maximizada real.
2. **Atraso/ausência de despacho do `configure` no event loop do Waylib (`WaylibSurfaceAdapter`):**
   - `wlr_xdg_toplevel_set_size` / `wlr_xdg_surface_schedule_configure` agenda um callback `idle` no `wl_event_loop` da `wlroots`, que na integração Qt do `WServer` podia não ser despachado nem drenado para o socket do cliente imediatamente após chamadas QML/C++.
   - A solução adicionou `setSurfaceMaximized(surface, maximized, clientWidth, clientHeight)` e despacho enfileirado (`QTimer::singleShot(0, this, ...)` com `wl_event_loop_dispatch` + `wl_display_flush_clients`) em `setSurfaceMaximized` e `resizeSurface`, garantindo envio imediato do `configure` com o tamanho exato da área cliente e resposta real do cliente Wayland (`surfA->getContentGeometry()` e `toplevelItem` acompanhando `clientViewport`).
3. **`opaqueClientBacking` com `anchors.fill: parent`:**
   - Antes do ajuste, `opaqueClientBacking` preenchia todo `clientViewport` independentemente do buffer já comitado pelo cliente, gerando o retângulo preto vazio. Agora `opaqueClientBacking` acompanha `Math.min(parent.width, toplevelItem.width)` × `Math.min(parent.height, toplevelItem.height)`.
4. **Moldura arredondada simétrica:**
   - Mantidos `frameMargin: 8` e `frameRadii: Qt.vector4d(cornerRadius, ...)` na janela maximizada em `[8, 60, desktopWidth - 16, desktopHeight - 68]`, garantindo cantos arredondados e padding simétrico compatíveis com o `ShellRoot.qml`.

---

## 2. Auto-Hide da `SystaliaDockV2` e Área Útil Maximizada (`[8, 60, width - 16, height - 68]`)

- Em `WaylibSurfaceAdapter.{hpp,cpp}`:
  - Adicionados `Q_PROPERTY(int maximizedRevision READ maximizedRevision NOTIFY maximizedStateChanged)` e `hasMaximizedSurfaceInWorkspace(int workspaceIndex)`.
- Em `WaylibNestedShell.qml`:
  - Adicionada a propriedade reativa `hasMaximizedWindowInActiveWorkspace`, verificando `filesWindowMaximized`, `materialWindowMaximized` e `surfaceAdapter.hasMaximizedSurfaceInWorkspace(WorkspaceManager.currentWorkspace)`.
  - Quando `true`:
    - `valoDock` recolhe suavemente (`opacity: 0.0`, `anchors.bottomMargin: -height`, `visible: opacity > 0.01`, `enabled: false`).
    - `clientLayer` expande `anchors.bottomMargin` de `valoDock.height + 24` para `8`, liberando a área útil maximizada para `[8, 60, renderWindow.width - 16, renderWindow.height - 68]`.
  - Ao desmaximizar, minimizar ou trocar para um workspace sem janela maximizada, `valoDock` reaparece automaticamente.

---

## 3. Ajustes nas Janelas Internas (`filesWindow` e `materialWindow`) e Branding Limpo na `SystaliaTopBarV2`

- **`toggleFileWindowMaximized()` e `toggleMaterialWindowMaximized()` (`WaylibNestedShell.qml`):**
  - Salvam `filesWindowRestoreGeometry` / `materialWindowRestoreGeometry` antes de maximizar, aplicam `[8, 60, renderWindow.width - 16, renderWindow.height - 68]` (acionando o auto-hide da Dock) e restauram exatamente `x`, `y`, `width` e `height` anteriores ao desmaximizar.
  - Listeners `onWidthChanged`/`onHeightChanged` em `renderWindow` mantêm janelas internas maximizadas sincronizadas com redimensionamentos de `WL-1`.
- **`SystaliaTopBarV2` (`WaylibNestedShell.qml`):**
  - Definido `activeWindowTitle: ""` na instância `valoTopBar`, removendo `· <título truncado>` ao lado de `SDE-KWIN | VALO` para paridade visual completa com `ShellRoot.qml`.

---

## 4. Resultados de Testes, Build Incremental e Smoke Test

- **Expansão em `tests/compositor/tst_WaylibBackend.cpp` (+ atualização em `tests/compositor/tst_WaylibTestClient.cpp` para repintar/commitar em `resizeEvent`):**
  - Maximização e restauração de cliente Wayland (`frameAMaxTest` em `[8, 60, width - 16, height - 68]`, `surfA->isMaximized()`, `surfA->getContentGeometry()` e `toplevelItemAInit` preenchendo `clientViewport`, auto-hide da `valoDock` e retorno exato a `normalX/Y/Width/Height`).
  - Maximização e restauração de `filesWindow` e `materialWindow` em `[8, 60, width - 16, height - 68]` com auto-hide/reexibição da Dock.
  - Verificação de `topBar.activeWindowTitle == ""` e `activeWindowTitleLabel` oculto, mantendo `SDE-KWIN | VALO` limpo.
- **Build incremental (`Release`, `-Werror`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`):** **`13.11 s`** (`real 13.11`), 0 warnings/erros.
- **Testes direcionados (`tst_xdg_resize`, `tst_valo_shell_v2`, `tst_shell_root_integration`, `tst_waylib_backend`):** **`4/4` aprovados (`15.95 s`)**.
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):** **`61/61` aprovados (`100% tests passed, 0 tests failed out of 61`, `42.19 s`)**.
- **Smoke test nested ao vivo (`./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500`):** **`RC=0`**, `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760`, `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!`, `waylib.output.buffer: Selected primary buffer format: "XR24" "(0x34325258)" for output: "WL-1"`, zero erros críticos de QML.

---

## 5. Prontidão para a Etapa 03.2.5.4 (XWayland no Waylib)

Com maximização/restauração real da superfície cliente, auto-hide da Dock, janelas internas maximizáveis, TopBar limpa e `61/61` no CTest com `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, o backend Waylib está pronto para a **Etapa 03.2.5.4** (integração de XWayland isolado).
