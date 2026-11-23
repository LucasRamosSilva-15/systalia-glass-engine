# SYSTALIA — Etapa 03.2.4.5.5: Correção do Despacho de Page-Flip e Integração Final do Pool Vulkan

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU de referência:** AMD Cezanne — `/dev/dri/card1`  
**Status:** Implementado e validado em build Release (`-Werror`), 57/57 CTest, PoC Vulkan/RADV e smoke tests

---

## 1. Diagnóstico dos Dois Bloqueadores

1. **Bloqueador A — Despacho de Page-Flip DRM (`user_data`):**
   - `LibdrmAtomicKmsBackend::commitAtomic()` (`src/platform/drm/AtomicKmsPresenter.cpp`) passava `nullptr` como `user_data` para `drmModeAtomicCommit()`.
   - `DrmEventDispatcher::defaultReadDrmEvents()` (`src/platform/drm/DrmEventDispatcher.cpp`) usava apenas `page_flip_handler` com uma ponte local (`DrmFlipDispatchBridge`) não passada ao kernel, impossibilitando a entrega real de `pageFlipCompleted` e a associação por `commitId` / geração.
2. **Bloqueador B — Inicialização do Pool Vulkan e Resolução Fixa no `main.cpp`:**
   - O caminho `SGE_USE_DRM_BACKEND=1` + `SessionBareMetalKms` de `src/app/main.cpp` passava uma resolução fixa `1920×1080` (`DP-1`) para `drmController->initialize()`, alocando um pool GBM separado via `allocateSwapchainPool()` sem chamar `VulkanQuickRenderer::createSharedPoolSlots()`.

---

## 2. Correção do Contexto de Page-Flip

- Adicionado `void *userData = nullptr` em `AtomicCommitRequest` (`AtomicKmsPresenter.hpp`) e passado diretamente para `drmModeAtomicCommit(drmFd, atomicReq, flags, req.userData)`.
- Criados `PendingFlipContext` e `DrmFlipContextRegistry` (`DrmEventDispatcher.hpp` / `.cpp`):
  - Cada commit em `AtomicKmsPresenter::submitBufferForScanout()` aloca um `PendingFlipContext` único (`commitId`, `generation`, `bufferId`, `crtcId`) registrado em `std::unordered_map<PendingFlipContext *, std::unique_ptr<PendingFlipContext>>`.
  - `DrmEventDispatcher::defaultReadDrmEvents()` configura `evCtx.page_flip_handler2 = onLibdrmPageFlipEvent2` (além de `page_flip_handler`) e encaminha `user_data` via ponte `thread_local` síncrona durante `drmHandleEvent()`.
  - `DrmFlipContextRegistry::resolveRawUserData()` valida a existência do ponteiro no mapa **antes** de desreferenciá-lo, rejeitando eventos duplicados, desconhecidos ou de gerações anteriores.

---

## 3. Ciclo de Vida dos Contextos

- **Commit falho:** `m_flipRegistry->discardContext(flipCtx->commitId)` remove o contexto imediatamente.
- **Timeout (`TimedOutInScanout`):** `AtomicKmsPresenter::markFlipTimedOut()` marca `ctx->timedOut = true` e mantém o `PendingFlipContext` vivo no registro sem reutilizar o buffer. Quando o evento tardio chega na mesma geração, `DrmEventDispatcher` emite `latePageFlipCompleted(commitId, bufferId)`, permitindo ao `IndependentFrameLoop` liberar o buffer de `TimedOutInScanout → Available` sem enviar callback Wayland duplicado nem concluir um novo frame.
- **Suspensão (`suspendPresentation()`):** `m_flipRegistry->advanceGeneration()` incrementa a geração mantendo contextos anteriores vivos até o descarte seguro; `DrmEventDispatcher::detachFd()` desabilita o `QSocketNotifier`.
- **Retomada (`resumePresentation(newFd)`):** avança a geração e invalida `kmsFramebufferId = 0` se o FD mudar. Qualquer evento de geração anterior é descartado em `resolveRawUserData()`.
- **Shutdown:** `IndependentFrameLoop::shutdown()` desanexa o `DrmEventDispatcher` antes de `AtomicKmsPresenter::shutdown()` limpar `m_flipRegistry`.

---

## 4. Integração do Pool Unificado em `main.cpp`

No bloco `SessionBareMetalKms && useExperimentalDrmBackend` de `src/app/main.cpp`:

1. `atomicPresenter->setUseExternalVulkanPool(true)` impede `AtomicKmsPresenter::configureOutput()` de chamar `allocateSwapchainPool()` (eliminando qualquer pool GBM concorrente).
2. `drmController->initialize(inputDeviceManager.activeDrmDevice(), {})` descobre conector, modo, CRTC e plano primário via `KmsOutputDiscovery` sem resolução hardcoded.
3. `VulkanQuickRenderer` é inicializado com `QSize(activeMode.width, activeMode.height)`.
4. `DrmVulkanMatcher` valida a correspondência entre o FD DRM adquirido e `VK_EXT_physical_device_drm` (`matchesDrmDevice`).
5. `vulkanRenderer.createSharedPoolSlots(atomicPresenter->bufferManager(), targetSize, 2)` cria o pool unificado Vulkan → DMA-BUF diretamente no `GbmBufferManager` do presenter.

---

## 5. Seleção Dinâmica de Modo

- Adicionado `IDrmPresenter::discoverPreferredMode()` (`DrmSessionTypes.hpp`), implementado em `AtomicKmsPresenter::discoverPreferredMode()` usando `KmsOutputDiscovery` (`primaryConnector()`, `preferredMode` ou `selectBestMode(primary->modes)`).
- `DrmBackendController::initialize()` aceita `DrmOutputMode{}` vazio para acionar a descoberta automática e falha de forma fechada (**fail-closed**, liberando o FD DRM) se não houver monitor conectado, modo válido, CRTC compatível ou plano primário.

