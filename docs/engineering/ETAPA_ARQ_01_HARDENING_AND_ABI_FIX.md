# SYSTALIA — ETAPA ARQ-01 (Etapa 1 do Blueprint de Arquitetura Limpa)
## Blindagem de Segurança (Ciclo de Vida/UAF), Correção do Shader de Sombra (`ARQ-02`) e Alinhamento Semântico do UBO `std140` (`ARQ-10`)

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Data:** 2026-11-28  
**Status:** Concluída (`61/61` CTest com `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, build incremental `29.39 s`, smoke test nested ao vivo `RC=0`)

---

## 1. Blindagem de Ciclo de Vida e Ponteiros (`ARQ-01`)

- **`src/compositor/waylib/WaylibSurfaceAdapter.{hpp,cpp}`:**
  - Removida a captura de ponteiro bruto `[display]` em `QTimer::singleShot(0, ...)` dentro de `setSurfaceMaximized` e `resizeSurface`.
  - Adicionados `attachServer(WServer *server)`, `QPointer<WServer> m_server`, `QPointer<WXdgShell> m_xdgShell` e `quint64 m_flushGeneration`.
  - Centralizado o despacho assíncrono em `scheduleDisplayFlush()`, que valida `generation == m_flushGeneration`, `safeServer` (`QPointer<WServer>`), `safeServer->isRunning()` e `safeServer->handle()` antes de chamar `wl_event_loop_dispatch` e `wl_display_flush_clients`.
  - Em `WaylibSurfaceAdapter::reset()`, `++m_flushGeneration` invalida imediatamente qualquer callback pendente e desconecta `m_xdgShell`.
- **`src/compositor/waylib/WaylibCompositorAdapter.{hpp,cpp}`:**
  - Ponteiros não-proprietários (`m_server`, `m_outputAdapter`, `m_surfaceAdapter`, `m_cursor`, `m_seat`, `m_backend`, `m_xdgShell`, `m_xdgDecorationManager`, `m_socket`) convertidos para `QPointer<...>`.
  - `shutdown()` verifica `m_surfaceAdapter` e `m_outputAdapter` antes do `reset()` e limpa `m_socket`, `m_xdgDecorationManager`, `m_xdgShell` e `m_backend`.
- **`src/compositor/waylib/WaylibOutputAdapter.{hpp,cpp}`:**
  - `m_outputs` convertido para `QList<QPointer<WOutput>>` e `RequestStateListener` atualizado com `QPointer<WaylibOutputAdapter>` e `QPointer<WOutput>`.
  - `removeRequestStateListener()` executa `wl_list_remove` seguido de `wl_list_init` antes de deletar a entrada, prevenindo remoção dupla.

---

## 2. Correção do Fragment Shader de Sombra (`ARQ-02`)

- **`shaders/v2/valo_shadow_v2.frag`:**
  - Substituído `float offsetY = (u_ShadowOffsetY > 0.0) ? u_ShadowOffsetY : 12.0;` por `float offsetY = max(u_ShadowOffsetY, 0.0);`.
  - Substituído `float opacity = (u_ShadowIntensity > 0.0) ? u_ShadowIntensity : 0.35;` por `float opacity = max(u_ShadowIntensity, 0.0);` com retorno antecipado `fragColor = vec4(0.0); return;` quando `opacity <= 0.0001`.
  - Os valores padrão (`m_shadowOffsetY = 12.0f`, `m_shadowIntensity = 0.35f`) continuam preservados na inicialização C++ de `ValoShadowItemV2` e `SystaliaSurfacePropsV2`, mantendo o visual padrão inalterado enquanto `shadowIntensity: 0.0` (modo `highContrast` ou `opacity == 0`) produz alpha estritamente nulo.

---

## 3. Alinhamento Semântico do UBO `std140` (`ARQ-10`)

- **`src/graphics/v2/core/SystaliaSurfacePropsV2.hpp`:**
  - Mantido o tamanho exato de **160 bytes** e alinhamento `std140` de **16 bytes**.
  - Offset **`124`** renomeado de `titleBarHeight` para `float inheritedOpacity = 1.0f;` (eliminando a sobrecarga semântica anterior). `SystaliaDefaultTitleBarHeight = 36.0f` permanece em `SystaliaClientClip.hpp` para cálculos de geometria CPU.
  - Mapa atualizado: `matrix` (`0`), `radii` (`64`), `windowSize` (`80`), `screenResolution` (`88`), `slant` (`96`), `ior` (`100`), `chromaticDispersion` (`104`), `causticIntensity` (`108`), `surfaceOpacity` (`112`), `time` (`116`), `hasClientTexture` (`120`), **`inheritedOpacity` (`124`)**, `specularStrokeWidth` (`128`), `geometryMode` (`132`), `shadowIntensity` (`136`), `shadowOffsetY` (`140`), `volumetricTint` (`144`).
- **`ValoGlassMaterialV2.cpp`, `ValoShadowItemV2.cpp` e Shaders `shaders/v2/*.{vert,frag}`:**
  - `ValoGlassMaterialV2` e `ValoShadowItemV2` gravam `props.inheritedOpacity = state.opacity();`.
  - Todos os shaders V2 e diagnósticos (`valo_shadow_v2.{vert,frag}`, `valo_diag_{blur,mask,refraction,sdf,specular,volume}.frag`) unificados para declarar `float u_InheritedOpacity;` no offset `124`.

---

## 4. Testes, Build Incremental e Smoke Test

- **Novos casos e atualizações de teste:**
  - `tests/compositor/tst_WaylibBackend.cpp`: aciona `setSurfaceMaximized` + `resizeSurface` imediatamente antes de `adapter.shutdown()` e executa `QCoreApplication::processEvents()` pós-shutdown (`ARQ-01`).
  - `tests/unit/tst_ValoGlassRenderingV2.cpp`: adicionado `shadowZeroIntensityProducesTransparentOutput()` validando `maxAlphaZero == 0` com `shadowIntensity = 0.0f` e `maxAlphaActive > 20` com `0.35f` (`ARQ-02`).
  - `tests/unit/tst_SurfacePropsAlignmentV2.cpp`, `tst_MemoryAlignment.cpp`, `tst_SystaliaEngine.cpp`, `tst_UnifiedComposition.cpp`: atualizados para validar `offsetof(SystaliaSurfacePropsV2, inheritedOpacity) == 124` (`1.0f`) (`ARQ-10`).
- **Build incremental (`build/waylib`, `Release`, `-Werror`, `-DSGE_ENABLE_WAYLIB_BACKEND=ON`):** **`29.39 s`** (`real 29.39`), 0 warnings/erros.
- **Testes direcionados (`8/8` aprovados, `7.22 s`):** `tst_systalia_engine`, `tst_unified_composition`, `tst_memory_alignment`, `tst_surface_props_alignment_v2`, `tst_valo_glass_shader_v2`, `tst_valo_glass_rendering_v2`, `tst_valo_shell_v2`, `tst_waylib_backend`.
- **Suíte completa CTest:** **`61/61` aprovados (`100% tests passed, 0 tests failed out of 61`, `41.64 s`)**.
- **Smoke test nested ao vivo (`./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500`):** **`RC=0`**, `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760`, `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!`, formato `XR24` em `WL-1`.

---

## 5. Prontidão para a Etapa 2 do Blueprint

Com a blindagem de ciclo de vida (`ARQ-01`), correção da sombra `0.0` (`ARQ-02`) e alinhamento semântico do UBO `std140` (`ARQ-10`) validados em 61/61 testes, o projeto está pronto para a **Etapa 2 do Blueprint (`sge_tokens`, remoção de `animTimer` ocioso e migração `.png` → `.svg`)**.
