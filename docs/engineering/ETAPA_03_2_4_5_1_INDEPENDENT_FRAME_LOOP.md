# SYSTALIA — Etapa 03.2.4.5.1: Integração do Loop Contínuo de Renderização Vulkan e Atomic KMS

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base:** `e77411e` (com alterações locais das Etapas 03.2.1–03.2.4.5.1 não commitadas)
- **Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, `libdrm`, `QtWaylandCompositor`
- **GPU de referência:** AMD Radeon Cezanne (`/dev/dri/card1`)
- **Estado:** **Loop contínuo sob demanda com backpressure, `DrmEventDispatcher` e callbacks Wayland implementados e validados (53/53 CTest)** *(acoplamento direto à raiz `ApplicationWindow` de `main.qml` auditado e pendente de extração de `ShellRoot.qml` antes da homologação física).*

---

## 1. Auditoria da Arquitetura Atual (Seção 3)

- **Sessão nested/headless:** [`src/app/main.cpp`](../../src/app/main.cpp) carrega [`src/shell/core/main.qml`](../../src/shell/core/main.qml), cujo elemento raiz é `ApplicationWindow` (`QQuickApplicationWindow`), contendo `SystaliaCompositor` (linha 393), `WaylandOutput { window: rootWindow }` (linha 460) e os componentes VALO Graphics V2.
- **Backend experimental (`SGE_USE_DRM_BACKEND=1`):** usa o QPA neutro `sge_vulkan_offscreen` + `VulkanQuickRenderer` (`QQuickWindow` instanciada com `QQuickRenderControl`).
- **Ponto de atenção arquitetural identificado:** como `main.qml` tem raiz `ApplicationWindow`, carregá-lo diretamente via `QQmlApplicationEngine` cria uma `QQuickWindow` própria **sem** `QQuickRenderControl`. Para evitar duas janelas/cenas concorrentes, a integração completa da shell no modo `QQuickRenderControl` requer extrair o conteúdo interno de `main.qml` para um `Item` reutilizável (ex.: `ShellRoot.qml`), preservando `ApplicationWindow` no `main.qml` para `nested`/`headless`.

---

## 2. Componentes Implementados

- [`src/platform/drm/DrmEventDispatcher.hpp`](../../src/platform/drm/DrmEventDispatcher.hpp) e [`.cpp`](../../src/platform/drm/DrmEventDispatcher.cpp):
  - Monitora o FD DRM único via `QSocketNotifier(QSocketNotifier::Read)` sem abrir novo descritor.
  - Despacha `drmHandleEvent()` (`DRM_EVENT_CONTEXT_VERSION` + `page_flip_handler`) no event loop Qt sem bloqueio.
  - Gerencia timeout de flip (`QTimer`) e suporta `detachFd()` / `attachFd(newFd)` em `PauseDevice` / `ResumeDevice`.
- [`src/platform/drm/IndependentFrameLoop.hpp`](../../src/platform/drm/IndependentFrameLoop.hpp) e [`.cpp`](../../src/platform/drm/IndependentFrameLoop.cpp):
  - Loop sob demanda com **backpressure**: múltiplos `requestFrame()` enquanto `m_flipInFlight == true` colapsam em `m_renderPending = true`, sem acumular frames nem usar timers contínuos.
  - Coordena `GbmBufferManager`, `IControlledRenderSource`, `AtomicKmsPresenter` e `WaylandFrameCoordinator`.
- [`tests/unit/tst_IndependentFrameLoop.cpp`](../../tests/unit/tst_IndependentFrameLoop.cpp): suíte dedicada de regressão.

---

## 3. Fluxo Completo de um Frame e Sincronização

