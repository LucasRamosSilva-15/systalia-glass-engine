# Estudo de Aproveitamento e Blueprint de Arquitetura Limpa — SYSTALIA (`REUSE_AND_CLEAN_MIGRATION_BLUEPRINT.md`)

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Data:** 2026-11-28  
**Base oficial única:** `Waylib + wlroots + VALO Glass V2` (`C++23 / Qt 6 / QML / SPIR-V`).  
**Referência de auditoria:** [`AUDITORIA_ARQUITETURA_COMPLETA.md`](AUDITORIA_ARQUITETURA_COMPLETA.md) (`ARQ-01` a `ARQ-16`).

---

## 1. Inventário de Classificação de Arquivos (`Keep / Refactor / Drop`)

### 1.1 `KEEP_AS_IS` (Reaproveitar Intacto)

| Diretório / Arquivo Real | Papel na Nova Arquitetura | Justificativa Técnica |
|---|---|---|
| `shaders/v2/valo_optical_v2.{vert,frag}`, `valo_kawase_{down,up}.{vert,frag}`, `valo_backdrop_copy.frag`, `valo_particles.frag`, `valo_shadow_v2.vert`, `valo_diag_*.frag` | `sge_graphics_v2` (SPIR-V) | Shaders V2 validados com UBO `std140`; nenhuma dependência de Wayland. |
| `src/graphics/shared/ValoStd140Matrix.hpp`, `SystaliaClientClip.hpp` | `sge_graphics_v2` | Contratos matemáticos `std140` (64 bytes) e clipping puros. |
| `src/graphics/v2/geometry/ValoGeometryEngineV2.*`, `ValoOrganicTransform.*` | `sge_graphics_v2` | Geometria SDF $L_2/G_2$ e transformações puras sem estado de shell. |
| `src/graphics/v2/core/ValoOpticalContext.*` | `sge_graphics_v2` | Políticas adaptativas (`highContrast`, `prefersReducedMotion`, `energySavingFactor`). |
| `src/graphics/v2/blur/ValoBlurPipelineV2.*`, `ValoGpuBlurPipelineV2.*` | `sge_graphics_v2` | Pirâmide Dual-Kawase multipass via `QRhi`, zero acoplamento com compositor. |
| `src/graphics/v2/materials/ValoGlassItemV2.*`, `ValoGlassMaterialV2.*` | `sge_graphics_v2` | Material RHI/Scene Graph V2 (nota: apenas `ValoGlassMaterialV2` será ajustado se slot `124` for renomeado em `ARQ-10`). |
| `src/graphics/v2/qml/ValoDockV2.qml`, `ValoDockV2Scene.qml`, `ValoDockItemV2.qml`, `ValoSurfaceV2.qml`, `ValoOpticalParticlesV2.qml`, `ValoSoftwareCursor.qml`, `ValoSingleWindowBackdropV2.qml`, `ValoLimitedBackdropV2.qml` | `sge_shell_valo` / `sge_graphics_v2` | Componentes QML V2 desacoplados; `ValoSoftwareCursor` já é 100% `Canvas` com `spinnerAngle` condicionado a `busy`. |
| `src/controls/frames/SystaliaG2Frame.qml`, `src/controls/windows/SystaliaWindowHeader.qml`, `SystaliaWindowAction.qml`, `src/controls/dock/ValoDockItem.qml` | `sge_shell_valo` | Controles visuais reutilizáveis sem lógica de compositor. |
| `src/shell/overlays/SnapPreviewOverlay.qml`, `SystaliaFlowMenu.qml`, `SystaliaNotificationHost.qml`, `SystaliaTaskSwitcher.qml`, `TileSplitter.qml`, `src/shell/core/WorkspaceViewport.qml` | `sge_shell_valo` | Overlays e viewport declarativos. |
| `src/services/applications/{AppRegistryService,ProcessLauncherService}.*`, `src/services/notifications/NotificationServer.*`, `src/services/session/LogindSessionService.*`, `src/services/system/{SystemControlService,SystemTelemetryService}.*`, `src/services/windows/{LiquidTilingService,WorkspaceManagerService}.*` | `sge_compositor_waylib` | Verificado em `src/services/`: nenhum destes possui dependência de `QWaylandCompositor`. |
| `assets/figma/*.svg` (todos os vetores SVG) | `sge_shell_valo` (QRC) | Fonte vetorial oficial do Design System VALO. |
| `protocols/z_systalia_surface_v1.xml`, `data/systalia.desktop.in`, `data/systalia-session.in`, `config/eglfs_kms.json` | Raiz / empacotamento | Contratos de protocolo e sessão preservados (ADR 0001). |
| `tests/graphics/*`, `tests/unit/tst_{AppRegistry,LiquidTiling,NotificationServer,ProcessLauncher,SdfGeometryMathV2,SurfacePropsAlignmentV2,TaskSwitcher,Telemetry,ValoBackdropBlurV2,ValoDockV2,ValoGlassRenderingV2,ValoGlassShaderV2,ValoGpuBlurPipelineV2,ValoOpticalContractV2,ValoOrganicTransformV2,ValoRefractionOpticsV2,ValoTheme,WorkspaceManager}.cpp` | Suíte CTest modular | Testes unitários do motor V2 e serviços desacoplados. |

