# SYSTALIA — Etapa 03.2.4.6.3: Auditoria de Layouts Vulkan, Sincronização e Fidelidade do Harness

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU:** AMD Radeon Cezanne/Renoir (`AMD Radeon Graphics (RADV RENOIR)`)  
**Prioridade:** P0 — estabilidade da renderização

---

## 1. Diagnóstico Inicial

Esta auditoria verificou em profundidade o ciclo do primeiro frame do SYSTALIA (`ShellRoot.qml → QQuickRenderControl → QRhi/Vulkan → ValoBackdropProviderV2 → VkImage → DMA-BUF`), focando em:
1. Transições de layout Vulkan com `QQuickRenderTarget::fromVulkanImage`.
2. Regras de payload temporário e reuso de `VkSemaphore` (`SYNC_FD`).
3. Tabela de ownership de memória externa e descritores.
4. Cinco instâncias `ValoBackdropProviderV2` / `ValoGpuBlurPipelineV2`.
5. Fidelidade estrutural do `sge_vulkan_dmabuf_harness` em relação ao caminho da TTY3.

---

## 2. Auditoria de Layouts Vulkan

Em `VulkanQuickRenderer::renderIntoBuffer(GbmExportedBuffer &slot)`:
- `m_window->setRenderTarget(QQuickRenderTarget::fromVulkanImage(vkImg, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1));`
- **Contrato Qt 6.11.2 / Vulkan:**
  - Antes do Frame 1, a `VkImage` do slot é criada com `initialLayout = VK_IMAGE_LAYOUT_UNDEFINED`. Informar `VK_IMAGE_LAYOUT_UNDEFINED` a `QQuickRenderTarget::fromVulkanImage()` é **válido** quando o render pass principal realiza clear completo do target (não preserva conteúdo anterior).
  - Internamente, o `QRhiVulkan` envolve `vkImg` e executa a barreira de transição `UNDEFINED → VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` antes do render pass principal, mantendo a imagem em `COLOR_ATTACHMENT_OPTIMAL` após `endFrame()`.
  - Ao reutilizar o slot após conclusão do scanout (`releaseFenceFd` sinalizado), o KMS não altera o layout lógico do Vulkan; descartar conteúdo anterior com `UNDEFINED → COLOR_ATTACHMENT_OPTIMAL` continua válido.
  - Em `readbackImage()`, a imagem é assumida em `COLOR_ATTACHMENT_OPTIMAL` (consistente com o estado pós-`endFrame()`), transicionada para `TRANSFER_SRC_OPTIMAL` e restaurada para `COLOR_ATTACHMENT_OPTIMAL`.

---

## 3. Auditoria de Semáforos e `SYNC_FD`

Inspeção de `QRhi::setQueueSubmitParams()`, `QRhiVulkanQueueSubmitParams`, `vkImportSemaphoreFdKHR` e `vkGetSemaphoreFdKHR`:

1. **Consumo de payload temporário em `vkWaitSemaphoreHandle`:**
   - `vkImportSemaphoreFdKHR` com `VK_SEMAPHORE_IMPORT_TEMPORARY_BIT` substitui temporariamente o payload do semáforo pelo `SYNC_FD` importado (`releaseFenceFd`).
   - **Defeito corrigido:** se `vkImportSemaphoreFdKHR` fosse bem-sucedido e `m_renderControl->sync()` retornasse `false` (frame abortado sem `vkQueueSubmit`), o payload temporário ficava pendente em `vkWaitSemaphoreHandle`. Agora, quando `!synced && hasImportedWaitSemaphore`, `vkWaitSemaphoreHandle` é destruído e recriado limpo.
2. **Estado do semáforo de sinalização após falha de exportação:**
   - Em operação normal, `vkGetSemaphoreFdKHR(SYNC_FD)` consome e reseta `vkSignalSemaphoreHandle` para não sinalizado (transferência temporária).
   - **Defeito corrigido:** se `vkGetSemaphoreFdKHR` falhar (sem `DEVICE_LOST`), `vkSignalSemaphoreHandle` poderia permanecer sinalizado; agora ele é destruído e recriado para nunca re-submeter um semáforo binário já sinalizado.
3. **Ordem de `setQueueSubmitParams(nullptr)`:** chamada imediatamente após `m_renderControl->endFrame()`, evitando ponteiros pendentes na stack.
4. **Ausência de ciclos:** Frame `N` espera apenas `releaseFenceFd` de scanout anterior do mesmo slot e sinaliza `acquireFenceFd` após `endFrame()`.

---

## 4. Ownership de Recursos

