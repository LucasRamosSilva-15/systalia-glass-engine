# SYSTALIA — Etapa 03.2.4.6.1: Investigação Forense do GPU Hang AMDGPU/RADV e Estabilidade do QtWaylandCompositor

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44 (kernel `7.3.4-200.fc44.x86_64`), Qt `6.11.2`, Mesa/RADV `26.2.7-1.fc44`, libdrm `2.4.134-1.fc44`  
**GPU:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Prioridade:** P0 — bloqueador crítico da homologação física

---

## 1. Linha do Tempo da Falha (Boot `7a99b324277b4379bde885327cbdf015`)

- **09:49:28 – 09:51:58:** Quatro crashes históricos de `tst_shell_root_` em `libQt6WaylandCompositor.so.6.11.2` no mesmo offset `0x3088b` (`QWaylandOutput::initialize()`).
- **11:02:53 (`tty3-homolog-20261122-110253.log`):** Início da homologação física na TTY3 (sessão `id=4`, `seat0`). Carregados `sge_vulkan_offscreen`, `libinput`, `Wayland` (`wayland-systalia-0`), `Xwayland :1` e `ShellRoot.qml`.
- **11:02:53–11:02:55:** Cinco instâncias de `ValoBackdropProviderV2` executaram `onBeforeRendering()` / `ValoGpuBlurPipelineV2::execute()` para `1920×1080` (com `rhiSrc=0x2a0a82a0` idêntico e `outRhi` distintos `0x2a100f40`..`0x2a1a20b0`) durante a gravação do **Frame 1** antes do `vkQueueSubmit()` em `QQuickRenderControl::endFrame()`.
- **11:02:56:** Kernel `amdgpu 0000:04:00.0` registrou `ring gfx timeout, signaled seq=820468, emitted seq=820469` no processo `systalia_glass_` (`PID 39575`), resetou o anel GFX e emitiu evento DRM `wedged` (`kwin_wayland` também registrou `/dev/dri/renderD128 was wedged`).
- **11:02:56:** RADV registrou `radv/amdgpu: The CS has been cancelled because the context is lost` e `Device loss detected in vkQueueSubmit()`.
- **Após 11:02:56:** Sem tratamento fail-closed de `VK_ERROR_DEVICE_LOST`, o processo não encerrou o `app.exec()` nem liberou o controle de sessão/VT, impedindo o retorno via `Ctrl+Alt+F2`.

---

## 2. Evidências do Kernel

Extraídas de `journalctl -b 7a99b324277b4379bde885327cbdf015`:
- `amdgpu 0000:04:00.0: ring gfx timeout, signaled seq=820468, emitted seq=820469`
- `amdgpu 0000:04:00.0: Process systalia_glass_ pid 39575 thread systalia_glass_ pid 39575`
- `amdgpu 0000:04:00.0: Starting gfx ring reset` / `Ring gfx reset succeeded` / `[drm] device wedged, but no recovery needed`
- Nenhum erro de `TakeDevice` (`EBUSY`), `libinput`, `KmsOutputDiscovery` ou `drmModeAtomicCommit` apareceu no log `tty3-homolog-20261122-110253.log` antes do `vkQueueSubmit()`.

---

## 3. Estado do Coredump AMDGPU

- `/sys/class/drm/card1/device/devcoredump/data`: **ausente** (`devcoredump: absent` na inspeção atual; não sobreviveu ao ciclo de reset/reinício).
- `coredumpctl list`: nenhum core dump salvo para `systalia_glass_engine` (`PID 39575`) nem para `tst_shell_root_`.
- **Procedimento seguro de coleta futura:** caso um novo `devcoredump` seja gerado pelo kernel em teste controlado, copiar apenas metadados/arquivo via `cp /sys/class/drm/card1/device/devcoredump/data` após o retorno do sistema antes de reiniciar, sem gravar dumps binários em `docs/` ou `artifacts/`.

---

## 4. Hipóteses Investigadas

