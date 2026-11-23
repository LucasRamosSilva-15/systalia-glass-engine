# Consolidação 06.6C — Reorganização Definitiva de Diretórios e CMake

- **Estado da Etapa 06.6C:** **COMPLETE** (árvore modular e CMake reorganizados sem alterações funcionais ou visuais; builds Debug/Release com `-Werror`, 48/48 CTest, 25 `.qsb`, instalação em prefixo temporário e execução Wayland real validados).
- **Branch:** `valo-graphics-2.0`
- **Base:** `b05363e` (após Consolidação 06.6B)
- **Commits de implementação:**
  - `b04c0b3` — `refactor(structure): reorganize C++ dirs into graphics, services, and theme` (57 arquivos movidos com `git mv`)
  - `3303f6d` — `refactor(structure): reorganize QML into shell, controls, graphics, theme, and assets` (111 arquivos movidos com `git mv`)
  - `8b8c801` — `build(cmake): modularize CMake into SgeCore, SgeQml, SgeInstall, and SgeTools`

---

## 1. Mapa de Diretórios (Antes → Depois)

| Origem (06.6B) | Destino (06.6C) | Observação |
|---|---|---|
| `src/app/` | `src/app/` | Mantido (`main.cpp`; fallback local atualizado para `src/shell/main.qml`) |
| `src/compositor/` | `src/compositor/` | Mantido (`SystaliaCompositor`, `SystaliaSurfaceExtension`, `WindowStackModel`, `OutputManagerService`, `XWaylandManager`) |
| `src/gfx/` + `src/gfx/v2/` | `src/graphics/` | Unificado (`ValoStd140Matrix.hpp`, `SystaliaClientClip.hpp`, `SystaliaSurfacePropsV2.hpp`, `ValoGlassItemV2.*`, `ValoGlassMaterialV2.*`, `ValoBackdropProviderV2.*`, `ValoGpuBlurPipelineV2.*`, `ValoBlurPipelineV2.*`, `ValoGeometryEngineV2.*`, `ValoOrganicTransform.*`, `ValoOpticalContext.*`, `ValoShadowItemV2.*`) |
| `src/qml/graphics/v2/` | `src/graphics/qml/` | QMLs ópticos V2 com `QT_RESOURCE_ALIAS` preservando `src/qml/graphics/v2/...` |
| `src/system/` | `src/services/` | Serviços de sistema (`AppRegistryService`, `InputDeviceManager`, `LiquidTilingService`, `LogindSessionService`, `NotificationServer`, `ProcessLauncherService`, `SystemControlService`, `SystemTelemetryService`, `WorkspaceManagerService`) |
| `src/tokens/` | `src/theme/` | `ValoTokens.hpp`, `valo.tokens.json` |
| `src/qml/theme/` | `src/theme/qml/` | Singletons `SystaliaTheme.qml`, `ValoTheme.qml` com `QT_RESOURCE_ALIAS` preservando `src/qml/theme/...` |
| `src/qml/shell/` (controles) | `src/controls/` | `SystaliaG2Frame.qml`, `SystaliaWindowAction.qml`, `SystaliaWindowHeader.qml`, `ValoDockItem.qml` com `QT_RESOURCE_ALIAS` preservando `src/qml/shell/...` |
| `src/qml/*.qml` + `src/qml/shell/` | `src/shell/` | `main.qml`, `WorkspaceViewport.qml`, `WaylandClientView.qml`, `WaylandWindowContainer.qml` e janelas/overlays da shell com `QT_RESOURCE_ALIAS` preservando `src/qml/...` |
| `src/qml/assets/figma/` | `assets/figma/` | Recursos SVG/PNG Figma com `QT_RESOURCE_ALIAS` preservando `src/qml/assets/figma/...` |
| `src/diagnostics/` | `src/diagnostics/` | Mantido (sondas de diagnóstico) |

---

## 2. Matriz de Dependências e Responsabilidades

