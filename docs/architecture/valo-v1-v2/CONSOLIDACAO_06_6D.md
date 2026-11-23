# Consolidação 06.6D — Organização Hierárquica por Responsabilidades e Versões

- **Estado da Etapa 06.6D:** **COMPLETE** (hierarquia por responsabilidade e separação clara entre implementação VALO V2, contratos compartilhados, protocolo ativo `z_systalia_surface_v1` e adaptadores de compatibilidade V1; builds Debug/Release com `-Werror`, 48/48 CTest, 25 `.qsb`, instalação em prefixo temporário e execução real Wayland validados).
- **Branch:** `valo-graphics-2.0`
- **Base:** `893b101` (após Consolidação 06.6C)
- **Commits de implementação:**
  - `709bc86` — `refactor(structure): split compositor and services by responsibility` (`28` arquivos movidos com `git mv`)
  - `41a3c0a` — `refactor(structure): split graphics/v2, graphics/shared, and theme by responsibility` (`33` arquivos movidos com `git mv`)
  - `8021c5c` — `refactor(structure): split shell and controls by responsibility and compatibility/v1` (`22` arquivos movidos com `git mv`)

---

## 1. Mapa de Diretórios (Antes → Depois)

| Subsistema | Origem (06.6C) | Destino (06.6D) | Arquivos |
|---|---|---|---|
| **Compositor** | `src/compositor/` | `src/compositor/core/` | `SystaliaCompositor.*` |
| | | `src/compositor/surfaces/` | `SystaliaSurfaceExtension.*` (`z_systalia_surface_v1`) |
| | | `src/compositor/windows/` | `WindowStackModel.*` |
| | | `src/compositor/outputs/` | `OutputManagerService.*` |
| | | `src/compositor/xwayland/` | `XWaylandManager.*` |
| **Graphics (Shared)** | `src/graphics/` | `src/graphics/shared/` | `ValoStd140Matrix.hpp`, `SystaliaClientClip.hpp` |
| **Graphics (VALO V2)** | `src/graphics/` | `src/graphics/v2/core/` | `SystaliaSurfacePropsV2.hpp`, `ValoOpticalContext.*` |
| | | `src/graphics/v2/geometry/` | `ValoGeometryEngineV2.*`, `ValoOrganicTransform.*` |
| | | `src/graphics/v2/materials/` | `ValoGlassMaterialV2.*`, `ValoGlassItemV2.*` |
| | | `src/graphics/v2/backdrop/` | `ValoBackdropProviderV2.*` |
| | | `src/graphics/v2/blur/` | `ValoBlurPipelineV2.*`, `ValoGpuBlurPipelineV2.*` |
| | | `src/graphics/v2/shadows/` | `ValoShadowItemV2.*` |
| | `src/graphics/qml/` | `src/graphics/v2/qml/` | `Valo*V2.qml` (9 arquivos) |
| **Services** | `src/services/` | `src/services/applications/` | `AppRegistryService.*`, `ProcessLauncherService.*` |
| | | `src/services/input/` | `InputDeviceManager.*` |
| | | `src/services/session/` | `LogindSessionService.*` |
| | | `src/services/windows/` | `LiquidTilingService.*`, `WorkspaceManagerService.*` |
| | | `src/services/notifications/` | `NotificationServer.*` |
| | | `src/services/system/` | `SystemControlService.*`, `SystemTelemetryService.*` |
| **Shell** | `src/shell/` | `src/shell/core/` | `main.qml`, `WorkspaceViewport.qml`, `WaylandClientView.qml`, `WaylandWindowContainer.qml` |
| | | `src/shell/dock/` | `SystaliaDockV2.qml` |
| | | `src/shell/topbar/` | `SystaliaTopBarV2.qml` |
| | | `src/shell/windows/` | `SystaliaFilesWindowV2.qml`, `SystaliaMaterialWindow.qml` |
| | | `src/shell/overlays/` | `SnapPreviewOverlay.qml`, `SystaliaFlowMenu.qml`, `SystaliaNotificationHost.qml`, `SystaliaTaskSwitcher.qml`, `TileSplitter.qml` |
| | | `src/shell/panels/` | `SystaliaControlCenterV2.qml` |
| | | `src/shell/compatibility/v1/` | `SystaliaControlCenter.qml`, `SystaliaDock.qml`, `SystaliaFilesWindow.qml`, `SystaliaTopBar.qml` |
| **Controls** | `src/controls/` | `src/controls/frames/` | `SystaliaG2Frame.qml` |
| | | `src/controls/windows/` | `SystaliaWindowAction.qml`, `SystaliaWindowHeader.qml` |
| | | `src/controls/dock/` | `ValoDockItem.qml` |
| **Theme** | `src/theme/` | `src/theme/tokens/` | `ValoTokens.hpp`, `valo.tokens.json` |
| | `src/theme/qml/` | `src/theme/qml/` | `ValoTheme.qml` |
| | | `src/theme/compatibility/v1/` | `SystaliaTheme.qml` |

---

## 2. Separação Explícita de Responsabilidades e Versões

- **VALO V2 ativo:** isolado em `src/graphics/v2/`, `src/shell/{dock,topbar,windows,panels}` e `src/theme/qml/ValoTheme.qml`.
- **Contratos compartilhados:** `src/graphics/shared/` (`ValoStd140Matrix.hpp`, `SystaliaClientClip.hpp`).
- **Adaptadores públicos V1 (compatibilidade):** isolados em `src/shell/compatibility/v1/` e `src/theme/compatibility/v1/`, sem reintroduzir o renderer óptico V1 já removido.
- **Protocolo Wayland ativo:** `protocols/z_systalia_surface_v1.xml` e `src/compositor/surfaces/SystaliaSurfaceExtension.*` preservados integralmente.
- **Compatibilidade QRC:** [`cmake/SgeCore.cmake`](../../../cmake/SgeCore.cmake) e [`cmake/SgeQml.cmake`](../../../cmake/SgeQml.cmake) atualizados preservando `Systalia.Engine 1.0`, `sge_shaders` (25 `.qsb`), geração de protocolos e todos os aliases `QT_RESOURCE_ALIAS` sob `qrc:/qt/qml/Systalia/Engine/src/qml/...`.

---

## 3. Validação Executada

- **Builds limpos com `-Wall -Wextra -Werror` e CTest completo (`QT_QPA_PLATFORM=offscreen`):**
  - **Debug:** **48/48 testes aprovados** (`76,23 s`), diretório temporário removido.
  - **Release:** **48/48 testes aprovados** (`49,05 s`), **25 arquivos `.qsb`** gerados, instalação validada em `/tmp/sge-install-066d` (`systalia_glass_engine`, `systalia-session`, `systalia.desktop`), diretórios temporários removidos.
- **Integridade estrutural e de scripts:** zero diretórios vazios sob `src/`; zero referências quebradas em `tools/` ou `data/`.
- **Execução real Wayland e comparação visual:** cliente `alacritty` em `WAYLAND_DISPLAY=wayland-systalia-0` encerrado com `exit 0` e limpeza automática via `EvidenceRun`; captura [valo_066d_shell_v2.png](file:///home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f/valo_066d_shell_v2.png) (`499.407` bytes) idêntica em qualidade visual às etapas 06.6B e 06.6C.

---

## 4. Limitações Registradas

- **XWayland ao vivo e multi-monitor físico:** `tst_xwayland_integration` e `tst_output_manager` passaram em Debug e Release; execução ao vivo adicional de clientes XWayland e testes em hardware multi-monitor físico com DPR heterogêneo permanecem fora da cobertura desta sessão.
