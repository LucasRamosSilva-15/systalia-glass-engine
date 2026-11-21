# SYSTALIA — Etapa 03.2.4.3: Integração Real DRM/KMS, GBM e Preparação Vulkan

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base:** `e77411e` (com alterações locais das Etapas 03.2.1–03.2.4.3 não commitadas)
- **Sistema:** Fedora Linux 44, Qt 6.11.2, C++23, `libdrm 2.4.134`, `gbm 26.2.7`, `libudev 259`, `vulkan-headers 1.4.341.0`
- **GPU de referência:** AMD Radeon Cezanne — `/dev/dri/card1` (RADV / Mesa `26.2.7`)
- **Estado:** **Infraestrutura DRM/KMS, GBM e contratos Vulkan/Qt Quick implementados e validados por testes determinísticos (50/50 CTest)** *(sem execução física sobre GPU DRM/KMS nesta etapa).*

---

## 1. Arquitetura Implementada

A biblioteca isolada `sge_drm_poc` (`cmake/SgeDrmPoc.cmake`, `src/platform/drm/`) foi evoluída mantendo separação total do caminho de inicialização de produção de `src/app/main.cpp`:

- **Sessão e Ownership único:** `ISessionDrmProvider` → `LogindDrmProvider` (`LogindSessionService`) → `DrmBackendController`.
- **Descoberta KMS:** `IKmsResourceBackend` → `LibdrmResourceBackend` (`xf86drm` / `xf86drmMode`) → `KmsOutputDiscovery`.
- **Buffers GBM:** `IGbmBackend` → `LibgbmBackend` (`libgbm`) → `GbmBufferManager`.
- **Preparação Vulkan / Qt Quick:** `VulkanQuickRenderContract`.

---

## 2. Arquivos Criados e Modificados

- **Criados:**
  - [`src/platform/drm/LogindDrmProvider.hpp`](../../src/platform/drm/LogindDrmProvider.hpp) e [`.cpp`](../../src/platform/drm/LogindDrmProvider.cpp)
  - [`src/platform/drm/KmsOutputDiscovery.hpp`](../../src/platform/drm/KmsOutputDiscovery.hpp) e [`.cpp`](../../src/platform/drm/KmsOutputDiscovery.cpp)
  - [`src/platform/drm/GbmBufferManager.hpp`](../../src/platform/drm/GbmBufferManager.hpp) e [`.cpp`](../../src/platform/drm/GbmBufferManager.cpp)
  - [`src/platform/drm/VulkanQuickContracts.hpp`](../../src/platform/drm/VulkanQuickContracts.hpp)
  - [`tests/unit/tst_DrmGbmIntegration.cpp`](../../tests/unit/tst_DrmGbmIntegration.cpp)
  - [`docs/engineering/ETAPA_03_2_4_3_DRM_GBM_INTEGRATION.md`](ETAPA_03_2_4_3_DRM_GBM_INTEGRATION.md)
- **Modificados:**
  - [`src/platform/drm/DrmSessionTypes.hpp`](../../src/platform/drm/DrmSessionTypes.hpp) *(inclusão de `FdOwnership`)*
  - [`cmake/SgeDrmPoc.cmake`](../../cmake/SgeDrmPoc.cmake) *(vínculo com `libdrm`, `gbm`, `Qt6::DBus` e `sge_core`)*
  - [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) *(alvo `tst_drm_gbm_integration`)*

---

## 3. Integração com `LogindSessionService`

`LogindDrmProvider` adapta `LogindSessionService` sem duplicar estado D-Bus e sem abrir `/dev/dri/card*`:
- `acquireDrmDevice(path)` resolve `major`/`minor` (`LogindSessionService::statDeviceMajorMinor` ou stub injetável em testes), chama `takeDevice(major, minor, &inactive)` e realiza rollback imediato via `releaseDevice(major, minor)` se `inactive == true`.
- Conecta `devicePaused` e `deviceResumed` ao `DrmBackendController` para suspender a apresentação durante troca de VT e substituir o `fd` quando `ResumeDevice` entregar um novo descritor.

---

## 4. Contrato Real de Ownership de FD

`FdOwnership` em `DrmSessionTypes.hpp` distingue explicitamente:
- `BorrowedFromSession`: `fd` pertence a `LogindSessionService::m_devices`, que é o único responsável por chamar `::close(fd)` em `releaseDevice()` / `onResumeDevice()`. `LogindDrmProvider` nunca chama `::close()` sobre ele (evitando *double-close* e *use-after-close*).
- `OwnedDirect` / `OwnedDuplicate`: usados apenas quando um componente detém posse direta ou duplicada explícita.

---

## 5. Descoberta de Outputs KMS

