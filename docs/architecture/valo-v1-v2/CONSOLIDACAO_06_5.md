# Consolidação 06.5 — Aposentadoria de `decorationV1`

**Estado da Etapa 06.5:** **COMPLETE** (aposentadoria de `decorationV1` concluída e validada).  
**Estado Geral da Migração VALO V1 → V2:** **PARTIAL** (`ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial`, `SystaliaSurfaceProps.hpp` e `shaders/valo_optical.*` permanecem compilados e cobertos por testes unitários/ABI).  
**Base:** `valo-graphics-2.0` a partir de `2e07244` (após Consolidação 06.4).  
**Commits desta etapa:** `7653ed6` (ADR 0003), `20ff507` (remoção de `decorationV1` e migração da sonda/teste) + commit de documentação/evidências.

---

## 1. Decisão de Compatibilidade ([ADR 0003](../adr/0003-aposentadoria-decoration-v1.md))

- **Inventário público:** `useGraphicsV2` era alternada apenas por `WaylandWindowContainer.qml`, `ValoStabilityProbe.hpp` e `tst_ValoDecorations.cpp`. `ValoGlass.qml` era instanciado em produção apenas por `decorationV1`.
- **Decisão:** Como a Alpha pública ainda não foi lançada e `decorationV2` comprovou paridade funcional na etapa 06.4, `decorationV1` e o `Loader` condicional foram removidos de [`WaylandWindowContainer.qml`](../../../src/qml/WaylandWindowContainer.qml). `useGraphicsV2` permanece como `readonly property bool useGraphicsV2: true` para compatibilidade de leitura.
- **Preservação de infraestrutura V1:** `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial`, `SystaliaSurfaceProps.hpp` e `shaders/valo_optical.*` foram preservados porque ainda sustentam cobertura de testes unitários e de ABI.

---

## 2. Alterações Implementadas

- **`src/qml/WaylandWindowContainer.qml` (`20ff507`):**
  - Removidos o `Loader` condicional, o componente `decorationV1` (`ValoGlass`) e o wrapper `Component { id: decorationV2 }`.
  - `ValoMaterialSurfaceV2` (`id: clientDecorationV2`, `objectName: "clientDecorationV2"`) é agora instanciado diretamente como decoração única de janelas de clientes.
  - `property bool useGraphicsV2: true` convertido para `readonly property bool useGraphicsV2: true`.
  - Preservados integralmente `ShellSurfaceItem`, apresentação direta de buffers, subsurfaces/popups, foco, resize, snap e ciclo de vida.
- **`src/diagnostics/ValoStabilityProbe.hpp` (`20ff507`):**
  - Removida a alternância `w.useGraphicsV2 = false` / `true` no passo `4000 ms` (`01-resized`), passando a validar exclusivamente a decoração oficial V2.
- **`tests/unit/tst_ValoDecorations.cpp` (`20ff507`):**
  - `windowContainerIntegratesValoGlass()` atualizado para verificar `clientDecorationV2` (`ValoGlassItemV2` em `SquircleG2`), ausência de `SystaliaGlassItem` e `clientDecorationV1`, e `useGraphicsV2 === true`, preservando os demais testes de `ValoGlass` sem redução de cobertura.

---

## 3. Matriz de Aposentadoria Atualizada

| Família | V1 no módulo | Implementação ativa V2 | Estado | Próximo passo para remoção |
|---|---|---|---|---|
| **Dock** | `src/qml/shell/SystaliaDock.qml` | `SystaliaDockV2.qml` | **ADAPTADOR V2** | Validar consumidores externos / remover alias após política de deprecação |
| **TopBar** | `src/qml/shell/SystaliaTopBar.qml` | `SystaliaTopBarV2.qml` | **ADAPTADOR V2** | Validar consumidores externos / remover alias após política de deprecação |
| **Files** | `src/qml/shell/SystaliaFilesWindow.qml` | `SystaliaFilesWindowV2.qml` | **ADAPTADOR V2** | Paridade de parâmetros ópticos públicos / deprecação formal |
| **Control Center** | `src/qml/shell/SystaliaControlCenter.qml` | `SystaliaControlCenterV2.qml` | **ADAPTADOR V2** | Homologação completa de hardware / deprecação formal |
| **Decoração de clientes** | `decorationV1` (removida) | `ValoMaterialSurfaceV2` (`clientDecorationV2`) | **APOSENTADA (V2 ÚNICA)** | — |
| **Renderer C++ e QML V1** | `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial`, `shaders/valo_optical.*` | `ValoGlassItemV2`, `ValoGlassMaterialV2`, `shaders/v2/*` | **PRESERVADO (SEM USO EM SHELL/CLIENTES)** | Migrar ou aposentar testes V1 (`tst_ValoGlass`, `tst_ItemGeometry`, `tst_BackdropBinding`, `tst_Phase4Audit`, `tst_ResourceLeakAudit`, `tst_SystaliaEngine`, `tst_MemoryAlignment`, `tst_OutputManager`) |

