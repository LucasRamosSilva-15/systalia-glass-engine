# ADR 0003 — Aposentadoria de `decorationV1` em `WaylandWindowContainer`

- **Estado:** aceita e implementada na Consolidação 06.5
- **Data:** 2026-11-21
- **Contexto:** As Consolidações 06.1–06.4 migraram os componentes da shell para V2, reduziram dependências redundantes do renderer V1 e comprovaram paridade funcional de `decorationV2` frente a `decorationV1` em clientes Wayland (Alacritty, GTK3, Qt6) e XWayland modernos (GTK3 X11, Qt6 XCB, Alacritty X11). A Alpha pública do SYSTALIA ainda não foi lançada.

## Decisão

1. **Aposentar `decorationV1` e o `Loader` condicional em `WaylandWindowContainer.qml`:** A decoração de janelas de clientes passa a instanciar diretamente `ValoMaterialSurfaceV2` (`clientDecorationV2`).
2. **Descontinuar a escrita de `useGraphicsV2`:** `WaylandWindowContainer.qml` mantém `readonly property bool useGraphicsV2: true` exclusivamente para compatibilidade de leitura em sondas/observadores, sem suporte a fallback para `ValoGlass`.
3. **Preservar temporariamente `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial` e `shaders/valo_optical.*`:** Como ainda existem testes unitários e de ABI acoplados à implementação C++ V1 (`tst_ValoGlass`, `tst_ItemGeometry`, `tst_BackdropBinding`, `tst_SystaliaEngine`, `tst_UnifiedComposition`, `tst_MemoryAlignment`, `tst_OutputManager`, `tst_Phase4Audit`, `tst_ResourceLeakAudit`), a remoção física dos arquivos V1 ocorrerá somente após a migração ou aposentadoria explícita desses consumidores.

## Consequências

- `WaylandWindowContainer.qml` deixa de ser consumidor ativo de `ValoGlass.qml` e elimina o `Loader` condicional V1/V2.
- `ValoStabilityProbe.hpp` e `tst_ValoDecorations.cpp` passam a validar exclusivamente a decoração oficial V2 em `WaylandWindowContainer`.
- A V2 ainda **não** é declarada renderer único do repositório enquanto `ValoGlass.qml`, `SystaliaGlassItem` e `shaders/valo_optical.*` permanecerem compilados e testados.
