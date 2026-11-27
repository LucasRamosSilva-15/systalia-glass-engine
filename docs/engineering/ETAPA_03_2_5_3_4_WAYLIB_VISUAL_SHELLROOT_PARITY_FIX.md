# ETAPA 03.2.5.3.4 — Paridade Visual e Funcional Completa com `ShellRoot.qml`: Janela `Systalia Control Center`, Molduras VALO Glass V2, Wallpaper Real, Z-Order e Conteúdo de Clientes Electron/Wayland

**Data:** 2026-11-27  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Waylib 0.6.13 (`$HOME/.cache/sge-waylib-prefix`) e wlroots 0.20.2  
**Regra permanente:** Build incremental único `Release` em `build/waylib` com `-DSGE_ENABLE_WAYLIB_BACKEND=ON` e `-Werror`. Execução física DRM/KMS e XWayland fora do escopo desta etapa (XWayland reservado para Etapa 03.2.5.4).

---

## 1. Auditoria Visual e Estrutural (`ShellRoot.qml` vs. `WaylibNestedShell.qml`)

A comparação direta entre [ShellRoot.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/core/ShellRoot.qml), [WaylandWindowContainer.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/core/WaylandWindowContainer.qml), [SystaliaMaterialWindow.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/windows/SystaliaMaterialWindow.qml) e os componentes Waylib anteriores identificou sete divergências:

1. **`System Core` na Dock vs. Popover da TopBar:** Em `ShellRoot.qml`, o ícone `System Core` da `SystaliaDockV2` (`onSettingsRequested`) controla `SystaliaMaterialWindow` (janela completa *Systalia Control Center* com sidebar *General, Status, Control Center, Software, Drivers, About* e controles ópticos IOR/Dispersão/Cáusticas/Opacidade/Blur), enquanto o gatilho da `SystaliaTopBarV2` controla o menu rápido compacto `SystaliaControlCenterV2`.
2. **Molduras de janelas Wayland:** `WaylibWindowFrame.qml` usava um retângulo escuro placeholder sem `ValoMaterialSurfaceV2` nem `SystaliaWindowHeader`.
3. **Clientes Electron/Chromium (`VS Code`) transparentes abaixo da barra de menu:** Diagnóstico via `WAYLAND_DEBUG=server` e análise binária de `libwaylibserver.so` detalhado na Seção 2.
4. **Wallpaper provisório:** `WaylibNestedShell.qml` exibia dois círculos gradientes em vez de `qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-ocean-atmosphere.png`.
5. **Z-order invertido:** `filesWindow` ficava na mesma camada de chrome e sobrepunha a Dock; a hierarquia correta coloca janelas internas abaixo de `shellChromeLayer` (`z: 40` < `z: 90`).
6. **Faixa redundante `waylibWindowStrip`:** Não existe em `ShellRoot.qml` e ocupava a faixa acima da Dock.
7. **`FPS 0` e assinatura `SDE-KWIN | VALO`:** `WOutputRenderWindow` utiliza `QQuickRenderControl` e emite `renderEnd`, não `QQuickWindow::frameSwapped`; logo `SystemTelemetry.fps` permanecia em `0`. Além disso, `SystaliaTopBarV2.qml` substituía `"SDE-KWIN | VALO"` pelo título da janela ativa.

---

## 2. Causa Raiz e Correção da Transparência Interna em Clientes Electron/Wayland (`VS Code`)

### Fatos confirmados na auditoria
1. **Falta de `WSurface::enterOutput(WOutput *)` (frame callbacks bloqueados):**
   - Na inspeção de `WSurfaceItemContentPrivate::updateFrameDoneConnection` em `libwaylibserver.so`, o Waylib só chama `WSurface::notifyFrameDone()` quando o `WOutput` associado ao `renderEnd` consta em `WSurface::outputs()`.
   - Sem `enterOutput`, o `WAYLAND_DEBUG=server` com `/usr/bin/code --enable-features=UseOzonePlatform --ozone-platform=wayland` confirmou que `wl_surface#10` anexava `wl_buffer#9` (`1232x802`, formato `AB24` / `DRM_FORMAT_ABGR8888`, `wl_surface#10.frame(wl_callback#20)`), mas **`wl_callback#20.done` nunca era enviado**, congelando o compositor GPU/Viz do Chromium após o primeiro frame parcial.
2. **Semântica de `opaque_region` em buffers `AB24`:**
   - O `VS Code` envia buffers DMA-BUF no formato `AB24` (com canal alpha) acompanhados de `wl_surface#10.set_opaque_region(...)` sobre o conteúdo opaco. Quando há pixels `alpha = 0` no `AB24` renderizado sobre um chassi de vidro translúcido (`ValoMaterialSurfaceV2`), o blending QSG deixa vazar o wallpaper atrás do frame.
3. **Protocolos e resize de janela:**
   - O compositor não criava `wp_viewporter` (`wlr_viewporter_create`), e `WaylibWindowFrame.qml` forçava `anchors.fill: parent` sobre `XdgToplevelSurfaceItem` (modo `SizeFromSurface`) sem enviar `resizeSurface()` quando o cliente inicial ultrapassava a *workarea*.

