# SYSTALIA — Etapa 03.2.4.4: Integração Vulkan + Qt Quick + Preparação da Apresentação Atomic KMS

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base:** `e77411e` (com alterações locais das Etapas 03.2.1–03.2.4.4 não commitadas)
- **Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan RADV (`Mesa 26.2.7`), `libdrm 2.4.134`, `gbm 26.2.7`
- **GPU de referência:** AMD Radeon Graphics (`RADV RENOIR` / Cezanne)
- **Estado:** **Ponte Vulkan/Qt Quick e contratos Atomic KMS implementados e comprovados em GPU Vulkan real (não-KMS) e 51/51 CTest** *(apresentação KMS física sobre `/dev/dri/card1` não executada nesta etapa).*

---

## 1. Diagnóstico das APIs Públicas Qt 6.11.2

Auditoria direta nos headers `/usr/include/qt6/QtQuick/` e `/usr/include/qt6/QtGui/`:
- **`QQuickRenderControl`** (`qquickrendercontrol.h`): API pública (`initialize()`, `polishItems()`, `beginFrame()`, `sync()`, `render()`, `endFrame()`, `rhi()`, `commandBuffer()`).
- **`QQuickGraphicsDevice::fromDeviceObjects(VkPhysicalDevice, VkDevice, queueFamilyIndex, queueIndex)`** (`qquickgraphicsdevice.h`): API pública para injetar dispositivo Vulkan externo no `QQuickWindow`.
- **`QQuickRenderTarget::fromVulkanImage(VkImage, VkImageLayout, VkFormat, QSize, sampleCount)`** (`qquickrendertarget.h`): API pública para renderizar diretamente em `VkImage` offscreen (`VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | TRANSFER_SRC_BIT | SAMPLED_BIT`).
- **`QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan)`**: obrigatório antes de instanciar o `QQuickWindow`; sem ele, o Qt Quick em `wayland` inicializa OpenGL e ignora o render target Vulkan.
- **Limitação crítica descoberta em `QT_QPA_PLATFORM=offscreen`:** o plugin `offscreen` do Qt 6.11.2 **não** implementa `QPlatformIntegration::createPlatformVulkanInstance()`. Mesmo envolvendo uma `VkInstance` externa com `QVulkanInstance::setVkInstance()`, `QVulkanInstance::create()` falha no `offscreen` (`This plugin does not support createPlatformVulkanInstance`). Em bare-metal independente sem compositor pai, será necessário um QPA mínimo que implemente `createPlatformVulkanInstance()` (apenas o wrapper `QPlatformVulkanInstance`, sem `eglfs_kms`) ou inicialização direta de `QRhi`.

---

## 2. Arquitetura da Ponte Vulkan / Qt Quick

Implementada em [`src/platform/drm/VulkanQuickRenderer.hpp`](../../src/platform/drm/VulkanQuickRenderer.hpp) e [`.cpp`](../../src/platform/drm/VulkanQuickRenderer.cpp) usando `QVulkanInstance`, `QVulkanFunctions` e `QVulkanDeviceFunctions` (`VK_NO_PROTOTYPES`):
- Seleciona GPU Vulkan (preferindo integrada/discreta sobre CPU), cria `VkDevice`, fila gráfica e `VkCommandPool`.
- Aloca `VkImage` (`VK_FORMAT_B8G8R8A8_UNORM`, `VK_IMAGE_TILING_OPTIMAL`) e `VkDeviceMemory` (`DEVICE_LOCAL`).
- Associa `QQuickWindow(QQuickRenderControl*)`, carrega QML via `QQmlEngine`/`QQmlComponent`, executa `polishItems() → beginFrame() → sync() → render() → endFrame()`.
- Suporta `resize()`, `setSuspended(true/false)` (bloqueando `renderFrame()` quando suspenso) e `shutdown()` em ordem determinística.

---

## 3. Estratégia de Memória Compartilhada (Vulkan + DMA-BUF/GBM)

- Consulta dinâmica de extensões: `VK_KHR_external_memory_fd`, `VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`, `VK_KHR_external_semaphore_fd`.
- Direção validada na PoC: **exportação** da memória Vulkan (`VkExportMemoryAllocateInfo` com `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT` + `vkGetMemoryFdKHR`) com fallback seguro caso a alocação exportável não seja suportada pelo driver.
- `vulkanExternalMemoryVerified` no `GbmBufferManager` permanece `false` até validação cruzada em hardware KMS real com modificadores DRM.

---

## 4. Implementação Realizada

- [`src/platform/drm/VulkanQuickRenderer.hpp`](../../src/platform/drm/VulkanQuickRenderer.hpp) / [`.cpp`](../../src/platform/drm/VulkanQuickRenderer.cpp)
- [`src/platform/drm/AtomicKmsPresenter.hpp`](../../src/platform/drm/AtomicKmsPresenter.hpp) / [`.cpp`](../../src/platform/drm/AtomicKmsPresenter.cpp) (`LibdrmAtomicKmsBackend` + `AtomicKmsPresenter` implementando `IDrmPresenter`)
- [`src/platform/drm/GbmBufferManager.hpp`](../../src/platform/drm/GbmBufferManager.hpp) / [`.cpp`](../../src/platform/drm/GbmBufferManager.cpp) (ciclo de 5 estados)
- [`src/platform/drm/sge_vulkan_quick_poc.cpp`](../../src/platform/drm/sge_vulkan_quick_poc.cpp) (alvo `sge_vulkan_quick_poc`)
- [`tests/unit/tst_VulkanQuickRenderer.cpp`](../../tests/unit/tst_VulkanQuickRenderer.cpp) (alvo `tst_vulkan_quick_renderer`)

