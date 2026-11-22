# SYSTALIA — Etapa 03.2.4.6.2: Validação Isolada do Pipeline Vulkan/DMA-BUF e Investigação do GPU Hang

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV (Mesa 26.2.7), GBM, libdrm e QtWaylandCompositor  
**GPU:** AMD Radeon Cezanne/Renoir (`AMD Radeon Graphics (RADV RENOIR)`)  
**Prioridade:** P0 — estabilidade gráfica

---

## 1. Diagnóstico Inicial

Após a Etapa 03.2.4.6.1, o pipeline `ShellRoot.qml → QQuickRenderControl → QRhi/Vulkan → ValoBackdropProviderV2 → VkImage → DMA-BUF` foi auditado em profundidade para verificar:
- Alocação dedicada e consulta de exportabilidade `DMA-BUF` em `VK_EXT_image_drm_format_modifier`.
- Matriz real de modificadores do driver RADV Renoir.
- Gravação de comandos `QRhi` e múltiplos pipelines `ValoBackdropProviderV2` / `ValoGpuBlurPipelineV2`.
- Isolamento seguro por cenários (A–H) sem expor a GPU compartilhada com o KDE Plasma a um novo hang sem autorização explícita.

---

## 2. Auditoria da Memória Externa Vulkan

Inspeção de `VulkanQuickRenderer::createSharedPoolSlots()` (`src/platform/drm/VulkanQuickRenderer.cpp`):

1. **Consulta de exportabilidade real:** Anteriormente `querySupportedDrmModifiers()` consultava apenas `vkGetPhysicalDeviceFormatProperties2` (`VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT` + `planeCount == 1`), sem validar `vkGetPhysicalDeviceImageFormatProperties2` com `VkPhysicalDeviceExternalImageFormatInfo` (`VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`). Implementado `queryDrmModifierCapabilities()` consultando ambas as APIs.
2. **Requisitos dedicados:** Substituído `vkGetImageMemoryRequirements` por `vkGetImageMemoryRequirements2` + `VkMemoryDedicatedRequirements` (`requiresDedicatedAllocation`, `prefersDedicatedAllocation`). No Cenário B (`128×128`, modifier `0x200000000401a01`), o RADV reportou `requiresDedicated=false`, `prefersDedicated=false`, `memType=0`, `allocBytes=65536`. `VkMemoryDedicatedAllocateInfo` permanece encadeado em `VkExportMemoryAllocateInfo`.
3. **Ownership e destruição:** `dmabufFd` fechado deterministicamente em `destroySharedPoolSlots()` após `vkDeviceWaitIdle` (omitido se `m_deviceLost`), verificado com `fcntl(fd, F_GETFD) == -1`.

---

## 3. Matriz de Formatos e Modificadores (RADV Renoir real via `--probe-modifiers`)

Formato Vulkan: `VK_FORMAT_B8G8R8A8_UNORM` ↔ KMS `DRM_FORMAT_XRGB8888` / `DRM_FORMAT_ARGB8888`:

| Modificador (`uint64_t`) | Planos | `tilingFeatures` | Color Attachment | DMA-BUF Exportable | Selecionado em `querySupportedDrmModifiers()` | KMS Físico |
|---|---:|---:|:---:|:---:|:---:|:---|
| `0x20000044051ba01` (AMD DCC/multi-plane) | 2 | `0x1dd81` | Sim | Sim | Não (filtrado: `planes > 1`) | NOT TESTED |
| `0x20000044051b901` (AMD DCC/multi-plane) | 2 | `0x1dd81` | Sim | Sim | Não (filtrado: `planes > 1`) | NOT TESTED |
| `0x200000440517901` (AMD DCC/multi-plane) | 3 | `0x1dd81` | Sim | Sim | Não (filtrado: `planes > 1`) | NOT TESTED |
| `0x20000044051b601` (AMD DCC/multi-plane) | 2 | `0x1dd81` | Sim | Sim | Não (filtrado: `planes > 1`) | NOT TESTED |
| `0x200000000401a01` (AMD GFX9 tiled) | 1 | `0x1dd83` | Sim | Sim | **Sim** (1º candidato sem restrição) | NOT TESTED |
| `0x200000000401901` (AMD GFX9 tiled) | 1 | `0x1dd83` | Sim | Sim | **Sim** | NOT TESTED |
| `0x200000000401601` (AMD GFX9 tiled) | 1 | `0x1dd83` | Sim | Sim | **Sim** | NOT TESTED |
| `0x200000000000a01` (AMD GFX9 tiled) | 1 | `0x1dd83` | Sim | Sim | **Sim** | NOT TESTED |
| `0x200000000000901` (AMD GFX9 tiled) | 1 | `0x1dd83` | Sim | Sim | **Sim** | NOT TESTED |
| `0x0` (`DRM_FORMAT_MOD_LINEAR`) | 1 | `0x1dd83` | Sim | Sim | **Sim** | NOT TESTED |

