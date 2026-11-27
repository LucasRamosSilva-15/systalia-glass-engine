# SYSTALIA — ETAPA 03.2.5.3.7
## Auditoria Profunda e Plano de Ação: Alternador de Quadros da TopBar, Movimento de Janelas Sob Dock/TopBar, Resize Multi-Direcional e Sistema de Cursores Dinâmicos VALO em QML

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Modo desta etapa:** Somente leitura no código-fonte (zero edições em `src/`, `tests/`, `cmake/`, `assets/`)  
**Próxima etapa de execução:** **Etapa 03.2.5.3.8** (build incremental exclusivo `Release` em `build/waylib` com `-DSGE_ENABLE_WAYLIB_BACKEND=ON` e `-Werror`)

---

## 1. Resumo Executivo das 4 Causas Raiz Confirmadas

| Frente | Sintoma no Waylib | Causas Raiz Confirmadas no Código | Hipóteses Residuais (a validar em runtime) |
| :--- | :--- | :--- | :--- |
| **1. Alternador de Quadros (`SystaliaTopBarV2` + `WorkspaceManager`)** | Clicar nos indicadores centrais da `SystaliaTopBarV2` não alterna o quadro/workspace das janelas. | **`WaylibNestedShell.qml` não consome `WorkspaceManager` nem instancia `WorkspaceViewport`:** `WaylibSurfaceAdapter`/`WaylibSurfaceListModel` não possuem `workspaceIndex`, `WaylibWindowFrame` usa `visible: !isMinimized` sem filtro de workspace, e `filesWindow`/`materialWindow` são estáticos em `internalWindowsLayer` sem `WorkspaceManager.setWindowWorkspace(...)` nem `switchToWorkspace(...)` ao focar/levantar janelas. | **Competição/alcance de clique na cápsula da TopBar:** em `WaylibNestedShell.qml:550-555`, o `MouseArea { z: -1; onPressed: mouse.accepted = true }` filho de `valoTopBar` combinado com `TapHandler` (`height: 14`, linha 265 de `SystaliaTopBarV2.qml`) dentro da faixa de `26px` (`workspaceIndicatorTrack`) pode interceptar cliques na margem ou disputar grab em `WOutputRenderWindow`. |
| **2. Janelas Presas Fora de Dock e TopBar** | Ao arrastar janelas (`Alacritty`, `Files`, `Control Center`), clientes ficam presos na área interna e não deslizam por trás da Dock e TopBar. | **Dois bloqueios duplos para clientes Wayland:** (a) `WaylibNestedShell.qml:272` define `clip: true` em `clientLayer` (`topMargin: 60`, `bottomMargin: valoDock.height + 24`, `left/rightMargin: 8`), cortando qualquer pixel fora da workarea; (b) `WaylibWindowFrame.qml:267-268` (`titleDragArea`) clampa `normalX` em `[0, workAreaWidth - width]` e `normalY` em `[0, workAreaHeight - height]`. Adicionalmente, `syncInitialSize()` (`191-192`) e `resizeCornerHandle` (`299-300`) aplicam o mesmo clamping; janelas internas não têm clamp de segurança para manter a barra de título alcançável. | **Clipe em bordas extremas no offscreen stage:** com `clip: false` em `clientLayer`, validar visualmente se `clientStageRepeater` (`sourceRect: Qt.rect(-framePos.x, -framePos.y, ...)`) recorta/renderiza sem artefatos quando `framePos.x < 0` ou `framePos.y < 0`. |
| **3. Resize Multi-Direcional Quebrado/Incompleto** | Não é possível redimensionar janelas pelas bordas e cantos (nem `Files`/`Control Center`). | **(a)** `WaylibNestedShell.qml:418-466` **não instancia `ShellResizeHandles`** em `filesWindow` e `materialWindow` (zero resize em janelas internas). **(b)** `WaylibWindowFrame.qml:272-308` possui **apenas 1 handle `14x14`** (`resizeCornerHandle`) no canto inferior direito — faltam as outras **7 direções** (`top`, `bottom`, `left`, `right`, `topLeft`, `topRight`, `bottomLeft`). **(c)** Falta `hoverEnabled: true` e `preventStealing: true` no `resizeCornerHandle`. **(d)** `onResizeRequested(surface, edgeFlags)` (`86-91`) ignora `edgeFlags`. **(e)** Falta ancoragem `fixedRight`/`fixedBottom` e clamping por `minimumSize`/`maximumSize` do toplevel em vez da workarea. | **Desempenho/fluidez durante arraste rápido:** `resizeSurface()` é disparado em todo `onPositionChanged` sem controle por `LiquidTiling.shouldDispatchInteractiveResize` e atualiza `normalWidth`/`normalHeight` antes do commit do buffer do cliente. |
| **4. Ícone de Resize Ausente e Motor de Cursores** | Ponteiro é sempre uma seta fixa estática, sem ícone de resize e sem `hotspot` dinâmico. | **(a)** `WaylibNestedShell.qml:730-757` renderiza um `Canvas` único estático (`softwarePointerArrow`, `20x24`, `x: position.x`, `y: position.y`) sem deslocamento de `hotspot` e sem reagir a `WCursor::cursorChanged`, `requestedCursorShapeChanged` ou `requestedCursorSurfaceChanged`. **(b)** `WCursor` (`wcursor.h`) não expõe `Qt::CursorShape` (via `QCursor::shape()`) nem `WGlobal::CursorShape` (`requestedCursorShape()`) como `Q_PROPERTY` legível em QML. **(c)** `resizeCornerHandle` em `WaylibWindowFrame.qml:281` não tem `hoverEnabled: true`, impedindo até a atualização da forma no `WOutputRenderWindow`. | **Superfície customizada de cliente (`wl_pointer.set_cursor`):** alguns apps (Electron/Chromium/GTK) enviam buffer de cursor ou `wp_cursor_shape_v1`; decidir precedência entre override da shell (resize/drag/busy) e pedido do cliente. |

---

## 2. Diagnóstico e Solução 1 — Alternador de Quadros da `SystaliaTopBarV2`

### 2.1 Fluxo na versão sem Waylib vs. estado atual no Waylib

1. **`SystaliaTopBarV2.qml` (linhas 47, 208-317):**
   - Recebe `property var workspaceManager: null`.
   - Renderiza `workspaceIndicatorTrack` (`objectName: "workspaceIndicatorTrack"`, `height: 26`, visível quando `width >= 720`) contendo um `Row` (`workspacePillsRow`) e `Repeater` sobre `workspaceManager.totalWorkspaces` (padrão `4`).
   - Cada `workspaceDelegate` (`width: isActive ? 28 : 18`, `height: 14`) usa `HoverHandler { id: pillHover; cursorShape: Qt.PointingHandCursor }` e `TapHandler` (linhas 306-313) que chama:
     ```qml
     if (topBar.workspaceManager)
         topBar.workspaceManager.switchToWorkspace(workspaceDelegate.index)
     topBar.workspaceSelected(workspaceDelegate.index)
     ```
   - O indicador de janelas abertas na pílula (`Rectangle` de `4x4` na linha 282) depende de `workspaceManager.workspaceWindowsRevision` e `workspaceManager.hasWindowsInWorkspace(index)`.