1. `requestFrame()` agenda `processNextFrame()` em fila (`Qt::QueuedConnection`) se `!m_flipInFlight && !m_suspended`.
2. `GbmBufferManager::acquireBufferForRendering()` (`Available → Rendering`).
3. `WaylandFrameCoordinator::beginFrame()` (`frameStarted()`).
4. `IControlledRenderSource::renderControlledFrame()` (`polishItems() → beginFrame() → sync() → render() → endFrame()`).
5. `markBufferReadyForScanout()` (`Rendering → ReadyForScanout`).
6. `AtomicKmsPresenter::submitBufferForScanout()` (`ReadyForScanout → PendingFlip`) + `armFlipTimeout()`.
7. Ao receber `pageFlipCompleted` no `DrmEventDispatcher`: `onPageFlipCompleted()` (`PendingFlip → Presented`, liberando buffer anterior para `Available`) e `WaylandFrameCoordinator::completeFrame(true)` (`sendFrameCallbacks()`).
8. Se `m_renderPending` estiver ativo após o flip, agenda o próximo frame.

Falhas de render, commit KMS ou timeout liberam o buffer para `Available` e chamam `completeFrame(false)` **sem** enviar `wl_surface.frame`.

---

## 4. Suspensão e Retomada de VT

- `suspend()`: bloqueia `requestFrame()`, cancela timer de flip, desanexa `DrmEventDispatcher`, libera eventual buffer `in-flight` para `Available`, suspende `WaylandFrameCoordinator` e `AtomicKmsPresenter`.
- `resume(int drmFd)`: reativa `AtomicKmsPresenter`, reanexa `DrmEventDispatcher` ao novo FD, reabilita `WaylandFrameCoordinator` e solicita um novo frame.
- Testado com reordenação `PauseDevice` → `PropertiesChanged("Active": true)` (mantém suspenso) → `ResumeDevice(DRM)` com substituição de FD.

---

## 5. Resultados Reais dos Testes (após a última edição)

1. **Build Release (`-Werror`):** compilada sem erros nem avisos.
2. **Testes específicos primeiro:** `tst_independent_frame_loop`, `tst_independent_vulkan_session`, `tst_vulkan_quick_renderer`, `tst_drm_gbm_integration`, `tst_drm_backend_poc` — **5/5 aprovados** (incluindo `tst_independent_frame_loop` em `0.15 s` sobre `QT_QPA_PLATFORM=sge_vulkan_offscreen`, com readback real em `AMD Radeon Graphics (RADV RENOIR)` verificando pixels `#14304d` e `#00d4ff`).
3. **PoC sem compositor hospedeiro (`env -u WAYLAND_DISPLAY -u DISPLAY QT_QPA_PLATFORM=sge_vulkan_offscreen`):** `sge_vulkan_quick_poc` Fases A–E aprovadas (`exit 0`).
4. **Suíte completa CTest (`QT_QPA_PLATFORM=offscreen` + testes `sge_vulkan_offscreen`):** **53/53 aprovados** (`50.21 s`, `0` falhas — evolução de `52/52` para `53/53`).
5. **Smoke tests (`systalia-session`, cwd `/tmp`, saída por `SIGTERM`):** headless (`offscreen`) e nested Wayland (`wayland`) com `sigterm_rc=0`.

---

## 6. Riscos, Limitações e Critérios para Homologação Física

- **Limitações atuais:**
  - `main.qml` ainda possui raiz `ApplicationWindow`; a extração de `ShellRoot.qml` para rodar a shell completa (`SystaliaCompositor` + `ValoMaterialSurfaceV2` + `Dock` + `TopBar`) dentro do `QQuickRenderControl` do `VulkanQuickRenderer` fica como passo prévio obrigatório antes do Teste A físico.
  - Nenhum commit KMS físico ou `TakeDevice` real foi executado nesta etapa.
- **Critérios para autorizar homologação física:**
  1. Conectar `ShellRoot.qml` (ou janela única controlada) ao `IndependentFrameLoop` sob `SGE_USE_DRM_BACKEND=1`.
  2. Validar execução assistida em TTY3 com instalação isolada (`$HOME/.local/systalia-homolog`), verificando `TakeDevice(226, 1)`, `DRM_MODE_ATOMIC_TEST_ONLY`, page-flip real, `libinput` e troca de VT.
