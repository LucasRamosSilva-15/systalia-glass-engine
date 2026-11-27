# ETAPA 03.2.5.3.2 — Auditoria Comparativa (`main.cpp` e `Main.qml` Upstream Waylib vs. SYSTALIA) e Melhorias Estruturais

**Data:** 2026-11-27  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Waylib 0.6.13 (`$HOME/.cache/sge-waylib-prefix`, revisão upstream `linuxdeepin/treeland` `baf2a5ecdfc87a052141e9e790920e1b2f817783`), wlroots 0.20.2 (`d7835334`)  
**Regra permanente:** Build incremental único `Release` em `build/waylib` com `-DSGE_ENABLE_WAYLIB_BACKEND=ON` e `-Werror`. Execução física DRM/KMS e XWayland fora do escopo desta etapa (XWayland reservado para Etapa 03.2.5.4).

---

## 1. Contexto e Escopo da Auditoria

Após a restauração do pipeline óptico VALO Glass V2 real sobre `WOutputRenderWindow` (Etapa 03.2.5.3) e a correção de captura de cursor/teclado, geometria da Dock e redimensionamento dinâmico do output nested (Etapa 03.2.5.3.1), realizou-se uma auditoria técnica comparando os cabeçalhos e padrões oficiais do **Waylib 0.6.13** (`$HOME/.cache/sge-waylib-prefix/include/waylibserver/`) com os componentes do SYSTALIA:

- `src/app/waylib_nested_main.cpp`
- `src/compositor/waylib/WaylibCompositorAdapter.hpp` / `.cpp`
- `src/compositor/waylib/WaylibOutputAdapter.hpp` / `.cpp`
- `src/compositor/waylib/WaylibSurfaceAdapter.hpp` / `.cpp`
- `src/qml/waylib/WaylibNestedShell.qml`
- `src/qml/waylib/WaylibWindowFrame.qml`

O objetivo foi fechar lacunas de ciclo de vida, protocolo XDG (popups e negociação de decoração), encaminhamento de solicitações interativas iniciadas pelo cliente (`move`/`resize`) e atalhos globais de compositor, preservando integralmente a estética VALO V2 (`SystaliaTopBarV2`, `SystaliaDockV2`, `desktopBackdropSource` com blur GPU real e `waylibSoftwarePointer`).

---

## 2. Comparativo `main.cpp` Upstream Waylib vs. SYSTALIA

| Aspecto | Padrão Upstream Waylib 0.6.13 | Estado Anterior no SYSTALIA | Ação na Etapa 03.2.5.3.2 |
|---|---|---|---|
| Inicialização QPA / OpenGL ES | `WServer::initializeQPA()` antes de `QGuiApplication`, `Qt::AA_UseOpenGLES` | Já alinhado em `waylib_nested_main.cpp` | Mantido |
| Tratamento de sinais POSIX (`SIGINT`/`SIGTERM`) | Encerramento limpo do loop Qt sem corromper teardown do `WServer`/`WBackend` | Ausente (`Ctrl+C` ou `SIGTERM` encerrava abruptamente sem `adapter.shutdown()`) | **Implementado:** `socketpair(AF_UNIX, SOCK_STREAM)` + `QSocketNotifier` + `sigaction` em `waylib_nested_main.cpp` chamando `QCoreApplication::quit()` |
| Teardown de QML antes do `WServer` | Destruição da árvore QML (`QQmlApplicationEngine`) antes de destruir `WServer` | Já garantido via ordem de destruição e `aboutToQuit` -> `adapter.shutdown()` | Mantido e validado |
| Negociação XDG Decoration (`zxdg_decoration_manager_v1`) | `WXdgDecorationManager` anexado ao `WServer` com modo preferencial explícito (`Server` ou `Client`) | Ausente (clientes podiam assumir CSD ou não negociar modo com SSD VALO) | **Implementado:** `WXdgDecorationManager` em `WaylibCompositorAdapter` com `setPreferredMode(WXdgDecorationManager::Server)` |
| Flags CLI de desenvolvimento | — | `--socket <nome>` e `--auto-quit-ms <ms>` | Mantidos intactos |

---

## 3. Comparativo `Main.qml` Upstream Waylib vs. SYSTALIA

| Aspecto | Padrão Upstream Waylib 0.6.13 | Estado Anterior no SYSTALIA | Ação na Etapa 03.2.5.3.2 |
|---|---|---|---|
| Janela raiz de renderização | `OutputRenderWindow` + `WOutputViewport` | Já alinhado em `WaylibNestedShell.qml` (`z: 0` `outputsContainer`) | Mantido |
| Separação de camadas VALO V2 (sem feedback loop) | — | `desktopContentLayer` (`z: 10`) -> `ShaderEffectSource` (`desktopBackdropSource`) -> `shellChromeLayer` (`z: 90`) | Mantido intacto |
| Popups XDG (`xdg_popup`) | Camada dedicada com `XdgPopupSurfaceItem` vinculado a `WXdgPopupSurface` e posicionado relativo ao pai | Ausente (`WXdgShell::popupSurfaceAdded` não era consumido; menus/tooltips não apareciam) | **Implementado:** `WaylibPopupListModel` em `WaylibSurfaceAdapter` + `waylibPopupLayer` (`z: 120`) em `WaylibNestedShell.qml` com `XdgPopupSurfaceItem` |
| `requestMove` / `requestResize` do cliente | Sinais `WToplevelSurface::requestMove` / `requestResize` conectados à lógica de janela | Não encaminhados em `WaylibSurfaceAdapter` / `WaylibWindowFrame.qml` | **Implementado:** sinais `moveRequested(surface)` e `resizeRequested(surface, edgeFlags)` em `WaylibSurfaceAdapter` + `Connections` em `WaylibWindowFrame.qml` |
| Atalhos de compositor | `Shortcut` global para alternância de janelas | Apenas clique na TopBar/Dock/Strip | **Implementado:** `Shortcut { sequence: "Alt+Tab"; context: Qt.ApplicationShortcut }` acionando `cycleFocus()` |
| Cursor software | Cursor renderizado acima de todo o shell | `waylibCursorOverlayLayer` (`z: 10000`) | Mantido acima de `waylibPopupLayer` (`z: 120`) |

