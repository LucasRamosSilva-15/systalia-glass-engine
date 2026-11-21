# ADR — 03.2.4.2: Arquitetura de Backend DRM/KMS com Proprietário Único via `QQuickRenderControl`

- **Status:** Proposto para implementação na próxima etapa (protótipo isolado validado na Etapa 03.2.4.2)
- **Data:** 21 de novembro de 2026
- **Contexto:** Etapas 03.2.4 e 03.2.4.1 (`valo-graphics-2.0` @ `e77411e`), Fedora Linux 44, Qt 6.11.2, AMD Cezanne / Vulkan RADV

---

## 1. Contexto e Problema

Na tentativa inicial de execução bare-metal em TTY3 (`ETAPA_03_2_4_1_DRM_LOGIND_EBUSY.md`), o plugin padrão **Qt 6.11.2 `eglfs_kms`** (`libQt6EglFsKmsGbmSupport.so.6`) abriu `/dev/dri/card1` diretamente via `open64()` durante a construção de `QGuiApplication`, adquirindo o DRM master no kernel antes de `LogindSessionService::acquirePrimaryDrmDevice()`. Como consequência, `systemd-logind::TakeDevice(226, 1)` falhou com `System.Error.EBUSY`.

Inspeção binária no Qt 6.11.2 padrão do Fedora 44 confirmou que `eglfs_kms` **não possui** integração com `systemd-logind` ou `libseat` nem aceita um descritor DRM externo. Portanto, manter `eglfs_kms` junto com `TakeDevice` cria dois proprietários DRM concorrentes ou falha de permissão (`-EACCES`) nas operações KMS.

---

## 2. Alternativas Avaliadas

| Critério | Alternativa A: QPA DRM/KMS próprio | Alternativa B: `QQuickRenderControl` + `QVulkanInstance` + DRM/KMS SYSTALIA *(Escolhida)* | Alternativa C: `libseat`/`logind` + `eglfs_kms` existente |
|---|---|---|---|
| **Compatibilidade Qt 6.11.2** | Média (exige manter plugin QPA para cada versão) | **Alta** (APIs públicas `QQuickRenderControl`, `QQuickGraphicsDevice`, `QVulkanInstance`) | **Incompatível** sem patchear `qtbase` |
| **Dependência de APIs privadas** | **Alta** (`Qt6::GuiPrivate`: `QPlatformIntegration`, `QPlatformScreen`, `QPlatformWindow`) | **Zero** no backend de apresentação (QPA `offscreen`/`minimal` como host de eventos) | N/A |
| **Complexidade e manutenção** | Muito alta | Moderada e encapsulada em `src/platform/drm/` | Baixa, porém inviável |
| **Integração `logind` / `libseat`** | Direta, porém acoplada ao QPA | **Direta e desacoplada** (`ISessionDrmProvider` → único `fd`) | Conflitante (dois proprietários DRM) |
| **Vulkan e QRhi (VALO V2)** | Complexa (`QPlatformVulkanInstance`) | **Nativa** via `QVulkanInstance` + `QQuickRenderTarget::fromVulkanImage` | Quebrada em `PauseDevice` |
| **Multi-monitor futuro** | Exige `QPlatformScreen` dinâmico | **Explicitamente modelado** por `DrmOutputMode` + `OutputManagerService` | Limitado ao `eglfs_kms.json` |
| **Suspensão e retomada de VT** | Acoplada ao QPA | **Explícita** (`onDevicePaused`/`onDeviceResumed` bloqueiam apresentação) | Quebrada |

---

## 3. Decisão

Adotar a **Alternativa B**: manter **um único proprietário do descritor DRM** adquirido exclusivamente pelo provedor de sessão (`LogindSessionService` ou futuro adaptador `libseat`), entregando esse único `fd` ao backend de apresentação do SYSTALIA baseado em **Atomic KMS + GBM/DMA-BUF + Vulkan (`QVulkanInstance` + `QQuickGraphicsDevice` + `QQuickRenderControl`)**.

- Nenhuma função de apresentação abrirá `/dev/dri/card*` diretamente.
- Os modos `SessionNested` (`wayland;xcb`) e `SessionHeadless` (`offscreen`) permanecem inalterados.
- `SessionBareMetalKms` utilizará uma plataforma Qt neutra (sem `eglfs_kms`) para o loop de eventos e renderizará a cena QML/VALO V2 (`main.qml`, `SystaliaCompositor`) off-screen via `QQuickRenderControl` diretamente para imagens Vulkan exportadas via DMA-BUF/GBM para o KMS.

---

## 4. Consequências

### Positivas
- Elimina definitivamente o conflito `EBUSY` / DRM master duplo sem depender de `Qt6::GuiPrivate` nem de Qt customizado no Fedora 44.
- Preserva integralmente o VALO Graphics V2 (`Dual-Kawase`, shaders `.qsb`), `SystaliaCompositor`, `XWaylandManager`, `InputDeviceManager` (`libinput`) e o ciclo `PauseDevice`/`ResumeDevice`.
- Permite testes unitários determinísticos de ownership, falhas e VT sem GPU física.

### Negativas / Custos
- Exige implementar na próxima etapa o apresentador físico Vulkan/GBM/Atomic KMS (`IDrmPresenter` real), além da instalação de `mesa-libgbm-devel` (ausente atualmente no host, onde apenas `libdrm-devel` está instalado).
- Exige validação física dedicada da sincronização DMA-BUF/fence entre Vulkan RADV e `drmModeAtomicCommit`.