2. **Como o `ShellRoot.qml` (sem Waylib) efetiva a troca de quadros:**
   - `ShellRoot.qml:554-563` instancia `WorkspaceViewport { id: clientWindowsLayer }` (`src/shell/core/WorkspaceViewport.qml:1-57`), que cria 4 páginas horizontais (`workspacePage_0..3`), anima `workspaceContainer.x: -currentWorkspace * viewport.width` (`NumberAnimation { duration: 320; easing.type: Easing.OutCubic }`), calcula `withinViewportFrustum` e expõe `parallaxOffset` aplicado ao wallpaper (`ShellRoot.qml:495`: `x: clientWindowsLayer.parallaxOffset * 0.12`).
   - `WaylandWindowContainer.qml:35-37, 52-53` mantém `workspaceIndex`, `inActiveWorkspace`, `workspaceTransitioning` e `visible: !minimized && clientMapped && (inActiveWorkspace || (workspaceTransitioning && withinFrustum))`.
   - `ShellRoot.qml:402-410, 427-465` registra tanto as janelas internas (`glassChassis`, `materialWindow`) quanto cada `WaylandWindowContainer` via `WorkspaceManager.setWindowWorkspace(...)` e reparenta ao mudar (`Connections { target: WorkspaceManager; function onWindowWorkspaceChanged(...) }`, linhas 966-978).
   - `raiseFileWindow()`, `raiseMaterialWindow()` e `waylandServer.onWindowSelected` chamam `WorkspaceManager.switchToWorkspace(...)` para trazer o usuário até o quadro onde a janela reside (`ShellRoot.qml:81, 121, 945`).

3. **Lacunas confirmadas no backend Waylib:**
   - `WaylibCompositorAdapter::registerQmlTypes()` (`WaylibCompositorAdapter.cpp:65-69`) já registra `WorkspaceManager` (`WorkspaceManagerService`) e `WaylibNestedShell.qml:532` já passa `workspaceManager: WorkspaceManager` para `valoTopBar`.
   - Entretanto:
     1. `WaylibNestedShell.qml` **não possui `WorkspaceViewport`** nem reação a `WorkspaceManager.currentWorkspace` ou `topBar.workspaceSelected`.
     2. `WaylibSurfaceListModel` (`WaylibSurfaceAdapter.hpp:29-71`, `WaylibSurfaceAdapter.cpp:59-71`) não possui `WorkspaceIndexRole` (`"workspaceIndex"`), nem `Entry::workspaceIndex`.
     3. `WaylibSurfaceAdapter` (`WaylibSurfaceAdapter.hpp:137-151`) não expõe `setSurfaceWorkspace(...)` / `surfaceWorkspace(...)`, não inicia novas superfícies no `WorkspaceManager::instance()->currentWorkspace()`, e `topVisibleSurface()` / `cycleFocus()` / `activateSurface()` não filtram pelo workspace ativo.
     4. `WaylibWindowFrame.qml` não possui `workspaceIndex` / `inActiveWorkspace` nem atualiza `visible` quando o workspace muda (`visible: !isMinimized`).
     5. `filesWindow` (`SystaliaFilesWindowV2`) e `materialWindow` (`SystaliaMaterialWindow`) em `WaylibNestedShell.qml:424-465` nunca são registrados em `WorkspaceManager` nem ocultados/transladados quando o usuário muda para os workspaces 1, 2 ou 3, e `raiseFileWindow()`/`raiseMaterialWindow()` não sincronizam o workspace.
     6. **Atenção ao bloqueio de clique na TopBar (`WaylibNestedShell.qml:550-555` e `SystaliaTopBarV2.qml:208-314`):** o `MouseArea { anchors.fill: parent; z: -1; onPressed: mouse.accepted = true }` em `valoTopBar` não é o ideal sob um filho com `TapHandler` (grab passivo no press), e a altura de `workspaceDelegate` (`14px`) deixa faixas vazias acima e abaixo dentro de `workspaceIndicatorTrack` (`26px`).

### 2.2 Especificação da Correção

1. **`WaylibSurfaceAdapter` & `WaylibSurfaceListModel` (`src/compositor/waylib/WaylibSurfaceAdapter.{hpp,cpp}`):**
   - Adicionar `WorkspaceIndexRole` (`"workspaceIndex"`) em `WaylibSurfaceListModel` e campo `int workspaceIndex = 0;` em `Entry`.
   - Em `onToplevelAdded()`, inicializar `entry.workspaceIndex = WorkspaceManagerService::instance() ? WorkspaceManagerService::instance()->currentWorkspace() : 0;` (clamped em `[0, totalWorkspaces - 1]`).
   - Adicionar `Q_INVOKABLE void setSurfaceWorkspace(WXdgToplevelSurface *surface, int workspaceIndex)` e `Q_INVOKABLE int surfaceWorkspace(WXdgToplevelSurface *surface) const`.
   - Em `focusOrRestore(surface)`, emitir ou permitir que o shell troque o workspace ativo caso `entry.workspaceIndex != currentWorkspace`.
   - Em `topVisibleSurface()` e `cycleFocus()`, aceitar filtro opcional ou usar `WorkspaceManagerService::instance()->currentWorkspace()` para priorizar/alternar foco dentro do workspace ativo (e sincronizar foco ao trocar de workspace).