---

### 1.2 `KEEP_WITH_FIX` (Reaproveitar com Refatoração Específica)

| Arquivo Real | Destino | Correção Obrigatória (`ARQ-XX`) |
|---|---|---|
| `src/compositor/waylib/WaylibSurfaceAdapter.{hpp,cpp}` | `sge_compositor_waylib` | **`ARQ-01`:** Substituir captura bruta `[display]` em `QTimer::singleShot` por guarda vinculada ao `WServer`/ciclo de vida. **`ARQ-04`/`ARQ-13`:** Absorver z-order unificado (clientes + internas), estado de maximização/snap e resize 8-direções, removendo `maximizedRevision`. |
| `src/compositor/waylib/WaylibCompositorAdapter.{hpp,cpp}` | `sge_compositor_waylib` | **`ARQ-01`:** Trocar ponteiros brutos (`m_server`, `m_cursor`, `m_backend`, `m_xdgShell`) por `QPointer`/posse explícita. **`ARQ-03`:** Integrar `InputDeviceManager` e futura fase XWayland/DRM sem depender de shell legado. |
| `src/compositor/waylib/WaylibOutputAdapter.{hpp,cpp}` | `sge_compositor_waylib` | **`ARQ-01`/`ARQ-11`:** Proteger `m_outputs`/`m_requestListeners` e expor modelo de outputs com geometria lógica e `scale` (DPR) por saída. |
| `shaders/v2/valo_shadow_v2.frag` & `src/graphics/v2/shadows/ValoShadowItemV2.*` | `sge_graphics_v2` | **`ARQ-02`:** Remover fallback `0.35` quando `u_ShadowIntensity <= 0.0` (e `u_ShadowOffsetY`), respeitando `0.0` em `highContrast` e `opacity: 0`. |
| `src/graphics/v2/core/SystaliaSurfacePropsV2.hpp` & `src/graphics/v2/materials/ValoGlassMaterialV2.cpp` | `sge_graphics_v2` | **`ARQ-10`:** Padronizar semanticamente o offset `124` (`inheritedOpacity` explícito ou campo dedicado) entre C++ e `valo_optical_v2.frag` / `valo_shadow_v2.frag`. |
| `src/graphics/v2/backdrop/ValoBackdropProviderV2.*` & `src/graphics/v2/qml/ValoMaterialSurfaceV2.qml` | `sge_graphics_v2` / `sge_shell_valo` | **`ARQ-05`/`ARQ-11`:** Compartilhar pirâmide `ValoGpuBlurPipelineV2` por `(source, blurProfile)` via cache/registro no provider; considerar `devicePixelRatio` em `createSyntheticBackdrop`. |
| `src/graphics/v2/qml/ValoInternalWindowBackdropV2.qml` | `sge_shell_valo` | **`ARQ-11`:** Substituir `Screen.devicePixelRatio` pelo `devicePixelRatio` do output/renderWindow pai. |
| `src/theme/tokens/{valo.tokens.json,ValoTokens.hpp}`, `src/theme/qml/ValoTheme.qml` | `sge_tokens` | **`ARQ-06`:** Gerar `ValoTokens.hpp` e `ValoTheme.qml` automaticamente no build a partir de `valo.tokens.json` (Single Source of Truth). |
| `src/shell/topbar/SystaliaTopBarV2.qml`, `src/shell/dock/SystaliaDockV2.qml`, `src/shell/panels/SystaliaControlCenterV2.qml`, `src/shell/windows/{SystaliaFilesWindowV2,SystaliaMaterialWindow}.qml` | `sge_shell_valo` | **`ARQ-06`:** Substituir opacidades hardcoded (`0.42`, `0.52`, `0.48`, `0.50`, `0.34`) pelos tokens semânticos de `ValoTheme`. |
| `src/services/input/InputDeviceManager.{hpp,cpp}` | `sge_compositor_waylib` | **`ARQ-03`:** Remover `#include <QtWaylandCompositor/...>` (`attachCompositor`/`targetSeat`); manter apenas detecção de sessão/udev ou adaptar para `WSeat`. |
| `src/app/waylib_nested_main.cpp` | `systalia` | **`ARQ-03`/`ARQ-12`:** Tornar o entry point único `systalia`, injetando `WOutputRenderWindow`/serviços via propriedades QML em vez de `rootObj->findChild<WOutputRenderWindow *>()`. |
| `cmake/{Dependencies,SgeShaders,SgeTools,SgeInstall}.cmake` | CMake modular | **`ARQ-08`/`ARQ-09`:** Tornar `SgeWaylib` obrigatório, separar `sge_shaders_diag` de `sge_shaders`, e ajustar `SgeInstall.cmake` para instalar `systalia`. |
| `tests/compositor/tst_WaylibBackend.cpp`, `tst_WaylibTestClient.cpp` | Suíte CTest Waylib | **`ARQ-15`:** Expandir cobertura para ciclo de vida/shutdown (`ARQ-01`), sombra `0.0` (`ARQ-02`), z-order/resize C++ e multi-output DPR. |