---

## 4. Auditoria QRhi

- `VulkanQuickRenderer::renderIntoBuffer()` vincula a `VkImage` do slot via `QQuickRenderTarget::fromVulkanImage(vkImg, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1)` antes de `beginFrame()` / `sync()` / `render()` / `endFrame()`.
- Antes de `endFrame()`, `QRhi::setQueueSubmitParams(&submitParams)` anexa `waitSemaphores` (se houver `releaseFenceFd` importado via `VK_SEMAPHORE_IMPORT_TEMPORARY_BIT`) e `signalSemaphores` (`slot.vkSignalSemaphoreHandle`), limpando `setQueueSubmitParams(nullptr)` logo após `endFrame()`.
- Após `endFrame()`, `QRhi::isDeviceLost()` é verificado antes de chamar `vkGetSemaphoreFdKHR(SYNC_FD)`.

---

## 5. Auditoria dos Efeitos VALO V2

- `ValoMaterialSurfaceV2.qml` e `ValoDockV2.qml` instanciam `ValoBackdropProviderV2` (presentes no TopBar, Dock, FilesWindow, MaterialWindow, ControlCenter, FlowMenu, TaskSwitcher, NotificationHost, SnapPreviewOverlay e WaylandWindowContainer).
- No log físico anterior (`tty3-homolog-20261122-110253.log`), 5 instâncias ativas executaram `ValoGpuBlurPipelineV2::execute()` durante o estágio `beforeRendering` do Frame 1 (`cb->beginPass`/`draw`/`endPass` para cada nível Dual-Kawase), gravando todos os passes no mesmo `QRhiCommandBuffer` submetido em `endFrame()`.
- A emissão de `[SGE-BACKDROP-V2] Blur GPU executado com sucesso!` ocorre na gravação de CPU antes da conclusão real na GPU (`vkQueueSubmit`).

---

## 6. Harness Criado