2. **`WaylibNestedShell.qml` + `WaylibWindowFrame.qml`:**
   - **Opções de composição dos quadros em Waylib (escolher a arquitetura mais limpa e acíclica para `ShaderEffectSource`):**
     - **Recomendado:** manter `surfaceRepeater` em `clientLayer` e as janelas internas em `internalWindowsLayer`, mas aplicar **offset animado horizontal baseado em workspace** (`workspaceX = (windowWorkspace - WorkspaceManager.currentWorkspace) * desktopContentLayer.width`, animado com `Behavior on workspaceShift` de `320 ms`, `Easing.OutCubic`, exatamente como `WorkspaceViewport.qml`) **e** visibilidade frustum (`visible: !isMinimized && (inActiveWorkspace || workspaceTransitioning)`). **Vantagem crítica no Waylib:** preserva intacta a cadeia acíclica `clientBackdropPool` e `overlayBackdropComposition` sem quebrar referências de `mapToItem(desktopContentLayer, ...)` nem duplicar `ShaderEffectSource` por página!
     - Registrar `filesWindow` e `materialWindow` em `WorkspaceManager` no `Component.onCompleted` (`WorkspaceManager.setWindowWorkspace(filesWindow, 0)`, `setWindowWorkspace(materialWindow, 0)`) e registrar cada `WaylibWindowFrame` criado/destruído (`WorkspaceManager.setWindowWorkspace(frame, frame.workspaceIndex)`), garantindo que os **pontos indicadores de janela** na `SystaliaTopBarV2` funcionem em tempo real para ambos os tipos de janela.
     - Em `raiseFileWindow()` e `raiseMaterialWindow()`, chamar `WorkspaceManager.switchToWorkspace(WorkspaceManager.windowWorkspace(...))` antes de elevar a janela (espelhando `ShellRoot.qml:81, 121`).
     - Adicionar parallax sutil no `valoWallpaper` durante a transição (semelhante a `ShellRoot.qml:495`, cuidando de não expor fundo vazio com `wallpaperParallaxExtension` ou scale/overscan `1.04`).
     - Conectar `topBar.onWorkspaceSelected`: além de `WorkspaceManager.switchToWorkspace(index)` já chamado pela própria `SystaliaTopBarV2`, atualizar `orderedClientFrames`, fechar menus flutuantes (`controlCenter.close()`, `flowMenu.close()`) e transferir foco para a janela de topo do novo workspace (`topVisibleSurfaceForWorkspace(index)` ou `activateSurface(null)` se vazio).
     - **Blindagem de cliques na `SystaliaTopBarV2` (`src/shell/topbar/SystaliaTopBarV2.qml` e `WaylibNestedShell.qml`):**
       - Em `SystaliaTopBarV2.qml`, expandir a área de toque de `workspaceDelegate` para `height: workspaceIndicatorTrack.height` (`26px`, centralizando visualmente a pílula `14px` dentro dele) e usar `MouseArea` (`acceptedButtons: Qt.LeftButton; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: ...`) ou garantir que o `MouseArea` anti-vazamento em `WaylibNestedShell.qml:550` não use `onPressed` aceitando sobre a área dos controles (ou seja removido caso `surfaceBase` / `SystaliaTopBarV2` já absorva cliques via `MouseArea` em `background` abaixo de `leftSection`/`centerSection`/`rightSection` **com** `MouseArea` de `workspaceDelegate` acima com `z` positivo).
     - Adicionar atalhos `Ctrl+Alt+Left` / `Ctrl+Alt+Right` (ou `Meta+1..4`) para troca rápida por teclado além do clique na TopBar.

---

## 3. Diagnóstico e Solução 2 — Movimento Livre de Janelas Sob Dock e TopBar

### 3.1 Regras na versão sem Waylib vs. bloqueios no Waylib

1. **Versão sem Waylib (`ShellRoot.qml`, `WaylandWindowContainer.qml`, `WorkspaceViewport.qml`, `SystaliaFilesWindowV2.qml`, `SystaliaMaterialWindow.qml`):**
   - `ShellRoot.qml:538-543, 554-557`: `workspace` tem `anchors.topMargin: 60` e `anchors.bottomMargin: 94`, mas **nem `workspace`, nem `WorkspaceViewport`, nem `workspacePage` definem `clip: true`**.
   - `SystaliaWindowHeader.qml:197-213` (`DragHandler { target: header.dragTarget }`), `SystaliaFilesWindowV2.qml:126-130` e `SystaliaMaterialWindow.qml:98-102` movem a janela diretamente em coordenadas sem clamping bloqueando `y` acima de `0` ou abaixo de `workspace.height - height`.
   - `topBar` (`z: 100`) e `dockCanvas` (`z: 95`) ficam acima de `clientWindowsLayer` (`z: 20`) e de `glassChassis` (`z: 10`) / `materialWindow` (`z: 11`), fazendo as janelas deslizar **por trás** do vidro de ambos e serem capturadas pelo `overlayBackdrop` / `backdropGrab`.

2. **Por que está bloqueado no Waylib (`WaylibNestedShell.qml` e `WaylibWindowFrame.qml`):**
   - **Bloqueio 1 — Recorte visual (`WaylibNestedShell.qml:264-272`):**
     ```qml
     Item {
         id: clientLayer
         anchors.fill: parent
         anchors.topMargin: 60
         anchors.bottomMargin: valoDock.height + 24
         anchors.leftMargin: 8
         anchors.rightMargin: 8
         clip: true
     ```
     Esse `clip: true` recorta todas as janelas clientes em `[60, height - (125 + 24)]` = `[60, height - 149]`.
   - **Bloqueio 2 — Clamping estrito no arraste (`WaylibWindowFrame.qml:265-268`):**
     ```qml
     frame.normalX = Math.max(0, Math.min(nextX, Math.max(0, frame.workAreaWidth - frame.width)))
     frame.normalY = Math.max(0, Math.min(nextY, Math.max(0, frame.workAreaHeight - frame.height)))
     ```
     Em `clientLayer`, `normalY = 0` equivale a `y = 60` na tela (abaixo da TopBar) e `normalY = workAreaHeight - frame.height` trava a base da janela exatamente em `height - 149` (acima da Dock). Logo, a janela **nunca** sobe atrás da TopBar nem desce atrás da Dock.
   - **Bloqueio 3 — Clamping em `syncInitialSize` (`191-192`) e `resizeCornerHandle` (`299-300`):** também restringem a janela ao retângulo interno da `workArea`.
   - **Compatibilidade de Backdrop já existente (`WaylibNestedShell.qml:214-218, 388-403, 468-508`):**
     - `desktopContentLayer` e `desktopBackdropSource` já têm `anchors.fill: parent` (cobrindo `0..renderWindow.width`, `0..renderWindow.height`).
     - `overlayCompositeBackdropSource` já combina `desktopContentLayer` (com `clientLayer`) + `internalWindowsLayer`, sendo consumido por `valoTopBar` (`z: 100`) e `valoDock` (`z: 95`) dentro de `shellChromeLayer` (`z: 90`).
     - Portanto, uma vez removido `clip: true` de `clientLayer` e liberado o movimento vertical/horizontal com margem segura, o vidro da Dock e da TopBar automaticamente refratará e desfocará qualquer janela que passar atrás deles.

### 3.2 Especificação da Correção (Limites Exatos e Segurança de Título)