---

## 6. Inicialização e Destruição

Ordem determinística no caminho experimental de `src/app/main.cpp`:

- **Inicialização:** `QGuiApplication` → `LogindSessionService` → `LogindDrmProvider` + `AtomicKmsPresenter(externalPool=true)` + `DrmBackendController::initialize({}, ...)` → `InputDeviceManager` → `VulkanQuickRenderer::initialize(targetSize)` → `DrmVulkanMatcher` → `createSharedPoolSlots()` → `loadQmlUrl(ShellRoot.qml)` → `DrmEventDispatcher::attachFd()` → `IndependentFrameLoop`.
- **Destruição (ordem reversa):**
  1. `frameLoop.shutdown()` (para agendamento e desanexa `DrmEventDispatcher`)
  2. `atomicPresenter->releaseKmsFramebuffers()` (remove FBs KMS antes de liberar memória Vulkan)
  3. `vulkanRenderer.destroySharedPoolSlots(atomicPresenter->bufferManager())` (`vkDeviceWaitIdle`, fecha FDs, destrói semáforos e `VkImage`/`VkDeviceMemory`)
  4. `vulkanRenderer.shutdown()`
  5. `drmController->shutdown()` (limpa registro e libera FD via `logind`)
  6. `inputDeviceManager.shutdownHardwareContext()`
  7. `logindSessionService.releaseControl()`

---

## 7. Testes Automatizados

Executados após a última modificação em build `Release` com `-DSGE_ENABLE_WARNINGS_AS_ERRORS=ON`:

1. **Testes específicos (9/9 aprovados, `1.55 s`):**
   - `tst_drm_backend_poc`: `0.04 s`
   - `tst_drm_gbm_integration`: `0.04 s`
   - `tst_vulkan_quick_renderer`: `0.09 s`
   - `tst_independent_vulkan_session`: `0.09 s`
   - `tst_independent_frame_loop`: `0.14 s`
   - `tst_shell_root_integration`: `0.71 s`
   - `tst_unified_vulkan_gbm_pool`: `0.17 s`
   - `tst_vulkan_kms_sync`: `0.14 s`
   - `tst_page_flip_and_unified_startup` (`tests/unit/tst_PageFlipAndUnifiedStartup.cpp`): `0.14 s` — cobrindo `user_data` não nulo, `page_flip_handler2`, commits sequenciais, callback duplicado, timeout + evento tardio, suspensão/retomada com rejeição de geração antiga, shutdown com commit pendente, seleção dinâmica `2560×1440@144`, fail-closed (sem monitor / CRTC / plano), falha de modificadores, ausência de pool GBM paralelo e ordem de destruição.
2. **Suíte CTest completa:** **57/57 aprovados** (`100% tests passed, 0 tests failed out of 57`, `51.13 s`: 38 `unit`, 18 `integration`, 1 `e2e`).
3. **Smoke tests (`systalia-session` instalado):**
   - Headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
   - Nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 8. Evidências Vulkan/RADV

Execução não invasiva do PoC `sge_vulkan_quick_poc` com `QT_QPA_PLATFORM=sge_vulkan_offscreen`:

- `[PHASE_A] PASS: device=AMD Radeon Graphics (RADV RENOIR) software=false ext_mem_fd=1 dmabuf=1 drm_mod=1`
- `[PHASE_B] PASS: QQuickRenderControl frame rendered.`
- `[PHASE_C] PASS: size=128x128 corner=#002244 center=#00ff88`
- `[PHASE_D] PASS: exported dmabufFd=13`
- `[PHASE_E] PASS: simulated Atomic KMS commit prepared (connector=20, crtc=55, plane=77)`

---

## 9. Regressões Encontradas

Nenhuma regressão detectada nos 56 testes pré-existentes nem nos modos nested Wayland e headless.

---

## 10. Limitações Restantes

- Não houve execução física de `TakeDevice`, DRM master, `drmModeAddFB2WithModifiers`, `drmModeAtomicCommit`, modeset ou troca de VT em `/dev/dri/card1`.
- O primeiro commit atômico bare-metal com `allowModeset = true` (modeset inicial caso o CRTC esteja inativo ou com modo distinto) e a aceitação física de modificadores pelo plano primário `amdgpu` serão validados na homologação física assistida.

---

## 11. Checklist de Homologação

- [x] `drmModeAtomicCommit` recebe `PendingFlipContext *` estável em `user_data`.
- [x] `DrmEventDispatcher` despacha `page_flip_handler2` validando geração e `commitId`.
- [x] Timeout preserva contexto e bloqueia reuso até confirmação segura.
- [x] `src/app/main.cpp` usa descoberta dinâmica de modo e pool unificado `createSharedPoolSlots()` sem `allocateSwapchainPool` paralelo.
- [x] Ordem determinística de destruição validada com fechamento de FDs (`fcntl == -1`).
- [ ] Executar primeiro teste físico assistido em TTY3 com `SGE_USE_DRM_BACKEND=1`.

---

## 12. Decisão Explícita

**APTO** para o primeiro teste físico assistido em TTY3 com `SGE_USE_DRM_BACKEND=1`.

**Ressalva:** Os dois bloqueadores identificados na Etapa 03.2.4.5.4 foram corrigidos e validados em 57/57 testes automatizados e GPU RADV offscreen; entretanto, o funcionamento bare-metal completo permanece condicionado à execução e observação do teste físico assistido em `/dev/dri/card1`.
