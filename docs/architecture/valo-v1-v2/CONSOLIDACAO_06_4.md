# Consolidação 06.4 — Homologação de clientes e preparação da aposentadoria de `decorationV1`

**Estado: PARTIAL — paridade funcional de `decorationV2` comprovada em clientes Wayland e XWayland modernos; `decorationV1` mantida por critérios formais de compatibilidade e cobertura pendente de multi-monitor físico/DPR.**  
**Base:** `valo-graphics-2.0` em `e08926a` (após Consolidação 06.3).

---

## 1. Matriz de Clientes e Cenários Testados

Todos os testes reais abaixo foram executados com o binário Release limpo (`/tmp/sge-build-release-063/systalia_glass_engine`, `-Werror`) em sessão Wayland real (`WAYLAND_DISPLAY=wayland-1`), comparando `decorationV1` (`useGraphicsV2 = false`) e `decorationV2` (`useGraphicsV2 = true`) via `ValoStabilityProbe` (`SGE_VALO_CLIENT_MIGRATION_PROBE=1`).

| Cliente / Toolkit | Backend | `decorationV1` | `decorationV2` | Cenários Exercitados | Evidência |
|---|---|---|---|---|---|
| **Alacritty** | Wayland nativo (`WINIT_UNIX_BACKEND=wayland`) | Aprovado | Aprovado | Mapeamento, conteúdo terminal, resize, maximização, snap, minimização, workspaces, overlays, fechamento | [`evidence-06-3/`](evidence-06-3/) |
| **GTK3 (`gtk3-demo`)** | Wayland nativo (`GDK_BACKEND=wayland`) | Aprovado | Aprovado | Mapeamento, UI completa visível, resize, maximização, snap, minimização, workspaces, fechamento | [`evidence-06-4/gtk-wayland/`](evidence-06-4/gtk-wayland/) |
| **Qt6 (`kdialog`)** | Wayland nativo (`QT_QPA_PLATFORM=wayland`) | Aprovado | Aprovado | Mapeamento, texto/ícone/botão visíveis, resize, maximização, snap, minimização, workspaces, fechamento | [`evidence-06-4/qt-wayland/`](evidence-06-4/qt-wayland/) |
| **GTK3 (`gtk3-demo`)** | X11 via XWayland (`DISPLAY=:91`, `GDK_BACKEND=x11`) | Aprovado | Aprovado | Mapeamento (`xwayland: true`), UI completa, maximização (`03-maximized.png`), snap (`05-snap.png`), workspaces, fechamento | [`evidence-06-4/xwayland-repro/`](evidence-06-4/xwayland-repro/) |
| **Qt6 (`kdialog`)** | X11/XCB via XWayland (`DISPLAY=:91`, `QT_QPA_PLATFORM=xcb`) | Aprovado | Aprovado | Mapeamento (`xwayland: true`), diálogo visível (`VALO Qt X11 Test`), ciclo completo | [`evidence-06-4/xwayland-qt-xcb/`](evidence-06-4/xwayland-qt-xcb/) |
| **Alacritty** | X11/GLX via XWayland (`DISPLAY=:91`, `WINIT_UNIX_BACKEND=x11`) | Aprovado | Aprovado | Mapeamento (`xwayland: true`), prompt `sh-5.3$` e cursor visíveis em V1 e V2 | [`evidence-06-4/xwayland-alacritty/`](evidence-06-4/xwayland-alacritty/) |
| **Xt/Xaw (`xmessage -bg white -fg black`)** | X11 legado via XWayland (`DISPLAY=:91`) | Fundo branco sem texto/botão | Fundo branco sem texto/botão | Mapeamento (`xwayland: true`); buffer `#FFFFFF` importado, porém widgets filhos Xt não redesenhados (idêntico em V1 e V2) | [`evidence-06-4/xwayland-xmessage/`](evidence-06-4/xwayland-xmessage/) |

### Cenários cobertos por CTest e limitações de ambiente

- **Popups, subsurfaces e resize:** cobertos por `tst_xdg_resize` (`popupIsPresentedOutsideTheCapturedClient`, `configureHandshake`, `dragEdges`, limites min/max), `tst_xdg_shell_decoration` e `tst_client_surface_visibility` (**47/47** aprovados em Debug e Release com `-Werror`).
- **Escala fracionária (DPR) e múltiplos outputs:** lógica de roteamento e migração hot-unplug coberta por `tst_output_manager`; **não executada** em hardware multimonitor físico ou escalas mistas nesta sessão (ambiente aninhado com 1 output lógico).

---

## 2. Diagnóstico do Conteúdo Preto em Clientes XWayland

A investigação reproduziu clientes X11 reais conectados ao servidor XWayland embutido (`Xwayland -rootless` em `DISPLAY=:91` via [`XWaylandManager.cpp`](../../../src/compositor/XWaylandManager.cpp)):

