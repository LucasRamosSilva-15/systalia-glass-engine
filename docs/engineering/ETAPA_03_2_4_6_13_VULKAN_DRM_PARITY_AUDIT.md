# SYSTALIA — Etapa 03.2.4.6.13: Auditoria de Paridade Vulkan/DRM, Investigação do GPU Hang e Preparação da Homologação Física

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, QtWaylandCompositor, GBM, libdrm e KMS Atomic  
**GPU de referência:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline de regressão:** 59/59 testes CTest (`38 unit`, `20 integration`, `1 e2e`)  
**Estado TTY3:** **BLOQUEADA (aguardando correções finais de paridade física e autorização humana)**

---

## 1. Resumo Executivo

Esta auditoria comparou o caminho offscreen (`sge_vulkan_dmabuf_harness.cpp`, que concluiu a matriz **A–H** com `rc=0`) contra o caminho físico bare-metal (`src/app/main.cpp` com `SGE_USE_DRM_BACKEND=1`).

- O GPU hang original da TTY3 (`2026-11-22T11:02:53`, `amdgpu: ring gfx timeout, signaled seq=820468, emitted seq=820469`) ocorreu em um commit anterior às correções de `VkMemoryDedicatedAllocateInfo`, tratamento fail-closed de `VK_ERROR_DEVICE_LOST`, quarentena de semáforos e lifecycle `QWaylandOutput`.
- Contudo, a inspeção estática e de paridade em `HEAD` identificou **divergências confirmadas** entre o harness e o backend físico, além de um **defeito real de vazamento + double-free de `VkImage`/`VkDeviceMemory`** em `VulkanQuickRenderer::renderIntoBuffer()` / `destroyOffscreenImage()` e ausência de negociação `IN_FORMATS` do plano primário KMS em `main.cpp`.
- As correções críticas e a instrumentação de paridade (`VULKAN_DEVICE_SELECTED`, `KMS_COMMIT_SUBMITTED`) foram aplicadas e validadas com `59/59` CTest e smoke tests limpos. Permanecem divergências de ambiente físico (`XWayland` ativo, `libinput`/`logind`, resolução dinâmica do QPA pós-modeset e `allowModeset` no primeiro commit) que exigem manutenção do bloqueio ou checklist estrito antes de qualquer nova tentativa em TTY3.

---

## 2. Evidências Históricas

### 2.1 Falha física na TTY3 (`$HOME/systalia-homolog-logs/tty3-homolog-20261122-110253.log`)

- **Data/Sessão:** `2026-11-22T11:02:53-03:00`, sessão `id=4`, `tty=tty3`, `seat=seat0`.
- **Sequência registrada:**
  1. Carregamento do QPA `sge_vulkan_offscreen` e inicialização do `libinput`/`udev` em `seat0`.
  2. Inicialização do compositor Wayland (`wayland-systalia-0`) e `Xwayland :1`.
  3. Execução de 5 passes `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!` em `1920×1080` (`format=1`, mesmo `rhiSrc=0x2a0a82a0`, 5 `outRhi` distintos).
  4. Falha no driver: `radv/amdgpu: The CS has been cancelled because the context is lost. This context is guilty of a hard recovery.` e `Device loss detected in vkQueueSubmit()`, antes de qualquer `drmModeAtomicCommit`.
- **Kernel (`ETAPA_03_2_4_6_1`):** `amdgpu 0000:04:00.0: ring gfx timeout, signaled seq=820468, emitted seq=820469` no processo `systalia_glass_` (`PID 39575`).

### 2.2 Logs offscreen C–H (`$HOME/systalia-homolog-logs/`)

