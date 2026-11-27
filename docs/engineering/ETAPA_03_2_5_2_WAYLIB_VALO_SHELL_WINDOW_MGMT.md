# SYSTALIA — ETAPA 03.2.5.2
## Integração da Shell VALO V2 e Gerenciamento de Janelas no Backend Waylib

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Waylib 0.6.13 (`$HOME/.cache/sge-waylib-prefix`) e wlroots 0.20.2  
**Execução física DRM/KMS:** Não executada (exclusivamente nested/headless)

---

## 1. Registro da Regra Permanente `SGE_ENABLE_WAYLIB_BACKEND=ON`

A regra permanente de desenvolvimento, compilação e validação exclusiva com `-DSGE_ENABLE_WAYLIB_BACKEND=ON` (build incremental único `Release` em `build/waylib` com `-Werror`, testes direcionados durante o desenvolvimento e suíte CTest completa na validação final) foi registrada em:
- `AGENTS.md` (item 8)
- `GEMINI.md` (item 7)
- `docs/README.md` (seção "Fluxo padrão de build e validação — Etapa 03.2.5.2+")

*(Observação: `CMakePresets.json` não existe no repositório; a opção `-DSGE_ENABLE_WAYLIB_BACKEND=OFF` permanece no `CMakeLists.txt` apenas por compatibilidade histórica).*

---

## 2. Auditoria das Interfaces de `SystaliaTopBarV2` e `SystaliaDockV2`

A auditoria direta de `src/shell/topbar/SystaliaTopBarV2.qml`, `src/shell/dock/SystaliaDockV2.qml`, `src/graphics/v2/qml/ValoMaterialSurfaceV2.qml` e `src/graphics/v2/qml/ValoDockV2.qml` identificou:
- Dependência do módulo `Systalia.Engine 1.0` (`qml_register_types_Systalia_Engine()` e singletons QML de tema `ValoTheme`).
- Dependência opcional dos singletons `Systalia.System 1.0`: `SystemTelemetry`, `ProcessLauncher`, `SystemControl` e `WorkspaceManager` (usados por `SystaliaTopBarV2` para relógio/telemetria/workspaces e por `SystaliaDockV2` para indicadores de aplicativos em execução).
- `backdropSource: null` e perfis leves (`blurProfile: 0` / `performanceProfile: "performance"`) permitem operar `SystaliaTopBarV2` e `SystaliaDockV2` de forma segura sobre `WOutputRenderWindow` via fallback sintético de `ValoBackdropProviderV2`, sem acoplamento ao renderer DRM legado.
- Adição não-intrusiva em `SystaliaTopBarV2.qml`: propriedades opcionais `activeWindowTitle` e `activeWindowAppId` (padrão `""`), exibidas no rótulo `activeWindowTitleLabel` quando há janela Wayland focada.

---

## 3. Arquitetura do Modelo de Janelas, Foco e Z-Order (`src/compositor/waylib/`)

- **`WaylibSurfaceListModel` (`QAbstractListModel`):**
  Expõe reativamente para o QML todas as `WXdgToplevelSurface` ativas com as roles `surface`, `shellSurface`, `title`, `appId`, `activated`, `minimized`, `maximized`, `surfaceId` e `zOrder`.
- **`WaylibSurfaceAdapter`:**
  - Propriedades: `surfaces`, `windowModel`, `surfaceCount`, `activeSurface`, `activeTitle`, `activeAppId`, `activeAppIds`.
  - Gerenciamento de foco e empilhamento:
    - `activateSurface(surface)`: desmarca e chama `setActivate(false)` na janela anteriormente ativa, restaura a janela alvo se minimizada, incrementa `zOrder` monotonicamente, chama `surface->setActivate(true)` e sincroniza foco de teclado em `WSeat::setKeyboardFocusSurface(surface->surface())` (com guarda `m_seat && m_seat->handle()` para evitar acesso nulo antes de `WServer::start()`).
    - Remoção segura (`removeSurfaceInternal` via `toplevelSurfaceRemoved`, `beforeDestroy` e `QObject::destroyed`): desconecta sinais antes de destruir e transfere foco automaticamente para a janela visível (não minimizada) de maior `zOrder`, ou limpa o foco do `WSeat` caso não restem janelas.
  - Ações de ciclo de vida invocáveis pelo QML: `requestClose()`, `toggleMaximize()`, `toggleMinimize()`, `focusOrRestore()`, `resizeSurface()`, `closeActiveSurface()`, `cycleFocus()`, `zOrderForSurface()`, `isMinimized()`, `isMaximized()`.
- **`WaylibCompositorAdapter`:**
  - Conecta `m_seat` ao `WaylibSurfaceAdapter` via `attachSeat(m_seat.data())` e expõe `Q_PROPERTY(WSeat *seat)`.
  - Implementa `WaylibCompositorAdapter::registerQmlTypes()` (idempotente), registrando `Systalia.Engine 1.0` e os singletons `SystemTelemetry`, `ProcessLauncher`, `SystemControl` e `WorkspaceManager` em `Systalia.System 1.0`.
  - Ligado a `sge_core` em `cmake/SgeWaylib.cmake`.

---

## 4. Container SSD VALO e Integração na Shell QML