Criado o executável isolado `sge_vulkan_dmabuf_harness` ([sge_vulkan_dmabuf_harness.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/tools/diagnostics/sge_vulkan_dmabuf_harness.cpp), registrado em [cmake/SgeDrmPoc.cmake](file:///home/lucasramos/Documentos/systalia-glass-engine/cmake/SgeDrmPoc.cmake)):
- Utiliza `sge_vulkan_offscreen`, `QQuickRenderControl`, `VulkanQuickRenderer` e `GbmBufferManager` (sem DRM master e sem KMS).
- Suporta `--probe-modifiers`, `--scenario <A..H>` e `--frames <N>`.
- **Trava de segurança obrigatória:** cenários `D..H` (`1920×1080`) retornam `[BLOCKED]` (exit code `3`) a menos que `--allow-gpu-risk` e `SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1` sejam fornecidos explicitamente.

---

## 7. Matriz de Cenários Preparados e Classificação

| Cenário | Resolução | Pool | Efeitos | Estado | Observação |
|---|---|---|---|---|---|
| **A** | `128×128` | Vulkan interno | Desativados | **PASS** | 2 frames no harness + 3 frames em `tst_vulkan_dmabuf_audit` |
| **B** | `128×128` | DMA-BUF (`0x200000000401a01` e `LINEAR`) | Desativados | **PASS** | 2 frames no harness (`SYNC_FD=40`) + 3 frames em `tst_vulkan_dmabuf_audit`, `fcntl == -1` |
| **C** | `128×128` | DMA-BUF | VALO V2 (`ShellRoot.qml`) | **NOT TESTED** | Preparado no harness (`--scenario C`); não executado preventivamente |
| **D** | `1920×1080` | Vulkan interno | Desativados | **BLOCKED** | Bloqueado por segurança (exit `3`) até autorização explícita |
| **E** | `1920×1080` | DMA-BUF linear (`0x0`) | Desativados | **BLOCKED** | Bloqueado por segurança até autorização explícita |
| **F** | `1920×1080` | DMA-BUF modificador RADV | Desativados | **BLOCKED** | Bloqueado por segurança até autorização explícita |
| **G** | `1920×1080` | DMA-BUF linear (`0x0`) | VALO V2 (`ShellRoot.qml`) | **BLOCKED** | Bloqueado por segurança até autorização explícita |
| **H** | `1920×1080` | DMA-BUF modificador RADV | VALO V2 (`ShellRoot.qml`) | **BLOCKED** | Bloqueado por segurança até autorização explícita |

---

## 8. Testes Efetivamente Executados

- `sge_vulkan_dmabuf_harness --probe-modifiers` (RADV Renoir real).
- `sge_vulkan_dmabuf_harness --scenario A --frames 2` (`PASS`).
- `sge_vulkan_dmabuf_harness --scenario B --frames 2` com `SGE_GRAPHICS_DIAGNOSTICS=1` (`PASS`).
- `sge_vulkan_dmabuf_harness --scenario D` sem flags de risco (`[BLOCKED]`, exit `3`).
- `tst_vulkan_dmabuf_audit` (`tests/unit/tst_VulkanDmabufAudit.cpp`, `PASS`, `0.20 s`).

---

## 9. Testes Bloqueados

- Cenários `D`, `E`, `F`, `G` e `H` (`1920×1080`) permanecem `BLOCKED` no harness por risco à GPU compartilhada com o KDE Plasma, aguardando autorização humana para execução progressiva (`D → E → F → G → H`).

---

## 10. Problemas Identificados

1. `querySupportedDrmModifiers()` não validava `vkGetPhysicalDeviceImageFormatProperties2` (`VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT`).
2. `createSharedPoolSlots()` usava `vkGetImageMemoryRequirements` em vez de `vkGetImageMemoryRequirements2` + `VkMemoryDedicatedRequirements`.

---

## 11. Correções Implementadas

- [VulkanQuickRenderer.hpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/VulkanQuickRenderer.hpp) / [VulkanQuickRenderer.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/VulkanQuickRenderer.cpp):
  - Adicionado `DrmModifierCapability` e `queryDrmModifierCapabilities()` com consulta real de `vkGetPhysicalDeviceImageFormatProperties2` (`VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT` / `IMPORTABLE_BIT`).
  - Atualizado `createSharedPoolSlots()` para consultar `vkGetImageMemoryRequirements2` + `VkMemoryDedicatedRequirements` e ampliar logs de `SGE_GRAPHICS_DIAGNOSTICS=1` (`frameId`, `modifier`, `allocBytes`, `memType`, `requiresDedicated`, `prefersDedicated`, `dmabufFd`).
- [sge_vulkan_dmabuf_harness.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/tools/diagnostics/sge_vulkan_dmabuf_harness.cpp) + [tst_VulkanDmabufAudit.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/tests/unit/tst_VulkanDmabufAudit.cpp).

---

## 12. Resultados de Regressão (após a última modificação)

- **Build Release (`-DCMAKE_CXX_FLAGS="-Werror"`):** compilação e instalação limpas.
- **Testes específicos (11/11 aprovados, `2.06 s`):** incluindo `tst_vulkan_dmabuf_audit` (`0.20 s`).
- **Suíte CTest completa:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`, `50.70 s`: `38 unit`, `20 integration`, `1 e2e`).
- **Smoke tests (`systalia-session` instalado):**
  - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
  - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 13. Riscos Restantes

- Os cenários `1920×1080` (`D–H`) ainda não foram executados, portanto não está isolado se o `ring gfx timeout` anterior ocorreu apenas com modificador RADV + `ShellRoot.qml` em `1920×1080` (`H`) ou também no caminho linear (`G`) ou sem VALO V2 (`E`/`F`).

---

## 14. Decisão de Segurança

**NÃO APTO** para nova homologação física bare-metal na TTY3 neste momento.

**Próximo experimento recomendado (mediante autorização do usuário):** executar o harness isolado em progressão controlada (`--allow-gpu-risk` + `SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1` + `SGE_GRAPHICS_DIAGNOSTICS=1`) na ordem `C → D → E → F → G → H` (1 frame por passo) para identificar exatamente qual combinação (`1920×1080`, modificador RADV vs linear, ou VALO V2) provoca ou não provoca perda do dispositivo antes de qualquer novo teste físico em TTY3.