---

### 1.3 `EXTRACT_AND_SPLIT` (Quebrar e Redistribuir)

| Arquivo Monolítico Atual | Para C++ (`sge_compositor_waylib`) | Para QML Declarativo (`sge_shell_valo`) |
|---|---|---|
| `src/qml/waylib/WaylibNestedShell.qml` (1002 linhas) | Extrair estado de janelas internas (`filesWindow`/`materialWindow`: aberto, maximizado, workspace, z-order unificado) e políticas de auto-hide da Dock para um **`ShellWindowManager` / `WaylibSurfaceAdapter`** em C++; eliminar `animTimer` contínuo (`ARQ-07`) e `clientBackdropPool` fixo de 6 estágios (`ARQ-05`). | Manter apenas composição visual: `OutputRenderWindow`, `OutputItem`/`OutputViewport`, wallpaper SVG (`valo-ocean-atmosphere.svg`), `SystaliaTopBarV2`, `SystaliaDockV2`, overlays e binding direto aos modelos C++. |
| `src/qml/waylib/WaylibWindowFrame.qml` (551 linhas) | Extrair máquina de estados de **resize 8-direções** (`beginResize`/`updateResize`/`finishResize`), clamping min/max e cálculo de geometria maximizada (`[8, 60, w-16, h-68]`) para **`WaylibWindowController`** em C++ (`ARQ-04`). | Manter apenas `ValoMaterialSurfaceV2`, `SystaliaWindowHeader`, `XdgToplevelSurfaceItem`, `MouseArea`/`HoverHandler` delegando intenções ao controller C++ e `opaqueClientBacking`. |
| `cmake/SgeCore.cmake` + `cmake/SgeQml.cmake` | Quebrar em alvos independentes: `sge_tokens`, `sge_graphics_v2`, `sge_compositor_waylib` (com visibilidade `PRIVATE` para headers internos/privados, respeitando `ADR 0002` e `ARQ-08`). | Isolar módulo QML `Systalia.Engine` em `sge_shell_valo` empacotando apenas `.qml` V2 e `assets/figma/*.svg` (`ARQ-09`). |