1. **`WaylibNestedShell.qml` (`clientLayer`):**
   - Alterar `clip: false` em `clientLayer` (preservando `anchors.topMargin: 60`, `anchors.bottomMargin: valoDock.height + 24`, `anchors.leftMargin: 8`, `anchors.rightMargin: 8` apenas como **referência de área útil de spawn/maximização/tiling** — equivalente a `workspace` no `ShellRoot.qml`, que também usa essas margens para spawn/maximização mas não recorta o arraste).
   - Em `desktopContentLayer`, manter `clip: true` apenas nos limites físicos do monitor (`0, 0, renderWindow.width, renderWindow.height`) para que sombras ou partes externas não vazem para fora de outputs adjacentes no futuro multi-output, sem jamais recortar nas margens da TopBar ou da Dock.
   - **Janelas internas (`filesWindow` e `materialWindow`):** continuam em `internalWindowsLayer` (`anchors.fill: parent`, `z: 40`, `clip: false`, abaixo de `shellChromeLayer` com `z: 90`). Como `filesWindow` (`SystaliaFilesWindowV2.qml:126`) e `materialWindow` (`SystaliaMaterialWindow.qml:98`) usam `DragHandler`, adicionar clamping leve de segurança nos listeners de `x`/`y` ou via `DragHandler.xAxis`/`yAxis` em `WaylibNestedShell.qml` (ex.: `minY` permitindo subir até `y = -12` passando por trás da TopBar de `y: 8..53`, ou mantendo a borda inferior da barra de título acessível; `maxY = renderWindow.height - 40`, permitindo passar todo o corpo atrás da Dock; horizontal mantendo ao menos `96px` visíveis na tela).

2. **`WaylibWindowFrame.qml` (`titleDragArea`):**
   - Substituir o clamping de workarea em `titleDragArea.onPositionChanged` por **clamping em coordenadas globais do compositor (com margem mínima de resgate da barra de título)** considerando que `clientLayer` começa em `x: 8, y: 60`:
     - Em coordenadas de tela:
       - TopBar ocupa `y ∈ [8, 53]` (`height: 45`). Se permitirmos `sceneY` mínimo tal que a janela passe atrás da TopBar mantendo parte da barra de título ou permitindo recuperar embaixo/lados (ex.: `minSceneY = 0` — o topo e os primeiros 45px da janela entram atrás da TopBar, e a base da barra de título `y = 50` ainda tem borda inferior abaixo/ou `minSceneY = -12` se desejado, ou manter pelo menos `24px` da barra de título fora/nas laterais; para máxima semelhança com o desktop tradicional onde o usuário arrasta a janela para cima da tela entrando atrás da TopBar: `minSceneY = -Math.max(0, titleBarHeight - 28)` permite cobrir toda a altura da TopBar até `sceneY = 0`, mantendo a faixa inferior da barra de título ou área arrastável acessível abaixo da TopBar `53px`? Calculemos: TopBar vai de `y = 8` a `y = 53`; se `sceneY = 4`, a janela cobre todo o fundo atrás da TopBar (`8..53`) e os últimos `1px` da titlebar (`50px` total -> `y = 4..54`) terminam em `54`, pouco abaixo da TopBar; se permitirmos `minSceneY = 0`, a titlebar vai de `0..50`, ficando os primeiros `8px` visíveis acima da TopBar (entre `y=0` e `topMargin=8`!) e os demais atrás dela — **e com `y=0` a `height - 36`, nunca se perde a janela!**).
       - Na parte inferior: permitir `maxSceneY = desktopHeight - titleBarHeight` (ou `desktopHeight - 40`), permitindo que **todo o corpo da janela deslize por trás da `SystaliaDockV2`** (`height: 125`, `bottomMargin: 12`), mantendo a barra de título alcançável no rodapé da tela.
       - Horizontalmente (`normalX`): permitir que saia pelas laterais mantendo pelo menos `Math.min(120, frame.width * 0.3)` pixels visíveis dentro da tela (`[-frame.width + visibleKeepW, desktopWidth - visibleKeepW]`, subtraído de `clientLayer.x = 8`).
   - Preservar o clamping de **maximização** dentro da workarea útil (`isMaximized ? 0 : normalY` e `workAreaWidth`/`workAreaHeight`), garantindo que janelas maximizadas continuem respeitando TopBar e Dock, enquanto janelas flutuantes deslizam livremente por trás de ambas.

---

## 4. Diagnóstico e Solução 3 — Sistema de Redimensionamento (`Resize`) Multi-Direcional

### 4.1 Arquitetura sem Waylib vs. falhas no Waylib

1. **Arquitetura na versão sem Waylib:**
   - **Clientes Wayland (`WaylandWindowContainer.qml:148-266, 618-657`):**
     - `Repeater` cria **8 `MouseArea`** (`objectName: "resize_" + modelData.name`), espessura `borderMargin = 8px`, `z: 100`, `hoverEnabled: true`, `preventStealing: true`, com os cantos sobrepondo as extremidades e `cursorShape` mapeado (`SizeVerCursor`, `SizeHorCursor`, `SizeFDiagCursor`, `SizeBDiagCursor`).
     - `beginResize(edges, position)` salva `resizeStartSize`, `fixedRight = x + width`, `fixedBottom = y + height`, ativa `resizing = true` e `resizeAnchored = true`, desabilita `clientSurfaceItem.inputEventsEnabled: !windowContainer.resizing` (`linha 526`) para impedir roubo de foco/seleção pelo cliente durante o arraste.
     - `updateResize(position)` calcula `w`/`h` delta com `LeftEdge` / `RightEdge` / `TopEdge` / `BottomEdge`, respeita mínimos (`minWindowWidth`, `minWindowHeight`, `toplevel.minSize`) e máximos (`toplevel.maxSize`) e chama `dispatchClientResize(..., true)`.
     - `syncClientGeometry()` atualiza `width`/`height` no commit do buffer e reposiciona `x = fixedRight - width` (para `LeftEdge`) e `y = fixedBottom - height` (para `TopEdge`), evitando tremor visual.
   - **Janelas internas (`ShellRoot.qml:170-236, 722-737`):**
     - `ShellResizeHandles` (componente reutilizável em `ShellRoot.qml`) anexa os mesmos 8 `MouseArea` de `8px` (`objectName: "internalResize_" + modelData.name`, `z: 1000`, `hoverEnabled: true`, `preventStealing: true`, cantos `z: 1` e arestas `z: 0`) em `glassChassis` (`min 610x530`) e `materialWindow` (`min 610x525`).
     - Em `move(edges, point)`, atualiza `width`, `height`, `x = startGeometry.x + startGeometry.width - width` (para `LeftEdge`) e `y = startGeometry.y + startGeometry.height - height` (para `TopEdge`).