| Recurso | Criador | Dono | Quem acessa | Quando pode reutilizar | Quando pode destruir |
|---|---|---|---|---|---|
| `VkImage` / `VkDeviceMemory` (slot) | `VulkanQuickRenderer::createSharedPoolSlots()` | Pool unificado (`VulkanQuickRenderer` + `GbmBufferManager`) | QRhi Vulkan (render), KMS via `dmabufFd` | Slot em `Available` após `releaseFenceFd` sinalizado | `destroySharedPoolSlots()` após `vkDeviceWaitIdle` (omitido se `deviceLost`) |
| `dmabufFd` | `vkGetMemoryFdKHR` | `GbmExportedBuffer` | `LibdrmAtomicKmsBackend::addFramebuffer` (`drmPrimeFDToHandle`) | Imutável durante vida do slot | `destroySharedPoolSlots()` / `releaseAllBuffers()` (`fcntl == -1`) |
| `vkSignalSemaphoreHandle` / `vkWaitSemaphoreHandle` | `VulkanQuickRenderer` | Slot em `GbmExportedBuffer` | `QRhiVulkan` queue submit | Após `vkGetSemaphoreFdKHR` (`SYNC_FD`) / consumo na fila | `destroySharedPoolSlots()` após `vkDeviceWaitIdle` |
| `acquireFenceFd` (`SYNC_FD`) | `vkGetSemaphoreFdKHR` | `GbmExportedBuffer` → `AtomicKmsPresenter::submitBufferForScanout` | `drmModeAtomicCommit` (`IN_FENCE_FD`) ou `poll()` | Nunca reutilizado (fechado após commit) | Fechado em `submitBufferForScanout()` ou `releaseBufferToAvailable()` |
| `releaseFenceFd` (`OUT_FENCE_PTR`) | `drmModeAtomicCommit` | `AtomicKmsPresenter` → buffer `Presented` anterior | `VulkanQuickRenderer::renderIntoBuffer` (`vkImportSemaphoreFdKHR`) | Nunca reutilizado (transferido ao Vulkan ou fechado após `poll()`) | Fechado por `vkImportSemaphoreFdKHR` ou `closeFdIfValid()` |
| `kmsFramebufferId` | `addFramebuffer()` | `AtomicKmsPresenter` | `drmModeAtomicCommit` (`FB_ID`) | Enquanto slot existir no mesmo FD DRM | `releaseKmsFramebuffers()` antes de `destroySharedPoolSlots()` |

---

## 5. Auditoria VALO V2

- `ValoMaterialSurfaceV2.qml` e `ValoDockV2.qml` criam instâncias independentes de `ValoBackdropProviderV2`, cada qual com seu próprio `ValoGpuBlurPipelineV2`.
- Todas compartilham o mesmo `QRhi *` (`m_resourceWindow->rhi()`) e o mesmo `QRhiCommandBuffer *cb` fornecido em `beforeRendering`.
- Cada instância possui seus próprios `QRhiTexture`, `QRhiTextureRenderTarget`, `QRhiBuffer` (UBO 80 bytes std140) e `QRhiShaderResourceBindings`.
- Não há render passes aninhados (`beginPass`..`endPass` sequenciais antes do pass principal do Scene Graph) nem leitura/escrita simultânea na mesma textura (`m_downPasses[i-1]` → `m_downPasses[i]`, `m_upPasses[i-1]` → `m_upPasses[i]`).
- `ValoBackdropProviderV2::releaseResources()` agenda a destruição na `renderThread` via `scheduleRenderJob` se chamado da GUI thread.

---

## 6. Comparação TTY3 × Harness (`sge_vulkan_dmabuf_harness`)

| Elemento | TTY3 (`main.cpp`) | Harness (`sge_vulkan_dmabuf_harness.cpp`) | Status |
|---|---|---|---|
| QPA Vulkan | `sge_vulkan_offscreen` | `sge_vulkan_offscreen` | Equivalente |
| `QQuickRenderControl` | Sim | Sim | Equivalente |
| `ShellRoot.qml` | Sim | Sim (Cenários C, G, H) | Equivalente |
| `SystaliaCompositor` + `QWaylandQuickOutput` | Sim (`QWaylandOutputFrameSink`) | **Atualizado:** agora instancia `QWaylandOutputFrameSink` + `WaylandFrameCoordinator` | Equivalente |
| Cinco blurs VALO V2 | Observados | Sim (Cenários C, G, H) | Equivalente |
| Pool Vulkan/DMA-BUF | `createSharedPoolSlots(..., 2)` | `createSharedPoolSlots(..., 2)` | Equivalente |
| `IndependentFrameLoop` + `AtomicKmsPresenter` | Sim (libdrm real) | **Atualizado:** integrado com `MockAtomic` + `DrmEventDispatcher` | Equivalente (simulado) |
| Resolução `1920×1080` | Sim | Preparada em D–H (bloqueada por segurança) | Gated |
| `XWayland` | `startXWayland(1)` ativo | `stopXWayland()` no harness | Diferença intencional (isolamento) |
| Atomic KMS físico | Planejado (não alcançado antes do hang) | Ausente (sem DRM master) | Diferença intencional |