| Hipótese | Estado | Evidência |
|---|---|---|
| **H1:** Ausência de tratamento fail-closed para `VK_ERROR_DEVICE_LOST` travou a saída e a troca de VT | **Confirmado** | `VulkanQuickRenderer` não consultava `QRhi::isDeviceLost()`, `IndependentFrameLoop` apenas emitia `frameFailed()` e `main.cpp` não encerrava `app.exec()`. |
| **H2:** `VkMemoryDedicatedAllocateInfo` ausente em `createSharedPoolSlots()` para imagens `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` externas | **Confirmado (defeito estrutural)** | `createSharedPoolSlots()` exportava `DMA-BUF` sem encadear `VkMemoryDedicatedAllocateInfo` em `VkMemoryAllocateInfo` (necessário para metadados de tiling/DCC em RADV). |
| **H3:** Falha nos shaders Dual-Kawase (`valo_kawase_down.frag` / `valo_kawase_up.frag`) | **Não confirmado / Improvável** | Shaders possuem apenas 5 e 8 amostras fixas sem loops; `tst_ShellRootIntegration` executa os mesmos shaders em RADV sem hang. |
| **H4:** Mensagem `Failed to initialize EGL display` causou o GPU hang | **Descartado** | Ocorre na inicialização da CPU em `SystaliaCompositor::create()` porque `sge_vulkan_offscreen` define `OpenGL = false`. |
| **H5:** Crashes de `tst_shell_root_` em `0x3088b` causados por destruição prematura de `QQuickWindow` | **Confirmado** | Desmontagem de `libQt6WaylandCompositor.so.6.11.2` em `0x3088b` (`QWaylandOutput::initialize()+0x5b`) lê `window->d_ptr` (`0x8(%rax)` → `0x178(%r14)`). |

---

## 5. Problemas Vulkan Confirmados

1. **Ausência de `VkMemoryDedicatedAllocateInfo` em `VulkanQuickRenderer::createSharedPoolSlots()`:** imagens externas `DMA-BUF` com `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` não recebiam alocação dedicada explícita.
2. **Ausência de detecção e bloqueio de `VK_ERROR_DEVICE_LOST`:** `renderFrame()` e `renderIntoBuffer()` não verificavam `QRhi::isDeviceLost()` após `endFrame()`, nem `vkImportSemaphoreFdKHR`/`vkGetSemaphoreFdKHR` tratavam `VK_ERROR_DEVICE_LOST`, e `destroySharedPoolSlots()` / `shutdown()` chamavam `vkDeviceWaitIdle()` mesmo após perda do dispositivo.

---

## 6. Problemas de Sincronização Confirmados

- Embora `SYNC_FD`, `IN_FENCE_FD`, `OUT_FENCE_PTR` e `PendingFlipContext` estivessem implementados, o **Frame 1** falhou ainda dentro de `vkQueueSubmit()` antes da exportação do primeiro `SYNC_FD` e antes do primeiro `drmModeAtomicCommit()`.
- Quando o erro ocorreu, `IndependentFrameLoop` não possuía estado `m_deviceLost` para impedir novas submissões e manter buffers incertos bloqueados em `TimedOutInScanout`.

---

## 7. Crashes do QtWaylandCompositor

- Os quatro crashes no kernel (`09:49:28` a `09:51:58`) ocorreram no mesmo endereço `0x3088b` de `libQt6WaylandCompositor.so.6.11.2`, dentro de `QWaylandOutput::initialize()` ao desreferenciar `d->window->d_ptr`.
- **Correções aplicadas:**
  - `ShellRoot.qml`: `Component.onDestruction` limpa `primaryWaylandOutput.window = null`.
  - `VulkanQuickRenderer::shutdown()`: limpa `m_rootItem->setProperty("hostWindow", QVariant())` e `m_window->setRenderTarget(QQuickRenderTarget{})` antes de `invalidate()` / `m_window.reset()`.
  - `SystaliaCompositor::~SystaliaCompositor()`: chama `stopXWayland()` no destrutor.

---

## 8. Compatibilidade EGL

- O aviso `qt.waylandcompositor.hardwareintegration: Failed to initialize EGL display. No native platform interface available.` é esperado porque `SgeVulkanOffscreenIntegration::hasCapability(OpenGL)` retorna `false`.
- Não é a causa do GPU hang. Clientes Wayland que dependam exclusivamente de `wayland-egl` não terão compartilhamento EGL nesse QPA sem integração dedicada `linux-dmabuf` / Vulkan. Nenhuma camada EGL fictícia foi adicionada.

---

## 9. Instrumentação Adicionada

Adicionada instrumentação opt-in controlada por **`SGE_GRAPHICS_DIAGNOSTICS=1`** (`VulkanQuickRenderer::graphicsDiagnosticsEnabled()`):
- Registra em `VulkanQuickRenderer::renderIntoBuffer()` (`bufferId`, `QThread::currentThread()`, `vkImage`, `releaseFenceFd`, `exportedSyncFd`).
- Registra em `IndependentFrameLoop` (`commit submitted` com `bufferId`/`commitId`, `VT resumed` com `drmFd`) sem poluir logs quando a variável não está ativa.

---

## 10. Correções Aplicadas

1. **[VulkanQuickRenderer.hpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/VulkanQuickRenderer.hpp) / [VulkanQuickRenderer.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/VulkanQuickRenderer.cpp):**
   - `VkMemoryDedicatedAllocateInfo` encadeado em `VkExportMemoryAllocateInfo` dentro de `createSharedPoolSlots()`.
   - Detecção de `QRhi::isDeviceLost()` e `VK_ERROR_DEVICE_LOST`, sinal `deviceLost()`, bloqueio de submissões futuras, omissão de `vkDeviceWaitIdle()` após device lost, e limpeza de `hostWindow` / `renderTarget` no `shutdown()`.
   - Instrumentação opt-in `SGE_GRAPHICS_DIAGNOSTICS=1`.