`LibdrmResourceBackend::probeResources(int drmFd)` opera estritamente sobre o `drmFd` já adquirido:
- Consulta `drmModeGetResources`, `drmModeGetConnector`, `drmModeGetEncoder`, `drmModeGetCrtc`, `drmModeGetPlaneResources` e `drmModeObjectGetProperties`.
- Detecta `DRM_CLIENT_CAP_ATOMIC`, `DRM_CLIENT_CAP_UNIVERSAL_PLANES`, conectores conectados/desconectados, modos preferenciais (`DRM_MODE_TYPE_PREFERRED`), `possible_crtcs` e planos primários/overlay/cursor, pronto para single e multi-monitor.

---

## 6. Gerenciamento de Buffers GBM

`LibgbmBackend` e `GbmBufferManager`:
- Criam dispositivo GBM sobre o `drmFd` (`gbm_create_device` / `gbm_device_destroy`).
- Alocam buffers `GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING` com `gbm_bo_create_with_modifiers2` (fallback para `gbm_bo_create`), exportam `dmabufFd` (`gbm_bo_get_fd`), `stride` e `modifier`.
- Gerenciam estados `Available → Rendering → PendingFlip → Presented → Available` e descritores de sincronização (`acquireFenceFd`, `releaseFenceFd`) + `kmsFramebufferId`.
- `vulkanExternalMemoryVerified` permanece explicitamente `false` até validação física com Vulkan/RADV.

---

## 7. Contratos Vulkan / Qt Quick

`VulkanQuickRenderContract` (`VulkanQuickContracts.hpp`) formaliza os requisitos auditados nas APIs públicas do Qt 6.11.2 (`QQuickRenderControl`, `QQuickGraphicsDevice`, `QQuickRenderTarget`, `QVulkanInstance`):
- `QQuickRenderControl` **não** cria swapchain KMS nem exporta DMA-BUF automaticamente.
- Exige `QVulkanInstance` + `QQuickGraphicsDevice::fromDeviceObjects()`, extensões `VK_KHR_external_memory_fd`, `VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`, `VK_KHR_external_semaphore_fd`, barreira de layout `COLOR_ATTACHMENT_OPTIMAL` antes do `drmModeAtomicCommit`, exportação de `sync_file` e suspensão/reconstrução segura em troca de VT.

---

## 8. Testes Executados (após a última edição)

- **Dependências verificadas (`rpm -q` / `pkg-config`):** `libdrm-devel 2.4.134`, `mesa-libgbm-devel 26.2.7`, `systemd-devel 259.10`, `vulkan-headers 1.4.341.0`, `vulkan-loader-devel 1.4.341.0`, `qt6-qtbase-devel 6.11.2`, `qt6-qtdeclarative-devel 6.11.2`.
- **Build Release (`-Werror`):** compilada sem erros nem avisos.
- **Testes específicos primeiro (`QT_QPA_PLATFORM=offscreen`):** `tst_input_hardware_backend`, `tst_logind_integration`, `tst_drm_backend_poc`, `tst_drm_gbm_integration` — **4/4 aprovados** (`0.17 s`).
- **Suíte completa CTest (`QT_QPA_PLATFORM=offscreen`):** **50/50 aprovados** (`49.56 s`, `0` falhas — evolução de `49/49` para `50/50`).
- **Smoke tests (`systalia-session`, cwd `/tmp`, saída por `SIGTERM`):**
  - Headless (`offscreen`): `early_poll=None`, `sigterm_rc=0`.
  - Nested Wayland (`wayland`): `early_poll=None`, `sigterm_rc=0`.

---

## 9. Riscos e Limitações

- **Sem execução física nesta etapa:** `TakeDevice`, `LibdrmResourceBackend`, `LibgbmBackend` e a importação DMA-BUF no Vulkan RADV não foram executados contra `/dev/dri/card1` físico por restrição de segurança da sessão principal.
- **Compatibilidade Vulkan/GBM:** modificadores DRM entre `amdgpu` KMS, `libgbm` e `RADV` precisam ser negociados e validados em hardware real.

---

## 10. Critérios de Entrada da Etapa 03.2.4.4

1. Implementar a ponte real `VulkanQuickGbmPresenter` (`QVulkanInstance` + `QQuickRenderControl` + importação DMA-BUF + `drmModeAddFB2WithModifiers` / `drmModeAtomicCommit`).
2. Integrar `DrmBackendController` atrás de flag explícita (ex.: `SGE_USE_DRM_BACKEND=1`) preservando fallback/compatibilidade nested e headless.
3. Executar homologação física assistida em TTY3 (Testes A–D) sobre `/dev/dri/card1`.