Todos os cenários `C–H` executados com `1` frame em modo isolado concluíram com `rc=0` e `deviceLost=false`:
- `offscreen-exp-C-20261122-125409.log` (commit `14cf98a`, `128×128`, DMA-BUF `0x200000000401a01` + `ShellRoot.qml`): `PASS`, `gpuLatencyUs=32`.
- `offscreen-exp-D-20261122-141902.log` (commit `c2fa6a0`, `1920×1080`, Vulkan interno): `PASS`, `method=VkFence`, `gpuLatencyUs=11`.
- `offscreen-exp-E-20261122-142031.log` (commit `c2fa6a0`, `1920×1080`, DMA-BUF linear): `PASS`.
- `offscreen-exp-F-20261122-142231.log` (commit `c2fa6a0`, `1920×1080`, DMA-BUF tiled `0x200000000401a01`): `PASS`.
- `offscreen-exp-G-20261122-142346.log` (commit `c2fa6a0`, `1920×1080`, DMA-BUF linear + `ShellRoot.qml`): `PASS`, 5 blurs, `gpuLatencyUs=66`.
- `offscreen-exp-H-20261122-142534.log` (commit `c2fa6a0`, `1920×1080`, DMA-BUF tiled `0x200000000401a01` + `ShellRoot.qml`): `PASS`, `valoPasses=8`, `valoGlassItems=8`, 5 blurs executados, `gpuLatencyUs=38`, `deviceLost=false`, `rc=0`.

---

## 3. Matriz A–H

| Cenário | Resolução | Render Target / Pool | VALO V2 (`ShellRoot.qml`) | Resultado (1–2 frames) | Limite de comprovação |
|---|---|---|---|---|---|
| **A** | `128×128` | Vulkan interno (`allowDmabufExport=false`) | Desativado (`0`) | **PASS** | Baseline CPU/GPU mínimo |
| **B** | `128×128` | DMA-BUF tiled + `SYNC_FD` | Desativado (`0`) | **PASS** | Pool externo sem carga QML complexa |
| **C** | `128×128` | DMA-BUF `0x200000000401a01` + `SYNC_FD` | Ativo (5 blurs) | **PASS** | Baixa resolução, 1 frame |
| **D** | `1920×1080` | Vulkan interno (`VkFence`) | Desativado (`0`) | **PASS** | Sem DMA-BUF nem VALO V2 |
| **E** | `1920×1080` | DMA-BUF linear (`DRM_FORMAT_MOD_LINEAR`) | Desativado (`0`) | **PASS** | Sem VALO V2 nem KMS físico |
| **F** | `1920×1080` | DMA-BUF tiled `0x200000000401a01` | Desativado (`0`) | **PASS** | Sem VALO V2 nem KMS físico |
| **G** | `1920×1080` | DMA-BUF linear + `ShellRoot.qml` | Ativo (5 blurs) | **PASS** | 1 frame; `MockAtomic`, sem `Xwayland` |
| **H** | `1920×1080` | DMA-BUF tiled `0x200000000401a01` + `ShellRoot.qml` | Ativo (5 blurs) | **PASS** | 1 frame; `MockAtomic`, sem `Xwayland` |

---

## 4. Fluxo Offscreen (Fluxo A — `sge_vulkan_dmabuf_harness.cpp`)

1. `qputenv("QT_QPA_PLATFORM", "sge_vulkan_offscreen")` + `QQuickWindow::setGraphicsApi(Vulkan)` + `QGuiApplication`.
2. `VulkanQuickRenderer::initialize(spec.size, spec.useDmabufPool)` cria `QVulkanInstance`, seleciona GPU física, cria `VkDevice`/`VkQueue`/`VkCommandPool`, `m_colorImage`, `QQuickRenderControl`, `QQuickWindow` e `QQuickRenderTarget`.
3. `AtomicKmsPresenter` recebe `HarnessMockKms`, `HarnessMockGbm` e `HarnessMockAtomic` em `fd=10` (sem DRM real).
4. `createSharedPoolSlots()` cria 2 slots `VkImage` (`VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` + `VkMemoryDedicatedAllocateInfo`), exporta `dmabufFd` e cria `vkSignalSemaphoreHandle` / `vkWaitSemaphoreHandle`.
5. `loadQmlUrl(ShellRoot.qml)` + `comp->stopXWayland()` (desativa XWayland).
6. `IndependentFrameLoop::processNextFrame()` → `WaylandFrameCoordinator::beginFrame()` → `renderIntoBuffer()` (`QRhi::setQueueSubmitParams`, `endFrame()`, `vkGetSemaphoreFdKHR(SYNC_FD)`) → `HarnessMockAtomic::commitAtomic()` → `observeSyncFdCompletion(poll)` → `dispatchKernelPageFlipForTest()` (`MOCK_PAGE_FLIP_COMPLETED`).
7. Shutdown ordenado (`frameLoop` → `releaseKmsFramebuffers` → `destroySharedPoolSlots` → `presenter` → `renderer`).