2. **Por que falha no Waylib:**
   - **Em `filesWindow` e `materialWindow` (`WaylibNestedShell.qml:424-465`):** `ShellResizeHandles` não foi portado para `WaylibNestedShell.qml` (e observe que em `WaylibNestedShell.qml:431-432, 456-457`, `width`/`height` de `filesWindow` e `materialWindow` estão vinculados em binding declarativo `Math.min(701, ...)`, o que seria sobrescrito ou causaria binding quebrado se não for convertido para valores iniciais + ajuste em resize/maximizacão).
   - **Em `WaylibWindowFrame.qml:272-308`:**
     1. Só existe `resizeCornerHandle` (`14x14` no canto inferior direito). Todas as outras 7 bordas/cantos estão ausentes.
     2. Falta `hoverEnabled: true` e `preventStealing: true` em `resizeCornerHandle`.
     3. Falta controle para impedir eventos na área cliente durante o resize (em `WaylibWindowFrame.qml:174-215`, `XdgToplevelSurfaceItem` e `MouseArea` dentro de `clientViewport` recebem eventos enquanto os limites são pequenos ou no limite da borda).
     4. Falta matemática de ancoragem `fixedRight`/`fixedBottom` para `LeftEdge` e `TopEdge`, falta leitura de `toplevelItem.minimumSize` / `toplevelItem.maximumSize` (`WXdgToplevelSurfaceItem` em `wxdgtoplevelsurfaceitem.h:24-25`), e o único handle ainda clampa tamanho à `workAreaWidth - normalX` / `workAreaHeight - normalY`.
     5. **`WaylibWindowFrame.qml:86-91`:** `onResizeRequested(surface, edgeFlags)` (usado por apps com SSD/CSD que pedem `xdg_toplevel::resize`, como Electron/GTK) apenas ativa e eleva a janela, descartando `edgeFlags`.

### 4.2 Especificação da Correção

1. **Refatorar `WaylibWindowFrame.qml` com 8 zonas de resize e sincronização completa:**
   - Definir `readonly property int borderMargin: 8`, `property bool resizing: false`, `property int resizeEdges: 0`, `property bool resizeAnchored: false`, `property point resizeOrigin: Qt.point(0, 0)`, `property rect resizeStartRect: Qt.rect(0, 0, 0, 0)`, `property size requestedClientSize: Qt.size(0, 0)`.
   - Adicionar `Repeater` com os 8 handles (`top`, `bottom`, `left`, `right`, `topLeft`, `bottomRight`, `topRight`, `bottomLeft`), `objectName: "resize_" + modelData.name`, `hoverEnabled: true`, `preventStealing: true`, `z: horizontal && vertical ? 102 : 101` (acima de `titleDragArea` `z: 3` e `sgeTitleBar` `z: 2` nas extremidades, permitindo redimensionar até pela borda superior da barra de título!).
   - **`mouse` / `WSurfaceItem` durante resize:** enquanto `frame.resizing` for `true`, desabilitar envio de eventos ao cliente via `toplevelItem.enabled: !frame.resizing` (ou flag `RejectEvent` em `WSurfaceItem::flags` definida em `wsurfaceitem.h:133`!) e restaurar em `finishResize()`.
   - **Cálculo geométrico para as 8 direções (`beginResize`, `updateResize`, `finishResize`):**
     - Usar `minClientW = Math.max(240, toplevelItem.minimumSize.width)`, `minClientH = Math.max(160, toplevelItem.minimumSize.height)` (observando `maximumSize` quando `> 0`).
     - Em `updateResize(scenePoint)` (em coordenadas de `frame.parent`):
       - Calcular `dx = scenePoint.x - resizeOrigin.x`, `dy = scenePoint.y - resizeOrigin.y`.
       - Para `LeftEdge`: `nextW = clampW(resizeStartRect.width - dx)`, `nextX = resizeStartRect.x + (resizeStartRect.width - nextW)`.
       - Para `RightEdge`: `nextW = clampW(resizeStartRect.width + dx)`.
       - Para `TopEdge`: `nextH = clampH(resizeStartRect.height - dy)`, `nextY = resizeStartRect.y + (resizeStartRect.height - nextH)`.
       - Para `BottomEdge`: `nextH = clampH(resizeStartRect.height + dy)`.
       - Atualizar `frame.userSized = true` e chamar `frame.surfaceAdapter.resizeSurface(frame.windowShellSurface, clientW, clientH)` (com throttle `LiquidTiling.shouldDispatchInteractiveResize(true)` e flush final no `finishResize()` com `(false)`).
       - **Sincronia visual cliente ↔ chassi (`sizeFromSurface` vs `normalWidth`/`normalHeight`):** atualizar `normalX`/`normalY` + `normalWidth`/`normalHeight` de forma coerente com `toplevelItem.implicitWidth` / `implicitHeight` (quando o buffer do cliente commitar o novo tamanho, alinhar `normalX`/`normalY` com `fixedRight - frame.width` / `fixedBottom - frame.height` enquanto `resizeAnchored` estiver ativo, e aplicar suavemente caso o cliente redimensione em passos).
     - Conectar `onResizeRequested(surface, edgeFlags)` para suportar resize interativo iniciado pelo cliente usando o mesmo motor de geometria, rastreando a posição de `cursor.position` enquanto o botão esquerdo estiver pressionado.

2. **Portar `ShellResizeHandles` para `WaylibNestedShell.qml` (ou extrair `ShellResizeHandles.qml` compartilhado):**
   - Anexar `filesResizeHandles` (`minimumWidth: 520, minimumHeight: 400`, adaptado ao viewport nested `1280x760`) e `materialResizeHandles` (`minimumWidth: 520, minimumHeight: 400`) em `filesWindow` e `materialWindow` em `WaylibNestedShell.qml`.
   - Usar tamanhos iniciais estáticos/imperativos no `Component.onCompleted` de `WaylibNestedShell.qml` (em vez de bindings contínuos de `width`/`height`), permitindo que `ShellResizeHandles` altere `x`/`y`/`width`/`height` livremente sem quebrar bindings, e atualizar `x`/`y`/`width`/`height` quando `filesWindowMaximized`/`materialWindowMaximized` alternar.

---

## 5. Diagnóstico e Solução 4 — Motor de Cursores Dinâmicos VALO em QML (`Canvas`)

### 5.1 Por que o cursor dinâmico não funciona hoje no Waylib

1. **`WaylibNestedShell.qml:724-758`:**
   - O único elemento de cursor é `Canvas { id: softwarePointerArrow; objectName: "waylibSoftwarePointer" }`, com `width: 20`, `height: 24`, `x: cursor.position.x`, `y: cursor.position.y`.
   - Desenha apenas uma seta fixa no `Component.onCompleted` e não tem deslocamento de hotspot (`x - hotspotX`, `y - hotspotY`).
