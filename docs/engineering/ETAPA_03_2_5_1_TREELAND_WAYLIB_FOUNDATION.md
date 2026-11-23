# SYSTALIA — Etapa 03.2.5.1: Fundação Treeland/Waylib — Migração Incremental do Compositor

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch de origem:** `main` (`ca7e8f0` — merge de `valo-graphics-2.0` confirmado)  
**Branch da etapa:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44 (kernel `7.3.4`), Qt 6.11.2, C++23, Waylib `0.6.13` + wlroots `0.20.2`  
**Execução física DRM/KMS:** PROIBIDA e NÃO EXECUTADA nesta etapa

---

## 1. Estado Inicial

- `origin/main` (`ca7e8f0`) já continha o merge da fase `VALO Graphics 2.0` (`bb09269`..`75fa5ed`..`288b7e6`).
- O branch local `main` (`232b4d8`) foi atualizado por fast-forward para `ca7e8f0`, e a branch `feat/treeland-waylib-backend` (que já possuía `f5a1657`) recebeu o merge limpo de `main` (`bddca4e`) sem operações destrutivas (`git reset --hard` não utilizado) e preservando `valo-graphics-2.0`.

---

## 2. Identidade da Branch

- **Branch base:** `main` (`ca7e8f0`)
- **Branch de trabalho:** `feat/treeland-waylib-backend`
- **Preservação:** branch experimental `valo-graphics-2.0` e backend DRM/Vulkan legado mantidos intactos.

---

## 3. Revisões Treeland / Waylib / wlroots Estudadas

- **`linuxdeepin/treeland` (monorepo contendo `waylib` `0.6.13` e `wlroots` `0.20.2` vendored):** `baf2a5ecdfc87a052141e9e790920e1b2f817783`
- **`wlroots` (pin em `wlroots/UPSTREAM`):** tag `0.20.2`, commit `d783533489e1f75d6886c2ab5c5960090ef268f8`
- **`wlr-protocols`:** `1106a4a1c57562ddd438c547da4c0d6dc150d624`
- **`treeland-protocols`:** `5d28a8ac78a6f78f70ed46146159c8e8a06780cf`

Observação importante: no estado atual do `linuxdeepin/treeland`, o `waylib` (`0.6.13`) é integrado ao monorepo (`waylib/`) junto com o `wlroots` `0.20.2` (`3rdparty/wlroots` + `wlroots/CMakeLists.txt`).

---

## 4. Licenças Relevantes

- **`treeland` e `waylib` (`0.6.13`):** multi-licenciamento `Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only` (além de `MIT`/`HPND` em arquivos específicos e `REUSE.toml`).
- **`wlroots` (`0.20.2`):** `MIT`.
- **`treeland-protocols` / `wlr-protocols`:** `MIT` / `HPND` / `LGPL` / `GPL` conforme cabeçalhos dos XMLs.

---

## 5. Dependências (Fedora Linux 44 + Qt 6.11.2)

### Disponíveis nativamente no host
- Qt `6.11.2` (`Core`, `Gui`, `GuiPrivate`, `Quick`, `QuickPrivate`, `QuickControls2`, `Qml`)
- `wayland-server` `1.26.0`, `wayland-client` `1.26.0`, `wayland-protocols` `1.50`, `wayland-scanner`
- `pixman-1` `0.46.2`, `xkbcommon` `1.13.1`, `libdrm` `2.4.134`, `gbm` `26.2.7`, `egl` `1.5`, `glesv2` `3.2`, `libinput` `1.31.3`, `lcms2` `2.16`

### Lacunas e soluções isoladas adotadas (sem instalação global)
- `waylib` e `qwlroots` não possuem pacote RPM no Fedora 44; `treeland-protocols-devel` no repo é `0.5.4` (Treeland pede `0.8.0`); `wlr-protocols`, `libseat-devel`, `libdisplay-info-devel`, `hwdata`, `xcb-renderutil` e `glslangValidator` não estavam instalados no ambiente.
- **Solução mínima isolada:** compilação local de `wlr-protocols`, `treeland-protocols`, `wlroots 0.20.2` (somente backend nested Wayland/headless + GLES2/EGL + GBM + udmabuf; `WLROOTS_BACKEND_DRM=OFF`, `WLROOTS_BACKEND_LIBINPUT=OFF`, `WLROOTS_SESSION=OFF`, `WLROOTS_BACKEND_X11=OFF`, `WLROOTS_RENDERER_VULKAN=OFF`, `DISABLE_XWAYLAND=ON`) e `waylibserver 0.6.13` em `$HOME/.cache/sge-waylib-prefix`, com 3 ajustes pontuais documentados:
  1. `pixman-1>=0.46.4` → `pixman-1>=0.46.0` em `wlroots/CMakeLists.txt` (compatível com Fedora 44 `0.46.2`).
  2. `#include "util/global.h"` movido para fora de `#if WLR_HAS_DRM_BACKEND` em `wlr_linux_dmabuf_v1.c`.
  3. `#if WLR_HAS_LIBINPUT_BACKEND` em `winputdevice.cpp` e `#if WLR_HAS_DRM_BACKEND` / `#if WLR_HAS_X11_BACKEND` em `wbackend.cpp` quando DRM/libinput/X11 estão desativados.

