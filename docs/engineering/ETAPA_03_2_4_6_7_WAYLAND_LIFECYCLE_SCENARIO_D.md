# SYSTALIA — Etapa 03.2.4.6.7: Correção do Ciclo de Vida Wayland e Preparação do Experimento Vulkan 1920×1080

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV e QtWaylandCompositor  
**GPU de referência:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline de regressão:** 59/59 testes CTest (`38 unit`, `20 integration`, `1 e2e`)  
**Estado da homologação física:** BLOQUEADA

---

## 1. Resultado Histórico do Cenário C

O primeiro experimento offscreen controlado do **Cenário C** foi concluído em `2026-11-22T12:54:09-03:00` (commit `14cf98a`, log `$HOME/systalia-homolog-logs/offscreen-exp-C-20261122-125409.log`):

- **Resultado:** `[PASS] Cenário C concluiu 1 frames.` (`rc=0`)
- **Resolução:** `128×128`
- **Composição:** `ShellRoot.qml` carregado com 5 instâncias de blur GPU `ValoBackdropProviderV2`
- **Pool DMA-BUF Vulkan:** modificador `0x200000000401a01`
- **Ciclo GPU observado:** `CPU_COMMAND_RECORDED` → `GPU_SUBMITTED` → `SYNC_FD exported (acquireFenceFd=51)` → `commit submitted` → `GPU_EXECUTION_COMPLETED (syncFd=52, gpuLatencyUs=32)` → `MOCK_PAGE_FLIP_COMPLETED`
- **Estabilidade:** nenhum `VK_ERROR_DEVICE_LOST` registrado.

No encerramento desse experimento, ocorreram dois avisos relacionados ao `QWaylandOutput`:

```text
qrc:/qt/qml/Systalia/Engine/src/qml/ShellRoot.qml:483:9: Unable to assign [undefined] to QWindow*
Setting QWindow 0x0 on QWaylandOutput 0x237f4d70 is not supported after QWaylandOutput has been initialized
```

---

## 2. Diagnóstico dos Avisos `QWaylandOutput`

1. **`Unable to assign [undefined] to QWindow*` (`ShellRoot.qml:483`):**  
   `VulkanQuickRenderer::shutdown()` executava `m_rootItem->setProperty("hostWindow", QVariant())`. Em QML, um `QVariant()` inválido equivale a `undefined`, disparando a reavaliação do binding `window: shellRoot.hostWindow` em `primaryWaylandOutput` (`QWaylandQuickOutput::window`, de tipo `QWindow*`).
2. **`Setting QWindow 0x0 on QWaylandOutput ... is not supported after QWaylandOutput has been initialized`:**  
   No contrato do `QWaylandOutput` (Qt 6.11.2), a propriedade `window` não pode ser reatribuída (inclusive para `nullptr`/`0x0`) após `QWaylandOutput::initialize()` ter sido executado. O aviso era disparado por `Component.onDestruction: { if (primaryWaylandOutput) { primaryWaylandOutput.window = null } }` em `src/shell/core/ShellRoot.qml` e por qualquer limpeza de `hostWindow` antes da destruição de `primaryWaylandOutput`.

---

## 3. Correções Implementadas

| Arquivo | Correção aplicada |
| --- | --- |
| `src/shell/core/ShellRoot.qml` | Remoção do bloco `Component.onDestruction` que atribuía `primaryWaylandOutput.window = null` após a inicialização do `QWaylandOutput`. |
| `src/platform/drm/VulkanQuickRenderer.hpp` / `.cpp` | Remoção de `m_rootItem->setProperty("hostWindow", QVariant())` em `shutdown()`; guarda de idempotência `m_shuttingDown`; uso de `QPointer<QQuickItem> m_rootItem` e `QPointer<QQmlEngine> m_activeEngine`; parâmetro `allowDmabufExport` em `initialize(pixelSize, allowDmabufExport = true)` para isolar o render target interno sem `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`; observação real de conclusão GPU no caminho interno via `observeInternalGpuCompletion()` (`VkFence` + `vkQueueSubmit(0, nullptr, fence)` + `vkWaitForFences`). |
| `src/platform/drm/WaylandFrameCoordinator.hpp` | Substituição de `QWaylandQuickOutput *m_output` por `QPointer<QWaylandQuickOutput> m_output` em `QWaylandOutputFrameSink`, tornando callbacks tardios após destruição do output no-ops seguros (sem UAF). |
| `tools/diagnostics/sge_vulkan_dmabuf_harness.cpp` | Inicialização com `renderer.initialize(spec.size, spec.useDmabufPool)`, verificação explícita de `0` instâncias de `ValoBackdropProviderV2` e `ValoGlassItemV2` quando `!spec.loadValoShell`, default de `1` frame para o Cenário D quando `--frames` não é informado, e logs `[SGE-DIAG]` de configuração (`SCENARIO_CONFIG`) e estado final (`RENDERER_FINAL_STATE`). |
| `tests/unit/tst_DeviceLostAndCompositorLifecycle.cpp` | Ampliação dos testes de ciclo de vida cobrindo ausência dos avisos `QWaylandOutput` durante teardown de `ShellRoot.qml`, callbacks tardios pós-destruição (`lateCallbacksAfterOutputDestructionAreIgnoredSafely`), falha parcial e ciclos repetidos seguros (`partialFailureAndRepeatedSafeCreateDestroyCycles`). |