2. **Ponte C++ ↔ QML incompleta para formas de cursor (`WaylibCompositorAdapter.{hpp,cpp}`):**
   - `WaylibCompositorAdapter.cpp:92-97, 135-137` configura `WCursor`, `WSeat` e `WCursorShapeManagerV1`.
   - Embora `QWlrootsCursor` atualize `m_cursor->setCursor(QCursor)` quando Qt Quick troca o cursor de um `MouseArea`/`HoverHandler` com `hoverEnabled: true`, e `WCursor` emita `requestedCursorShapeChanged()` / `requestedCursorSurfaceChanged()` quando clientes Wayland pedem cursores (`wp_cursor_shape_v1` / `wl_pointer.set_cursor`), **`WCursor` (`wcursor.h`) não expõe `QCursor::shape()`, `WGlobal::CursorShape` nem hotspot como propriedades QML reativas**.
3. **Handles sem hover em QML (`WaylibWindowFrame.qml:281`):**
   - `resizeCornerHandle` não tem `hoverEnabled: true`, e `titleDragArea` não define `cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor`.

### 5.2 Arquitetura do Sistema de Cursores Dinâmicos VALO em QML (`Canvas`, sem `.svg` nesta etapa)

> **Decisão Arquitetural Explícita:** Todo o catálogo de cursores será desenhado inicialmente **100% em QML (`Canvas` vetorial 2D com `devicePixelRatio` nítido)** usando a paleta oficial VALO (preenchimento escuro `#0b1626`, contorno luminoso `#e2f1ff` e acento ciano `#00f2fe` / `#39F5F4`), encapsulado em um componente dedicado **`ValoSoftwareCursor.qml`** (sugerido em `src/graphics/v2/qml/ValoSoftwareCursor.qml` e registrado no módulo `Systalia.Engine` via `cmake/SgeQml.cmake`). A futura migração para arquivos `.svg` (~2 meses) ocorrerá apenas trocando a camada visual interna do componente, mantendo inalterado o contrato de estados e hotspots.

#### 5.2.1 Catálogo Completo dos 11 Estados de Cursor VALO

| # | Nome do Estado (`cursorState`) | Ação / Tarefa | `Qt::CursorShape` / Protocolo Wayland (`WGlobal::CursorShape`) | Dimensões (`w × h`) | `Hotspot (x, y)` | Geometria Vetorial QML (`Canvas`) |
| :---: | :--- | :--- | :--- | :---: | :---: | :--- |
| **1** | `default` | Navegação padrão / desktop | `Qt.ArrowCursor` / `Default` | `24 × 24` | `(1, 1)` | Seta assimétrica VALO (`#0b1626`, borda `#e2f1ff` `1.4px`, chanfro ciano sutil `#39F5F4` na base da haste) |
| **2** | `pointer` | Hover em botões, Dock, TopBar pílulas, menus, links | `Qt.PointingHandCursor` / `Pointer`, `Hand1`, `Alias`, `Copy`, `ContextMenu` | `24 × 24` | `(7, 2)` | Mão estilizada apontando com dedo indicador na coordenada `(7, 2)`, contorno luminoso `#e2f1ff` |
| **3** | `grab` | Hover em barra de título arrastável, slider handle ou `TileSplitter` | `Qt.OpenHandCursor` / `Grab`, `AllScroll` | `24 × 24` | `(12, 12)` | Mão aberta estilizada com 4 dedos curvos simétricos centrados em `(12, 12)` |
| **4** | `grabbing` | Durante arraste ativo de janela ou slider | `Qt.ClosedHandCursor` / `Grabbing`, `Move`, `DndAsk` | `24 × 24` | `(12, 12)` | Mão fechada em punho compacto com brilho ciano de feedback ativo em `(12, 12)` |
| **5** | `resize-ew` | Bordas esquerda / direita e splitter vertical | `Qt.SizeHorCursor`, `Qt.SplitHCursor` / `EWResize`, `WResize`, `EResize`, `LeftSide`, `RightSide`, `ColResize` | `26 × 24` | `(13, 12)` | Barra central vertical curta + pontas de seta duplas esquerda/direita (`↔`) com contorno duplo |
| **6** | `resize-ns` | Bordas superior / inferior e splitter horizontal | `Qt.SizeVerCursor`, `Qt.SplitVCursor` / `NSResize`, `NResize`, `SResize`, `TopSide`, `BottomSide`, `RowResize` | `24 × 26` | `(12, 13)` | Barra central horizontal curta + pontas de seta duplas cima/baixo (`↕`) com contorno duplo |
| **7** | `resize-nwse` | Cantos superior-esquerdo e inferior-direito | `Qt.SizeFDiagCursor` / `NWSEResize`, `NWResize`, `SEResize`, `TopLeftCorner`, `BottomRightCorner` | `24 × 24` | `(12, 12)` | Seta dupla diagonal a `-45°` (`⤡`) centrada com pontas biseladas `#e2f1ff` / `#0b1626` |
| **8** | `resize-nesw` | Cantos superior-direito e inferior-esquerdo | `Qt.SizeBDiagCursor` / `NESWResize`, `NEResize`, `SWResize`, `TopRightCorner`, `BottomLeftCorner` | `24 × 24` | `(12, 12)` | Seta dupla diagonal a `+45°` (`⤢`) centrada com pontas biseladas `#e2f1ff` / `#0b1626` |
| **9** | `text` | Campos de busca (`SystaliaFilesWindowV2`, `SystaliaFlowMenu`), terminal (`Alacritty`) e editores | `Qt.IBeamCursor` / `Text`, `Xterm`, `VerticalText` | `20 × 24` | `(10, 12)` | I-beam clássico com serifas curvas zenitais, haste fina e contorno externo luminoso para contraste em fundo claro/escuro |
| **10** | `busy` | Lançamento de apps (`ProcessLauncher`) ou cliente em espera | `Qt.BusyCursor`, `Qt.WaitCursor` / `Watch`, `Wait`, `Progress` | `24 × 24` | `(12, 12)` | Anel duplo VALO + arco giratório ciano (`#00f2fe` → `#39F5F4`, animação ativa somente quando `cursorState === "busy"`) + micro-seta de canto opcional no modo `Progress` |
| **11** | `not-allowed` | Operação/drop inválido ou janela bloqueada | `Qt.ForbiddenCursor` / `NotAllowed`, `NoDrop` | `24 × 24` | `(12, 12)` | Círculo diagonal proibido (`⊘`) com borda `#e2f1ff`, preenchimento `#0b1626` e traço de alerta vermelho/ciano |

*(Nota de extensibilidade futura em ~2 meses: adicionar `readonly property string svgFallbackPath` ou Loader condicional por tema no `ValoSoftwareCursor.qml` para substituir o `Canvas` pelos futuros assets `.svg` mantendo exatamente as mesmas chaves `cursorState`, dimensões e tabela de `hotspot`.)*

#### 5.2.2 Ponte C++ (`WaylibCompositorAdapter`) e Resolução de Prioridade

