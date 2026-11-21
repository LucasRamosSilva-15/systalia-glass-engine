# Consolidação 06.3 — Dependências restantes do VALO V1

**Estado: PARTIAL — redução segura de dependências C++, QML/ABI e testes concluída; remoção de `ValoGlass.qml`, `decorationV1`, `SystaliaGlassItem`, `SystaliaGlassMaterial` e shaders legados ainda não autorizada.**  
**Base:** `valo-graphics-2.0` a partir de `b1b3382` (após Consolidação 06.2).  
**Commits desta etapa:** `5588d61`, `8a94012`, `8eb40b6` (+ commit de documentação/evidências).

---

## 1. Resumo Executivo e Inventário Atualizado

Após a migração dos adaptadores QML da shell (`SystaliaDock`, `SystaliaTopBar`, `SystaliaFilesWindow` e `SystaliaControlCenter`) nas etapas 06.1 e 06.2, foi refeito o inventário completo de referências aos símbolos e recursos V1 (`ValoGlass`, `SystaliaGlassItem`, `SystaliaGlassMaterial`, `SystaliaSurfaceProps`, `SystaliaRenderUniforms`, `valo_optical.vert`/`.frag`, `decorationV1` e `useGraphicsV2`) em `src/`, `tests/`, `tools/`, `CMakeLists.txt` e `cmake/`.

As dependências restantes foram classificadas em **efetivamente ativas/compatibilidade obrigatória** versus **redundantes ou migradas de forma segura**:

### 1.1 Dependências eliminadas nesta etapa (por família)

1. **Contrato de dados V2 → V1 (`5588d61`):**
   - `src/gfx/v2/SystaliaSurfacePropsV2.hpp` incluía transitivamente `SystaliaSurfaceProps.hpp` apenas para obter `Matrix4x4_std140`.
   - Substituído por `#include "ValoStd140Matrix.hpp"`. Toda a pilha C++ V2 (`ValoGlassItemV2`, `ValoGlassMaterialV2`, `ValoGpuBlurPipelineV2`, testes e ferramentas V2) deixou de depender em tempo de compilação do cabeçalho V1.
   - `SystaliaSurfaceProps.hpp` (128 bytes / `SystaliaRenderUniforms` 144 bytes) permanece intacto para os testes de ABI e para o renderer V1.

2. **Registros QML redundantes de `SystaliaGlassItem` (`8a94012`):**
   - `SystaliaGlassItem.hpp` já declara `QML_ELEMENT` e é registrado automaticamente por `qml_register_types_Systalia_Engine()` (em `sge_core_qmltyperegistrations.cpp`).
   - Removido o registro manual redundante `qmlRegisterType<SystaliaGlassItem>("Systalia.Engine", 1, 0, "SystaliaGlassItem")` e os `#include "SystaliaGlassItem.hpp"` não utilizados em:
     - `src/app/main.cpp`
     - `tests/tst_XdgResize.cpp`
     - `tests/integration/tst_ClientSurfaceVisibility.cpp`
     - `tests/e2e/tst_WaylandClientLifecycleE2E.cpp`
     - `tests/integration/tst_XdgShellDecoration.cpp`
   - Removido apenas o `qmlRegisterType<SystaliaGlassItem>` redundante (preservando o `#include` para inspeção de filhos / anti-regressão) em:
     - `tests/integration/tst_ShellStateRegression.cpp` (asserção `findChildren<SystaliaGlassItem *>().size() == 0`)
     - `tests/unit/tst_ValoDecorations.cpp` (teste explícito do fallback `decorationV1`)

3. **Teste de protocolo Wayland (`8eb40b6`):**
   - `tests/tst_SystaliaProtocol.cpp` (`qmlItemTracksMetadataChanges`) instanciava `SystaliaGlassItem` apenas para validar bindings QML dos metadados de `z_systalia_surface_v1`.
   - Migrado para `ValoGlassItemV2` (com `cornerRadii: metadata.radii`) usando `qml_register_types_Systalia_Engine()`, preservando integralmente a cobertura do protocolo.

---

## 2. Inventário Restante de Referências V1 (Mantidas por Segurança)