---

## 7. Instrumentação (`SGE_GRAPHICS_DIAGNOSTICS=1`)

Logs validados na execução do Cenário B (`sge_vulkan_dmabuf_harness --scenario B --frames 2`):
- Separação explícita entre:
  - `CPU_COMMAND_RECORDED` (após `m_renderControl->render()`)
  - `GPU_SUBMITTED` (após `m_renderControl->endFrame()`)
  - `SYNC_FD exported` (`acquireFenceFd`)
  - `GPU_COMPLETED` (somente em `IndependentFrameLoop::onPageFlipCompleted()` / confirmação real, nunca apenas pelo retorno de `endFrame()`).
- Inclui `frameId`, `slotId`, `size`, `vkFormat`, `modifier`, `expectedLayout`, `waitSem`, `signalSem`, `releaseFenceFd` e `commitId`.

---

## 8. Problemas Confirmados

1. `vkWaitSemaphoreHandle` mantinha payload temporário não consumido caso `m_renderControl->sync()` abortasse após `vkImportSemaphoreFdKHR`.
2. `vkSignalSemaphoreHandle` não era recriado se `vkGetSemaphoreFdKHR` falhasse (sem `DEVICE_LOST`).
3. `sge_vulkan_dmabuf_harness` anteriormente não incluía `QWaylandOutputFrameSink`, `WaylandFrameCoordinator`, `AtomicKmsPresenter` e `IndependentFrameLoop`.

---

## 9. Correções Implementadas

- [VulkanQuickRenderer.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/VulkanQuickRenderer.cpp): reciclagem de `vkWaitSemaphoreHandle` em abort pós-importação, reciclagem de `vkSignalSemaphoreHandle` em falha de exportação, e logs `CPU_COMMAND_RECORDED` / `GPU_SUBMITTED` / `expectedLayout` sob `SGE_GRAPHICS_DIAGNOSTICS=1`.
- [IndependentFrameLoop.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/IndependentFrameLoop.cpp): log `GPU_COMPLETED` em `onPageFlipCompleted()` sob `SGE_GRAPHICS_DIAGNOSTICS=1`.
- [sge_vulkan_dmabuf_harness.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/tools/diagnostics/sge_vulkan_dmabuf_harness.cpp): paridade estrutural com `WaylandFrameCoordinator` + `AtomicKmsPresenter` (`MockAtomic`) + `IndependentFrameLoop` (sem DRM master).
- [tst_VulkanDmabufAudit.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/tests/unit/tst_VulkanDmabufAudit.cpp): adicionado `recyclesWaitSemaphoreWhenFrameAbortedAfterImport()`.

---

## 10. Testes Executados (após a última modificação)

- **Build Release (`-DCMAKE_CXX_FLAGS="-Werror"`):** limpa.
- **Harness seguro (`SGE_GRAPHICS_DIAGNOSTICS=1`):** Cenário B (`128×128`, DMA-BUF, 2 frames) aprovado com rastreamento completo `CPU_COMMAND_RECORDED → GPU_SUBMITTED → SYNC_FD exported → commit submitted → GPU_COMPLETED`.
- **Testes específicos (11/11 aprovados, `2.10 s`):** incluindo `tst_vulkan_dmabuf_audit` (`0.24 s`).
- **Suíte CTest completa:** **59/59 aprovados** (`100% tests passed, 0 tests failed out of 59`, `50.77 s`: `38 unit`, `20 integration`, `1 e2e`).
- **Smoke tests (`systalia-session` instalado):**
  - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
  - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 11. Cenários Não Executados

- Cenários **C, D, E, F, G e H** do `sge_vulkan_dmabuf_harness` não foram executados automaticamente conforme restrição de segurança.
- Nenhuma execução física em TTY3 (`SGE_USE_DRM_BACKEND=1`) foi realizada.

---

## 12. Riscos Restantes

- A causa raiz do `ring gfx timeout` em `1920×1080` durante o Frame 1 da homologação física anterior continua pendente de confirmação experimental controlada nos cenários `D → E → F → G → H`.

---

## 13. Decisão de Segurança

- **Testes automatizados (CTest / mocks / cenários seguros A–B):** **APTO** (59/59 aprovados).
- **Experimento offscreen controlado (`sge_vulkan_dmabuf_harness` em progressão C → D → E → F → G → H com `--allow-gpu-risk` e `SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1`):** **APTO**, mediante autorização explícita do usuário.
- **Homologação física TTY3 (`SGE_USE_DRM_BACKEND=1`):** **NÃO APTO** até que o experimento offscreen controlado `1920×1080` isole e confirme a ausência de `VK_ERROR_DEVICE_LOST`.