- **`src/qml/waylib/WaylibWindowFrame.qml`:**
  Container SSD VALO envolvendo `XdgToplevelSurfaceItem` dentro da *workarea*, com:
  - Barra de título SSD (`ssdTitleBar`), título dinâmico (`windowTitle` / `windowAppId`), indicador visual de estado ativo/inativo, botões funcionais de minimizar, maximizar/restaurar e fechar (`closeButton`), arraste na barra de título dentro dos limites da *workarea* e alça de redimensionamento (`resizeCornerHandle`) com chamada segura a `surfaceAdapter.resizeSurface(...)`.
  - Interceptação de cliques na barra de título sem vazamento para janelas inferiores e ativação ao clicar na área cliente com propagação (`propagateComposedEvents: true`, `mouse.accepted = false`) para o `XdgToplevelSurfaceItem`.
- **`src/qml/waylib/WaylibNestedShell.qml`:**
  Hospeda diretamente os componentes reais `SystaliaTopBarV2` (ancorado no topo, vinculado a `activeTitle`, `activeAppId`, `cycleFocus()` e `closeActiveSurface()`) e `SystaliaDockV2` (ancorado na base, vinculado a `activeAppIds`), acompanhados de uma faixa interativa de janelas abertas/minimizadas (`waylibWindowStrip`) sobre o `WOutputRenderWindow` e áreas de contenção de cliques (`MouseArea` com `z: -1`) na TopBar e Dock.

---

## 5. Testes Automatizados Multi-Janela (`tst_WaylibBackend` e `tst_WaylibTestClient`)

- `tests/compositor/tst_WaylibTestClient.cpp` estendido com `SGE_TEST_CLIENT_TITLE`, `SGE_TEST_CLIENT_APP_ID`, `SGE_TEST_CLIENT_MS` (`0` = aguarda fechamento remoto) e tratamento explícito de `QEvent::Close` encerrando com `rc=0`.
- `tests/compositor/tst_WaylibBackend.cpp`:
  - `initializationFailsGracefullyOnInvalidState`: valida falhas seguras sem crash mesmo com `WSeat` ainda sem `wlr_seat` alocado.
  - `headlessAndMultiWindowValoShellLifecycle`: valida carregamento real de `SystaliaTopBarV2` e `SystaliaDockV2` na árvore QML, isolamento de `WAYLAND_DISPLAY`, ciclo completo de cliente único e cenário simultâneo com **2 clientes (`Window Alpha` e `Window Beta`)** validando `windowModel`, foco inicial em B, alternância de foco para A (`activateSurface`, `isActivated()`, `zOrder`, `WSeat::keyboardFocusSurface`, `topBar.activeWindowTitle`), minimizar/restaurar, maximizar/restaurar, fechamento seletivo (`requestClose(A)` retornando foco para B) e shutdown limpo com cliente B ainda conectado.

---

## 6. Resultado de Build e CTest (`SGE_ENABLE_WAYLIB_BACKEND=ON`)

- **Build (`Release`, `-Werror`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`) em `build/waylib`:** `real 61.24 s` para compilação dos alvos completos do workspace; rebuild incremental de teste individual após edição < 5 s.
- **Testes direcionados (`tst_waylib|tst_valo_shell_v2`):** **2/2 PASS** (`tst_valo_shell_v2` 0.51 s + `tst_waylib_backend` 1.32 s).
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):** **61/61 PASS** (`Total Test time (real) = 37.44 sec`).

---

## 7. Evidências do Smoke Test ao Vivo Nested (KDE Plasma Wayland `wayland-1`)

- Comando compositor: `./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500` (conectado ao host `wayland-1`, output `WL-1`).
- Clientes simultâneos em `WAYLAND_DISPLAY=sge-nested-poc`:
  - `Smoke Client A` (`org.systalia.smoke.a`, 1800 ms) → **`RC1=0`**
  - `Smoke Client B` (`org.systalia.smoke.b`, 1800 ms) → **`RC2=0`**
  - Compositor `systalia_waylib_nested` → **`RC_COMP=0`**
- Eventos de ciclo de vida XDG e encerramento limpo confirmados nos logs sem erros QML nem crash.

---

## 8. Riscos Residuais e Preparação para Etapa 03.2.5.3

- **Avisos conhecidos esperados:**
  - `ValoBackdropProviderV2` emite `[SGE-BACKDROP-V2] ATENÇÃO: Recorrendo ao fallback sintético createSyntheticBackdrop (source=0x0)` pois a captura real de cena do `WOutputViewport` será integrada na **Etapa 03.2.5.3** (Material VALO Glass V2 completo).
  - Waylib emite `qt.qml.propertyCache.append: Member implicitWidth/implicitHeight of Waylib::Server::WSurfaceItem overrides a member of the base object` (originário dos headers Waylib 0.6.13) e `QDBusError` benigno de portal no cliente de teste sem `.desktop` instalado.
  - Em testes automatizados no mesmo processo, `WServer::initializeQPA()` mantém estado global QPA único (`QWlrootsIntegration`); múltiplos subtestes devem reutilizar o ciclo do `WaylibCompositorAdapter` ativo ou rodar em processos separados.
- **Prontidão para Etapa 03.2.5.3:** Base pronta — `SystaliaTopBarV2`, `SystaliaDockV2`, `WaylibSurfaceListModel`, `WSeat` e `WaylibWindowFrame` operam sobre `WOutputRenderWindow`, restando apenas conectar o `backdropSource` real da cena Waylib e o suporte XWayland.
