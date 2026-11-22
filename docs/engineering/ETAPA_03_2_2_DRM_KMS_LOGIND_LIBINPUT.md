# Etapa 03.2.2 — Integração DRM/KMS, logind e libinput

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Commit base auditado:** `4ddce41` (com alterações locais da 03.2.1 + 03.2.2 não commitadas)
- **Ambiente:** Fedora Linux 44, Qt 6.11.2, `libinput-devel-1.31.3-1.fc44.x86_64`, `systemd-devel-259.10-1.fc44.x86_64`, `wayland-devel-1.26.0`
- **Estado:** **Backend `libinput` compilado (`SGE_HAVE_LIBINPUT=1`) e validado em testes automatizados**; **homologação física bare-metal/DRM/KMS pendente** (ambiente atual é sessão gráfica aninhada `wayland-1` / `DISPLAY=:0`).

---

## 1. Evidência de Ativação de `SGE_HAVE_LIBINPUT=1`

- `rpm -q libinput-devel`: `libinput-devel-1.31.3-1.fc44.x86_64`.
- `pkg-config --modversion libinput libudev`: `1.31.3` e `259`.
- Configuração CMake limpa em `/tmp/sge-build-0322` (`-DCMAKE_BUILD_TYPE=Release`) gerou em `build.ninja`:
  - `DEFINES = ... -DSGE_HAVE_LIBINPUT=1 -DSGE_HAVE_LIBUDEV=1 -DSGE_HAVE_XCB=1`
- Ligação dinâmica verificada com `ldd` no executável `systalia_glass_engine`:
  - `libinput.so.10 => /lib64/libinput.so.10`
  - `libudev.so.1 => /lib64/libudev.so.1`
  - `libwayland-server.so.0 => /lib64/libwayland-server.so.0`

---

## 2. Diagnóstico dos Subsistemas de Hardware e Sessão

1. **Detecção e inicialização (`src/app/main.cpp` e `InputDeviceManager`):**
   - `InputDeviceManager::configureProcessPlatformEnvironment()` detecta `SessionHeadless`, `SessionNested` ou `SessionBareMetalKms` antes de `QGuiApplication`, configurando `QT_QPA_PLATFORM=eglfs`, `QT_QPA_EGLFS_INTEGRATION=eglfs_kms`, `QT_QPA_EGLFS_KMS_ATOMIC=1`, `QSG_RHI_BACKEND=vulkan` e `QT_QPA_EGLFS_KMS_CONFIG`.
   - `SystaliaCompositor::create()` vincula `InputDeviceManager::instance()->attachCompositor(this)` e `OutputManagerService::instance()->attachCompositor(this)`.
2. **Falha de `systemd-logind` em sessão bare-metal (`src/app/main.cpp`):**
   - **Problema encontrado:** anteriormente, se `LogindSessionService::initializeSession(false)` falhasse em `SessionBareMetalKms`, `main.cpp` apenas emitia `qWarning()` e prosseguia para `initializeHardwareContext()` e abertura de janela `eglfs_kms`, deixando o compositor em estado parcialmente inicializado (sem `TakeControl`, sem lease DRM/input sem root e sem resposta a `PauseDevice`/`ResumeDevice`).
3. **Lease inativo em `InputDeviceManager::openRestrictedCallback()`:**
   - **Problema encontrado:** quando `logind->takeDeviceByPath(...)` retornava um descritor válido com `inactive == true` (dispositivo pausado no momento da abertura), `openRestrictedCallback` retornava `-EACCES` ao `libinput`, mas mantinha o descritor retido em `LogindSessionService::m_devices`.
4. **Ciclo `PauseDevice` / `ResumeDevice` e troca de VT:**
   - `LogindSessionService` suspende a submissão GPU (`gpuSubmissionSuspendedChanged(true)`), `main.cpp` oculta a janela e chama `window->releaseResources()`, e `InputDeviceManager::setHardwareSuspended(true)` desativa `QSocketNotifier` e chama `libinput_suspend(m_libinput)`, retomando com `libinput_resume()` em `deviceResumed`.
5. **Compatibilidade EGLFS/KMS + Vulkan/QRhi + QtWaylandCompositor:**
   - A combinação `eglfs_kms` + `QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan)` está estruturada no código, mas depende do suporte efetivo do driver DRM/Vulkan do hardware alvo na homologação física.

---

## 3. Correções Efetivamente Realizadas