### Arquivos efetivamente removíveis em fase futura (após decisão sobre testes e ABI V1)
- `src/qml/ValoGlass.qml` (zero consumidores em `src/` após `20ff507`; restam apenas `tst_ValoGlass.cpp` e `tst_ValoDecorations.cpp`).
- `src/gfx/SystaliaGlassItem.hpp`, `src/gfx/SystaliaGlassItem.cpp`.
- `src/gfx/SystaliaGlassMaterial.hpp`, `src/gfx/SystaliaGlassMaterial.cpp`.
- `shaders/valo_optical.vert`, `shaders/valo_optical.frag` (e entradas correspondentes em `cmake/SgeShaders.cmake`).
- `src/gfx/SystaliaSurfaceProps.hpp` (condicionado à decisão sobre o contrato ABI legado 128/144 bytes).

---

## 4. Validação Executada

1. **Builds limpos com `-Wall -Wextra -Werror` (GCC 16.2.1, Qt 6.11.2, Ninja):**
   - **Debug** (`/tmp/sge-build-debug-065`): compilação limpa e **CTest 47/47 aprovados** (50,26 s).
   - **Release** (`/tmp/sge-build-release-065`): compilação limpa e **CTest 47/47 aprovados** (49,35 s).
2. **Verificação QRC, QSB e ABI:**
   - **27 arquivos `.qsb`** gerados; manifestos `.qt/rcc/*.qrc` íntegros; `tst_systalia_engine`, `tst_memory_alignment` (128 bytes) e `tst_surface_props_alignment_v2` (160 bytes) aprovados.
3. **Execução real de 6 clientes no SDE Release (`WAYLAND_DISPLAY=wayland-1`, decoração V2 única):**
   - **Wayland nativo:** `alacritty-wayland`, `gtk3-wayland` (`gtk3-demo`), `qt6-wayland` (`kdialog`) — todos `SDE exit 0`, conteúdo visível em [`evidence-06-5/`](evidence-06-5/).
   - **XWayland (`DISPLAY=:91`):** `gtk3-x11` (`gtk3-demo`), `qt6-xcb` (`kdialog`), `alacritty-x11` (`alacritty`) — todos `SDE exit 0`, conteúdo visível em [`evidence-06-5/`](evidence-06-5/).
   - **Comparação visual com 06.4:** as capturas `02-v2.png` de `evidence-06-5/` mantêm a mesma apresentação nítida de cliente e chassi `ValoMaterialSurfaceV2` observada em `evidence-06-4/`.

---

## 5. Fatos Comprovados vs. Hipóteses e Limitações Registradas

- **Fatos comprovados (diagnóstico `xmessage` / XWayland):**
  - Clientes XWayland modernos (GTK3 X11, Qt6 XCB, Alacritty X11) exibem conteúdo completo em `decorationV2`.
  - `xmessage -bg white -fg black` exibe `#FFFFFF` (cor de fundo da janela X11) sem texto nem botão de forma idêntica em `decorationV1` (etapa 06.4) e `decorationV2`, provando que o buffer XWayland é importado e que o defeito é independente do renderer óptico.
- **Hipótese (não alterada por escopo):**
  - A ausência dos widgets filhos em `xmessage` está associada ao tratamento de sub-janelas X11 / eventos `Expose` em clientes Xt/Xaw legados sob `Xwayland -rootless`.
- **Limitações de ambiente não cobertas nesta sessão:**
  - Clientes Xt/Xaw legados (`xmessage`), múltiplos monitores físicos e DPR heterogêneo em hardware real (validado apenas em sessão aninhada de único output e em testes unitários de `tst_output_manager`).
