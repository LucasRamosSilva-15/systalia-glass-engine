# SYSTALIA — Etapa 03.2.4.5: Integração do Backend Vulkan Independente, Atomic KMS e Ciclo de VT

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base:** `e77411e` (com alterações locais das Etapas 03.2.1–03.2.4.5 não commitadas)
- **Sistema:** Fedora Linux 44, Qt 6.11.2, C++23, `QtWaylandCompositor`, Vulkan/RADV, GBM, `libdrm`, `systemd-logind`
- **Hardware de referência:** AMD Radeon Cezanne — `/dev/dri/card1`
- **Estado:** **Infraestrutura experimental integrada sob `SGE_USE_DRM_BACKEND=1`, QPA Vulkan neutro validado em GPU RADV real sem compositor hospedeiro e 52/52 CTest aprovados** *(homologação física bare-metal em TTY3 pendente de autorização humana).*

---

## 1. Decisão sobre o QPA Independente

- **Alternativa escolhida (A — QPA mínimo com suporte Vulkan: `sge_vulkan_offscreen`):**
  - Implementado em [`src/platform/qpa/SgeVulkanOffscreenQpa.hpp`](../../src/platform/qpa/SgeVulkanOffscreenQpa.hpp), [`.cpp`](../../src/platform/qpa/SgeVulkanOffscreenQpa.cpp) e [`sge_vulkan_offscreen.json`](../../src/platform/qpa/sge_vulkan_offscreen.json) (`libsge_vulkan_offscreen.so`).
  - Fornece `QPlatformIntegration`, `QPlatformScreen`, `QPlatformWindow`, `QPlatformBackingStore`, `QAbstractEventDispatcher` (`createUnixEventDispatcher()`) e `QPlatformVulkanInstance` (`QBasicPlatformVulkanInstance`) usando `libvulkan.so` — **sem abrir `/dev/dri/*` e sem `eglfs_kms`**.
- **Por que B (`QRhi` direto) e C foram descartadas:** `QGuiApplication` e `QWaylandQuickOutput` exigem `QScreen` e `QPlatformIntegration`, e os plugins nativos `offscreen`/`minimal` do Qt 6.11.2 não implementam `createPlatformVulkanInstance()`.

---

## 2. Dependências Públicas e Privadas do Qt 6.11.2

- **APIs públicas:** `QQuickRenderControl`, `QQuickWindow`, `QQuickGraphicsDevice`, `QQuickRenderTarget`, `QVulkanInstance`, `QWaylandQuickOutput` (`setAutomaticFrameCallback`, `frameStarted`, `sendFrameCallbacks`).
- **APIs privadas (`Qt6::GuiPrivate` — já exigida pelo projeto em `cmake/Dependencies.cmake`):**
  - `qpa/qplatformintegration.h`, `qpa/qplatformintegrationplugin.h`, `qpa/qplatformscreen.h`, `qpa/qplatformwindow.h`, `qpa/qplatformbackingstore.h`, `qpa/qwindowsysteminterface.h`, `private/qbasicvulkanplatforminstance_p.h`, `private/qgenericunixeventdispatcher_p.h`.
- **Consequência para Fedora/empacotamento:** o plugin `libsge_vulkan_offscreen.so` (instalado em `${CMAKE_INSTALL_BINDIR}/platforms`) fica vinculado à versão exata do `qt6-qtbase` contra a qual foi compilado (`Qt_6.11_PRIVATE_API`), exigindo rebuild em atualizações minor do Qt.

---

## 3. Fluxo de Inicialização (`SGE_USE_DRM_BACKEND=1`)

1. `InputDeviceManager::evaluatePlatformEnvironment()` detecta `SessionBareMetalKms` (sem `WAYLAND_DISPLAY` e sem `DISPLAY`) e, **apenas** se `SGE_USE_DRM_BACKEND=1`, seleciona `QT_QPA_PLATFORM=sge_vulkan_offscreen` e adiciona `<exeDir>/platforms` a `QT_QPA_PLATFORM_PLUGIN_PATH` (sem `eglfs` nem `eglfs_kms`).
2. `QGuiApplication` carrega `libsge_vulkan_offscreen.so` sem tocar em `/dev/dri/card1`.
3. Em [`src/app/main.cpp`](../../src/app/main.cpp), antes de qualquer uso DRM:
   - `LogindSessionService::initializeSession(false)` assume controle da sessão (`TakeControl`); se falhar, aborta com código `1`.
   - `LogindDrmProvider` + `AtomicKmsPresenter` + `DrmBackendController` adquirem o **único** FD DRM via `TakeDevice` (nunca `SGE_ALLOW_NO_LOGIND`).
   - `InputDeviceManager::initializeHardwareContext()` inicializa `libinput` via `LogindSessionService` (com rollback DRM/logind em falha).
4. Sem `SGE_USE_DRM_BACKEND=1`, ou em `SessionNested`/`SessionHeadless`, o comportamento de produção permanece **100% inalterado**.

---

## 4. Integração `logind` / DRM / Vulkan e GBM / Atomic KMS

- **Correspondência DRM ↔ Vulkan:** [`src/platform/drm/DrmVulkanMatcher.hpp`](../../src/platform/drm/DrmVulkanMatcher.hpp) e [`.cpp`](../../src/platform/drm/DrmVulkanMatcher.cpp) validam `major`/`minor` do FD DRM (`fstat`) contra `VkPhysicalDeviceDrmPropertiesEXT` (nó `primary` ou `render` com offset `+128`).
- **GBM / Atomic KMS:** `GbmBufferManager` e `AtomicKmsPresenter` (`LibdrmResourceBackend`, `LibgbmBackend`, `LibdrmAtomicKmsBackend`) permanecem separados em preparação de parâmetros, validação simulada em CTest e execução física futura assistida.

