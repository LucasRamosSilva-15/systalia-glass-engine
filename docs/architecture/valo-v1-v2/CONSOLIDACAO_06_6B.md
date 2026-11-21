# Consolidação 06.6B — Remoção Definitiva do Renderer VALO V1

- **Estado da Etapa 06.6B:** **COMPLETE** (renderer óptico V1 não é mais compilado, instanciado ou empacotado; VALO Graphics V2 é o único renderer ativo).
- **Branch:** `valo-graphics-2.0`
- **Base:** `9f0eaff` (após Consolidação 06.6A.2)

---

## 1. Fase A — Auditoria Final de Consumidores V1

Antes da remoção física, a varredura estática em `src/`, `shaders/`, `cmake/`, `tools/` e `tests/` confirmou:
- **Produção (`src/`, `shaders/`, `cmake/`):** zero instâncias ativas de `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial`, `SystaliaSurfaceProps` e `shaders/valo_optical.*`. Apenas entradas de build (`CMakeLists.txt`, `cmake/SgeShaders.cmake`) e comentários informativos (`SystaliaG2Frame.qml`, `SystaliaSurfacePropsV2.hpp`).
- **Infraestrutura preservada:** `ValoStd140Matrix.hpp`, `SystaliaSurfacePropsV2.hpp`, `SystaliaClientClip.hpp`, `protocols/z_systalia_surface_v1.xml`, `SystaliaTheme.qml`, `ValoTheme.qml`, adaptadores públicos da shell (`SystaliaDock.qml`, `SystaliaTopBar.qml`, `SystaliaFilesWindow.qml`, `SystaliaControlCenter.qml`) e toda a pilha Wayland/XWayland (`ShellSurfaceItem`).

---

## 2. Fase B — Migração de Testes para VALO V2

Todos os testes que ainda dependiam da implementação V1 foram migrados para os equivalentes V2 sem exclusão silenciosa de cobertura:

- [`tests/tst_SystaliaEngine.cpp`](../../../tests/tst_SystaliaEngine.cpp): migrado para `SystaliaSurfacePropsV2` (160 bytes) e verificação dos `.qsb` oficiais V2 (`:/shaders/v2/valo_optical_v2.*.qsb`, `valo_shadow_v2.*.qsb`, `valo_kawase_*.qsb`).
- [`tests/tst_ItemGeometry.cpp`](../../../tests/tst_ItemGeometry.cpp): migrado de `SystaliaGlassItem`/`SystaliaSurfaceProps` para `ValoGlassItemV2`/`SystaliaSurfacePropsV2`.
- [`tests/tst_BackdropBinding.cpp`](../../../tests/tst_BackdropBinding.cpp): migrado para `ValoGlassItemV2`, `ValoBackdropProviderV2` (`backdropProviderChanged`, `releaseResources()`) e `SystaliaSurfacePropsV2`.
- [`tests/tst_UnifiedComposition.cpp`](../../../tests/tst_UnifiedComposition.cpp): migrado para `SystaliaSurfacePropsV2` mantendo intactos os 6 testes de `SystaliaClientClipGeometry`.
- [`tests/unit/tst_MemoryAlignment.cpp`](../../../tests/unit/tst_MemoryAlignment.cpp) e [`tests/unit/tst_SurfacePropsAlignmentV2.cpp`](../../../tests/unit/tst_SurfacePropsAlignmentV2.cpp): validação consolidada em `Matrix4x4_std140` (64 bytes) e `SystaliaSurfacePropsV2` (160 bytes, todos os 17 offsets).
- [`tests/unit/tst_SdfGeometryMath.cpp`](../../../tests/unit/tst_SdfGeometryMath.cpp): contrato `fwidth(dist)` e `discard` migrado para `shaders/v2/valo_optical_v2.frag`.
- [`tests/unit/tst_OutputManager.cpp`](../../../tests/unit/tst_OutputManager.cpp): migrado para `SystaliaSurfacePropsV2` e `ValoGlassItemV2`.
- [`tests/unit/tst_Phase4Audit.cpp`](../../../tests/unit/tst_Phase4Audit.cpp): migrado para `ValoGlassItemV2` e `ValoBackdropProviderV2`.
- [`tests/unit/tst_ResourceLeakAudit.cpp`](../../../tests/unit/tst_ResourceLeakAudit.cpp): migrado para ciclo de vida de `ValoGlassItemV2` (100 iterações) e compartilhamento de `QSGMaterialType`/batching em `ValoGlassMaterialV2`.
- [`tests/unit/tst_ValoGlass.cpp`](../../../tests/unit/tst_ValoGlass.cpp) e [`tests/unit/tst_ValoDecorations.cpp`](../../../tests/unit/tst_ValoDecorations.cpp): migrados de `ValoGlass.qml`/`SystaliaGlassItem` para `ValoMaterialSurfaceV2` e `ValoGlassItemV2` (respeitando `SGE_KEEP_EVIDENCE_DIR` para capturas opcionais).
- [`tests/unit/tst_ValoShellV2.cpp`](../../../tests/unit/tst_ValoShellV2.cpp) e [`tests/integration/tst_ShellStateRegression.cpp`](../../../tests/integration/tst_ShellStateRegression.cpp): removidas as asserções residuais contra `SystaliaGlassItem`.

