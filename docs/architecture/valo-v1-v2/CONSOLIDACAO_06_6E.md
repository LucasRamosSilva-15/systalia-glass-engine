# Consolidação 06.6E — Finalização da Organização Arquitetural do Repositório

- **Estado da Etapa 06.6E:** **COMPLETE** (organização final de `tests/`, `tools/`, `docs/` e `cmake/Dependencies.cmake` validada com builds limpos Debug/Release `-Werror`, 48/48 CTest, 25 `.qsb`, instalação temporária e execução real Wayland).
- **Branch:** `valo-graphics-2.0`
- **Base:** `2081171` (após Consolidação 06.6D)
- **Commits de implementação:**
  - `f2bef87` — `refactor(tests): organize root tests into graphics, compositor, and unit` (`15` arquivos movidos com `git mv`)
  - `0fba4f8` — `refactor(tools): organize tools into benchmarks, artifacts, diagnostics, and maintenance` (`19` arquivos movidos com `git mv`)
  - `2512a7d` — `build(cmake): extract Dependencies.cmake from root CMakeLists`

---

## 1. Inventário Antes → Depois (`tests/`, `tools/`, `docs/`, `cmake/`)

| Área | Origem (06.6D) | Destino (06.6E) | Arquivos Movidos / Criados |
|---|---|---|---|
| **Testes Gráficos / ABI** | `tests/tst_*.cpp` (raiz) | `tests/graphics/` | `tst_SystaliaEngine.cpp`, `tst_ItemGeometry.cpp`, `tst_SdfGeometry.cpp`, `tst_BackdropBinding.cpp`, `tst_OpticalPhysics.cpp`, `tst_UnifiedComposition.cpp` (6) |
| **Testes de Compositor** | `tests/tst_*.cpp` (raiz) | `tests/compositor/` | `tst_WaylandSocket.cpp`, `tst_XdgShell.cpp`, `tst_XdgDecorations.cpp`, `tst_XdgResize.cpp`, `tst_SystaliaProtocol.cpp` (5) |
| **Testes Unitários** | `tests/tst_*.cpp` (raiz) | `tests/unit/` | `tst_Telemetry.cpp`, `tst_ProcessLauncher.cpp`, `tst_AppRegistry.cpp`, `tst_NotificationServer.cpp` (4) |
| **Ferramentas: Benchmarks** | `tools/` | `tools/benchmarks/` | `bench_and_artifacts.cpp` (1) |
| **Ferramentas: Artefatos** | `tools/` | `tools/artifacts/` | `generate_dock_v2_artifacts.cpp`, `generate_shell_v2_artifacts.cpp`, `generate_stage_05_1_artifacts.cpp` (3) |
| **Ferramentas: Diagnósticos** | `tools/` | `tools/diagnostics/` | `raw_render_diagnostics/` (7 arquivos com shaders), `validate_valo_header.py`, `validate_valo_stage06*.py` (13 no total) |
| **Ferramentas: Manutenção** | `tools/` | `tools/maintenance/` | `sge_evidence.py`, `sge_artifact_dir.hpp` (2) |
| **CMake** | `CMakeLists.txt` | `cmake/Dependencies.cmake` | Extraído módulo de dependências Qt6/Vulkan/Wayland/PkgConfig |
| **Documentação** | — | `docs/README.md` | Índice navegável preservando relatórios históricos |

**Total de arquivos movidos com `git mv` na 06.6E:** `34` arquivos (`15` em `tests/` + `19` em `tools/`).

---

## 2. Decisões sobre `src/`, `assets/` e `shaders/`

- **Subdiretórios avaliados em `src/` (Fase E):**
  - `src/compositor/input/`: não criado — `InputDeviceManager` já reside em `src/services/input/` (serviço libinput/udev).
  - `src/shell/desktop/`, `src/shell/workspaces/`, `src/services/workspaces/`: não criados para evitar fragmentação artificial de 0–1 arquivos (`WorkspaceViewport.qml` permanece em `src/shell/core/`; `WorkspaceManagerService` e `LiquidTilingService` permanecem em `src/services/windows/`).
- **`assets/figma/` e `shaders/v2/`:** mantidos intactos com os 25 arquivos `.qsb` compilados e aliases `QT_RESOURCE_ALIAS` preservando `src/qml/assets/figma/...`.
- **`Options.cmake`:** não extraído porque `SGE_GRAPHICS_V2` foi removido na 06.6C e não há opções de build restantes.

---

## 3. Mapa Final de Diretórios e Matriz de Responsabilidades

- `src/app/` — Bootstrap (`main.cpp`)
- `src/compositor/{core,surfaces,windows,outputs,xwayland}/` — Compositor Wayland, protocolo `z_systalia_surface_v1`, stack de janelas, outputs e XWayland
- `src/graphics/{shared,v2/{core,geometry,materials,backdrop,blur,shadows,qml}}/` — Contratos std140 compartilhados e renderer único VALO Graphics 2.0
- `src/shell/{core,dock,topbar,windows,overlays,panels,compatibility/v1}/` — Shell SDE V2 e adaptadores públicos de compatibilidade V1
- `src/controls/{frames,windows,dock}/` — Primitivas de UI reutilizáveis
- `src/services/{applications,input,session,windows,notifications,system}/` — Serviços de sistema e D-Bus
- `src/theme/{tokens,qml,compatibility/v1}/` — Tokens e temas QML
- `tests/{unit,integration,e2e,graphics,compositor,protocol-client}/` — 48 testes automatizados CTest
- `tools/{benchmarks,artifacts,diagnostics,maintenance}/` — Benchmarks, geradores de artefatos, sondas e retenção de evidências
- `cmake/` — `Dependencies.cmake`, `SgeShaders.cmake`, `SgeCore.cmake`, `SgeQml.cmake`, `SgeInstall.cmake`, `SgeTools.cmake`
- `shaders/v2/`, `protocols/`, `assets/figma/`, `docs/` — Shaders SPIR-V, protocolo Wayland, recursos Figma e documentação indexada em [`docs/README.md`](../../README.md)

---

## 4. Validação Final

- **Builds limpos com `-Wall -Wextra -Werror` e CTest completo (`QT_QPA_PLATFORM=offscreen`):**
  - **Debug:** **48/48 aprovados** (`49,73 s`), diretório temporário removido.
  - **Release + instalação em prefixo temporário (`/tmp/sge-install-066e`):** **48/48 aprovados** (`48,87 s`), **25 arquivos `.qsb`** gerados e instalação de `systalia_glass_engine`, `systalia-session` e `systalia.desktop` validada (diretórios temporários limpos).
- **Ferramentas e retenção:** 5 utilitários em `tools/` compilados; `tst_EvidenceCleanup.py` **10/10 aprovados** (`0,117 s`).
- **Execução real Wayland e comparação visual:** cliente `alacritty` em `WAYLAND_DISPLAY=wayland-systalia-0` encerrado com `exit 0` e limpeza automática via `EvidenceRun`; captura [`valo_066e_shell_v2.png`](file:///home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f/valo_066e_shell_v2.png) (`498.787` bytes) visualmente idêntica às etapas 06.6B–06.6D.
- **Limitações registradas:** `tst_xwayland_integration` e `tst_output_manager` passaram em Debug e Release; execução ao vivo adicional de clientes XWayland e testes em hardware multi-monitor físico permanecem fora da cobertura desta sessão.