---

## 4. Melhorias Implementadas

1. **Encerramento limpo por sinal POSIX (`src/app/waylib_nested_main.cpp`):**
   - Instalado handler async-signal-safe com `sigaction` para `SIGINT` e `SIGTERM` escrevendo em `socketpair`, consumido por `QSocketNotifier` na thread principal do Qt para invocar `QCoreApplication::quit()` e executar o `shutdown()` completo.
2. **Gerenciador de decorações XDG (`src/compositor/waylib/WaylibCompositorAdapter.hpp` / `.cpp`):**
   - Instanciado `WXdgDecorationManager` anexado ao `WServer` em `initialize()` com `setPreferredMode(WXdgDecorationManager::Server)` e propriedade `Q_PROPERTY(WXdgDecorationManager *xdgDecorationManager ...)`.
3. **Suporte completo a `WXdgPopupSurface` (`src/compositor/waylib/WaylibSurfaceAdapter.hpp` / `.cpp`, `src/qml/waylib/WaylibNestedShell.qml`, `src/qml/waylib/WaylibWindowFrame.qml`):**
   - Adicionado `WaylibPopupListModel` expondo `surface`, `popupSurface`, `parentSurface`, `popupX` e `popupY`, conectado a `WXdgShell::popupSurfaceAdded` / `popupSurfaceRemoved`, `WXdgPopupSurface::reposition`, `beforeDestroy` e `destroyed`.
   - Adicionada camada `waylibPopupLayer` (`z: 120`) em `WaylibNestedShell.qml` usando `Repeater` sobre `popupModel`, encapsulando `XdgPopupSurfaceItem` (`objectName: "waylibPopupItem"`) em um `Item` delegado para evitar colisão de nome na propriedade `surface`, posicionado a partir de `WaylibWindowFrame.clientOriginIn(popupLayer)` + `(popupX, popupY)`.
4. **Solicitações de movimentação/redimensionamento e `Alt+Tab`:**
   - Conectados `WToplevelSurface::requestMove` e `WToplevelSurface::requestResize` em `WaylibSurfaceAdapter` emitindo `moveRequested` e `resizeRequested`, tratados em `WaylibWindowFrame.qml` para ativar e elevar a janela correspondente.
   - Adicionado `Shortcut` `Alt+Tab` em `WaylibNestedShell.qml` chamando `cycleFocus()`.
5. **Cobertura de testes (`tests/compositor/tst_WaylibTestClient.cpp` e `tests/compositor/tst_WaylibBackend.cpp`):**
   - `tst_WaylibTestClient` ganhou suporte a `SGE_TEST_CLIENT_POPUP=1` criando uma janela filha `Qt::ToolTip` (emite `xdg_popup` não-grabbing sem exigir serial de input prévio).
   - `tst_WaylibBackend` passou a validar `xdgDecorationManager()->preferredMode() == WXdgDecorationManager::Server`, `popupAdded` / `popupModel` / `waylibPopupItem` / `popupRemoved`, além de `moveRequested` e `resizeRequested`.

---

## 5. Evidências de Compilação e Testes

- **Build incremental (`Release`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, `-Werror`):**
  - `cmake --build build/waylib -j "$(nproc)"` concluído sem erros nem warnings.
- **Testes direcionados (`ctest --test-dir build/waylib -R "^(tst_waylib_backend|tst_valo_shell_v2)$" --output-on-failure`):**
  - `tst_valo_shell_v2`: **Passed** (`0.56 sec`)
  - `tst_waylib_backend`: **Passed** (`2.00 sec`)
  - Total: **2/2 passed** (`2.56 sec`).
- **Suíte completa (`ctest --test-dir build/waylib --output-on-failure`):**
  - **61/61 Passed** (`100% tests passed, 0 tests failed out of 61`), tempo total `36.58 sec`.
- **Smoke test nested ao vivo (2 clientes em `wayland-1` com `systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500`):**
  - Cliente 1 (`Window Alpha`, `SGE_TEST_CLIENT_POPUP=1`) e Cliente 2 (`Window Beta`) conectaram, mapearam toplevels e encerraram normalmente: `RC1=0 RC2=0 RC_COMP=0`.
  - Durante a validação headless em `tst_waylib_backend`, o pipeline óptico registrou `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760` e `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!`.

---

## 6. Limitações Conhecidas e Próximos Passos

- **Popups grabbing (`Qt::Popup`):** Clientes Qt que abrem `Qt::Popup` sem um serial de entrada prévio no mesmo cliente recebem `Failed to create grabbing popup` do QPA Wayland; popups não-grabbing (`Qt::ToolTip`) e popups após clique/tecla funcionam normalmente.
- **XWayland:** Mantido fora desta etapa conforme planejado; será implementado na **Etapa 03.2.5.4**.
- **Reorganização de diretórios V2:** Nenhuma movimentação em massa realizada nesta etapa.