---

## 3. Fase C — Arquivos Removidos Fisicamente

Foram removidos 8 arquivos V1 e suas inscrições em [`CMakeLists.txt`](../../../CMakeLists.txt) e [`cmake/SgeShaders.cmake`](../../../cmake/SgeShaders.cmake):

1. `src/qml/ValoGlass.qml`
2. `src/gfx/SystaliaGlassItem.hpp`
3. `src/gfx/SystaliaGlassItem.cpp`
4. `src/gfx/SystaliaGlassMaterial.hpp`
5. `src/gfx/SystaliaGlassMaterial.cpp`
6. `src/gfx/SystaliaSurfaceProps.hpp`
7. `shaders/valo_optical.vert`
8. `shaders/valo_optical.frag`

---

## 4. Fase D — Validação e Métricas

1. **Builds limpos com `-Wall -Wextra -Werror` e CTest completo (`QT_QPA_PLATFORM=offscreen`):**
   - **Debug:** **48/48 testes aprovados** (`49,37 s`), diretório temporário removido.
   - **Release:** **48/48 testes aprovados** (`49,06 s`), diretório temporário removido.
2. **Integridade QRC/QSB e binário instalado em prefixo temporário (`/tmp/sge-install-066b`):**
   - **Shaders `.qsb`:** reduzido de **27** (Consolidação 06.5) para **25** arquivos `.qsb` (removidos `valo_optical.vert.qsb` e `valo_optical.frag.qsb`).
   - **Binário Release (`systalia_glass_engine`):** `5.024.744` bytes (`4,8 MiB`), com **0 referências** a `SystaliaGlassItem`, `SystaliaGlassMaterial`, `valo_optical.frag` ou `ValoGlass.qml` em `strings`.
3. **Execução real Wayland e inspeção da Shell V2:**
   - Cliente Wayland nativo (`alacritty` em `WAYLAND_DISPLAY=wayland-systalia-0`): `Wayland SDE exit: 0`, 17 capturas (`00-native-default.png` a `16-closed.png`) geradas via `EvidenceRun` e removidas automaticamente após sucesso.
   - Captura da shell V2 (`valo_066b_shell_v2.png`, `543.621` bytes) inspecionada com renderização íntegra de TopBar V2, Systalia Files V2, Systalia Control Center V2 e Dock V2.

---

## 5. Limitações Não Homologadas

- **XWayland ao vivo nesta sessão:** o teste unitário `tst_xwayland_integration` passou em Debug e Release, porém na tentativa de execução ao vivo no compositor aninhado o `DISPLAY` interno do XWayland não foi detectado no log de inicialização (`Detected XWayland DISPLAY: None`). A paridade de clientes XWayland modernos (`gtk3-x11`, `qt6-xcb`, `alacritty-x11`) sobre `decorationV2` permanece registrada pela Consolidação 06.5, e clientes Xt/Xaw legados (`xmessage`) seguem como limitação conhecida.
- **Múltiplos monitores físicos / DPR heterogêneo em hardware real:** coberto por `tst_output_manager`, não testado em hardware multi-monitor físico nesta sessão.