---

## 5. Prova de Renderização QML e Readback (`sge_vulkan_quick_poc`)

Execução em GPU real **sem DRM master** (sobre sessão `wayland` hospedeira):

```text
[PHASE_A] PASS: device=AMD Radeon Graphics (RADV RENOIR) software=false ext_mem_fd=1 dmabuf=1 drm_mod=1
[PHASE_B] PASS: QQuickRenderControl frame rendered.
[PHASE_C] PASS: size=128x128 corner=#002244 center=#00ff88
[PHASE_D] PASS: exported dmabufFd=23
[PHASE_E] PASS: simulated Atomic KMS commit prepared (connector=20, crtc=55, plane=77)
```

---

## 6. Contratos Atomic KMS e Sincronização de Frames

- `LibdrmAtomicKmsBackend` prepara `drmModeAddFB2WithModifiers`, `drmModeAtomicAlloc`, propriedades `FB_ID`, `CRTC_ID`, `SRC_W/H`, `CRTC_W/H`, `IN_FENCE_FD`, `OUT_FENCE_PTR`, `DRM_MODE_ATOMIC_TEST_ONLY` e `DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT` (não executados contra a GPU física nesta etapa).
- Máquina de estados do `GbmBufferManager`:
  `Available → Rendering → ReadyForScanout → PendingFlip → Presented → Available`.
- Falhas no commit devolvem o buffer para `Available`; `suspendPresentation()` bloqueia novos commits; `onPageFlipCompleted()` libera o buffer anteriormente apresentado. `vkDeviceWaitIdle` não é usado como sincronização por frame no fluxo KMS.

---

## 7. Impacto sobre o VALO Graphics V2 e `QtWaylandCompositor`

- **VALO Graphics V2:** shaders `.qsb`, UBO `SystaliaSurfacePropsV2` (160B), `Dual-Kawase` e `ValoBackdropProviderV2` permanecem intactos e compatíveis, pois operam sobre o mesmo `QRhi` Vulkan do `QQuickRenderControl`.
- **`QtWaylandCompositor` (`QWaylandQuickOutput`):** `QWaylandQuickOutput` aceita `QWindow*`, mas no modo `QQuickRenderControl` offscreen o sinal `QQuickWindow::frameSwapped` não é disparado automaticamente pelo QPA. Será necessário disparar explicitamente `frameStarted()` / `sendFrameCallbacks()` no ciclo de page-flip, além de resolver o QPA neutro com suporte a `QPlatformVulkanInstance` para `SessionBareMetalKms`.

---

## 8. Testes Executados e Resultados (após a última edição)

- **Build Release (`-Werror`):** compilada sem erros nem avisos.
- **Testes específicos (`QT_QPA_PLATFORM=offscreen`):** `tst_drm_backend_poc`, `tst_drm_gbm_integration`, `tst_vulkan_quick_renderer` — **3/3 aprovados** (`0.17 s`).
- **Teste `tst_vulkan_quick_renderer` em GPU Vulkan RADV real (`QT_QPA_PLATFORM=wayland`):** **6/6 casos aprovados, 0 skipped** (`98 ms`), validando renderização QML + readback de pixels `#102030`/`#00ff00` + exportação DMA-BUF + `AtomicKmsPresenter` simulado.
- **Suíte completa CTest (`QT_QPA_PLATFORM=offscreen`):** **51/51 aprovados** (`49.30 s`, `0` falhas).
- **Smoke tests (`systalia-session`, cwd `/tmp`, saída por `SIGTERM`):**
  - Headless (`offscreen`): `sigterm_rc=0`.
  - Nested Wayland (`wayland`): `sigterm_rc=0`.

---

## 9. Riscos, Limitações e Critérios de Entrada da Etapa 03.2.4.5

- **Limitações atuais:** apresentação física `drmModeAtomicCommit` sobre `/dev/dri/card1` e importação direta de `gbm_bo` com modificadores para KMS não foram executadas fisicamente; `QT_QPA_PLATFORM=offscreen` não suporta `QVulkanInstance::create()`.
- **Critérios para a Etapa 03.2.4.5:**
  1. Prover QPA mínimo para `SessionBareMetalKms` com `QPlatformVulkanInstance` (ou integração equivalente sem `eglfs_kms`).
  2. Conectar callbacks de `QWaylandQuickOutput` ao ciclo `QQuickRenderControl` + page-flip.
  3. Validar fisicamente em TTY3 sob flag isolada (`SGE_USE_DRM_BACKEND=1`) com `TakeDevice(226, 1)` único e `DRM_MODE_ATOMIC_TEST_ONLY` / flip real.
