# SYSTALIA — Etapa 03.2.4.6.21: Análise Forense e Correção do VK_ERROR_DEVICE_LOST na Reutilização de DMA-BUF

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44 (kernel `7.3.4-200.fc44.x86_64`), Qt 6.11.2, C++23, Mesa RADV `26.2.7-1.fc44`, AMD Radeon Renoir (`0x1002:0x1638`), GBM, libdrm e QtWaylandCompositor  
**Baseline:** 60/60 CTest (`39 unit`, `20 integration`, `1 e2e`)  
**Estado físico:** 2 page-flips reais aprovados; falha na 3ª submissão (`tty3-homolog-3frames-20261122-161437.log`); execução física BLOQUEADA nesta etapa

---

## 1. Resumo Executivo

Durante a homologação física de 3 frames na TTY3 (`tty3-homolog-3frames-20261122-161437.log`, `base_commit=546cb1c`), os Frames 1 (Slot 1) e 2 (Slot 2) concluíram submissão Vulkan, Atomic Commit KMS e page-flip real com sucesso. Na 3ª submissão (reutilização do Slot 1), imediatamente após `RELEASE_FENCE_IMPORTED (releaseFenceFd=13)` via `vkImportSemaphoreFdKHR` e registro de comandos (`CPU_COMMAND_RECORDED`), `vkQueueSubmit()` dentro de `m_renderControl->endFrame()` falhou com:

`radv/amdgpu: The CS has been rejected, see dmesg for more information (-2).` → `Device loss detected in vkQueueSubmit()`.

A auditoria do pipeline demonstrou que `VulkanQuickRenderer::renderIntoBuffer()` importava incondicionalmente `slot.releaseFenceFd` (`OUT_FENCE_PTR` do commit KMS anterior) em `waitSem` (`vkImportSemaphoreFdKHR` + `VK_SEMAPHORE_IMPORT_TEMPORARY_BIT`) e o passava em `QRhiVulkanQueueSubmitParams::waitSemaphores`, mesmo quando o `KMS_PAGE_FLIP_COMPLETED` já havia sinalizado a fence no kernel. O código foi corrigido para verificar e aguardar a `sync_file` do KMS via `poll()` (`isSyncFdSignaled`) antes de gravar/submeter o frame, fechando o FD com segurança e evitando submeter `OUT_FENCE_PTR` em `QRhiVulkanQueueSubmitParams::waitSemaphores`.

---

## 2. Linha do Tempo Física dos Três Frames (`tty3-homolog-3frames-20261122-161437.log`)

1. **Inicialização:** `HDMI-A-1` `1920×1080@60Hz`, `RADV RENOIR` (`0x1002:0x1638`), modificador `0x200000000401901`, 2 slots DMA-BUF (`dmabufFd=65` e `64`, `stride=7680`, `allocBytes=8912896`).
2. **Frame 1 (Slot 1):** `releaseFenceFd=-1` → 5 blurs VALO V2 → `CPU_COMMAND_RECORDED` → `GPU_SUBMITTED` → `SYNC_FD exported (acquireFenceFd=79)` → `KMS_COMMIT_SUBMITTED (commitId=1, bufferId=1, fbId=151, inFenceFd=79, outFenceFd=81, succeeded=true)` → `KMS_PAGE_FLIP_COMPLETED (seq=1836446)` → `KMS_BUFFER_RELEASED`.
3. **Frame 2 (Slot 2):** `releaseFenceFd=-1` → `CPU_COMMAND_RECORDED` → `GPU_SUBMITTED` → `SYNC_FD exported (acquireFenceFd=10)` → `KMS_COMMIT_SUBMITTED (commitId=2, bufferId=2, fbId=150, inFenceFd=10, outFenceFd=13, succeeded=true)` → `RELEASE_FENCE_CREATED (outFenceFd=13)` → `KMS_PAGE_FLIP_COMPLETED (seq=1836448)` → `KMS_BUFFER_RELEASED` (Slot 1 recebe `releaseFenceFd=13` e volta a `Available`).
4. **Frame 3 (Slot 1):** `FRAME_START frameId=3 slotId=1 releaseFenceFd=13` → `RELEASE_FENCE_IMPORTED frameId=3 slotId=1 releaseFenceFd=13` → `CPU_COMMAND_RECORDED frameId=3 slotId=1` → `radv/amdgpu: The CS has been rejected, see dmesg for more information (-2)` → `Device loss detected in vkQueueSubmit()` → `RENDERER_SHUTDOWN_COMPLETE deviceLost=true`, XWayland encerrado normalmente (`Código: 0`), saída `rc=1`.