---

## 4. Ordem de Destruição Validada

A sequência de encerramento auditada e preservada é:

`IndependentFrameLoop::shutdown()` → `WaylandFrameCoordinator` (`m_suspended = true`, `QPointer<QWaylandQuickOutput>` em `QWaylandOutputFrameSink`) → destruição de `m_rootItem` (`ShellRoot` → `QWaylandQuickOutput` → `SystaliaCompositor::~SystaliaCompositor()` → `stopXWayland()`) enquanto `m_window` ainda é válida → `m_qmlComponent.reset()` → `m_ownedQmlEngine.reset()` → `m_window->setRenderTarget(QQuickRenderTarget{})` → `m_renderControl->invalidate()` → `m_window.reset()` → `m_renderControl.reset()` → `vkDeviceWaitIdle` (com verificação de `VK_ERROR_DEVICE_LOST`) → `flushQuarantinedSemaphores()` → `destroyOffscreenImage()` → `vkDestroyCommandPool()` → `destroyVulkanDevice()` → `m_vulkanInstance.destroy()`.

---

## 5. Testes de Lifecycle

Em `tests/unit/tst_DeviceLostAndCompositorLifecycle.cpp` (`tst_device_lost_and_compositor_lifecycle`, aprovado em `0.69 s`):

- `waylandCompositorOutputTeardownClearsWindowSafely`: carrega `ShellRoot.qml`, valida `QWaylandQuickOutput`, renderiza 1 frame com conclusão GPU `Completed`, executa `renderer.shutdown()` e confirma via `qInstallMessageHandler` que **nenhum** dos dois avisos (`Unable to assign [undefined] to QWindow*` ou `is not supported after QWaylandOutput has been initialized`) foi emitido.
- `lateCallbacksAfterOutputDestructionAreIgnoredSafely`: confirma que `WaylandFrameCoordinator::beginFrame()` / `completeFrame()` e `QWaylandOutputFrameSink` após `renderer.shutdown()` não causam UAF nem segfault.
- `partialFailureAndRepeatedSafeCreateDestroyCycles`: valida shutdown idempotente após falha de carregamento QML e 3 ciclos consecutivos `128×128` internos sem DMA-BUF e sem VALO V2.

---

## 6. Auditoria do Cenário D

Configuração efetiva verificada no código para `--scenario D`:

- **Resolução:** `1920×1080`
- **Backend:** Vulkan/RADV via QPA `sge_vulkan_offscreen`
- **Render target:** interno (`spec.useDmabufPool = false`, `allowDmabufExport = false`, `dmabufExportImageCreated = false`, sem `VK_EXTERNAL_MEMORY_IMAGE_CREATE_INFO`)
- **Modificadores DRM externos:** desativados (`forceLinearModifier = false`)
- **Efeitos VALO V2:** desativados (`loadValoShell = false`)
- **Quantidade padrão de frames:** `1` frame (`defaultFrames = 1` para `D`)
- **DRM/KMS físico:** desativado (`atomicKms=false`, `drmMaster=false`, `takeDevice=false`; sem abertura de `/dev/dri/card*`).

---

## 7. Comprovação de Desativação dos Efeitos

Quando `!spec.loadValoShell` (Cenários A e D), `sge_vulkan_dmabuf_harness.cpp` carrega exclusivamente:
`"import QtQuick 2.15\nRectangle { anchors.fill: parent; color: '#0b1e36' }\n"`
e executa verificação programática na árvore instanciada (`findChildren<ValoBackdropProviderV2 *>()` e `findChildren<ValoGlassItemV2 *>()`), abortando com falha se qualquer instância existir. Nos testes executados no Cenário A, ambos os contadores registraram `valoPasses=0 valoGlassItems=0`.

---

## 8. Instrumentação de Conclusão GPU (Caminho Interno)

O caminho Vulkan interno (`VulkanQuickRenderer::renderFrame()`) não utiliza `SYNC_FD`. Para observar conclusão GPU real sem interferir no `QRhi` nem modificar `QRhiVulkanQueueSubmitParams`:

- Após `m_renderControl->endFrame()` e verificação de `rhi->isDeviceLost()`, `observeInternalGpuCompletion()` cria um `VkFence`, submete `vkQueueSubmit(m_graphicsQueue, 0, nullptr, fence)` (barreira de fila permitida pela especificação Vulkan que sinaliza após todos os comandos anteriores na `m_graphicsQueue` concluírem) e aguarda `vkWaitForFences(m_device, 1, &fence, VK_TRUE, timeoutNs)`.
- `GPU_EXECUTION_COMPLETED` (`method=VkFence`, `gpuLatencyUs`) e a transição `GpuSubmissionTracker::markCompleted()` só ocorrem quando `vkWaitForFences` retorna `VK_SUCCESS`.
- `VK_ERROR_DEVICE_LOST` e `VK_TIMEOUT` marcam `DeviceLost` / `CompletionTimedOut` (`GPU_COMPLETION_TIMED_OUT`), retornando `false`.

---

## 9. Resultados da Regressão (Após Edições Finais)

1. **Build Release com `-Werror`:** compilação limpa sem erros ou warnings.
2. **Testes específicos de lifecycle/sincronização (`4/4` em `1.82 s`):**
   - `tst_vulkan_quick_renderer`: `Passed` (`0.09 s`)
   - `tst_shell_root_integration`: `Passed` (`0.73 s`)
   - `tst_device_lost_and_compositor_lifecycle`: `Passed` (`0.70 s` / `0.69 s` na suíte completa)
   - `tst_vulkan_dmabuf_audit`: `Passed` (`0.29 s`)
3. **Suíte completa CTest:** `59/59` aprovados (`100% tests passed, 0 tests failed out of 59`) em `51.89 s` (`38 unit`, `20 integration`, `1 e2e`).
4. **Harness seguro (Cenários A e B, 2 frames cada, `SGE_GRAPHICS_DIAGNOSTICS=1`):**
   - Cenário A (`internal`, `dmabuf=false`, `dmabufExportImage=false`, `valoPasses=0`, `method=VkFence`, `gpuLatencyUs=9` / `4`): `[PASS] Cenário A concluiu 2 frames.`
   - Cenário B (`dmabufPool`, `SYNC_FD`, `gpuLatencyUs=26` / `13`, `MOCK_PAGE_FLIP_COMPLETED`): `[PASS] Cenário B concluiu 2 frames.`
5. **Bloqueio de D sem autorização:**
   - `sge_vulkan_dmabuf_harness --scenario D` → saída `3` (`[BLOCKED]`)
   - `./tools/homologation/run_offscreen_experiment.sh --scenario D` → saída `3` (`[BLOCKED]`)
6. **Smoke tests `systalia-session`:**
   - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`, `wayland_output_warnings=0`.
   - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`, `wayland_output_warnings=0`.

---

## 10. Limitações

- O executor `run_offscreen_experiment.sh` utiliza os binários de `SYSTALIA_HARNESS_BUILD` (padrão `$HOME/.cache/sge-build-homolog`), que deve ser atualizado com o build atual antes da execução assistida do Cenário D.
- O Cenário D ainda não foi executado; o GPU hang original (`amdgpu: ring gfx timeout`) em `1920×1080` com carga completa **não** pode ser declarado resolvido.

---

## 11. Riscos

- A GPU `/dev/dri/card1` é compartilhada com a sessão KDE Plasma ativa; uma carga que provoque `ring gfx timeout` no kernel AMDGPU pode congelar a sessão gráfica.
- Um timeout ou `SIGKILL` no espaço de usuário não recupera uma fila travada no kernel AMDGPU.

---

## 12. Decisão de Segurança

| Categoria | Classificação | Justificativa |
| --- | --- | --- |
| **Testes automatizados (Build Release `-Werror`, CTest 59/59, Harness A–B, Smoke tests)** | **APTO** | Todos os 59 testes CTest, os novos testes de ciclo de vida sem avisos `QWaylandOutput`, o harness A–B e os smoke tests headless/nested passaram após as edições finais. |
| **Cenário D (`1920×1080`, Vulkan interno, 1 frame, sem DMA-BUF, sem VALO V2)** | **APTO para execução experimental isolada (com autorização humana)** | Implementação auditada: render target interno sem `DMA_BUF_BIT_EXT`, `0` passes VALO V2 comprovados, conclusão GPU via `VkFence` e bloqueio ativo sem `--authorize`. |
| **Experimentos E–H (`1920×1080` com DMA-BUF / VALO V2)** | **NÃO EXECUTADOS / BLOQUEADOS (aguardando resultado de D)** | Permanecem atrás de autorização individual e devem prosseguir um-a-um somente após aprovação sem falhas do Cenário D. |
| **Homologação física TTY3 (`SGE_USE_DRM_BACKEND=1`)** | **NÃO APTO (BLOQUEADA)** | Proibida até conclusão integral e sem falhas dos experimentos offscreen `D–H`. |