---

### 1.4 `DROP` (Descartar na Nova Base)

| Categoria | Arquivos / Diretórios Reais a Descartar | Motivo (`ADR` / `ARQ`) |
|---|---|---|
| Compositor legado | `src/app/main.cpp`, `src/compositor/core/SystaliaCompositor.*`, `src/compositor/outputs/OutputManagerService.*`, `src/compositor/surfaces/SystaliaSurfaceExtension.*`, `src/compositor/windows/WindowStackModel.*`, `src/compositor/xwayland/XWaylandManager.*` | **`ARQ-03`:** Pilha `QtWaylandCompositor` substituída pelo Waylib (XWayland será reimplementado nativamente sobre Waylib na Etapa 03.2.5.4). |
| POC DRM/Vulkan legado | `cmake/SgeDrmPoc.cmake`, `src/platform/drm/*`, `src/platform/qpa/*`, `tools/diagnostics/sge_vulkan_dmabuf_harness.cpp` | **`ARQ-03`:** `wlroots` DRM/KMS backend do Waylib substitui o POC manual `sge_drm_poc`. |
| Shell legado & V1 | `src/shell/core/{main,ShellRoot,WaylandClientView,WaylandWindowContainer}.qml`, `src/theme/compatibility/v1/SystaliaTheme.qml` | **`ARQ-03`/`ARQ-14`:** `ShellRoot.qml` substituído pela shell Waylib; `SystaliaTheme.qml` tem zero consumidores ativos. |
| Assets raster redundantes | `assets/figma/*.png` (~20 arquivos com `.svg` equivalente, incluindo `valo-ocean-atmosphere.png`) | **`ARQ-09`:** Duplicação desnecessária no `.qrc`. |
| Sondas de transição | `src/diagnostics/{ValoHeaderAlignmentProbe,ValoInternalResizeProbe,ValoStabilityProbe}.hpp` | **`ARQ-12`/`ARQ-14`:** Acopladas a `SystaliaCompositor`, `findChild` e `useGraphicsV2`. |
| Testes do legado | `tests/compositor/tst_{SystaliaProtocol,WaylandSocket,XdgDecorations,XdgResize,XdgShell}.cpp`, `tests/e2e/tst_WaylandClientLifecycleE2E.cpp`, `tests/integration/tst_{ClientSurfaceVisibility,ProtocolRoundtrip,ShellStateRegression,XdgShellDecoration}.cpp`, `tests/unit/tst_{DrmBackendPoc,DrmGbmIntegration,IndependentFrameLoop,IndependentVulkanSession,VulkanDmabufAudit,VulkanKmsSync,VulkanQuickRenderer,DeviceLostAndCompositorLifecycle,PageFlipAndUnifiedStartup,UnifiedVulkanGbmPool,ShellRootIntegration,ValoDecorations,ValoGlass,OutputManager,InputHardwareBackend,XWaylandIntegration}.cpp` | **`ARQ-15`:** Vinculados a `SystaliaCompositor` / `sge_drm_poc` / `ShellRoot.qml` descartados. |
| Evidências versionadas | `artifacts/graphics-v2-stage-05-{4,5}/`, `docs/architecture/valo-v1-v2/evidence-06-*`, `docs/architecture/valo-v1-v2/*.log`, `tests/unit/__pycache__/` | **`ARQ-16`:** Violação da `SYSTALIA_DEBUG_RETENTION_POLICY.md`. |

---

## 2. Nova Arquitetura de Módulos (C++ / CMake / QML)

### 2.1 Grafo de Dependências Unidirecional (ADR 0002)

```text
sge_tokens ──▶ sge_graphics_v2 ──▶ sge_shell_valo ──┐
     │                                              ├──▶ systalia (executável único)
     └────────▶ sge_compositor_waylib ──────────────┘
```

### 2.2 Estrutura de Diretórios e Targets CMake