---

## 3. Análise da Release Fence

- `commitAtomic()` do Frame 2 recebeu `outFenceFd=13` via `OUT_FENCE_PTR` no CRTC `108`.
- `AtomicKmsPresenter::onPageFlipCompleted(2, -1)` transferiu `m_pendingOutFenceFd` (`13`) para `GbmBufferManager::markBufferPresented(2, 0, 13)`, anexando `releaseFenceFd=13` ao Slot 1 (que estava `Presented`) e transitando Slot 1 para `Available`.
- A associação ao slot correto está íntegra: o Slot 1 deixou de ser exibido quando o Commit 2 completou.
- Como `KMS_PAGE_FLIP_COMPLETED` do Commit 2 já havia ocorrido antes do início do Frame 3, `releaseFenceFd=13` já representava um flip concluído.

---

## 4. Contrato DRM/KMS

- **Garantia de `KMS_PAGE_FLIP_COMPLETED`:** evento assíncrono do DRM indicando que o flip do novo framebuffer ocorreu no vblank/CRTC; nesse ponto o buffer anterior não é mais varrido pelo CRTC.
- **Garantia de `OUT_FENCE_PTR`:** `sync_file` criado no CRTC pelo kernel (`drm_crtc_create_out_fence`) que sinaliza quando o commit atômico passa a valer no display (liberando o framebuffer anterior).
- **Regra aplicada:** `KMS_PAGE_FLIP_COMPLETED` continua liberando o slot para `Available` carregando `releaseFenceFd`, e `VulkanQuickRenderer::renderIntoBuffer()` exige confirmação explícita da `releaseFenceFd` via `poll()` (`POLLIN`) antes de reutilizar a `VkImage` (falhando de forma fail-closed em timeout/erro).

---

## 5. Contrato Vulkan/QRhi

- `QRhiVulkanQueueSubmitParams` (`qrhi_platform.h`) expõe `waitSemaphoreCount`/`waitSemaphores` e `signalSemaphoreCount`/`signalSemaphores`, sem campo público para `pWaitDstStageMask`.
- Nos testes offscreen (`tst_VulkanKmsSync`), importar `SYNC_FD` exportado pelo próprio RADV (`vkGetSemaphoreFdKHR`) em `waitSem` funcionou; porém, na execução física na TTY3, importar a `sync_file` de `OUT_FENCE_PTR` do DRM KMS em `waitSem` (`VK_SEMAPHORE_IMPORT_TEMPORARY_BIT`) e submetê-la via `QRhiVulkanQueueSubmitParams` coincidiu exatamente com a rejeição `-2` (`-ENOENT`) em `vkQueueSubmit()`.
- Ao aguardar/consumir a `releaseFenceFd` do KMS via `poll()` (`isSyncFdSignaled`) no host antes da gravação/submissão e manter `waitSemaphoreCount = 0` no `QRhiVulkanQueueSubmitParams`, elimina-se a dependência de `drmSyncobjImportSyncFile` + `in_syncobj` para `OUT_FENCE_PTR` dentro de `vkQueueSubmit()`.

---

## 6. Análise de Layouts

- As `VkImage` do pool são criadas com `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT`, `initialLayout = VK_IMAGE_LAYOUT_UNDEFINED` e uso `COLOR_ATTACHMENT_BIT | TRANSFER_SRC_BIT | SAMPLED_BIT`.
- Em cada frame, `QQuickRenderTarget::fromVulkanImage(vkImg, VK_IMAGE_LAYOUT_UNDEFINED, m_vkFormat, m_pixelSize, 1)` informa ao `QRhi` que o conteúdo anterior do slot não precisa ser preservado, permitindo transição limpa `UNDEFINED → COLOR_ATTACHMENT_OPTIMAL`.
- Em `destroySharedPoolSlots()`, foi adicionado `m_window->setRenderTarget(QQuickRenderTarget{})` antes de destruir as `VkImage` do pool para evitar referências residuais no `QQuickWindow`.

