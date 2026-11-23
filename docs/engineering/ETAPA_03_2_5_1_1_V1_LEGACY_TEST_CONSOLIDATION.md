# SYSTALIA — ETAPA 03.2.5.1.1
## Auditoria e Eliminação Definitiva de Componentes e Testes V1 Obsoletos

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, CMake/Ninja  
**Backend:** Waylib/wlroots nested (sem execução física DRM/KMS)

---

## 1. Resumo Executivo

A auditoria sobre o módulo QML `Systalia.Engine 1.0`, o código de produção, os diagnósticos e a suíte CTest confirmou que os componentes ópticos V1 (`ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial`, `SystaliaSurfaceProps.hpp` e `shaders/valo_optical.*`) já haviam sido removidos em etapas anteriores (Consolidação 06.6B), restando na árvore apenas:

1. **4 adaptadores QML V1 de shell** em `src/shell/compatibility/v1/` (`SystaliaTopBar.qml`, `SystaliaDock.qml`, `SystaliaFilesWindow.qml`, `SystaliaControlCenter.qml`), que apenas encapsulavam os equivalentes `*V2.qml` sem nenhum consumidor em produção ou ferramentas externas.
2. **1 singleton de tema de compatibilidade** em `src/theme/compatibility/v1/SystaliaTheme.qml`, com contrato público ativo validado por `tests/unit/tst_ValoTheme.cpp` e preservado integralmente.

Os 4 adaptadores V1 obsoletos de shell foram removidos, seus aliases QRC foram limpos em `cmake/SgeQml.cmake`, os subpassos legados de `src/diagnostics/ValoStabilityProbe.hpp` foram removidos e `tests/unit/tst_ValoShellV2.cpp` foi migrado para testar `SystaliaDockV2`, `SystaliaTopBarV2`, `SystaliaFilesWindowV2` e `SystaliaControlCenterV2` diretamente.

---

## 2. Auditoria Histórica e de Consumidores

Referências consultadas:
- `docs/architecture/valo-v1-v2/CONSOLIDACAO_06_QML.md`
- `docs/architecture/valo-v1-v2/CONSUMIDORES_V1.md`
- `docs/architecture/valo-v1-v2/PLANO_E_INVENTARIO.md`
- `docs/architecture/adr/0003-aposentadoria-decoration-v1.md`

Busca textual (`rg`) em `src/`, `tests/`, `tools/`, `cmake/` e `data/` confirmou:
- **Zero consumidores em produção** para `SystaliaTopBar.qml`, `SystaliaDock.qml`, `SystaliaFilesWindow.qml` e `SystaliaControlCenter.qml`.
- As únicas referências existentes estavam em `cmake/SgeQml.cmake`, `src/diagnostics/ValoStabilityProbe.hpp` (subpassos `13-files-legacy-name` e `14-control-center-legacy-name`) e `tests/unit/tst_ValoShellV2.cpp` (`testLegacyDockAndTopBarResources` e `testLegacyFilesAndControlCenterResources`).
- A propriedade `useGraphicsV2: true` em `WaylandWindowContainer.qml` e `ShellRoot.qml` é `readonly` e não possui fallback V1; é lida apenas por `ValoStabilityProbe.hpp` e `tst_ValoDecorations.cpp`.

---

## 3. Classificação dos Componentes e Arquivos V1 Auditados

| Item | Caminho | Classificação | Decisão |
|---|---|---|---|
| `SystaliaTopBar.qml` | `src/shell/compatibility/v1/SystaliaTopBar.qml` | **B — Obsoleto sem uso real** (wrapper de `SystaliaTopBarV2`) | **Removido** |
| `SystaliaDock.qml` | `src/shell/compatibility/v1/SystaliaDock.qml` | **B — Obsoleto sem uso real** (wrapper de `SystaliaDockV2`) | **Removido** |
| `SystaliaFilesWindow.qml` | `src/shell/compatibility/v1/SystaliaFilesWindow.qml` | **B — Obsoleto sem uso real** (wrapper de `SystaliaFilesWindowV2`) | **Removido** |
| `SystaliaControlCenter.qml` | `src/shell/compatibility/v1/SystaliaControlCenter.qml` | **B — Obsoleto sem uso real** (wrapper de `SystaliaControlCenterV2`) | **Removido** |
| `SystaliaTheme.qml` | `src/theme/compatibility/v1/SystaliaTheme.qml` | **A — Contrato público ativo** (`tst_ValoTheme.cpp`) | **Preservado** |

---

## 4. Auditoria da Suíte de Testes (`tests/CMakeLists.txt`)