---

## 6. Arquitetura Selecionada

**Selecionada: Alternativa A — Reutilizar Waylib diretamente (`waylibserver 0.6.13` + `wlroots 0.20.2`), usando Treeland apenas como referência arquitetural.**

Justificativa:
- Evita acoplamento com DTK, DDM/greeter, DConfig e políticas DDE do Treeland.
- Mantém 100% da identidade visual VALO e políticas de janela no SYSTALIA.
- Reduz dependências externas e permite integração incremental via `SGE_ENABLE_WAYLIB_BACKEND` (desativado por padrão).

---

## 7. Auditoria de Reutilização dos Componentes SYSTALIA

| Componente | Classificação | Observação |
|---|---|---|
| `ValoTheme.qml` / `SystaliaTheme.qml` / `valo.tokens.json` | 1. Reutilizável sem alterações | Tokens independentes do compositor |
| `SystaliaTopBar.qml` / `SystaliaTopBarV2.qml` | 2. Reutilizável com adaptador | Conectar ao modelo/sinais de janelas e workspaces |
| `SystaliaDock.qml` / `SystaliaDockV2.qml` | 2. Reutilizável com adaptador | Adaptar launcher/running apps ao `WaylibSurfaceAdapter` |
| `SystaliaControlCenter.qml` / `SystaliaControlCenterV2.qml` | 2. Reutilizável com adaptador | Independente do protocolo Wayland |
| `SnapPreviewOverlay.qml` | 2. Reutilizável com adaptador | Recebe geometria de output/janela via adaptadores |
| `WaylandWindowContainer.qml` / `WaylandClientView.qml` | 4. Necessita redesenho técnico | Hoje acoplados a `QtWayland.Compositor` (`ShellSurfaceItem`); migrar para `Waylib.Server` (`XdgToplevelSurfaceItem`) |
| `XWaylandWindowContainer` / `XWaylandManager` | 2/4. Adaptador futuro | No Waylib será integrado via `WXWayland` em etapa posterior |
| Shaders VALO V2 (`shaders/v2/*.frag`, `*.vert`) e `ValoMaterialSurfaceV2.qml` | 2/3. Adaptador / fase posterior | Avaliar compatibilidade sobre QRhi GLES2/Vulkan do `WOutputRenderWindow` após validar infraestrutura base |
| `VulkanQuickRenderer`, `IndependentFrameLoop`, `GbmBufferManager`, `AtomicKmsPresenter`, `DrmEventDispatcher`, `GpuSubmissionTracker` | 5. Legado experimental | Preservados intactos no repositório para regressão e referência |

---

## 8. Mudanças CMake

- `CMakeLists.txt`: adicionada `option(SGE_ENABLE_WAYLIB_BACKEND "Enable experimental Waylib/wlroots nested compositor backend" OFF)`.
- `cmake/SgeWaylib.cmake`: criada configuração isolada que busca `waylibserver` e `waylib-wlroots` em `SGE_WAYLIB_PREFIX` / `$ENV{HOME}/.cache/sge-waylib-prefix` / `CMAKE_PREFIX_PATH` com erro claro se ausente, criando os targets `sge_waylib_backend` e `systalia_waylib_nested`.
- `tests/CMakeLists.txt`: adicionados `tst_waylib_test_client` e `tst_waylib_backend` apenas quando `SGE_ENABLE_WAYLIB_BACKEND=ON`.
- Build tradicional (`SGE_ENABLE_WAYLIB_BACKEND=OFF`): inalterado.

---

## 9. Implementação do Protótipo

- `src/compositor/waylib/WaylibCompositorAdapter.hpp` / `.cpp`: gerencia `WServer`, `WBackend` (forçando `WLR_BACKENDS=wayland` em modo nested ou `headless` em testes, sem DRM/libinput), `WSeat`, `WCursor`, `WXdgShell(5)`, `WSocket` isolado (sem sobrescrever `WAYLAND_DISPLAY` global), `wlr_renderer`, `wlr_allocator` e shutdown idempotente.
- `src/compositor/waylib/WaylibOutputAdapter.hpp` / `.cpp`: acopla `WBackend::outputAdded`/`outputRemoved`, `WQuickOutputLayout`, `WQmlCreator` e habilitação do output no `outputViewportInitialized`.
- `src/compositor/waylib/WaylibSurfaceAdapter.hpp` / `.cpp`: gerencia `WXdgShell::toplevelSurfaceAdded`/`toplevelSurfaceRemoved` expondo lista reativa `surfaces` para o QML.
- `src/qml/waylib/WaylibNestedShell.qml`: shell mínimo com TopBar e Dock em paleta VALO, `OutputRenderWindow`, `OutputItem`, `OutputViewport` e `XdgToplevelSurfaceItem` (`surface: modelData.surface`, `shellSurface: modelData`).
- `src/app/waylib_nested_main.cpp`: executável `systalia_waylib_nested` com `--socket`, `--headless` e `--auto-quit-ms`.

