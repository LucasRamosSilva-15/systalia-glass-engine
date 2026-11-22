# SYSTALIA — Etapa 03.2.4.5.2: Extração de `ShellRoot.qml` e Integração da Shell Completa com `QQuickRenderControl`

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base:** `e77411e` (com alterações locais das Etapas 03.2.1–03.2.4.5.2 não commitadas)
- **Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, `QtWaylandCompositor`, GBM, `libdrm`
- **GPU de referência:** AMD Radeon Cezanne (`/dev/dri/card1`)
- **Estado:** **`ShellRoot.qml` extraído, `main.qml` preservado como adaptador tradicional, `ShellRoot.qml` renderizado via `QQuickRenderControl` em GPU Vulkan RADV real e 54/54 CTest aprovados** *(lacuna explícita restante: ligação direta de cada `gbm_bo` do `GbmBufferManager` como `VkImage`/`QQuickRenderTarget` antes do Atomic KMS físico — escopo da Etapa 03.2.4.6).*

---

## 1. Auditoria de `main.qml` e Componentes Extraídos

Auditoria integral de [`src/shell/core/main.qml`](../../src/shell/core/main.qml) (1004 linhas) identificou:
- Raiz `ApplicationWindow` (`id: rootWindow`, 1662×975), com `SystaliaCompositor`, `WaylandOutput { window: rootWindow }`, `Wallpaper`, `WorkspaceViewport`, `SystaliaTopBarV2`, `SystaliaDockV2`, `SystaliaFilesWindowV2`, `SystaliaMaterialWindow`, `SystaliaControlCenterV2`, `SystaliaNotificationHost`, `SystaliaTaskSwitcher`, `SystaliaFlowMenu`, `SnapPreviewOverlay`, `TileSplitter`, `ValoInternalWindowBackdropV2`, `ValoSingleWindowBackdropV2`, `ValoLimitedBackdropV2` e `Shortcut`.
- Todas as referências a `rootWindow.devicePixelRatio`, `rootWindow.contentItem`, `SystemTelemetry.attachWindow(rootWindow)`, `waylandServer.attachTaskSwitcherWindow(rootWindow)` e `WaylandOutput.window` puderam ser parametrizadas via `property var hostWindow: null` em um `Item` raiz (`ShellRoot.qml`), sem duplicar UI ou serviços.

---

## 2. Estrutura Final de `ShellRoot.qml` e Adaptação de `main.qml`

- [`src/shell/core/ShellRoot.qml`](../../src/shell/core/ShellRoot.qml) (`Item`, `id: shellRoot`, `hostWindow`): reúne toda a árvore visual reutilizável (Dock, TopBar, VALO V2, janelas internas, `SystaliaCompositor`, `WaylandOutput { id: primaryWaylandOutput; window: shellRoot.hostWindow }`, `Shortcut`, overlays) e expõe `property alias` públicos.
- [`src/shell/core/main.qml`](../../src/shell/core/main.qml): adaptador mínimo `ApplicationWindow` hospedando `ShellRoot { id: shellRoot; anchors.fill: parent; hostWindow: rootWindow }` e encaminhando os aliases e métodos públicos (`raiseFileWindow`, `raiseMaterialWindow`, `unsnapShellWindow`, `finishShellWindowDrag`, etc.) exigidos pela suíte `tst_ShellStateRegression`.
- [`cmake/SgeQml.cmake`](../../cmake/SgeQml.cmake): registra `ShellRoot.qml` com alias QRC `src/qml/ShellRoot.qml`.

---

## 3. Integração com `QQuickRenderControl`, `QtWaylandCompositor` e `IndependentFrameLoop`

