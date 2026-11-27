# SYSTALIA — ETAPA 03.2.5.3.8
## Execução do Plano de Interação Desktop Waylib: Alternador de Quadros da TopBar, Movimento Livre Sob Dock/TopBar, Resize 8-Direções e Sistema de Cursores Dinâmicos VALO em QML

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Documento de referência:** [`docs/engineering/ETAPA_03_2_5_3_7_WAYLIB_WORKSPACES_ZORDER_RESIZE_CURSORS_PLAN.md`](ETAPA_03_2_5_3_7_WAYLIB_WORKSPACES_ZORDER_RESIZE_CURSORS_PLAN.md)  
**Data:** 2026-11-27  
**Status:** Concluída (`61/61` CTest com `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, build incremental `16.67 s`, smoke test nested ao vivo com `RC=0`)

---

## 1. Alternador de Quadros da `SystaliaTopBarV2` e Sincronização com `WorkspaceManager` / `WaylibSurfaceAdapter`

- **`src/compositor/waylib/WaylibSurfaceAdapter.{hpp,cpp}`:**
  - Adicionado `WorkspaceIndexRole` (`"workspaceIndex"`) em `WaylibSurfaceListModel` e campo `int workspaceIndex = 0;` em `Entry`.
  - `onToplevelAdded()` atribui automaticamente o `currentWorkspace()` do `WorkspaceManagerService::instance()`.
  - Expostos `setSurfaceWorkspace(surface, index)`, `surfaceWorkspace(surface)` e `topVisibleSurfaceForWorkspace(index, exclude)`.
  - `topVisibleSurface()` e `cycleFocus()` passam a priorizar superfícies do workspace ativo.
- **`src/shell/topbar/SystaliaTopBarV2.qml`:**
  - Expandida a área clicável das pílulas (`workspaceDelegate`) para toda a altura da pista (`height: workspaceIndicatorTrack.height`, `26 px`) com `MouseArea` (`objectName: "workspacePillMouseArea_" + index`, `hoverEnabled: true`, `cursorShape: Qt.PointingHandCursor`) chamando `workspaceManager.switchToWorkspace(index)` e `topBar.workspaceSelected(index)`.
- **`src/qml/waylib/WaylibNestedShell.qml` & `src/qml/waylib/WaylibWindowFrame.qml`:**
  - Adicionada transição horizontal animada (`320 ms`, `Easing.OutCubic`) e filtragem de visibilidade por workspace para clientes Wayland (`WaylibWindowFrame`) e janelas internas (`filesWindow` e `materialWindow` via `Translate`), mantendo a cadeia acíclica de backdrop intacta.
  - Registradas todas as janelas em `WorkspaceManager.setWindowWorkspace(...)` para acender os pontos indicadores da TopBar; `raiseFileWindow()` e `raiseMaterialWindow()` alternam automaticamente para o workspace da janela.
  - Conectado `valoTopBar.onWorkspaceSelected` (fechando popups e focando `topVisibleSurfaceForWorkspace(index)`) e adicionados atalhos `Ctrl+Alt+Left` e `Ctrl+Alt+Right`.

---

## 2. Remoção do Recorte de `clientLayer` e Movimentação Livre Sob Dock e TopBar

- **`src/qml/waylib/WaylibNestedShell.qml`:**
  - Alterado `clip: false` em `clientLayer`, mantendo `anchors.topMargin: 60` e `anchors.bottomMargin: valoDock.height + 24` apenas como geometria de `workAreaWidth`/`workAreaHeight` para spawn e maximização.
  - Adicionados limites leves de segurança em `filesWindow` e `materialWindow` (`y` entre `-12` e `renderWindow.height - 40`), permitindo passagem por trás da `SystaliaTopBarV2` (`z: 100`) e da `SystaliaDockV2` (`z: 95`).
- **`src/qml/waylib/WaylibWindowFrame.qml`:**
  - Substituído o clamping restrito na workarea em `titleDragArea.onPositionChanged` por clamping em coordenadas globais do desktop (`sceneY` de `0` até `desktopHeight - Math.max(36, titleBarHeight)` e lateral mantendo pelo menos `Math.min(120, width * 0.3)` px visíveis), permitindo deslize livre por trás da TopBar e da Dock com blur/refração em tempo real via `overlayCompositeBackdropSource`.

---

## 3. Sistema de Redimensionamento (`Resize`) em 8 Direções

- **`src/qml/waylib/WaylibWindowFrame.qml`:**
  - Substituído o único `resizeCornerHandle` pelo `Repeater` de **8 zonas** (`resize_top`, `resize_bottom`, `resize_left`, `resize_right`, `resize_topLeft`, `resize_bottomRight`, `resize_topRight`, `resize_bottomLeft`) com `borderMargin: 8` nas bordas, `cornerMargin: 12` nos cantos (`z: 102`), `hoverEnabled: true` e `preventStealing: true`.
  - Implementados `beginResize`, `updateResize` e `finishResize` com ancoragem `fixedRight`/`fixedBottom` para `LeftEdge`/`TopEdge`, bloqueio de entrada na superfície cliente (`toplevelItem.enabled: !frame.resizing`), respeito a `toplevelItem.minimumSize`/`maximumSize` e throttle `LiquidTiling.shouldDispatchInteractiveResize(true)` com flush no release.
  - Conectado `onResizeRequested(surface, edgeFlags)` para resize iniciado por clientes (`xdg_toplevel::resize`).
- **`src/qml/waylib/WaylibNestedShell.qml`:**
  - Inicializadas as geometrias de `filesWindow` e `materialWindow` de forma imperativa em `Component.onCompleted` e anexado `ShellResizeHandles` (8 direções, `minimumWidth: 520`, `minimumHeight: 400`) em ambas (`waylibFilesResizeHandles` e `waylibMaterialResizeHandles`).

---

## 4. Sistema de Cursores Dinâmicos VALO em QML (`ValoSoftwareCursor.qml`)

- **`src/graphics/v2/qml/ValoSoftwareCursor.qml` + `cmake/SgeQml.cmake`:**
  - Criado componente 100% em QML `Canvas` (sem arquivos `.svg` nesta etapa) com paleta VALO (`#0b1626`, `#e2f1ff`, `#39F5F4`, `#ff5f6d`) e **11 estados contextuais** com dimensões e `hotspot (x, y)` dedicados: `default` (`24×24`, `(1,1)`), `pointer` (`24×24`, `(7,2)`), `grab` (`24×24`, `(12,12)`), `grabbing` (`24×24`, `(12,12)`), `resize-ew` (`26×24`, `(13,12)`), `resize-ns` (`24×26`, `(12,13)`), `resize-nwse` (`24×24`, `(12,12)`), `resize-nesw` (`24×24`, `(12,12)`), `text` (`20×24`, `(10,12)`), `busy` (`24×24`, `(12,12)` animado) e `not-allowed` (`24×24`, `(12,12)`).
  - Registrado em `cmake/SgeQml.cmake` no módulo `Systalia.Engine 1.0`.
- **`src/compositor/waylib/WaylibCompositorAdapter.{hpp,cpp}`:**
  - Expostas as propriedades reativas `cursorShapeKey`, `compositorCursorOverride` e `hoveredCursorKey`, além dos mappers estáticos `cursorShapeKeyForQtShape` e `cursorShapeKeyForWaylibShape` e listeners de `WCursor` (`cursorChanged`, `requestedCursorShapeChanged`, `requestedCursorSurfaceChanged`).
  - Precedência determinística: `compositorCursorOverride` > `hoveredCursorKey` > `m_cursor->cursor().shape()` > `m_cursor->requestedCursorShape()` > `"default"`.
  - Integrado `onContainsMouseChanged` nos 8 handles de resize e `titleDragArea` (`"grab"`) em `WaylibWindowFrame.qml` e nos `ShellResizeHandles` em `WaylibNestedShell.qml`, garantindo mudança imediata do cursor ao passar o mouse sobre qualquer borda/canto no compositor nested.
  - Conectado `ProcessLauncher.onProcessStarted` a feedback `"busy"` de `900 ms`.

---

## 5. Validação (Build Incremental, CTest e Smoke Test Nested)

- **Build incremental (`Release`, `-Werror`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`):**
  - `cmake --build build/waylib -j "$(nproc)"` concluído em **`16.67 s`** (`real 16.67`) com 0 erros/warnings.
- **Testes direcionados:**
  - `ctest --test-dir build/waylib -R "^(tst_waylib_backend|tst_valo_shell_v2|tst_shell_root_integration|tst_xdg_resize)$" --output-on-failure`: **`4/4` aprovados (`15.75 sec`)**.
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):**
  - **`61/61` aprovados (`100% tests passed, 0 tests failed out of 61`, `42.41 sec`)**.
- **Smoke test ao vivo nested (`./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500`):**
  - Encerrado com **`RC=0`**, exibindo `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760`, `[SGE-BACKDROP-V2] Blur GPU executado com sucesso! ... format=1, size=1280x760`, `waylib.output.buffer: Selected primary buffer format: "XR24" "(0x34325258)" for output: "WL-1"` e zero erros críticos de QML.

---

## 6. Prontidão para a Etapa 03.2.5.4 (XWayland no Waylib)

Com o pipeline óptico VALO V2, protocolos Wayland essenciais, alternância de workspaces, movimentação livre sob chrome, redimensionamento em 8 direções e motor completo de cursores dinâmicos em QML validados em `61/61` no CTest, o backend Waylib está pronto para a **Etapa 03.2.5.4** (integração de XWayland isolado).