A inspeção dos testes apontados para auditoria mostrou que a maioria já havia sido migrada para V2 em etapas anteriores:
- **Preservados (cobertura V2 ou contrato ativo):**
  - `tst_ValoGlass.cpp`: testa `ValoMaterialSurfaceV2` (defaults, `blurProfile` 0/1/2, `contentContainer`, `active` e renderização).
  - `tst_ValoDecorations.cpp`: testa `WaylandWindowContainer` com `ValoGlassItemV2` (`clientDecorationV2`), `ValoMaterialSurfaceV2`, `SystaliaWindowAction` e renderização.
  - `tst_SystaliaEngine.cpp`: valida `sizeof(SystaliaSurfacePropsV2) == 160`, offsets `std140` e presença dos shaders `:/shaders/v2/*.qsb`.
  - `tst_ItemGeometry.cpp`: valida `ValoGlassItemV2` e atributos de vértice `QSGGeometry`.
  - `tst_BackdropBinding.cpp`: valida `ValoGlassItemV2` + `ValoBackdropProviderV2`.
  - `tst_UnifiedComposition.cpp`: valida `SystaliaSurfacePropsV2` + `SystaliaClientClipGeometry`.
  - `tst_ResourceLeakAudit.cpp`: valida ciclo de vida de `ValoGlassItemV2`, `ValoGlassMaterialV2`, `AppRegistryService`, `NotificationServer` e `WindowStackModel`.
  - `tst_OpticalPhysics.cpp` e `tst_SdfGeometry.cpp`: testes matemáticos locais rápidos de referência.
  - `tst_ValoTheme.cpp`: protege o singleton `SystaliaTheme.qml` e `ValoTheme.qml`.
  - Backend experimental Vulkan/DRM/KMS (`tst_drm_backend_poc`, `tst_drm_gbm_integration`, `tst_vulkan_quick_renderer`, `tst_independent_vulkan_session`, `tst_independent_frame_loop`, `tst_unified_vulkan_gbm_pool`, `tst_vulkan_kms_sync`, `tst_page_flip_and_unified_startup`, `tst_device_lost_and_compositor_lifecycle`, `tst_vulkan_dmabuf_audit`) e `tst_waylib_backend`: **preservados integralmente**.
- **Migrado (removido consumo de wrappers V1, preservada cobertura interativa V2):**
  - `tests/unit/tst_ValoShellV2.cpp`: `testLegacyDockAndTopBarResources` → `testDockAndTopBarV2Interactions` e `testLegacyFilesAndControlCenterResources` → `testFilesAndControlCenterV2Interactions`, carregando `SystaliaDockV2`, `SystaliaTopBarV2`, `SystaliaFilesWindowV2` e `SystaliaControlCenterV2` diretamente via `qrc:/qt/qml/Systalia/Engine/src/qml/shell/*V2.qml`.

---

## 5. Alterações Realizadas

1. **Remoção dos 4 arquivos QML V1 de shell:**
   - `src/shell/compatibility/v1/SystaliaTopBar.qml`
   - `src/shell/compatibility/v1/SystaliaDock.qml`
   - `src/shell/compatibility/v1/SystaliaFilesWindow.qml`
   - `src/shell/compatibility/v1/SystaliaControlCenter.qml`
   (e diretórios vazios `src/shell/compatibility/v1` e `src/shell/compatibility`).
2. **`cmake/SgeQml.cmake`:**
   - Removido o bloco `foreach(compat_qml IN ITEMS ...)` e as 4 entradas em `qt_add_qml_module(sge_core ... QML_FILES ...)`.
   - Mantidos o URI `Systalia.Engine 1.0` e o singleton `src/theme/compatibility/v1/SystaliaTheme.qml`.
3. **`src/diagnostics/ValoStabilityProbe.hpp`:**
   - Removidos os subpassos `13-files-legacy-name` e `14-control-center-legacy-name` que instanciavam `SystaliaFilesWindow.qml` e `SystaliaControlCenter.qml`; mantidos `11-files-v2`, `12-control-center-v2` e `13-client-closed`.
4. **`tests/unit/tst_ValoShellV2.cpp`:**
   - Migrados os casos interativos para `SystaliaDockV2`, `SystaliaTopBarV2`, `SystaliaFilesWindowV2` e `SystaliaControlCenterV2`.

---

## 6. Validação Técnica

Todas as compilações foram executadas com `-DCMAKE_CXX_FLAGS="-Werror"` em diretórios temporários fora do repositório (removidos após validação):

| Configuração | Flags / Opções | Resultado Build | CTest |
|---|---|---|---|
| **Debug (`SGE_ENABLE_WAYLIB_BACKEND=OFF`)** | `-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Werror"` | 0 erros, 0 warnings | **60/60 PASS** (35.57 s) |
| **Release (`SGE_ENABLE_WAYLIB_BACKEND=ON`)** | `-DCMAKE_BUILD_TYPE=Release -DSGE_WAYLIB_PREFIX="$HOME/.cache/sge-waylib-prefix" -DCMAKE_CXX_FLAGS="-Werror"` | 0 erros, 0 warnings | **61/61 PASS** (38.04 s) |

### Smoke Test ao vivo do Compositor Nested Waylib
- Comando: `QT_QPA_PLATFORM=offscreen systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 2500` + `WAYLAND_DISPLAY=sge-nested-poc tst_waylib_test_client`
- Resultado: cliente conectado, toplevel XDG adicionado e removido (`[SGE-WAYLIB] XDG toplevel added` / `removed`), `CLIENT_RC=0`, `COMP_RC=0`.
