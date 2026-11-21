# SYSTALIA — Etapa 03.2.4.2: Arquitetura e Protótipo de Backend DRM/KMS com Proprietário Único

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base:** `e77411e` (com alterações locais das Etapas 03.2.1–03.2.4.2 não commitadas)
- **Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, AMD Cezanne/RADV
- **Estado:** **Arquitetura decidida (ADR) e protótipo isolado validado em testes determinísticos** *(renderização física KMS/GBM/Vulkan ainda pendente da etapa de implementação de produção).*

---

## 1. Comparação das Alternativas

A análise completa e os critérios comparativos encontram-se registrados em [`docs/architecture/ADR_03_2_4_2_DRM_BACKEND.md`](../architecture/ADR_03_2_4_2_DRM_BACKEND.md).

Resumo da decisão:
- **Rejeitada (C — `libseat`/`logind` com `eglfs_kms` existente):** Qt 6.11.2 `eglfs_kms` chama `open64("/dev/dri/card*")` internamente e não aceita FD externo.
- **Rejeitada (A — QPA DRM/KMS próprio):** exige `Qt6::GuiPrivate` (`QPlatformIntegration`, `QPlatformScreen`, `QPlatformWindow`) e acopla o projeto à ABI interna de cada release do Qt.
- **Escolhida (B — `QQuickRenderControl` + `QVulkanInstance` + `QQuickGraphicsDevice` sobre DRM/KMS SYSTALIA):** usa apenas APIs públicas do Qt 6.11.2 (`/usr/include/qt6/QtQuick/QQuickRenderControl`, `QQuickGraphicsDevice`, `QVulkanInstance`) com propriedade única do FD DRM cedido pela sessão.

---

## 2. Fluxo Alvo DRM / `logind` / GBM / Vulkan / Qt Quick

```text
[LogindSessionService / libseat]
   │  TakeControl(false) + TakeDevice(226, 1)
   ▼  (único FD DRM master)
[DrmBackendController]  ──(onDevicePaused / onDeviceResumed)──▶ suspende/retoma apresentação
   │  passa FD não-proprietário
   ▼
[IDrmPresenter (produção futura)]
   ├── Atomic KMS (xf86drmMode.h): conectores, CRTCs, planos, page-flip
   ├── GBM + DMA-BUF (gbm.h): alocação de buffers scanout
   └── Qt Quick (QQuickRenderControl + QVulkanInstance + QQuickGraphicsDevice)
         └── renderiza cena VALO Graphics V2 + SystaliaCompositor
```

---

## 3. Componentes Implementados no Protótipo Isolado

Criados em `src/platform/drm/` e compilados na biblioteca estática separada `sge_drm_poc` via [`cmake/SgeDrmPoc.cmake`](../../cmake/SgeDrmPoc.cmake), **sem modificar `sge_core` nem `systalia_glass_engine`**:

- [`src/platform/drm/DrmSessionTypes.hpp`](../../src/platform/drm/DrmSessionTypes.hpp):
  - `DrmOwnershipState` (`Uninitialized`, `Acquired`, `Suspended`, `Released`, `Failed`)
  - `DrmDeviceDescriptor` (estrutura move-only garantindo posse única do `fd`, `major`, `minor` e `devicePath`)
  - `DrmOutputMode` (`connectorName`, `width`, `height`, `refreshHz`, `preferred`)
  - Interfaces `ISessionDrmProvider` (aquisição/liberação de sessão) e `IDrmPresenter` (inicialização no `fd`, configuração de saída, apresentação, suspensão, retomada e encerramento)
- [`src/platform/drm/DrmBackendController.hpp`](../../src/platform/drm/DrmBackendController.hpp) e [`.cpp`](../../src/platform/drm/DrmBackendController.cpp):
  - Orquestra aquisição única, bloqueia reinicialização duplicada, impede `renderAndPresentFrame()` durante `Suspended` ou `Released`, garante liberação imediata do `fd` se o apresentador falhar na inicialização e realiza `shutdown()` idempotente no destrutor.