1. **Separação entre renderer óptico e apresentação de buffers:**
   - Em [`WaylandWindowContainer.qml`](../../../src/qml/WaylandWindowContainer.qml), o `ShellSurfaceItem` (`clientViewport` em `z: 1`) permanece **fora e acima** do `Loader` que alterna `decorationV1` e `decorationV2` (`z: 0`). A textura do cliente não passa pelos shaders V1 nem V2.
2. **Clientes XWayland modernos (GTK3 X11, Qt6 XCB e Alacritty X11/OpenGL):**
   - Todos apresentaram conteúdo completo e nítido em **ambas** as decorações (`decorationV1` e `decorationV2`), comprovando que a importação de buffers SHM e acelerados (DRI3/Glamor) via `WL_SURFACE_ID` e `XCB_COMPOSITE_REDIRECT_MANUAL` está funcional no `XWaylandManager`.
3. **Causa do sintoma histórico em `xmessage` (Xt/Xaw):**
   - Ao executar `xmessage -bg white -fg black "VALO X11 TEST"`, a janela exibiu um retângulo sólido `#FFFFFF` (cor de fundo `-bg white`) sem texto nem botão, tanto em `decorationV1` quanto em `decorationV2` (e antes da troca de decoração em `00-native-default.png`).
   - Isso demonstra que: (a) o buffer XWayland **está** sendo importado e composto pelo `ShellSurfaceItem` (sem fundo preto forçado pelo renderer); e (b) a ausência dos widgets filhos/texto em `xmessage` decorre do tratamento de sub-janelas X11 / eventos `Expose` em clientes Xt/Xaw legados sob `Xwayland -rootless`, sendo **100% independente do renderer óptico V1 ou V2**.

---

## 3. Correções Implementadas e Commits

Nenhuma alteração invasiva foi necessária no compositor ou nos renderers ópticos nesta fase, pois:
- A paridade funcional de `decorationV2` foi comprovada em todos os clientes Wayland e XWayland modernos testados.
- A limitação observada em `xmessage` (Xt/Xaw) é idêntica em V1 e V2 e pertence à camada X11/Xt, não ao motor óptico.

---

## 4. Evidências de Renderização Real e Verificações

- **Builds limpos com `-Werror` e CTest:** mantidos íntegros os builds Debug (`/tmp/sge-build-debug-063`) e Release (`/tmp/sge-build-release-063`) sobre `e08926a`, ambos com **47/47** testes aprovados.
- **Ciclo de vida e limpeza de recursos:** todos os cenários da sonda finalizaram com `SDE exit: 0`, desmontagem limpa dos containers (`windows: 0`, `xManaged: 0` após fechamento) e cobertura preservada em `tst_resource_leak_audit` e `tst_wayland_client_lifecycle_e2e`.
- **Comparação V1 vs V2:** capturas `01-v1-fallback.png` e `02-v2.png` disponíveis em cada subdiretório de [`evidence-06-4/`](evidence-06-4/) (`xwayland-repro`, `xwayland-alacritty`, `xwayland-qt-xcb`, `xwayland-xmessage`, `gtk-wayland`, `qt-wayland`).

---

## 5. Riscos e Incompatibilidades Públicas

1. **Propriedade pública `useGraphicsV2` e `decorationV1`:** `WaylandWindowContainer.qml` e `ValoStabilityProbe.hpp` ainda expõem `useGraphicsV2` e alternam para `ValoGlass.qml`.
2. **Testes acoplados à V1:** `tst_ValoDecorations` e `tst_ValoGlass` testam explicitamente `decorationV1` e `ValoGlass.qml`.
3. **Cobertura física pendente:** validação manual com múltiplos monitores físicos e DPR heterogêneo não pôde ser executada em ambiente aninhado de único output.

---

## 6. Decisão Fundamentada sobre Aposentadoria de `decorationV1`

**Decisão:** **NÃO remover `decorationV1` nem declarar a V2 como renderer único neste commit.**

Embora `decorationV2` tenha demonstrado paridade funcional e visual completa frente a `decorationV1` em clientes Wayland (Alacritty, GTK3, Qt6) e XWayland modernos (GTK3 X11, Qt6 XCB, Alacritty X11), a retirada física de `decorationV1` e da pilha V1 fica condicionada aos seguintes **critérios objetivos de saída**:

1. Decisão explícita de descontinuação da propriedade `useGraphicsV2` e do tipo público `ValoGlass.qml` / `SystaliaGlassItem`.
2. Validação em sessão física com múltiplos monitores/DPR misto.
3. Atualização coordenada de `ValoStabilityProbe.hpp`, `tst_ValoDecorations.cpp` e `tst_ValoGlass.cpp` no momento da remoção definitiva.
