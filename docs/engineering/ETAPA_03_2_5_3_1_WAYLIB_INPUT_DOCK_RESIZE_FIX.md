# SYSTALIA — ETAPA 03.2.5.3.1
## Correção de Captura de Mouse/Teclado (`WSeat`/`WCursor`), Geometria da Dock e Redimensionamento Dinâmico do Output Nested

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Waylib 0.6.13 (`$HOME/.cache/sge-waylib-prefix`) e wlroots 0.20.2  
**Execução física DRM/KMS:** Não executada (exclusivamente nested/headless)

---

## 1. Captura de Ponteiro, Cursor e Teclado (`WBackend` → `WSeat` → `WCursor` → `WOutputRenderWindow`)

### Causa raiz
- Em `WaylibCompositorAdapter::initialize()`, `m_cursor->setEventWindow(window)` e `m_seat->setKeyboardFocusWindow(window)` não estavam configurados, impedindo o roteamento de eventos de ponteiro, hover e teclado do `WL-1` para o `WOutputRenderWindow`.
- Dispositivos de entrada já existentes antes de `connect(WBackend::inputAdded, ...)` ou após `wlr_backend_start()` não eram varridos via `m_backend->inputDeviceList()`.
- `OutputItem` em `WaylibNestedShell.qml` não definia `cursorDelegate: Cursor { ... }`, não exibindo cursor ativo sobre o output nested.

### Correção
- **`src/compositor/waylib/WaylibCompositorAdapter.{hpp,cpp}`:**
  - Exposta `Q_PROPERTY(WCursor *cursor READ cursor CONSTANT)`.
  - No construtor: definidos `m_cursor->setCursor(QCursor(Qt::ArrowCursor))` e `m_cursor->setVisible(true)`, vinculados a `m_seat->setCursor(m_cursor)` e `m_cursor->setLayout(m_outputAdapter->outputLayout())`.
  - No `initialize()`: chamados `m_cursor->setEventWindow(window)`, `m_seat->setKeyboardFocusWindow(window)` e anexados dispositivos de `m_backend->inputDeviceList()` (antes e após `wlr_backend_start`, com guarda `!m_seat->deviceList().contains(device)` em `onInputAdded`).
  - No `shutdown()`: desconexão ordenada de `m_backend`, `onInputRemoved`, `clearKeyboardFocusWindow()`, `setEventWindow(nullptr)` antes de `m_surfaceAdapter->reset()`, `m_outputAdapter->reset()` e `m_server->stop()`.
- **`src/qml/waylib/WaylibNestedShell.qml`:**
  - Adicionado `cursorDelegate: Cursor { property var outputCursor; cursor: root.compositorAdapter.cursor; output: outputItem.output }` em `OutputItem` (eliminando o aviso de `initialProperties` para `outputCursor`).

---

## 2. Geometria e Visibilidade Integral da `SystaliaDockV2`

### Causa raiz
- O `WOutputRenderWindow` iniciava com `width: 1280`, `height: 760`, enquanto `WaylibOutputAdapter::onViewportInitialized` aplicava `wlr_output_state_set_custom_mode(..., 1280, 720, 0)` quando não havia `preferred_mode` (caso do output nested `WL-1`). Os 40 px excedentes faziam com que a base da `SystaliaDockV2` fosse recortada pela borda inferior de `WL-1` (`720 px`).

### Correção
- Em `WaylibOutputAdapter::onViewportInitialized`, quando não há `preferred_mode`, o modo customizado inicial usa o tamanho do `WOutputRenderWindow` (`1280x760`), mantendo janela Qt e `wlr_output` em paridade exata `1:1`.
- Em `WaylibNestedShell.qml`: `valoDock` (`SystaliaDockV2`, altura `125 px`) ancorada com `anchors.bottomMargin: 12` e `clientLayer` com `anchors.bottomMargin: valoDock.height + 20`, deixando 100% da pílula e sua borda luminosa inferior visíveis dentro do viewport.

---

## 3. Redimensionamento Dinâmico e Maximização de `WL-1` (`WOutput::requestState`)

### Implementação
- **`src/compositor/waylib/WaylibOutputAdapter.{hpp,cpp}`:**
  - Registrado listener `wl_listener` por `WOutput` para `wlr_output::events.request_state` (`handleWlrRequestState`), com remoção limpa em `onOutputRemoved` e `reset()`.
  - Quando o host Wayland (KWin) maximiza ou redimensiona `wlroots - WL-1`, `request_state` com `WLR_OUTPUT_STATE_MODE` / `WLR_OUTPUT_STATE_ENABLED` / `WLR_OUTPUT_STATE_SCALE` é imediatamente aplicado via `wlr_output_commit_state`.
  - Adicionado `Q_PROPERTY(QSize primaryOutputSize)` e `Q_INVOKABLE bool requestOutputResize(int width, int height)` + `syncPrimaryOutputSize()`, sincronizando automaticamente `WOutputRenderWindow::setWidth/setHeight` quando `effectiveSizeChanged` / `transformedSizeChanged` / `scaleChanged` disparam.
- **`src/qml/waylib/WaylibNestedShell.qml` & `WaylibWindowFrame.qml`:**
  - `OutputItem` acompanha `Math.max(renderWindow.{width,height}, waylandOutput.size.{width,height} / scale)`, mantendo `OutputViewport`, `desktopContentLayer`, `desktopBackdropSource.textureSize`, `shellChromeLayer`, `valoTopBar` e `valoDock` sincronizados com a resolução atual do output.
  - `WaylibWindowFrame.qml` recalcula e reenvia `surfaceAdapter.resizeSurface(...)` em `onWorkAreaWidthChanged` e `onWorkAreaHeightChanged` quando maximizada.

---

## 4. Novos Casos de Teste em `tst_WaylibBackend.cpp`

Em `headlessAndMultiWindowValoShellLifecycle()` foram validados:
1. **Input & Cursor:** `seat()->cursor()`, `cursor()->layout()`, `cursor()->isVisible()`, `cursor()->eventWindow() == renderWindow`, `seat()->keyboardFocusWindow() == renderWindow`, atualização de posição via `cursor()->setPosition(QPointF(240, 180))` e posicionamento sobre o `closeButton` visível na SSD.
2. **Geometria da Dock:** verificação em coordenadas de cena (`dockV2->mapToScene`) de que `dockTop >= 0` e `dockBottom <= renderWindow->height() - 8.0` tanto em `1280x760` quanto após resize para `1920x1040`.
3. **Redimensionamento dinâmico do Output:** `requestOutputResize(1920, 1040)` atualizando `primaryOutputSize`, `renderWindow`, `waylibOutputViewport`, `desktopContentLayer`, `waylibDesktopBackdropSource.textureSize` (`1920x1040`), `topBar.width` (`1904`) e restauração para `1280x760`.

---

## 5. Resultados de Build, CTest e Smoke Test Nested

- **Build incremental Release (`build/waylib`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, `-Werror`):** **`6.99 s`** (`real`).
- **Testes direcionados (`tst_waylib|tst_valo_shell_v2`):** **2/2 PASS** (`1.81 s` reais).
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):** **61/61 PASS** (`36.55 s` reais).
- **Smoke test ao vivo nested (`WAYLAND_DISPLAY=wayland-1`, output `WL-1`):** `./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500` + 2 instâncias de `tst_waylib_test_client` (`Smoke Client A` e `Smoke Client B`) encerraram com **`RC1=0`**, **`RC2=0`**, **`RC_COMP=0`**, textura real `1280x760` e blur GPU ativos sem fallback sintético.