- [`tests/unit/tst_DrmBackendPoc.cpp`](../../tests/unit/tst_DrmBackendPoc.cpp):
  - `singleFdOwnershipLifecycleAndTeardown()`
  - `acquisitionFailuresLeaveZeroHeldDescriptors()`
  - `vtSuspendAndResumeBlockRenderingWhilePaused()`
  - `descriptorTransferAndMoveSemantics()`

---

## 4. Contratos de Ownership dos Descritores

1. **Provedor único:** somente `ISessionDrmProvider::acquireDrmDevice()` pode originar um `DrmDeviceDescriptor`.
2. **Proibição de cópia:** `DrmDeviceDescriptor` deleta construtor e operador de cópia; apenas transferência explícita (`std::move`).
3. **Rollback imediato em falha:** se `IDrmPresenter::initializeOnFd()` ou `configureOutput()` falhar, `DrmBackendController` chama `m_presenter.shutdown()` e `releaseDrmDevice()` antes de retornar `false`.
4. **Bloqueio de renderização pausada:** `onDevicePaused(major, minor)` move o estado para `Suspended` e faz `renderAndPresentFrame()` retornar `false` até `onDeviceResumed(major, minor, fd)` concluir com sucesso.

---

## 5. Resultados de Compilação e Testes Executados (após a última edição)

- **Build Release (`-Wall -Wextra -Werror`):** compilada sem erros nem avisos (`sge_drm_poc` + núcleo principal).
- **Teste específico do protótipo primeiro (`QT_QPA_PLATFORM=offscreen`):**
  - `tst_drm_backend_poc`: **Passed** (`0.01 s`).
- **Suíte completa CTest (`QT_QPA_PLATFORM=offscreen`):**
  - **49/49 aprovados** (`100% tests passed, 0 tests failed out of 49`, `50.29 s`).
- **Smoke tests (`systalia-session`, cwd `/tmp`, encerrado por `SIGTERM`):**
  - Headless (`offscreen`): `early_poll=None`, `sigterm_rc=0`.
  - Nested Wayland (`wayland`): `early_poll=None`, `sigterm_rc=0`.
- Diretórios temporários `/tmp/sge-build-03242` e `/tmp/sge-install-03242` removidos após validação.

---

## 6. Limitações do Protótipo e Riscos de Integração

- **Dependência física pendente:** o protótipo valida o contrato de ownership e ciclo de vida com provedores simulados; **não** abre `/dev/dri/card1` nem executa `drmModeAtomicCommit` ou `gbm_create_device` reais.
- **Pacote de desenvolvimento ausente no host:** `/usr/include/xf86drm.h` e `/usr/include/xf86drmMode.h` (`libdrm-devel`) estão presentes, mas `/usr/include/gbm.h` (`mesa-libgbm-devel`) **não** está instalado neste host e será necessário na etapa de implementação física.
- **Risco de integração Vulkan/KMS:** a exportação DMA-BUF de imagens Vulkan (`VK_KHR_external_memory_fd` / modificadores DRM) para `gbm_bo` + `drmModeAddFB2WithModifiers` e sincronização explícita por `sync_file` requer testes cuidadosos no driver `amdgpu`/RADV.

---

## 7. Próxima Etapa Necessária

1. Revisão e aprovação humana do [`ADR_03_2_4_2_DRM_BACKEND.md`](../architecture/ADR_03_2_4_2_DRM_BACKEND.md).
2. Instalação de `mesa-libgbm-devel` no ambiente de desenvolvimento Fedora 44.
3. Implementação da etapa de produção do `IDrmPresenter` real (`Atomic KMS + GBM + QQuickRenderControl + QVulkanInstance`) conectado ao `LogindSessionService`, seguida da repetição assistida dos Testes Físicos A–D em TTY3.
