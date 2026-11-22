# Consumidores VALO V1

**Estado na Consolidação 06.6B:** **ZERO consumidores ativos ou de teste** para `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial`, `SystaliaSurfaceProps` e `shaders/valo_optical.*` (todos removidos fisicamente do repositório).

| Camada | Situação após Consolidação 06.6B | Classificação |
|---|---|---|
| C++ e ABI óptica | `SystaliaGlassItem`, `SystaliaGlassMaterial` e `SystaliaSurfaceProps.hpp` removidos. `ValoStd140Matrix.hpp` (64 bytes) e `SystaliaSurfacePropsV2.hpp` (160 bytes) são os únicos contratos std140 ativos. | **Removido (V2 único)** |
| QML óptico | `src/qml/ValoGlass.qml` removido. `WaylandWindowContainer.qml` e toda a shell utilizam `ValoMaterialSurfaceV2` / `ValoGlassItemV2`. | **Removido (V2 único)** |
| Adaptadores QML públicos | `SystaliaFilesWindow.qml`, `SystaliaControlCenter.qml`, `SystaliaDock.qml` e `SystaliaTopBar.qml` permanecem como adaptadores sobre implementações V2. | **Compatibilidade pública V2** |
| QML compartilhado | `SystaliaTheme.qml`, `ValoTheme.qml` e `SystaliaG2Frame.qml` preservados sem referências a tipos V1. | **Compartilhado V2** |
| GLSL, qrc e build | `shaders/valo_optical.vert` e `shaders/valo_optical.frag` removidos de `cmake/SgeShaders.cmake`. Restam 25 `.qsb` exclusivamente V2/diagnósticos. | **Removido (V2 único)** |
| Testes | `tst_SystaliaEngine`, `tst_ItemGeometry`, `tst_BackdropBinding`, `tst_UnifiedComposition`, `tst_MemoryAlignment`, `tst_SurfacePropsAlignmentV2`, `tst_SdfGeometryMath`, `tst_OutputManager`, `tst_Phase4Audit`, `tst_ResourceLeakAudit`, `tst_ValoGlass`, `tst_ValoDecorations`, `tst_ValoShellV2` e `tst_ShellStateRegression` migrados para V2. | **Migrados para V2 (48/48 CTest)** |
| Protocolo e clientes | `protocols/z_systalia_surface_v1.xml`, `ShellSurfaceItem` e infraestrutura Wayland/XWayland preservados integralmente. | **Preservado** |