| Target CMake | Tipo / Visibilidade | Diretório Proposto | Responsabilidade Única | Dependências Permitidas |
|---|---|---|---|---|
| `sge_tokens` | `INTERFACE` / `STATIC` + codegen Python/CMake | `src/theme/` (`valo.tokens.json` -> gerado `ValoTokens.hpp` + `ValoTheme.qml`) | Single Source of Truth de cores, métricas, opacidades e durações VALO. | `Qt6::Core`, `Qt6::Gui` (somente `QColor`). |
| `sge_graphics_v2` | `STATIC` (`PRIVATE` para `Qt6::GuiPrivate` e `sge_shaders`) | `src/graphics/v2/`, `shaders/v2/` (`sge_shaders`) | Motor óptico RHI: SDF $L_2/G_2$, `ValoGlassItemV2`, `ValoShadowItemV2`, `ValoGpuBlurPipelineV2` e cache compartilhado de `ValoBackdropProviderV2`. | `sge_tokens`, `sge_shaders`, `Qt6::Quick`, `Qt6::GuiPrivate` (`PRIVATE`), `Vulkan::Vulkan` (`PRIVATE`). **Zero Wayland.** |
| `sge_compositor_waylib` | `STATIC` (`PRIVATE` para headers `wlr_*`) | `src/compositor/waylib/`, `src/services/`, `protocols/` | Backend Waylib/wlroots, `WaylibOutputAdapter` (DPR real), `WaylibSurfaceAdapter` + `WindowManagerController` (z-order, resize 8-direções, snap, workspaces) e serviços de sistema. | `sge_tokens`, `Sge::WaylibServer`, `Qt6::Quick`, `Qt6::DBus`, `PkgConfig::WAYLAND_SERVER`. **Zero QML visual.** |
| `sge_shell_valo` | Módulo QML (`Systalia.Engine 1.0`, `NO_PLUGIN`) | `src/shell/`, `src/controls/`, `assets/figma/*.svg` | UI puramente declarativa (`WaylibShell.qml`, `WaylibWindowFrame.qml`, `SystaliaTopBarV2`, `SystaliaDockV2`, `SystaliaControlCenterV2`, `SystaliaFilesWindowV2`, overlays). | `sge_tokens`, `sge_graphics_v2`, `sge_compositor_waylib` (via `Q_PROPERTY`/modelos). **Zero `findChild`, zero `Timer` contínuo ocioso, zero `.png`.** |
| `systalia` | `EXECUTABLE` único | `src/app/main.cpp` (portado de `waylib_nested_main.cpp`) | Bootstrap único (`NestedWayland`, `Headless`, e futuro `DRM/KMS` via `WBackend`), injeção explícita de dependências no contexto QML e instalação SDDM. | `sge_compositor_waylib`, `sge_shell_valo`, `Qt6::QuickControls2`. |

---

## 3. Caminho Predefinido de Migração em 6 Etapas Incrementais