1. **`WaylibCompositorAdapter.{hpp,cpp}`:**
   - Adicionar propriedades reativas em `WaylibCompositorAdapter` (expostas diretamente em QML via `root.compositorAdapter`):
     - `Q_PROPERTY(QString cursorShapeKey READ cursorShapeKey NOTIFY cursorVisualChanged)` — mapeia para os 11 nomes canônicos acima (`"default"` a `"not-allowed"`).
     - `Q_PROPERTY(QString compositorCursorOverride READ compositorCursorOverride WRITE setCompositorCursorOverride NOTIFY compositorCursorOverrideChanged)` — permite que a shell force `"grabbing"` durante drag ou `"resize-*"` durante resize ativo (mesmo se o ponteiro sair momentaneamente da borda de 8px durante arraste rápido!), ou `"busy"` por um curto feedback visual (`~1200 ms`) ao disparar `ProcessLauncher.launch(...)`.
     - Conectar no construtor/inicialização a:
       - `m_cursor->cursorChanged()` (atualizado pelo `QWlrootsCursor` a partir de QML `MouseArea`/`HoverHandler`/`TextField`)
       - `m_cursor->requestedCursorShapeChanged()` (vindo de `WCursorShapeManagerV1` / clientes Wayland)
       - `m_cursor->requestedCursorSurfaceChanged()` (vindo de `wl_pointer::set_cursor` de clientes Wayland)
   - **Regra de precedência (`resolveCursorShapeKey()`):**
     1. Se `!compositorCursorOverride.isEmpty()` (shell em `resize-*` / `grabbing` / `busy` ativo) → usa `compositorCursorOverride`.
     2. Senão, se `m_cursor->cursor().shape()` (vindo de itens QML como bordas de resize, `SystaliaDockV2`, `SystaliaTopBarV2`, `TextField`, botões) for diferente de `Qt::ArrowCursor` → mapeia `Qt::CursorShape` para a respectiva `cursorShapeKey`.
     3. Senão, se o foco de ponteiro estiver sobre uma `WSurface` cliente (`m_seat->pointerFocusSurface() != nullptr`), consultar `m_cursor->requestedCursorShape()` (`WGlobal::CursorShape` via função pura determinística) -> se diferente de `Default`/`Invalid`, usar a chave correspondente; opcionalmente renderizar `WSurfaceItem` dedicado quando `requestedCursorSurface().first != nullptr && requestedCursorShape() == CursorShape::ClientResource` e o item da shell estiver em `default`.
     4. Fallback final → `"default"`.
   - Expor helper estático testável em C++: `static QString cursorShapeKeyForQtShape(Qt::CursorShape)` e `static QString cursorShapeKeyForWaylibShape(WGlobal::CursorShape)` para testes unitários diretos em `tst_WaylibBackend.cpp`.

2. **Renderização em `WaylibNestedShell.qml` (`cursorOverlayLayer`):**
   - Substituir o `Canvas { id: softwarePointerArrow }` estático por `ValoSoftwareCursor` (com `objectName: "waylibSoftwarePointer"` preservado para compatibilidade de testes existentes!):
     - `cursorState: root.compositorAdapter.cursorShapeKey`
     - `x: (root.compositorAdapter.cursor ? root.compositorAdapter.cursor.position.x : 0) - hotspotX`
     - `y: (root.compositorAdapter.cursor ? root.compositorAdapter.cursor.position.y : 0) - hotspotY`
     - `visible: root.compositorAdapter.cursor && root.compositorAdapter.cursor.visible`
     - `renderWindow.devicePixelRatio` aplicado ao `Canvas` para nitidez HiDPI.
   - Conectar `ProcessLauncher.processStarted` a um `Timer` curto (~`900 ms`) que ativa `compositorCursorOverride = "busy"` enquanto nenhum override de drag/resize estiver ativo, entregando feedback visual imediato de inicialização ao clicar nos ícones da Dock/FlowMenu.
   - Configurar `hoverEnabled: true` e `cursorShape` adequados em:
     - `titleDragArea` (`WaylibWindowFrame.qml`): `Qt.OpenHandCursor` no hover, `Qt.ClosedHandCursor` quando `pressed`.
     - As 8 bordas/cantos de `WaylibWindowFrame.qml` e `ShellResizeHandles` (`filesWindow`, `materialWindow`).
     - `DragHandler`/`HoverHandler` das barras de título de `SystaliaFilesWindowV2.qml` e `SystaliaMaterialWindow.qml` (`OpenHandCursor`/`ClosedHandCursor`) e `TextField`s com cursor `IBeamCursor`.

---

## 6. Plano de Ação Passo a Passo (Etapa 03.2.5.3.8)

Todas as sub-etapas abaixo serão executadas na **Etapa 03.2.5.3.8** mantendo exclusivamente `-DSGE_ENABLE_WAYLIB_BACKEND=ON` no diretório `build/waylib` (`Release`, `-Werror`), sem mover diretórios V2, sem XWayland e sem DRM/KMS físico:

### Sub-etapa A — Ponte de Workspaces e Cursores em C++ (`WaylibSurfaceAdapter`, `WaylibCompositorAdapter`)
- **Arquivos:**
  - `src/compositor/waylib/WaylibSurfaceAdapter.hpp` & `.cpp`: adicionar `WorkspaceIndexRole` (`"workspaceIndex"`), `setSurfaceWorkspace`, `surfaceWorkspace`, filtro por workspace ativo em `topVisibleSurface`/`cycleFocus` e atribuição automática ao `WorkspaceManagerService::instance()->currentWorkspace()`.
  - `src/compositor/waylib/WaylibCompositorAdapter.hpp` & `.cpp`: adicionar `cursorShapeKey`, `compositorCursorOverride`, mappers `cursorShapeKeyForQtShape`/`cursorShapeKeyForWaylibShape`, listeners para `m_cursor->cursorChanged`, `requestedCursorShapeChanged` e `requestedCursorSurfaceChanged`, e fallback seguro de ponteiro.
- **Critério de aceitação:** Compila sem warnings (`-Werror`); símbolos de mapeamento de cursor e roles de workspace presentes e testáveis em C++.

### Sub-etapa B — Componente Vetorial `ValoSoftwareCursor.qml` (100% QML `Canvas`)
- **Arquivos:**
  - Criar `src/graphics/v2/qml/ValoSoftwareCursor.qml` com os 11 estados (`default`, `pointer`, `grab`, `grabbing`, `resize-ew`, `resize-ns`, `resize-nwse`, `resize-nesw`, `text`, `busy`, `not-allowed`), propriedades `cursorState`, `hotspotX`, `hotspotY`, animação para `busy`, repaint sob mudança de estado/DPR e paleta VALO (`#0b1626`, `#e2f1ff`, `#39F5F4`/`#00f2fe`).
  - Registrar `ValoSoftwareCursor.qml` em `cmake/SgeQml.cmake` (`QT_RESOURCE_ALIAS "src/qml/graphics/v2/ValoSoftwareCursor.qml"` + `QML_FILES`).