---

## 7. Ownership de Recursos

| Recurso | Criador | Proprietário | Transferência | Destruição |
|---|---|---|---|---|
| DMA-BUF FD | `VulkanQuickRenderer` (`vkGetMemoryFdKHR`) | `GbmExportedBuffer` (`GbmBufferManager`) | Compartilhado por FD com `drmPrimeFDToHandle` em `addFramebuffer` | `VulkanQuickRenderer::destroySharedPoolSlots()` (`::close`) |
| `acquireFenceFd` (`SYNC_FD`) | `VulkanQuickRenderer` (`vkGetSemaphoreFdKHR`) | `GbmExportedBuffer` → `AtomicKmsPresenter` | `IN_FENCE_FD` no `commitAtomic`; fechado após commit ou flip | `AtomicKmsPresenter::commitAtomic()` / `GbmBufferManager` (`::close`) |
| `releaseFenceFd` (`OUT_FENCE_PTR`) | DRM Kernel (`commitAtomic`) | `AtomicKmsPresenter::m_pendingOutFenceFd` → `GbmExportedBuffer` | `onPageFlipCompleted()` → `markBufferPresented()` no slot anterior | `VulkanQuickRenderer::renderIntoBuffer()` (`::close` após `poll`) ou `GbmBufferManager`/`AtomicKmsPresenter` |
| `VkSemaphore` (`wait`/`signal`) | `VulkanQuickRenderer` (`createSharedPoolSlots`) | `GbmExportedBuffer` (handles) / `VulkanQuickRenderer` (lifetime) | `signalSem` passado ao `QRhiVulkanQueueSubmitParams`; quarentena se falhar exportação | `VulkanQuickRenderer::destroySharedPoolSlots()` após `vkDeviceWaitIdle` |
| `VkImage` / `VkDeviceMemory` | `VulkanQuickRenderer` (`createSharedPoolSlots`) | `VulkanQuickRenderer` (alocação dedicada DMA-BUF) | Handle passado por frame via `QQuickRenderTarget::fromVulkanImage` | `VulkanQuickRenderer::destroySharedPoolSlots()` após `setRenderTarget({})` e `vkDeviceWaitIdle` |
| DRM FB (`fbId`) | `LibdrmAtomicKmsBackend::addFramebuffer` | `AtomicKmsPresenter` / `GbmExportedBuffer` | Criado sob demanda quando `kmsFramebufferId == 0`; `gemHandle` fechado logo após `AddFB2` | `AtomicKmsPresenter::releaseKmsFramebuffers()` (`drmModeRmFB`) |

---

## 8. Defeitos Identificados

1. **Importação incondicional de `OUT_FENCE_PTR` em `vkImportSemaphoreFdKHR` + `QRhiVulkanQueueSubmitParams` (`src/platform/drm/VulkanQuickRenderer.cpp`):** importava `slot.releaseFenceFd` mesmo após `KMS_PAGE_FLIP_COMPLETED` já ter sinalizado a fence, submetendo `in_syncobj` oriundo de `OUT_FENCE_PTR` ao `vkQueueSubmit()` do `QRhi` — ponto exato da rejeição `-2` (`-ENOENT`) no RADV/AMDGPU.
2. **Ausência de limpeza explícita de `renderTarget` em `destroySharedPoolSlots()` (`src/platform/drm/VulkanQuickRenderer.cpp`):** `m_window` podia manter o último `QQuickRenderTarget` apontando para a `VkImage` do slot durante a destruição do pool.
3. **Observabilidade insuficiente pré-submissão:** faltava log do handle `vkImage`, layout anterior (`prevLayout`) e resultado da verificação da `releaseFenceFd` em `SGE_GRAPHICS_DIAGNOSTICS=1`.

---

## 9. Correções Aplicadas