| Etapa | Objetivo | Dependências | Arquivos Portados / Criados | Critério de Conclusão (Build + CTest + Validação Visual) |
|---|---|---|---|---|
| **Etapa 1 — Blindagem de Segurança e ABI Gráfica (`ARQ-01`, `ARQ-02`, `ARQ-10`)** | Corrigir UAF no `QTimer::singleShot` do `wl_display`, eliminar fallback `0.35` em `valo_shadow_v2.frag` para `shadowIntensity == 0.0` e unificar o slot `124` do UBO `std140`. | Nenhuma (executada sobre a baseline atual antes do corte de legado). | `WaylibSurfaceAdapter.cpp`, `WaylibCompositorAdapter.hpp`, `WaylibOutputAdapter.hpp`, `shaders/v2/valo_shadow_v2.frag`, `SystaliaSurfacePropsV2.hpp`, `ValoGlassMaterialV2.cpp`. | Build `Release` `-Werror` (`-DSGE_ENABLE_WAYLIB_BACKEND=ON`) limpo; `ctest` `61/61` + novo teste de `shutdown()` após resize/maximize e validação de sombra `0.0`. |
| **Etapa 2 — `sge_tokens` e Paridade do Design System VALO (`ARQ-06`, `ARQ-07`, `ARQ-09`)** | Criar gerador de `ValoTokens.hpp` e `ValoTheme.qml` a partir de `valo.tokens.json`, eliminar opacidades hardcoded, remover `SystaliaTheme.qml` V1, trocar `.png` por `.svg` no QRC e substituir `animTimer` 60 Hz contínuo por atualização sob demanda. | Etapa 1 | `cmake/SgeTokens.cmake`, `src/theme/tokens/*`, `cmake/SgeQml.cmake`, `SystaliaTopBarV2.qml`, `SystaliaDockV2.qml`, `SystaliaControlCenterV2.qml`, `SystaliaFilesWindowV2.qml`, `WaylibNestedShell.qml`. | `ctest -R "^(tst_valo_theme\|tst_valo_shell_v2\|tst_waylib_backend)$"`; verificação visual de TopBar (`opacityTopbar`), wallpaper SVG nítido e 0 frames/s ociosos. |
| **Etapa 3 — `sge_graphics_v2` Desacoplado e Cache Dual-Kawase (`ARQ-05`, `ARQ-08`, `ARQ-11`)** | Extrair `sge_graphics_v2` como target CMake independente (sem Wayland e sem `GuiPrivate` `PUBLIC`), implementar compartilhamento de `ValoBackdropProviderV2` por `(source, blurProfile)` e corrigir DPR por output (`WOutput::scale`). | Etapa 2 | `cmake/SgeGraphicsV2.cmake`, `src/graphics/v2/**`, `ValoInternalWindowBackdropV2.qml`, `WaylibNestedShell.qml` (pool sob demanda). | `ctest -R "^(tst_valo_backdrop_blur_v2\|tst_valo_gpu_blur_pipeline_v2\|tst_valo_glass.*\|tst_resource_leak_audit)$"`; contagem reduzida de FBOs e renderização correta em DPR `1.25`/`1.5`. |
| **Etapa 4 — `sge_compositor_waylib` e Extração de WM para C++ (`ARQ-04`, `ARQ-12`, `ARQ-13`)** | Mover resize 8-direções, z-order unificado (clientes + janelas internas), estado de maximização/snap e workspaces para C++ em `sge_compositor_waylib`; desacoplar `InputDeviceManager` de `QWaylandCompositor` e remover `findChild` no startup. | Etapa 3 | `cmake/SgeCompositorWaylib.cmake`, `src/compositor/waylib/*`, `InputDeviceManager.{hpp,cpp}`, `WaylibNestedShell.qml`, `WaylibWindowFrame.qml`, `waylib_nested_main.cpp`. | `ctest -R "^(tst_waylib_backend\|tst_workspace_manager\|tst_liquid_tiling)$"` expandido cobrindo z-order, resize e workspaces via C++; smoke test nested `RC=0`. |
| **Etapa 5 — `sge_shell_valo` Limpo e Remoção do Legado (`ARQ-03`, `ARQ-14`, `ARQ-15`)** | Consolidar `systalia` como executável único, remover `QtWaylandCompositor` legado, `sge_drm_poc`, `ShellRoot.qml`, `WaylandWindowContainer.qml`, sondas antigas e testes legados; atualizar `SgeInstall.cmake` e `tests/CMakeLists.txt`. | Etapa 4 | `CMakeLists.txt`, `cmake/{SgeShellValo,SgeInstall}.cmake`, `src/app/main.cpp` (unificado), `tests/CMakeLists.txt`. | Build limpo sem `sge_drm_poc` nem `QtWaylandCompositor`; 100% da suíte CTest limpa aprovada; execução `systalia --socket ...` com clientes Wayland reais. |
| **Etapa 6 — Higiene Final e Homologação da Arquitetura Limpa (`ARQ-16`)** | Remover `artifacts/graphics-v2-stage-05-*`, `docs/architecture/valo-v1-v2/evidence-06-*`, logs antigos versionados e atualizar ADRs (`0001`–`0003`) refletindo `systalia` (Waylib + VALO V2) como arquitetura única. | Etapa 5 | `docs/architecture/adr/*`, `.gitignore`, limpeza de logs/artefatos rastreados. | `git status` limpo de binários/logs temporários, build `Release` `-Werror` verde e suíte completa CTest 100% aprovada. |