- **Critério de aceitação:** Componente instanciável em QML puro, com `hotspotX`/`hotspotY` coerentes e mudança de `cursorState` sem erros QML.

### Sub-etapa C — Resize 8-Direções e Movimento Livre Sob Chrome em `WaylibWindowFrame.qml`
- **Arquivos:**
  - `src/qml/waylib/WaylibWindowFrame.qml`:
    - Remover clamping de `workAreaWidth`/`workAreaHeight` no arraste de `titleDragArea`, aplicando clamping global permissivo (mantendo barra de título alcançável e permitindo deslize por trás da TopBar e da Dock).
    - Substituir `resizeCornerHandle` pelo `Repeater` de **8 direções** (`resize_top`, `resize_bottom`, `resize_left`, `resize_right`, `resize_topLeft`, `resize_bottomRight`, `resize_topRight`, `resize_bottomLeft`) com `hoverEnabled: true`, `preventStealing: true`, ancoragem `fixedRight`/`fixedBottom` para `LeftEdge`/`TopEdge`, limites `minimumSize`/`maximumSize` e propagação de `compositorCursorOverride` durante resize/drag ativo.
    - Integrar `onResizeRequested(surface, edgeFlags)` para resize iniciado por cliente e expor `workspaceIndex` / visibilidade por workspace.
    - Atualizar `titleDragArea` com `hoverEnabled: true` e `cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor`.
- **Critério de aceitação:** Redimensionamento funcional nas 8 bordas/cantos respeitando `minimumSize`, janela arrastável acima/abaixo sobre TopBar e Dock, e cursor mudando corretamente.

### Sub-etapa D — Alternância de Quadros, `ShellResizeHandles` nas Janelas Internas e Cursor em `WaylibNestedShell.qml` + `SystaliaTopBarV2.qml`
- **Arquivos:**
  - `src/shell/topbar/SystaliaTopBarV2.qml`: garantir hit-area de `26px` de altura por pílula dentro de `workspaceIndicatorTrack` com `MouseArea` confiável (sem competição com `MouseArea` de fundo) mantendo `hoverEnabled: true` e `cursorShape: Qt.PointingHandCursor`.
  - `src/qml/waylib/WaylibNestedShell.qml`:
    - Mudar `clientLayer` para `clip: false` (removendo recorte em TopBar/Dock).
    - Implementar transição horizontal animada de workspace para clientes Wayland, `filesWindow` e `materialWindow` (preservando a cadeia acíclica de backdrop + parallax no wallpaper), registrar janelas internas e clientes em `WorkspaceManager.setWindowWorkspace(...)`, sincronizar `raiseFileWindow()`/`raiseMaterialWindow()` com `WorkspaceManager.switchToWorkspace(...)`, e conectar `topBar.onWorkspaceSelected` + atalhos de teclado.
    - Remover ou ajustar `MouseArea { z: -1 }` dentro de `valoTopBar` caso interfira em cliques da TopBar (mantendo bloqueio apenas sem roubar dos controles internos).
    - Anexar `ShellResizeHandles` (8 direções) a `filesWindow` e `materialWindow` em `internalWindowsLayer`, convertendo dimensões iniciais para valores ajustáveis sem quebrar layout.
    - Substituir `softwarePointerArrow` por `GfxV2.ValoSoftwareCursor` (`objectName: "waylibSoftwarePointer"`) com offset `-hotspotX`/`-hotspotY`, vinculado a `compositorAdapter.cursorShapeKey` e estado `busy` no lançamento de apps.
- **Critério de aceitação:** Cliques nas pílulas centrais alternam os quadros e janelas visíveis; `Files`, `Control Center` e apps externos passam livremente atrás da TopBar (`z: 100`) e da Dock (`z: 95`) com vidro refrativo; todas as janelas redimensionam pelas 8 direções; o cursor VALO muda forma e hotspot por contexto.

### Sub-etapa E — Novos Casos de Teste Automatizados em `tests/compositor/tst_WaylibBackend.cpp` e Validação Final
- **Novos casos em `tests/compositor/tst_WaylibBackend.cpp`:**
  1. **Alternância de quadros via TopBar:** simular clique em pílula de `workspaceIndicatorTrack` -> verificar mudança de `WorkspaceManager.currentWorkspace`, ocultação/translação das janelas do workspace 0 (`clientFrame`, `filesWindow`, `materialWindow`), abertura de novo cliente no workspace 1, e indicadores `hasWindowsInWorkspace(0)`/`(1)` corretos; clicar de volta no workspace 0 e confirmar restauração das janelas originais.
  2. **Movimento livre sob Dock e TopBar:** arrastar/posicionar `WaylibWindowFrame`, `filesWindow` e `materialWindow` de modo que invadam `y < 53` (atrás da TopBar `z: 100`) e `y + height > renderWindow.height - 137` (atrás da Dock `z: 95`); confirmar `clientLayer.clip == false`, coordenadas aceitas sem clamping de workarea e `zOrder` menor que TopBar e Dock.
  3. **Resize multi-direcional (bordas e cantos):** testar handles de `WaylibWindowFrame` (`resize_right`, `resize_left`, `resize_top`, `resize_bottom`, `resize_topLeft`, `resize_bottomRight`) e `ShellResizeHandles` de `filesWindow`/`materialWindow`, verificando alteração correta de `x`/`y`/`width`/`height` sem violar `minimumWidth`/`minimumHeight` e envio de `resizeSurface` ao cliente Wayland.
  4. **Sistema de cursores dinâmicos VALO em QML:** validar mapeamento dos **11 estados** em C++ (`cursorShapeKeyForQtShape` / `cursorShapeKeyForWaylibShape`), verificar `hotspotX`/`hotspotY` distintos (ex.: `default` `(1, 1)` vs `resize-ew` `(13, 12)`), verificar troca de `cursorShapeKey` ao passar/pressionar sobre borda de resize (`resize-nwse`/`resize-ew`), barra de título (`grab`/`grabbing`), pílula da TopBar (`pointer`), campo de busca (`text`), `WCursorShapeManagerV1` de cliente, estado `busy` de `ProcessLauncher` e `not-allowed`.
- **Validação final:** Build incremental `build/waylib`, testes direcionados (`tst_waylib_backend`, `tst_valo_shell_v2`, `tst_shell_root_integration`, `tst_xdg_resize`), suíte completa `ctest --test-dir build/waylib --output-on-failure` (100% PASS) e smoke test nested ao vivo em `wayland-1`.