---

## 5. Fluxo Físico (Fluxo B — `src/app/main.cpp` com `SGE_USE_DRM_BACKEND=1`)

1. `InputDeviceManager::configureProcessPlatformEnvironment()` define `QT_QPA_PLATFORM=sge_vulkan_offscreen` + `QQuickWindow::setGraphicsApi(Vulkan)` + `QGuiApplication` (QPA lê `SGE_QPA_WIDTH`/`HEIGHT`/`REFRESH_HZ` **antes** da descoberta KMS).
2. `logindSessionService.initializeSession(false)` + `DrmBackendController::initialize()` (`TakeDevice` no `/dev/dri/card1`) + `KmsOutputDiscovery` + `AtomicKmsPresenter::initializeOnFd()` + `inputDeviceManager.initializeHardwareContext()` (`libinput`/`udev`).
3. `VulkanQuickRenderer::initialize(targetSize)` + verificação `matchesDrmDevice(vkCaps.drmDeviceIds, drmMaj, drmMin)`.
4. `createSharedPoolSlots(atomicPresenter->bufferManager(), targetSize, 2, atomicPresenter->primaryPlaneModifiers())` *(corrigido nesta etapa; antes não passava modificadores KMS)*.
5. `shellQmlEngine` registra `DesktopIconProvider` + `loadQmlUrl(ShellRoot.qml)` (**mantém `Xwayland :1` ativo**).
6. `QQuickRenderControl::renderRequested` / `sceneChanged` conectados a `IndependentFrameLoop::requestFrame()`.
7. `processNextFrame()` → `renderIntoBuffer()` → `AtomicKmsPresenter::submitBufferForScanout()` → `drmModeAddFB2WithModifiers()` + `drmModeAtomicCommit(..., DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT)` → `DrmEventDispatcher::onFdReadable()` (`drmHandleEvent`, `KMS_PAGE_FLIP_COMPLETED`).

---

## 6. Matriz de Paridade (12 Pontos)