| Família / Arquivo | Tipo de Uso | Motivo da Preservação |
|---|---|---|
| `src/qml/WaylandWindowContainer.qml` (`decorationV1`, `useGraphicsV2`) e `src/qml/ValoGlass.qml` | **Uso ativo (fallback)** | Fallback operacional quando `useGraphicsV2 == false`. Não pode ser removido antes da homologação integral de clientes Wayland e XWayland em V2. |
| `src/diagnostics/ValoStabilityProbe.hpp` | **Uso ativo (sonda)** | Alterna `useGraphicsV2 = false` / `true` para validar o fallback e a transição V1→V2 em sessão real. |
| `src/gfx/SystaliaGlassItem.*`, `src/gfx/SystaliaGlassMaterial.*`, `src/gfx/SystaliaSurfaceProps.hpp` | **Implementação V1 e ABI** | Sustentam `ValoGlass.qml` (`decorationV1`) e os contratos ABI 128/144 bytes verificados em CTest. |
| `shaders/valo_optical.vert`, `shaders/valo_optical.frag`, `cmake/SgeShaders.cmake` | **Recursos QRC ativos** | Carregados por `SystaliaGlassMaterial` via `:/shaders/valo_optical.*.qsb` e verificados por `tst_SystaliaEngine`. |
| `tests/unit/tst_ValoGlass.cpp`, `tests/unit/tst_ValoDecorations.cpp`, `tests/integration/tst_ShellStateRegression.cpp` | **Cobertura V1 / fallback / anti-regressão** | Verificam a API pública de `ValoGlass.qml`, a alternância `decorationV1`/`decorationV2` e a ausência de `SystaliaGlassItem` em superfícies V2. |
| `tests/tst_ItemGeometry.cpp`, `tests/tst_BackdropBinding.cpp`, `tests/tst_SystaliaEngine.cpp`, `tests/tst_UnifiedComposition.cpp`, `tests/unit/tst_MemoryAlignment.cpp`, `tests/unit/tst_OutputManager.cpp`, `tests/unit/tst_Phase4Audit.cpp`, `tests/unit/tst_ResourceLeakAudit.cpp` | **Testes unitários e ABI V1** | Cobrem geometria, culling de oclusão, leak audit, QSBs e `static_assert`s de 128/144 bytes enquanto as classes V1 existirem. |
| `src/qml/shell/SystaliaG2Frame.qml` (linha 7), `src/gfx/v2/SystaliaSurfacePropsV2.hpp` (linha 29), `tests/unit/tst_SurfacePropsAlignmentV2.cpp` | **Apenas comentário** | Referências documentais sem dependência de código ou import. |

---

## 3. Evidências de Validação

1. **Build limpo Debug com `-Wall -Wextra -Werror`:**
   - Diretório isolado `/tmp/sge-build-debug-063`, GCC 16.2.1, Qt 6.11.2, Ninja.
   - Compilação concluída sem avisos ou erros.
   - **CTest completo (offscreen):** **47/47 testes aprovados** (50,52 s).

2. **Build limpo Release com `-Wall -Wextra -Werror`:**
   - Diretório isolado `/tmp/sge-build-release-063`, GCC 16.2.1, Qt 6.11.2, Ninja.
   - Compilação concluída sem avisos ou erros.
   - **CTest completo (offscreen):** **47/47 testes aprovados** (48,96 s).

3. **Verificação QRC (Build Release):**
   - Manifestos gerados em `.qt/rcc/` confirmam empacotamento íntegro de `sge_core_raw_qml_0.qrc` (incluindo `ValoGlass.qml` e adaptadores V2), `sge_shaders_resource.qrc` (`:/shaders/valo_optical.*.qsb`) e `sge_shaders_v2_resource.qrc` (todos os shaders V2 e diagnósticos).
   - `tst_SystaliaEngine` e todos os testes QRC passaram em Debug e Release.

4. **Execução real com Alacritty Wayland (Binário Release):**
   - Executado `/tmp/sge-build-release-063/systalia_glass_engine` em sessão Wayland real (`WAYLAND_DISPLAY=wayland-1`) com `SGE_VALO_CLIENT_MIGRATION_PROBE=1` e `alacritty` (`WINIT_UNIX_BACKEND=wayland`).
   - Saída do compositor: `SDE exit: 0`.
   - Evidências salvas em [`evidence-06-3/`](evidence-06-3/):
     - [`01-v1-fallback.png`](evidence-06-3/01-v1-fallback.png) (`v2: false`, `xwayland: false`) — Alacritty Wayland visível no fallback `decorationV1`.
     - [`02-v2.png`](evidence-06-3/02-v2.png) (`v2: true`, `xwayland: false`) — Alacritty Wayland visível e decorado em `decorationV2`.

---

## 4. Limitações, Riscos e Próximos Bloqueadores

- **Limitação registrada (XWayland):** A execução real validou **Alacritty Wayland nativo** (`xwayland: false`). A renderização de conteúdo em clientes XWayland reais (problema histórico de conteúdo preto registrado no ADR 0001 e no `PLANO_E_INVENTARIO.md`) **não foi homologada** nesta etapa e permanece como limitação conhecida.
- **V2 ainda não é renderer único:** `decorationV1` (`ValoGlass.qml` + `SystaliaGlassItem`) e os shaders `shaders/valo_optical.*` continuam ativos no módulo e no binário para sustentar o fallback `useGraphicsV2 = false`.
- **Próximos bloqueadores para remoção definitiva da V1:**
  1. Homologação de clientes XWayland reais e múltiplos monitores em `decorationV2`.
  2. Definição formal de descontinuação da API pública de `SystaliaGlassItem`, `ValoGlass.qml` e `useGraphicsV2` para consumidores externos.
  3. Migração ou aposentadoria explícita dos testes unitários específicos da implementação V1 (`tst_ValoGlass`, `tst_Phase4Audit`, `tst_ResourceLeakAudit`, `tst_ItemGeometry`, `tst_BackdropBinding`) somente após a retirada de `decorationV1`.