2. **[IndependentFrameLoop.hpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/IndependentFrameLoop.hpp) / [IndependentFrameLoop.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/platform/drm/IndependentFrameLoop.cpp):**
   - Adicionado `handleDeviceLost()` fail-closed (bloqueia `requestFrame`/`processNextFrame`/`resume`, desanexa `DrmEventDispatcher`, mantém buffer incerto em `TimedOutInScanout`).
3. **[src/app/main.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/app/main.cpp):**
   - Conectado `VulkanQuickRenderer::deviceLost` a `frameLoop.handleDeviceLost()` e `QCoreApplication::exit(2)` para encerrar com segurança e liberar a sessão `logind`.
4. **[ShellRoot.qml](file:///home/lucasramos/Documentos/systalia-glass-engine/src/shell/core/ShellRoot.qml) e [SystaliaCompositor.cpp](file:///home/lucasramos/Documentos/systalia-glass-engine/src/compositor/core/SystaliaCompositor.cpp):**
   - Limpeza explícita de `primaryWaylandOutput.window = null` em `Component.onDestruction` e `stopXWayland()` em `~SystaliaCompositor()`.

---

## 11. Testes Executados

Executados após a última modificação em build `Release` com `-DCMAKE_CXX_FLAGS="-Werror"`:

1. **Testes específicos (10/10 aprovados, `1.86 s`):**
   - `tst_drm_backend_poc`, `tst_drm_gbm_integration`, `tst_vulkan_quick_renderer`, `tst_independent_vulkan_session`, `tst_independent_frame_loop`, `tst_shell_root_integration`, `tst_unified_vulkan_gbm_pool`, `tst_vulkan_kms_sync`, `tst_page_flip_and_unified_startup` e o novo `tst_device_lost_and_compositor_lifecycle` (`tests/unit/tst_DeviceLostAndCompositorLifecycle.cpp`, `0.33 s`).
2. **Suíte CTest completa:** **58/58 aprovados** (`100% tests passed, 0 tests failed out of 58`, `50.55 s`: `38 unit`, `19 integration`, `1 e2e`).
3. **PoC Vulkan/RADV (`sge_vulkan_quick_poc`):** `PHASE_A` a `PHASE_E` aprovadas em `AMD Radeon Graphics (RADV RENOIR)`.
4. **Smoke tests (`systalia-session` instalado):**
   - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
   - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 12. Limitações

- Como o `devcoredump` do kernel não sobreviveu ao reset/reboot, não foi possível inspecionar os registradores da GPU ou o command buffer exato que causou o `ring gfx timeout` (`seq=820469`).
- Nenhum teste físico em TTY3 ou `SGE_USE_DRM_BACKEND=1` foi executado nesta etapa por restrição de segurança.

---

## 13. Riscos Restantes

1. **Causa exata do `ring gfx timeout` em `1920×1080` ainda não comprovada em hardware:** embora `VkMemoryDedicatedAllocateInfo` corrija uma omissão estrutural na alocação DMA-BUF com modificadores e `handleDeviceLost()` garanta saída segura em caso de falha, não está comprovado sem coredump se o hang no Frame 1 decorreu apenas da falta de alocação dedicada ou de interação entre RADV/QRhi (`VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` com modificador comprimido/DCC vs `DRM_FORMAT_MOD_LINEAR`) e os 5 pipelines `ValoGpuBlurPipelineV2` em `1920×1080`.
2. **Primeiro commit KMS sem `allowModeset = true`:** já documentado na Etapa 03.2.4.6.

---

## 14. Decisão de Segurança

**NÃO APTO** para um novo teste físico assistido da shell completa neste momento.

**Justificativa objetiva:** O `devcoredump` do `ring gfx timeout` não está mais disponível para confirmar a causa raiz exata no CS submetido pelo RADV em `1920×1080`, e testes simulados/offscreen de `128×128` ou sem `createSharedPoolSlots()` não comprovam sozinhos que o hang em `1920×1080` com `ShellRoot.qml` + pool unificado foi eliminado. Recomenda-se antes uma verificação isolada controlada (ex.: renderizar 1 frame de `ShellRoot.qml` em `1920×1080` sobre `createSharedPoolSlots()` com `DRM_FORMAT_MOD_LINEAR` vs modificador RADV em modo `sge_vulkan_offscreen` sem KMS físico e com `SGE_GRAPHICS_DIAGNOSTICS=1`) antes de autorizar nova tentativa na TTY3.