- [`src/platform/drm/VulkanQuickRenderer.hpp`](../../src/platform/drm/VulkanQuickRenderer.hpp) / [`.cpp`](../../src/platform/drm/VulkanQuickRenderer.cpp): implementa `IControlledRenderSource`, adiciona `loadQmlUrl(qmlUrl, externalEngine)` usando `QQmlComponent::createWithInitialProperties({{"hostWindow", m_window.get()}})` para inicializar `ShellRoot.qml` sobre a única `QQuickWindow(m_renderControl.get())`, sem instanciar `main.qml`.
- [`src/app/main.cpp`](../../src/app/main.cpp): sob `SGE_USE_DRM_BACKEND=1` + `SessionBareMetalKms`, utiliza um único `QQmlEngine` (não `QQmlApplicationEngine`), carrega `ShellRoot.qml`, conecta `QWaylandQuickOutput` ao `QWaylandOutputFrameSink` → `WaylandFrameCoordinator` (`setAutomaticFrameCallback(false)`) e conecta `QQuickRenderControl::renderRequested` / `sceneChanged` ao `IndependentFrameLoop`.
- **Modo tradicional (`nested` / `headless`):** permanece 100% inalterado usando `QQmlApplicationEngine` + `main.qml`.

---

## 4. Evidências de Renderização e Resultados de Testes (após a última edição)

- **Build Release (`-Werror`):** compilada sem erros nem avisos.
- **`tst_shell_root_integration` (QPA `sge_vulkan_offscreen`, GPU Vulkan RADV real):**
  - `traditionalMainQmlLoadsSingleWindowAndCompositor`: valida 1 `QQuickWindow`, 1 `SystaliaCompositor`, 1 `QWaylandQuickOutput` e itens `ValoGlassItemV2` em `main.qml`.
  - `experimentalShellRootRendersThroughControlledVulkanLoop`: carrega `ShellRoot.qml` no `VulkanQuickRenderer` (`1280×720`), confirma 0 `QQuickWindow` filhas, 1 `SystaliaCompositor`, 1 `QWaylandQuickOutput` vinculado à janela controlada, executa frame via `IndependentFrameLoop`, realiza `readbackImage()` comprovando pixels não nulos da shell VALO V2, e valida `resize(1662×975)` + `suspend()` + `resume()` + teardown com `0` buffers retidos.
  - `handlesQmlLoadErrorsCleanly`: valida falha limpa em URL inexistente e raiz não-`QQuickItem`.
- **`tst_shell_state_regression`:** **29/29 casos aprovados** (`5.84 s`).
- **Suíte completa CTest:** **54/54 aprovados** (`49.96 s`, `0` falhas — evolução de `53/53` para `54/54`).
- **Smoke tests (`systalia-session`, cwd `/tmp`, saída por `SIGTERM`):** headless (`offscreen`) e nested Wayland (`wayland`) com `sigterm_rc=0`.

---

## 5. Regressões Encontradas e Corrigidas Durante a Etapa

1. `tst_ShellStateRegression` avalia expressões QML diretamente no escopo de `main.qml`: corrigido adicionando `import Systalia.System 1.0`, `import Systalia.Compositor 1.0`, `import QtWayland.Compositor` e aliases/funções encaminhadas em `main.qml`.
2. Conflito de `XDG_RUNTIME_DIR` compartilhado (`wayland-systalia-0.lock`) em `tst_ShellRootIntegration`: corrigido isolando `QTemporaryDir` por processo.

---

## 6. Pendências para Atomic KMS Físico e Critérios da Etapa 03.2.4.6

- **Lacuna explícita:** `VulkanQuickRenderer` renderiza em uma `VkImage` offscreen única (com exportação DMA-BUF validada na PoC), enquanto `GbmBufferManager` aloca `gbm_bo`s separados. Na **Etapa 03.2.4.6**, falta importar/vincular diretamente os DMA-BUFs de cada `gbm_bo` do pool como `VkImage`/`QQuickRenderTarget` alternados por frame (com `vkGetMemoryFdKHR` / `VK_EXT_external_memory_dma_buf` + `IN_FENCE_FD` / `OUT_FENCE_PTR`), antes de autorizar o primeiro reteste físico em TTY3 sobre `/dev/dri/card1`.
- **Procedimento de recuperação:** `Ctrl+Alt+F2` para retornar à sessão Plasma principal, ou `pkill -TERM -x systalia_glass_engine` em outra TTY (`Ctrl+Alt+F4`).