| # | Componente / Etapa | Harness (Fluxo A) | Backend Físico (Fluxo B) | Divergência Identificada | Classificação | Risco Potencial | Evidência | Correção Necessária | Validação |
|---|---|---|---|---|---|---|---|---|---|
| 1 | **Inicialização QPA / Resolução** | `SGE_QPA_*` default `1920×1080@60` coincide com `spec.size` | `QGuiApplication` cria `SgeOffscreenScreen` antes de `KmsOutputDiscovery` | Se monitor preferir modo != `1920×1080@60`, `QScreen` diverge de `targetSize` | **Confirmada** | Geometria/DPR/cursor desalinhados em monitores não-1080p | `SgeVulkanOffscreenQpa.cpp:74`, `main.cpp:58,114` | Propagar modo descoberto ou atualizar `QScreen`/geom após modeset | Documentado |
| 2 | **Ordem de criação** | `VulkanQuickRenderer` antes de `MockAtomic` | `logind` + `DrmBackendController` + `libinput` antes de `VulkanQuickRenderer` | Hardware context (`TakeDevice`/`libinput`) ativo durante renderização | **Confirmada** | Falha de `libinput`/VT afeta sessão | `main.cpp:98-126`, `harness.cpp:193-210` | Manter ordem atual + rollback fail-closed | Inspecionado |
| 3 | **GPU física selecionada** | `createVulkanDevice()` (RADV RENOIR) | `createVulkanDevice()` + `matchesDrmDevice()` | Mesma seleção; físico valida `primaryMajor`/`renderMajor` | **Descartada por evidência** | Baixo (mesma GPU `/dev/dri/card1`) | `VulkanQuickRenderer.cpp:1355`, `main.cpp:211` | Log `VULKAN_DEVICE_SELECTED` adicionado | Validado |
| 4 | **Fila gráfica** | `m_graphicsQueueFamily` queue `0` | `m_graphicsQueueFamily` queue `0` | Nenhuma | **Descartada por evidência** | Baixo | `VulkanQuickRenderer.cpp:1491` | Nenhuma | Validado |
| 5 | **Extensões Vulkan** | `VK_KHR_external_*`, `VK_EXT_image_drm_format_modifier` | Idênticas | Nenhuma | **Descartada por evidência** | Baixo | `VulkanQuickRenderer.cpp:1444-1465` | Nenhuma | Validado |
| 6 | **Características `VkImage`** | `createSharedPoolSlots()` com `VkMemoryDedicatedAllocateInfo` | Idêntico; porém `renderIntoBuffer()` sobrescrevia `m_colorImage` | Leak de `m_colorImage` inicial + double-free no `shutdown()` | **Confirmada** | Corrupção de handles Vulkan no teardown | `VulkanQuickRenderer.cpp:389,1025` | Separado `m_activeRenderImage` de `m_colorImage` | **Corrigido e testado** |
| 7 | **Render target** | `QQuickRenderTarget::fromVulkanImage(..., UNDEFINED, ...)` | Idêntico | Nenhuma | **Descartada por evidência** | Baixo | `VulkanQuickRenderer.cpp:502` | Nenhuma | Validado |
| 8 | **Formato e modificador DRM** | `HarnessMockAtomic` aceita qualquer modificador | `main.cpp` passava `kmsModifiers={}` sem ler `IN_FORMATS` | `drmModeAddFB2WithModifiers` podia receber modificador não aceito pelo plano | **Confirmada** | Falha no primeiro `addFramebuffer`/`commitAtomic` | `main.cpp:222`, `KmsOutputDiscovery.cpp` | Parse de `IN_FORMATS` + repasse em `main.cpp` | **Corrigido e testado** |
| 9 | **Semáforos e fences** | `SYNC_FD` observado por `poll()` + `dispatchKernelPageFlipForTest` | `IN_FENCE_FD` / `OUT_FENCE_PTR` em `drmModeAtomicCommit` | Harness valida `SYNC_FD` síncrono antes do mock flip; físico entrega `IN_FENCE_FD` ao kernel | **Confirmada** | Em erro de `commitAtomic`, `IN_FENCE_FD` precisa ser fechado sem vazar | `AtomicKmsPresenter.cpp:300-318` | Coberto por teste determinístico | **Testado** |
| 10 | **Ciclo de vida dos slots** | 2 slots; 1 frame no Cenário H | Contínuo via `renderRequested`/`sceneChanged` | Harness H rodou só 1 frame (não exercita reutilização contínua pós-`releaseFenceFd`) | **Confirmada** | Bugs de multi-frame/reuso em `1920×1080` não cobertos por H(1f) | `offscreen-exp-H`, `main.cpp:264-293` | Testar multi-frame controlado antes da TTY3 | Pendente |
| 11 | **Composição QML / XWayland** | `stopXWayland()` chamado; `DesktopIconProvider` ausente até esta etapa | `Xwayland :1` ativo; `DesktopIconProvider` registrado | `Xwayland` concorre no `/dev/dri/renderD128`; ícones reais não carregavam no harness | **Confirmada** (XWayland = **Hipótese ainda não validada** para o hang) | Upload de ícones e `glamor` concorrente | `tty3-homolog`, `main.cpp:194`, `harness.cpp:265` | `DesktopIconProvider` adicionado ao harness | Parcial |
| 12 | **Encerramento** | `QWaylandOutput` limpo (sem warnings em `c2fa6a0`) | `frameLoop` → `releaseKmsFramebuffers` → `destroySharedPoolSlots` → `renderer` → `drmController` | Ordem equivalente após correção `c2fa6a0` | **Descartada por evidência** | Baixo | `main.cpp:295-302`, `harness.cpp:340` | Nenhuma | Validado |