---

## 5. Ciclo de Frame `QtWaylandCompositor`

Auditoria em [`/usr/include/qt6/QtWaylandCompositor/qwaylandquickoutput.h`](file:///usr/include/qt6/QtWaylandCompositor/qwaylandquickoutput.h) e [`qwaylandoutput.h`](file:///usr/include/qt6/QtWaylandCompositor/qwaylandoutput.h):
- `QWaylandQuickOutput::setAutomaticFrameCallback(false)` desabilita o disparo automático ligado a `QQuickWindow::frameSwapped`.
- [`src/platform/drm/WaylandFrameCoordinator.hpp`](../../src/platform/drm/WaylandFrameCoordinator.hpp) e [`.cpp`](../../src/platform/drm/WaylandFrameCoordinator.cpp) garantem:
  - `beginFrame()` chama `frameStarted()` uma única vez por frame em voo (bloqueando duplicatas e frames durante suspensão).
  - `completeFrame(presentedSuccessfully)` chama `sendFrameCallbacks()` **somente** quando `presentedSuccessfully == true && !suspended`, evitando `wl_surface.frame` prematuro ou em falhas de commit KMS.

---

## 6. Suspensão e Retomada de VT e Recuperação

- `LogindDrmProvider` conecta `devicePaused`/`deviceResumed` do `LogindSessionService` ao `DrmBackendController`, bloqueando apresentação quando pausado e atualizando o FD em `ResumeDevice`.
- Falhas em `initializeSession()`, `drmController->initialize()` ou `inputDeviceManager.initializeHardwareContext()` executam `drmController->shutdown()` e `logindSessionService.releaseControl()` antes de encerrar com `return 1`.

---

## 7. Arquivos Criados e Modificados

- **Criados:**
  - `src/platform/qpa/SgeVulkanOffscreenQpa.hpp`, `.cpp`, `sge_vulkan_offscreen.json`
  - `src/platform/drm/WaylandFrameCoordinator.hpp`, `.cpp`
  - `src/platform/drm/DrmVulkanMatcher.hpp`, `.cpp`
  - `tests/unit/tst_IndependentVulkanSession.cpp`
  - `docs/engineering/ETAPA_03_2_4_5_INDEPENDENT_VULKAN_SESSION.md`
- **Modificados:**
  - `cmake/SgeDrmPoc.cmake`, `cmake/SgeInstall.cmake`
  - `src/platform/drm/DrmSessionTypes.hpp`, `DrmBackendController.hpp`, `DrmBackendController.cpp` *(renomeação `majorNumber`/`minorNumber` para evitar conflito com macros `<sys/sysmacros.h>`)*
  - `src/services/input/InputDeviceManager.hpp`, `.cpp` *(suporte a `SGE_USE_DRM_BACKEND=1` apenas em `SessionBareMetalKms`)*
  - `src/app/main.cpp` *(caminho experimental isolado atrás de `SGE_USE_DRM_BACKEND=1`)*
  - `tests/CMakeLists.txt`, `tests/unit/tst_DrmBackendPoc.cpp`

---

## 8. Resultados Reais dos Testes (após a última edição)

1. **Build Release (`-Werror`):** compilada sem erros nem avisos.
2. **Testes específicos primeiro:** `tst_independent_vulkan_session`, `tst_vulkan_quick_renderer`, `tst_drm_gbm_integration`, `tst_drm_backend_poc`, `tst_input_hardware_backend`, `tst_logind_integration` — **6/6 aprovados**.
3. **PoC sem compositor hospedeiro (`env -u WAYLAND_DISPLAY -u DISPLAY QT_QPA_PLATFORM=sge_vulkan_offscreen`):** `sge_vulkan_quick_poc` executou Fases A–E em GPU `AMD Radeon Graphics (RADV RENOIR)` sem `WAYLAND_DISPLAY` nem `DISPLAY` (`exit 0`).
4. **Suíte completa CTest (`QT_QPA_PLATFORM=offscreen`, com `tst_independent_vulkan_session` rodando em `sge_vulkan_offscreen`):** **52/52 aprovados** (`50.34 s`, `0` falhas).
5. **Smoke tests (`systalia-session`, cwd `/tmp`, saída por `SIGTERM`):** headless (`offscreen`) e nested Wayland (`wayland`) encerraram com `sigterm_rc=0`.

---

## 9. Limitações, Critérios para Reteste Físico e Riscos Alpha 0.1

- **Limitações atuais:**
  - No caminho `SGE_USE_DRM_BACKEND=1` de `main.cpp`, o loop de apresentação contínua acoplando `QQuickRenderControl` da cena completa `main.qml` ao `page_flip` do `AtomicKmsPresenter` ainda requer a conexão de loop de render/timer e leitura de eventos DRM (`drmHandleEvent`) antes da exibição visual completa em TTY3.
  - Nenhum teste físico (`TakeDevice` ou `drmModeAtomicCommit` real) foi executado automaticamente.
- **Critérios para o primeiro reteste físico:**
  - Aprovação humana desta etapa e preparação do loop `QQuickRenderControl → Vulkan/DMA-BUF → AtomicKmsPresenter` no `main.qml`, com execução assistida em TTY3 usando instalação isolada (`$HOME/.local/systalia-homolog`) e `SGE_USE_DRM_BACKEND=1`.
- **Riscos pendentes Alpha 0.1:** dependência de `Qt6::GuiPrivate` no QPA `sge_vulkan_offscreen`, sincronização `IN_FENCE_FD`/`OUT_FENCE_PTR` em `amdgpu`/RADV e troca de VT sob carga real.