---

## 10. Evidências de Execução Nested e Headless

1. **Execução nested real dentro do KDE Plasma (`WAYLAND_DISPLAY=wayland-1`):**
   - Comando: `systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 2500` + cliente `tst_waylib_test_client` conectado com `WAYLAND_DISPLAY=sge-nested-poc QT_QPA_PLATFORM=wayland`.
   - Evidência observada:
     - `[SGE-WAYLIB] Compositor nested/headless iniciado no socket: "sge-nested-poc"`
     - `waylib.output.buffer: Selected primary buffer format: "XR24" "(0x34325258)" for output: "WL-1"`
     - `[SGE-WAYLIB] XDG toplevel added: "" appId= ""`
     - `[SGE-WAYLIB] XDG toplevel removed: ""`
     - `NESTED_CLIENT_RC=0 NESTED_SERVER_RC=0`
   - Nota: não foi capturado screenshot interativo nesta execução automatizada (`--auto-quit-ms 2500`).
2. **Execução headless automatizada (`tst_waylib_backend`):**
   - Criou output `HEADLESS-1` (`XR24`), isolou o socket `systalia-waylib-test-<pid>` sem alterar `WAYLAND_DISPLAY`, instanciou `WXdgToplevelSurfaceItem` na árvore Qt Quick com `surface != nullptr` e `shellSurface != nullptr`, e encerrou limpo sem crash.

---

## 11. Resultados dos Testes

- **Com `SGE_ENABLE_WAYLIB_BACKEND=ON` (Release `-Werror`):**
  - `ctest --output-on-failure`: **`61/61` aprovados** (`100% tests passed, 0 tests failed out of 61`, `54.06 sec`: `39 unit`, `21 integration`, `1 e2e`), incluindo `tst_waylib_backend` (`2.00 sec`).
- **Com `SGE_ENABLE_WAYLIB_BACKEND=OFF` (padrão, Release `-Werror`):**
  - `ctest --output-on-failure`: **`60/60` aprovados** (`100% tests passed, 0 tests failed out of 60`, `51.84 sec`: `39 unit`, `20 integration`, `1 e2e`).
- **Execução física DRM/KMS:** não realizada.

---

## 12. Problemas Encontrados

1. `wlroots/CMakeLists.txt` do Treeland exigia `pixman-1>=0.46.4` enquanto o Fedora 44 fornece `0.46.2`.
2. `wlr_linux_dmabuf_v1.c` e `waylib` (`winputdevice.cpp`, `wbackend.cpp`) continham trechos sem guarda `#if WLR_HAS_DRM_BACKEND` / `#if WLR_HAS_LIBINPUT_BACKEND` / `#if WLR_HAS_X11_BACKEND`.
3. `Waylib::Server::WXdgToplevelSurfaceItem` exige em QML tanto `surface: modelData.surface` quanto `shellSurface: modelData` para evitar ponteiro nulo em `getContentGeometry()`.
4. No encerramento do `systalia_waylib_nested`, `rootObj` (`WOutputRenderWindow`) deve ser destruído antes de `adapter.shutdown()` para encerrar o contexto EGL/QRhi antes de `wlr_renderer_destroy()`.

---

## 13. Riscos Restantes

- O build local do Waylib em `$HOME/.cache/sge-waylib-prefix` desativou DRM/libinput/session/Vulkan/XWayland para segurança desta PoC nested; habilitar DRM/input físicos futuramente exigirá `libseat-devel`, `libdisplay-info-devel` e `hwdata`.
- Os componentes VALO V2 (`ValoBackdropProviderV2` / blur multicamadas) ainda não foram portados para o pipeline GLES2/QRhi do `WOutputRenderWindow`.

---

## 14. Próxima Etapa Recomendada

Integrar incrementalmente os componentes visuais VALO (TopBar, Dock e container de janela adaptando `WaylandWindowContainer` para `XdgToplevelSurfaceItem`) no protótipo nested Waylib, mantendo `SGE_ENABLE_WAYLIB_BACKEND=OFF` por padrão até paridade funcional de janelas e input.