---

## 7. Divergências Anteriores ao Atomic Commit (Prioridade Máxima)

O travamento original (`11:02:53`) ocorreu **antes** do primeiro `drmModeAtomicCommit`. Entre os fatores anteriores ao commit:
1. **Já corrigidos após `11:02:53`:** ausência de `VkMemoryDedicatedAllocateInfo` em `createSharedPoolSlots()`, ausência de tratamento fail-closed de `VK_ERROR_DEVICE_LOST`, destruição prematura de semáforos e avisos/lifecycle do `QWaylandOutput`.
2. **Corrigido nesta etapa:** sobrescrita de `m_colorImage`/`m_colorMemory` em `renderIntoBuffer()` (vazamento + risco de double-free em `destroyOffscreenImage()`).
3. **Divergências ainda presentes no caminho físico antes do commit:**
   - **Concorrência com `Xwayland :1`:** iniciado por `ShellRoot.qml` (`startXWayland(1)`) durante o Frame 1 no fluxo físico, mas explicitamente parado (`stopXWayland()`) no harness. *(Classificação: Divergência confirmada; papel no GPU hang: hipótese ainda não validada).*
   - **Upload real de ícones (`DesktopIconProvider`):** ativo em `main.cpp`, ausente no harness até esta etapa. *(Corrigido no harness).*
   - **Contexto de hardware (`libinput`/`udev` + `TakeDevice`):** ativo em `main.cpp`, ausente no harness. *(Divergência confirmada; improvável como causa direta de `ring gfx timeout`).*

---

## 8. Auditoria Vulkan/QRhi (Device & Queue Ownership)

- **`VkInstance` / `VkPhysicalDevice` / `VkDevice` / `VkQueue`:** há uma única instância lógica (`QVulkanInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue` na família gráfica `m_graphicsQueueFamily`, índice `0`) criada por `VulkanQuickRenderer::createVulkanDevice()` e compartilhada com `QRhiVulkan` via `QQuickGraphicsDevice::fromDeviceObjects(...)`.
- **`VkCommandPool` e Threading:** a renderização usa `QQuickRenderControl` em loop single-thread na thread principal; `readbackImage()` e `observeInternalGpuCompletion()` executam apenas após `m_renderControl->endFrame()`, sem submissões concorrentes de outra thread na `m_graphicsQueue`.
- **Defeito corrigido:** `renderIntoBuffer()` sobrescrevia `m_colorImage` e `m_colorMemory` com o handle do slot externo, vazando a imagem interna de `initialize()` e causando `vkDestroyImage`/`vkFreeMemory` duplicados em `destroyOffscreenImage()`. Agora `m_activeRenderImage` rastreia o alvo ativo sem alterar `m_colorImage`/`m_colorMemory`.

---

## 9. Auditoria VALO V2

- `ShellRoot.qml` instancia **8** `ValoGlassItemV2` / `ValoBackdropProviderV2` na árvore QML (`valoPasses=8 valoGlassItems=8` no `SCENARIO_CONFIG` de H).
- No Frame 1, tanto na sessão física (`tty3-homolog-20261122-110253.log`) quanto no Cenário H (`offscreen-exp-H-20261122-142534.log`), ocorrem **exatamente 5 execuções** de `ValoBackdropProviderV2::onBeforeRendering()` / `ValoGpuBlurPipelineV2` em `1920×1080` (`format=1`, mesma textura `rhiSrc` compartilhada, 5 `outRhi` distintos). Os outros 3 providers pertencem a overlays inicialmente ocultos.
- Cada instância possui suas próprias texturas piramidais, `QRhiTextureRenderTarget`, UBO e `QRhiShaderResourceBindings`, sem passes aninhados no mesmo `QRhiCommandBuffer`.

