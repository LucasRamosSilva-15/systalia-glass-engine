# SYSTALIA — ETAPA ARQ-02 (Etapa 2 do Blueprint de Arquitetura Limpa)
## Single Source of Truth de Tokens (`sge_tokens`), Unificação de Opacidades VALO, Remoção de `animTimer` Ocioso e Migração `.png` → `.svg`

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Data:** 2026-11-28  
**Status:** Concluída (`61/61` CTest com `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, build incremental `8.75 s` / `11.87 s`, smoke test nested ao vivo `RC=0`)

---

## 1. Gerador `valo.tokens.json` → `ValoTokens.hpp` + `ValoTheme.qml` (`sge_tokens`)

- **Pipeline CMake + Python:**
  - Criado [`tools/codegen/generate_valo_tokens.py`](../../tools/codegen/generate_valo_tokens.py) e [`cmake/SgeTokens.cmake`](../../cmake/SgeTokens.cmake), incluído em [`CMakeLists.txt`](../../CMakeLists.txt) e vinculado a `sge_core` via `add_dependencies(sge_core sge_tokens)`.
  - O gerador escreve apenas quando o conteúdo muda (`write_if_changed`), evitando recompilações espúrias no build incremental.
- **Tabela canônica de opacidades de vidro (`src/theme/tokens/valo.tokens.json`):**

| Token JSON (`material`) | Propriedade C++ / `ValoTheme` | Valor |
|---|---|---|
| `opacity/glass-topbar` | `opacityGlassTopbar` | `0.50` |
| `opacity/glass-dock` | `opacityGlassDock` | `0.34` |
| `opacity/glass-window-active` | `opacityGlassWindowActive` | `0.50` |
| `opacity/glass-window-inactive` | `opacityGlassWindowInactive` | `0.42` |
| `opacity/glass-panel` | `opacityGlassPanel` | `0.52` |
| `opacity/glass-internal-window` | `opacityGlassInternalWindow` | `0.50` |
| `opacity/glass-overlay` | `opacityGlassOverlay` | `0.88` |
| `opacity/glass-high-contrast` | `opacityGlassHighContrast` | `0.92` |

---

## 2. Eliminação de Opacidades Hardcoded na Shell V2 (`ARQ-06`)

Todos os componentes principais passaram a consumir `ValoTheme` diretamente:
- [`src/shell/topbar/SystaliaTopBarV2.qml`](../../src/shell/topbar/SystaliaTopBarV2.qml): `ValoTheme.opacityGlassHighContrast : ValoTheme.opacityGlassTopbar`.
- [`src/shell/dock/SystaliaDockV2.qml`](../../src/shell/dock/SystaliaDockV2.qml) e [`src/graphics/v2/qml/ValoDockV2.qml`](../../src/graphics/v2/qml/ValoDockV2.qml): `ValoTheme.opacityGlassDock` / `ValoTheme.opacityGlassHighContrast`.
- [`src/shell/panels/SystaliaControlCenterV2.qml`](../../src/shell/panels/SystaliaControlCenterV2.qml): `ValoTheme.opacityGlassHighContrast : ValoTheme.opacityGlassPanel`.
- [`src/shell/windows/SystaliaFilesWindowV2.qml`](../../src/shell/windows/SystaliaFilesWindowV2.qml) e [`src/shell/windows/SystaliaMaterialWindow.qml`](../../src/shell/windows/SystaliaMaterialWindow.qml): `ValoTheme.opacityGlassInternalWindow` / `ValoTheme.opacityGlassHighContrast`.
- [`src/qml/waylib/WaylibWindowFrame.qml`](../../src/qml/waylib/WaylibWindowFrame.qml) e [`src/shell/core/WaylandWindowContainer.qml`](../../src/shell/core/WaylandWindowContainer.qml): `ValoTheme.opacityGlassWindowActive : ValoTheme.opacityGlassWindowInactive`.

---

## 3. Remoção do `animTimer` Contínuo em Idle (`ARQ-07`)

- Removido `Timer { id: animTimer; interval: 16; running: true; repeat: true }` de [`src/qml/waylib/WaylibNestedShell.qml`](../../src/qml/waylib/WaylibNestedShell.qml) e de [`src/shell/core/ShellRoot.qml`](../../src/shell/core/ShellRoot.qml), substituindo `animTimer.tick` por `0.0` (sem repaints contínuos de 60 Hz em repouso).
- `SystemTelemetry.recordFrame()` continua conectado diretamente a `WOutputRenderWindow.onRenderEnd`, medindo apenas frames reais renderizados.

---

## 4. Migração Exclusiva para `.svg` e Limpeza de `SystaliaTheme.qml` (`ARQ-09`, `ARQ-14`)

- Wallpaper migrado de `valo-ocean-atmosphere.png` para `valo-ocean-atmosphere.svg` (100% vetorial, sem `<image>` raster embutido) em `WaylibNestedShell.qml` (com `sourceSize` escalado por `devicePixelRatio`), `ShellRoot.qml` e `tests/unit/tst_ValoDecorations.cpp`.
- Removido `"assets/figma/*.png"` (28 arquivos PNG redundantes) do QRC em [`cmake/SgeQml.cmake`](../../cmake/SgeQml.cmake).
- Removido `src/theme/compatibility/v1/SystaliaTheme.qml` do módulo `Systalia.Engine 1.0` em `cmake/SgeQml.cmake`.

---

## 5. Testes, Build Incremental e Smoke Test

- **Novos casos de teste:**
  - [`tests/unit/tst_ValoTheme.cpp`](../../tests/unit/tst_ValoTheme.cpp): `glassSurfaceOpacityTokensMatchSpecification()` e `qmlSystaliaThemeRemovedFromModule()`.
  - [`tests/compositor/tst_WaylibBackend.cpp`](../../tests/compositor/tst_WaylibBackend.cpp): valida wallpaper `.svg`, ausência de `valo-ocean-atmosphere.png` e `SystaliaTheme.qml` no QRC, ausência de `QTimer` ativo de `<= 20 ms` (`animTimer`) e opacidades via `ValoTokens`.
  - [`tests/integration/tst_ShellStateRegression.cpp`](../../tests/integration/tst_ShellStateRegression.cpp): atualizado `clientBackdropAndAnimationFollowTheCanvas()` para validar `animTime == 0.0` estável em idle.
- **Testes direcionados:** `tst_valo_theme`, `tst_valo_decorations`, `tst_valo_shell_v2`, `tst_valo_optical_contract_v2`, `tst_waylib_backend` (`5/5` em `13.68 s`) e `tst_shell_state_regression` (`1/1` em `3.25 s`).
- **Build incremental (`build/waylib`, `Release`, `-Werror`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`):** **`8.75 s`** (última recompilação de teste; `11.87 s` em `tst_waylib_backend`).
- **Suíte completa CTest:** **`61/61` aprovados (`100% tests passed, 0 tests failed out of 61`, `43.37 s`)**.
- **Smoke test nested ao vivo (`./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500`):** **`RC=0`**, `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760`, `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!`, `XR24` em `WL-1`.

---

## 6. Prontidão para a Etapa 3 do Blueprint

Com `sge_tokens` ativo, opacidades unificadas, zero timers contínuos em idle e QRC 100% vetorial `.svg`, o projeto está pronto para a **Etapa 3 do Blueprint (`sge_graphics_v2` Desacoplado e Cache Dual-Kawase — `ARQ-05`, `ARQ-08`, `ARQ-11`)**.