1. **`src/platform/drm/VulkanQuickRenderer.cpp` (`renderIntoBuffer`):** substituída a importação de `slot.releaseFenceFd` via `vkImportSemaphoreFdKHR` por verificação imediata `isSyncFdSignaled(observedFd, 0)` (caminho não-bloqueante quando já sinalizada após `KMS_PAGE_FLIP_COMPLETED`) com fallback bloqueante limitado via `poll()` (`isSyncFdSignaled(observedFd, 2000)`) caso ainda pendente, fechando `observedFd` e abortando em fail-closed se `!signaled`.
2. **`src/platform/drm/VulkanQuickRenderer.cpp` (`destroySharedPoolSlots`):** adicionado `m_window->setRenderTarget(QQuickRenderTarget{})` antes de `flushQuarantinedSemaphores()` e destruição dos slots, além de log `SLOT_RESOURCES_DESTROYED`.
3. **`src/platform/drm/VulkanQuickRenderer.cpp` (`renderIntoBuffer`):** adicionados `vkImage`, `prevLayout=UNDEFINED`, `nonBlocking` e `importResult=skipped_sync_file_poll` aos logs `[SGE-DIAG]` sob `SGE_GRAPHICS_DIAGNOSTICS=1`.
4. **`tests/unit/tst_VulkanDmabufAudit.cpp`:** adicionado `slotReuseScenariosAtoDAndShellRootWithReleaseFence` cobrindo cenários A (sem fence), B (release fence sinalizada), C (release fence pendente) e D (falha/POLLHUP na release fence sem retorno indevido a `Available`).

---

## 10. Testes de Regressão

Executados após as edições finais em compilação limpa `Release` com `-Werror`:

- **Testes específicos (`tst_vulkan_kms_sync` + `tst_vulkan_dmabuf_audit`):** `2/2` aprovados (`0.60 sec`).
- **Suíte completa CTest (`ctest --output-on-failure`):** **`60/60` aprovados** (`100% tests passed, 0 tests failed out of 60`, `51.81 sec`: `39 unit`, `20 integration`, `1 e2e`).
- **Smoke test offscreen (`QT_QPA_PLATFORM=offscreen systalia_glass_engine`):** encerramento limpo via `SIGTERM` (`OFFSCREEN_SMOKE_RC=124` do `timeout` sem crash/abort).
- **Nenhuma execução física DRM/KMS realizada.**

---

## 11. Diagnóstico RADV

- A mensagem `radv/amdgpu: The CS has been rejected, see dmesg for more information (-2)` corresponde a `-ENOENT` retornado por `amdgpu_cs_ioctl` durante a submissão do command stream em `radv_amdgpu_cs_submit()`.
- No kernel `7.3.4`, `journalctl -k` no intervalo `16:14:00–16:16:00` não registrou mensagens adicionais do `amdgpu` (`-- No entries --`), comportamento compatível com retorno silencioso `-ENOENT` em lookup de dependência/recurso no ioctl (como `drm_syncobj_find_fence()` em `amdgpu_cs_syncobj_in`).
- Como a falha ocorreu exclusivamente na 3ª submissão real após `RELEASE_FENCE_IMPORTED` com `OUT_FENCE_PTR` do KMS, e não foi reproduzida quando `OUT_FENCE_PTR` não é importado em `vkQueueSubmit()`, a eliminação dessa importação remove o gatilho observado; todavia, sem novo teste físico autorizado na TTY3, a confirmação em hardware real permanece pendente.

---

## 12. Riscos Restantes

- A correção ainda precisa ser confirmada fisicamente em um novo teste controlado de 3 frames na TTY3 com `OUT_FENCE_PTR` real do DRM KMS.
- O diagnóstico histórico do `ring gfx timeout` da primeira homologação física permanece aberto até validação multi-frame física bem-sucedida.
- `/dev/dri/card1` permanece compartilhado com a sessão KDE Plasma na TTY2.

---

## 13. Próximo Experimento Recomendado

Após revisão humana e atualização da instalação isolada (`$HOME/.local/systalia-homolog`), repetir o experimento físico controlado de 3 frames na TTY3 via:

```bash
./tools/homologation/run_tty3_3frames_homolog.sh --authorize-3frames
```

---

## 14. Decisão de Segurança

- **Correção e regressão offscreen/unitária:** **CONCLUÍDAS** (`60/60` CTest aprovados).
- **Execução física:** **BLOQUEADA** aguardando revisão e autorização humana explícita.