---

## 10. Auditoria DMA-BUF / GBM

Distinção explícita de compatibilidade:
1. **Renderização Vulkan:** validada em RADV (`1920×1080`, `VK_FORMAT_B8G8R8A8_UNORM`, `VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT`).
2. **Exportação DMA-BUF:** validada via `vkGetMemoryFdKHR` (`VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`) com `VkMemoryDedicatedAllocateInfo` e modificador `0x200000000401a01` (stride `7680`, `allocBytes=8912896`).
3. **Importação GBM:** em `createSharedPoolSlots()`, os slots são alocados pelo Vulkan e registrados em `GbmBufferManager` via `registerExternalSlot()` (sem chamar `gbm_bo_create` para esses slots).
4. **Compatibilidade de scanout KMS (`IN_FORMATS`):** **não** era verificada em `main.cpp`, pois `createSharedPoolSlots()` recebia `kmsModifiers={}`. Nesta etapa, `KmsOutputDiscovery::queryPlaneModifiers()` passa a extrair os modificadores do blob `IN_FORMATS` do plano primário para `DRM_FORMAT_XRGB8888`, e `main.cpp` os repassa a `createSharedPoolSlots()`.
5. **Primeiro `drmModeAtomicCommit`:** `submitBufferForScanout()` usa `allowModeset = false`; se o display exigir modeset inicial para ativar o plano/CRTC no `card1`, o primeiro commit pode precisar de `allowModeset = true` ou modeset prévio.

---

## 11. Auditoria de Sincronização

Ciclo completo:
`GPU submit (QRhi endFrame) → signalSemaphore (vkGetSemaphoreFdKHR) → SYNC_FD (acquireFenceFd) → IN_FENCE_FD → drmModeAtomicCommit → OUT_FENCE_PTR (m_pendingOutFenceFd) → page-flip event → releaseFenceFd → slot Available`

- **`GPU_EXECUTION_COMPLETED` ≠ liberação do buffer:** `observeSyncFdCompletion()` confirma apenas que a GPU terminou de renderizar; o slot só volta a `Available` após `DrmEventDispatcher` entregar o page-flip (ou `releaseBufferToAvailable` em erro).
- **Falha de `commitAtomic` / cancelamento:** `AtomicKmsPresenter::submitBufferForScanout()` descarta `PendingFlipContext`, fecha `acquireFenceFd` / `outFenceFd` e devolve o slot a `Available` sem vazar FDs.
- **Quarentena de semáforos:** semáforos em falha parcial (`!synced` pós-importação ou falha em `vkGetSemaphoreFdKHR`) vão para `m_quarantinedSemaphores` e só são destruídos em `flushQuarantinedSemaphores()` após `vkDeviceWaitIdle` (omitido se `m_deviceLost`).

---

## 12. Correções Aplicadas

1. **Vazamento e double-free de `m_colorImage` / `m_colorMemory` (`VulkanQuickRenderer.hpp/.cpp`):**
   - *Defeito:* `renderIntoBuffer()` sobrescrevia `m_colorImage` e `m_colorMemory` com os handles do slot DMA-BUF.
   - *Solução:* criado `m_activeRenderImage` separado dos recursos próprios de `initialize()`.
   - *Regressão:* `sharedPoolSlotsPreserveInternalOffscreenImageWithoutDoubleFree` em `tst_VulkanDmabufAudit.cpp`.
2. **Negociação de modificadores KMS `IN_FORMATS` (`KmsOutputDiscovery.hpp/.cpp`, `AtomicKmsPresenter.hpp/.cpp`, `main.cpp`):**
   - *Defeito:* `main.cpp` chamava `createSharedPoolSlots()` sem modificadores KMS.
   - *Solução:* `KmsOutputDiscovery::queryPlaneModifiers()` extrai `IN_FORMATS` para `DRM_FORMAT_XRGB8888` e `main.cpp` repassa `atomicPresenter->primaryPlaneModifiers()`.
   - *Regressão:* `atomicPresenterNegotiatesModifiersAndHandlesCommitFailureAndLateFlip` em `tst_VulkanDmabufAudit.cpp`.