| Arquivo | Correção | Justificativa |
|---|---|---|
| [`src/services/session/LogindSessionService.hpp`](../../src/services/session/LogindSessionService.hpp) | Adição de `shouldAbortOnLogindFailure(bool isBareMetalKms, quint32 effectiveUid, bool allowNoLogind)` | Política determinística e testável para evitar inicialização parcial insegura sem `logind` |
| [`src/app/main.cpp`](../../src/app/main.cpp) | Em `SessionBareMetalKms`, aborta com `qCritical()` e `return 1` se `initializeSession(false)` falhar (quando `geteuid() != 0` e sem `SGE_ALLOW_NO_LOGIND`), e registra `qWarning()` explícito se `acquirePrimaryDrmDevice()` ou `initializeHardwareContext()` falharem | Impedir estado parcialmente inicializado sem controle de sessão/DRM |
| [`src/services/input/InputDeviceManager.cpp`](../../src/services/input/InputDeviceManager.cpp) | Em `openRestrictedCallback()`, libera o lease via `logind->releaseDevice(major, minor)` se `leaseFd >= 0 && inactive` antes de retornar `-EACCES` | Evitar retenção de descritor de dispositivo não utilizado |
| [`tests/unit/tst_LogindIntegration.cpp`](../../tests/unit/tst_LogindIntegration.cpp) | Casos unitários cobrindo `LogindSessionService::shouldAbortOnLogindFailure(...)` | Regressão automatizada da política de falha `logind` |

---

## 4. Resultados Completos da Build e do CTest

1. **Build limpa Release (`-Wall -Wextra -Werror`, `SGE_HAVE_LIBINPUT=1`, `SGE_HAVE_LIBUDEV=1`):** compilada sem erros nem warnings.
2. **Ordem obrigatória de execução de testes:**
   - **Testes específicos primeiro** (`tst_input_hardware_backend`, `tst_logind_integration`): **2/2 aprovados** (`0,04 s` cada).
   - **Suíte completa CTest (`QT_QPA_PLATFORM=offscreen`):** **48/48 aprovados** (`49,86 s`).
3. **Limitação explícita dos testes:** `tst_input_hardware_backend` e `tst_logind_integration` validam parsing, detecção de modo, política de falha e chamadas D-Bus com mocks/eventos simulados; **não** acessam `/dev/dri/card0` ou `/dev/input/event*` físicos.

---

## 5. Testes Físicos: Roteiro Seguro para Homologação (Pendentes)

O ambiente atual foi identificado como sessão gráfica aninhada (`SESSION_TYPE=wayland`, `WAYLAND_DISPLAY=wayland-1`, `DISPLAY=:0`, bare-metal não virtualizado). **Nenhum teste físico que tome o controle da GPU ou interrompa o display manager foi executado nesta etapa.**

### Roteiro recomendado para a fase física (Etapa 03.2.3 / execução assistida)
1. **Pré-requisitos e backup:** criar snapshot/backup de `/etc/` relevante e dados de usuário; garantir usuário com acesso SSH ou segunda TTY (`Ctrl+Alt+F3`) para recuperação caso o vídeo trave.
2. **Instalação isolada de teste:** instalar em `/usr` (ou prefixo dedicado com `.desktop` em `/usr/share/wayland-sessions/`) em máquina ou VM dedicada de teste Fedora 44.
3. **Inicialização pelo SDDM ou TTY:**
   - Via TTY livre (`Ctrl+Alt+F3`), encerrar ou não interferir na sessão principal apenas em ambiente de teste dedicado, e executar `systalia-session` observando `journalctl --user` / logs em `/tmp/systalia-runs/`.
   - Via SDDM, selecionar **Systalia (VALO Glass)**.
4. **Validação física:**
   - Confirmar saída de vídeo DRM/KMS (`eglfs_kms` + Vulkan) e resolução nativa (`config/eglfs_kms.json`).
   - Testar teclado físico, ponteiro relativo/absoluto, clique e gestos de 3 dedos via `libinput` real (`seat0`).
   - Testar troca de VT (`Ctrl+Alt+F2` → retorno à VT do SYSTALIA): confirmar `PauseDevice`, suspensão de submissão GPU, `ResumeDevice` e restauração da apresentação sem tela preta ou perda de input.
5. **Encerramento limpo:** sair da sessão, confirmar retorno ao SDDM/TTY e coletar `dmesg`/logs do compositor.

---

## 6. Riscos e Falhas Não Resolvidos

- **Vulkan sobre `eglfs_kms` no Fedora 44:** não validado em hardware físico; drivers específicos (Intel/AMD/NVIDIA) podem exigir ajustes de `QT_QPA_EGLFS_KMS_CONFIG` ou fallback de backend.
- **Hotplug de dispositivos `libinput` e múltiplos monitores físicos:** a retomada após perda física de dispositivo ou remoção de monitor em bare-metal ainda precisa de teste real.
- **Sem homologação física:** o SYSTALIA continua **não homologado** como sessão desktop independente até a conclusão dos testes físicos acima.

---

## 7. Critérios para Avançar à Etapa 03.2.3

1. Executar o roteiro físico em VM/hardware Fedora 44 dedicado sem travamento de vídeo ou perda de entrada.
2. Confirmar troca de VT (`PauseDevice`/`ResumeDevice`) com restauração íntegra do Scene Graph e dos clientes Wayland.
3. Registrar logs reais de DRM/KMS, `logind` e `libinput` (sem mocks) e atualizar a matriz de implementação vs. homologação.