| Módulo / Diretório | Responsabilidade | Dependências Principais |
|---|---|---|
| `src/app` | Ponto de entrada (`main.cpp`), inicialização Qt/QML e `Systalia.Engine` | `sge_core` (`compositor`, `services`, `shell`) |
| `src/compositor` | Compositor Wayland, superfícies, modelo de janelas, `OutputManagerService`, `XWaylandManager`, `z_systalia_surface_v1` | `Qt6::WaylandCompositor`, `Wayland::Server`, `sge_core` |
| `src/graphics` | Motor óptico VALO Graphics 2.0 (SDF L2/G2, UBO std140 160B, Dual-Kawase GPU, sombras, QMLs `Valo*V2`) | `Qt6::Quick`, `Qt6::GuiPrivate`, `Vulkan::Vulkan`, `sge_shaders` |
| `src/services` | Telemetria, lançador, registro de apps, notificações D-Bus, workspaces, tiling líquido, input/logind | `Qt6::Core`, `Qt6::DBus`, `libudev`, `libinput`, `xcb` |
| `src/shell` | Shell SDE (`main.qml`, TopBar V2, Dock V2, Files V2, Control Center V2, `WaylandWindowContainer`) | `Systalia.Engine`, `controls`, `graphics/qml`, `theme/qml` |
| `src/controls` | Primitivas reutilizáveis de UI (`SystaliaG2Frame`, `SystaliaWindowAction`, `SystaliaWindowHeader`, `ValoDockItem`) | `Systalia.Engine`, `theme/qml` |
| `src/theme` | Tokens C++/JSON (`ValoTokens.hpp`, `valo.tokens.json`) e singletons QML (`SystaliaTheme`, `ValoTheme`) | `Qt6::Qml` |
| `cmake/` | `SgeShaders.cmake`, `SgeCore.cmake`, `SgeQml.cmake`, `SgeInstall.cmake`, `SgeTools.cmake` | `CMakeLists.txt` |

---

## 3. Decisões de CMake e Compatibilidade

1. **Remoção de `SGE_GRAPHICS_V2`:** A opção `option(SGE_GRAPHICS_V2 ... ON)` em `CMakeLists.txt` não possuía nenhum consumidor (`if(SGE_GRAPHICS_V2)` ou `#ifdef`) no repositório após a remoção do renderer V1 (06.6B) e foi removida.
2. **Modularização preservando targets (`ADR 0002`):** `CMakeLists.txt` foi dividido em [`cmake/SgeShaders.cmake`](../../../cmake/SgeShaders.cmake), [`cmake/SgeCore.cmake`](../../../cmake/SgeCore.cmake), [`cmake/SgeQml.cmake`](../../../cmake/SgeQml.cmake), [`cmake/SgeInstall.cmake`](../../../cmake/SgeInstall.cmake) e [`cmake/SgeTools.cmake`](../../../cmake/SgeTools.cmake), mantendo `sge_shaders`, `sge_core`, `systalia_glass_engine`, `tools/*` e `tests/*` sem bibliotecas artificiais adicionais.
3. **Preservação de URLs QRC e imports relativos:** Todos os QMLs e assets movidos mantêm `QT_RESOURCE_ALIAS` compatível com `qrc:/qt/qml/Systalia/Engine/src/qml/...`, garantindo compatibilidade integral com `Systalia.Engine 1.0`, testes, sondas e ferramentas.

---

## 4. Validação Executada

- **Builds limpos com `-Wall -Wextra -Werror` e CTest completo (`QT_QPA_PLATFORM=offscreen`):**
  - **Debug:** **48/48 testes aprovados** (`49,35 s`), diretório temporário removido.
  - **Release:** **48/48 testes aprovados** (`48,94 s`), **25 arquivos `.qsb`** gerados, instalação validada em `/tmp/sge-install-066c` (`systalia_glass_engine`, `systalia-session`, `systalia.desktop`), diretórios temporários removidos.
- **Execução real Wayland e comparação visual:**
  - Cliente Wayland real (`alacritty` em `WAYLAND_DISPLAY=wayland-systalia-0`) executado com `Wayland SDE exit: 0` e limpeza automática de evidências via `EvidenceRun`.
  - Captura da shell V2 (`valo_066c_shell_v2.png`, `499.418` bytes) inspecionada sem regressão visual em relação à etapa 06.6B (`valo_066b_shell_v2.png`).
- **Verificação de scripts e caminhos:**
  - Varredura em `tools/` e `data/` confirmou ausência de referências quebradas a caminhos locais antigos (`src/gfx/`, `src/system/`, `src/tokens/`, `src/qml/`).

---

## 5. Limitações Registradas

- **XWayland ao vivo e multi-monitor físico:** `tst_xwayland_integration` e `tst_output_manager` passaram em Debug e Release; execução ao vivo de clientes XWayland adicionais e testes em múltiplos monitores físicos com DPR heterogêneo permanecem fora da cobertura desta sessão.