3. **Instrumentação de paridade (`VulkanQuickRenderer.cpp`, `AtomicKmsPresenter.cpp`):**
   - Adicionados `[SGE-DIAG] VULKAN_DEVICE_SELECTED` e `[SGE-DIAG] KMS_COMMIT_SUBMITTED` sob `SGE_GRAPHICS_DIAGNOSTICS=1`.
4. **Paridade de `DesktopIconProvider` no harness (`sge_vulkan_dmabuf_harness.cpp`):**
   - Registrado `DesktopIconProvider` no `QQmlEngine` do harness.

---

## 13. Testes e Regressões (após edições finais)

- **Build Release (`-Werror`):** limpa, zero warnings/erros.
- **Testes específicos de paridade/DRM (`7/7` em `2.24 s`):** `tst_drm_gbm_integration`, `tst_shell_root_integration`, `tst_unified_vulkan_gbm_pool`, `tst_vulkan_kms_sync`, `tst_page_flip_and_unified_startup`, `tst_device_lost_and_compositor_lifecycle`, `tst_vulkan_dmabuf_audit` (`0.33 s`).
- **CTest completo:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`) em `51.13 s` (`38 unit`, `20 integration`, `1 e2e`).
- **Smoke tests `systalia-session`:**
  - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
  - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 14. Riscos Residuais

1. Cenário H offscreen validou apenas **1 frame**; multi-frame contínuo (`>= 3` frames) com reciclagem de `releaseFenceFd` em `1920×1080` + VALO V2 ainda não foi exercitado.
2. `Xwayland :1` ativo no fluxo físico (`main.cpp`) vs. desativado no harness permanece como diferença não isolada.
3. Modo preferencial diferente de `1920×1080@60` causaria divergência com `SGE_QPA_WIDTH`/`HEIGHT` inicializados antes de `KmsOutputDiscovery`.
4. `allowModeset = false` em `submitBufferForScanout()` pode falhar no primeiro commit se o CRTC/plano exigir modeset explícito.

---

## 15. Checklist de Pré-Homologação Física (TTY3)

- [x] Build Release `-Werror` e `59/59` CTest aprovados.
- [x] Matriz offscreen `A–H` concluída com `rc=0`.
- [x] `VkMemoryDedicatedAllocateInfo`, fail-closed `DEVICE_LOST`, quarentena de semáforos, lifecycle `QWaylandOutput` e `m_activeRenderImage` corrigidos.
- [x] `IN_FORMATS` KMS integrado ao `createSharedPoolSlots()` em `main.cpp`.
- [ ] Validar Cenário H offscreen com **múltiplos frames** (`--frames 5`) no harness atualizado (com `DesktopIconProvider` ativo).
- [ ] Verificar/tratar `allowModeset = true` no primeiro `AtomicCommitRequest` quando necessário e garantir captura persistente de logs (`SGE_GRAPHICS_DIAGNOSTICS=1` + `journalctl -k` em arquivo fora do repo).
- [ ] Plano de recuperação definido (encerramento automático em `deviceLost`, `Ctrl+Alt+F2` / SSH, ambiente separado da sessão principal quando possível, teste inicial de 1 frame com interrupção imediata em qualquer erro).

---

## 16. Decisão Final de Segurança

- **Homologação física TTY3 (`SGE_USE_DRM_BACKEND=1`):** **NÃO APTO — MANTER BLOQUEADA** até validar Cenário H multi-frame (`--frames 5`) com `DesktopIconProvider` e confirmar o comportamento do primeiro modeset (`allowModeset`) em `AtomicKmsPresenter`.
- **Próxima etapa recomendada:** executar autorização controlada de **Cenário H multi-frame** (`--frames 5`) no harness atualizado e preparar ajuste de `allowModeset` no primeiro commit KMS antes de liberar o teste físico de 1 frame na TTY3.