### Correções aplicadas
- **Atribuição automática e reativa de output às superfícies (`src/compositor/waylib/WaylibSurfaceAdapter.cpp` e `WaylibWindowFrame.qml`):**
  - `WaylibSurfaceAdapter::attachOutputAdapter` associa todas as superfícies toplevel e popup ao `primaryOutput()` na criação e no `outputReady`, e `WaylibWindowFrame.qml` instancia `OutputLayoutItem` chamando `enterOutput`/`leaveOutput`.
  - **Verificação com `/usr/bin/code`:** Após a correção, o trace `WAYLAND_DEBUG=server` comprovou entrega contínua de `wl_callback#19.done` / `wl_callback#22.done` (~16 ms / ~60 Hz) com múltiplos `attach` + `commit` subsequentes.
- **Viewporter e backing para regiões opacas (`src/compositor/waylib/WaylibCompositorAdapter.cpp`, `WaylibSurfaceAdapter.cpp`, `WaylibWindowFrame.qml`):**
  - Adicionado `wlr_viewporter_create(m_server->handle())`.
  - Exposto `WaylibSurfaceAdapter::isSurfaceOpaque(WSurface *)` via `pixman_region32_not_empty(&surface->handle()->opaque_region)`, ativando `opaqueClientBacking` dentro de `clientViewport` apenas quando o cliente declara região opaca (preservando transparência genuína em clientes sem `opaque_region`, como terminais translúcidos).
  - Removido `anchors.fill: parent` de `XdgToplevelSurfaceItem`, mantendo `subsurfacesVisible: true` e `resizeSurface()` quando o tamanho inicial excede `workAreaWidth`/`workAreaHeight`.

---

## 3. Separação entre Janela `Systalia Control Center` e Popover da TopBar

- **Janela completa (`SystaliaMaterialWindow`, `objectName: "waylibMaterialWindow"`):**
  - Instanciada em `internalWindowsLayer` (`z: 40`), ligada ao ícone `System Core` da `SystaliaDockV2` (`onSettingsRequested`, `settingsOpen: materialWindow.visible`) e sincronizada com os controles ópticos (`refractiveIndex`, `effectiveDispersion`, `caustics`, `surfaceOpacity`, `surfaceSlant`, `cornerRadius`, `materialActive`, `blurProfile`) de `WaylibNestedShell.qml`, `valoTopBar`, `valoDock`, `filesWindow` e `WaylibWindowFrame`.
- **Menu rápido compacto (`SystaliaControlCenterV2`, `objectName: "waylibControlCenterV2"`):**
  - Mantido em `shellChromeLayer` (`z: 115`), acionado apenas por `valoTopBar.onControlCenterRequested` / `valoTopBar.onPowerRequested`, com fechamento automático ao elevar `materialWindow`/`filesWindow`, clicar fora ou pressionar `Escape`.

---

## 4. Paridade de `WaylibWindowFrame.qml`, Wallpaper, Z-Order e Telemetria FPS

- **Moldura VALO Glass V2 ([WaylibWindowFrame.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/qml/waylib/WaylibWindowFrame.qml)):**
  - Adotada a mesma arquitetura visual de `WaylandWindowContainer.qml`: `ValoMaterialSurfaceV2` (`SquircleG2`, `cornerRadius: 20`, sombra analítica, borda especular) alimentada por `wallpaperOnlyBackdropSource` (sem loop com `clientLayer`), `clientViewport` (`clip: true`, `frameMargin: 8`) e `SystaliaWindowHeader` (`iconGlyph: "▰"`, título VALO e os 3 botões esféricos `minimizeToplevelButton`, `maximizeToplevelButton`, `closeToplevelButton`).
- **Wallpaper e Z-Order ([WaylibNestedShell.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/qml/waylib/WaylibNestedShell.qml)):**
  - Restaurado `valo-ocean-atmosphere.png` em `desktopContentLayer` (`z: 10`), com `wallpaperOnlyBackdropSource` para os frames externos e `desktopBackdropSource` para `internalWindowsLayer` (`z: 40`) e `shellChromeLayer` (`z: 90`).
  - `filesWindow.z` e `materialWindow.z` alternam entre `31` e `32` dentro de `internalWindowsLayer` (`z: 40 < shellChromeLayer.z = 90` e `< valoDock.z = 95`).
  - Removido `waylibWindowStrip`.
- **Branding e FPS ([SystaliaTopBarV2.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/topbar/SystaliaTopBarV2.qml), [SystemTelemetryService.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/services/system/SystemTelemetryService.cpp)):**
  - `desktopBrandingLabel` exibe `"SDE-KWIN | VALO"` continuamente quando `showDesktopBranding` está ativo, com título discreto ao lado em `activeWindowTitleLabel`.
  - Adicionado `SystemTelemetryService::recordFrame()` conectado ao sinal `WOutputRenderWindow::renderEnd` junto a `animTimer` (`16 ms`).

---

## 5. Resultados de Compilação e Validação

- **Build incremental Release (`-DSGE_ENABLE_WAYLIB_BACKEND=ON`, `-Werror`):**
  - `cmake --build build/waylib -j "$(nproc)"`: **20.38s** (`real 20.38`, `user 77.82`, `sys 19.73`).
- **Testes direcionados (`tst_telemetry`, `tst_process_launcher`, `tst_valo_shell_v2`, `tst_waylib_backend`):**
  - **4/4 Passed** (`tst_telemetry` 1.15s, `tst_process_launcher` 0.54s, `tst_valo_shell_v2` 0.50s, `tst_waylib_backend` 4.20s; total `6.39 sec`).
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):**
  - **61/61 Passed** (`100% tests passed, 0 tests failed out of 61`, tempo total `41.69 sec`).
- **Smoke test ao vivo nested (`./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500`):**
  - Encerrado com código `0`, zero erros QML, `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760` e `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!` no output `WL-1`.
