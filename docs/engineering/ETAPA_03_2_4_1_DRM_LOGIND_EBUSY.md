# SYSTALIA — Etapa 03.2.4.1: Correção da Integração DRM/KMS e systemd-logind

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base:** `e77411e` (com alterações locais das Etapas 03.2.1–03.2.4.1 não commitadas)
- **Sistema:** Fedora Linux 44, Qt 6.11.2, AMD Radeon Cezanne (`amdgpu`), Vulkan RADV (`Mesa 26.2.7`), `/dev/dri/card1` (`226:1`)
- **Classificação da etapa:** **BLOQUEADA — LIMITAÇÃO ARQUITETURAL COMPROVADA (Qt 6.11.2 `eglfs_kms` sem integração `systemd-logind`/`libseat`)**

---

## 1. Resumo Executivo

A investigação identificou e comprovou a causa raiz do erro `System.Error.EBUSY` observado na primeira tentativa de homologação física em TTY3 (`Id=9`, `VTNr=3`, `Seat=seat0`):

1. O plugin **Qt 6.11.2 `eglfs_kms`** (`libQt6EglFsKmsGbmSupport.so.6`) **não possui integração com `systemd-logind` ou `libseat`** e abre `/dev/dri/card1` diretamente via `open64()` durante a construção de `QGuiApplication app(argc, argv)`.
2. Como `/dev/dri/card1` possui ACL `uaccess` para o usuário ativo na `seat0` e a TTY3 não tinha DRM master ativo, o descritor `fd 6` aberto pelo `QGuiApplication` recebeu automaticamente o **DRM master** do kernel Linux.
3. Em seguida, `main.cpp` chamou `LogindSessionService::initializeSession(false)` e `acquirePrimaryDrmDevice("/dev/dri/card1")` (`TakeDevice(226, 1)`). O `systemd-logind` tentou abrir `/dev/dri/card1` e executar `DRM_IOCTL_SET_MASTER`, recebendo `-EBUSY` (`System.Error.EBUSY Device or resource busy`) porque o `fd 6` já detinha o DRM master.
4. Inverter a ordem (chamar `TakeDevice(226, 1)` antes de `QGuiApplication`) **não resolve o problema** sem criar dois proprietários DRM concorrentes: `eglfs_kms` continuaria abrindo um segundo descritor independente sem DRM master (falhando em `drmModeAtomicCommit` com `-EACCES`) e sem responder a `PauseDevice`/`ResumeDevice`.
5. Foi corrigido e testado o bug secundário de destruição de `QProcess` em execução durante abortos/encerramento (`SystemControlService::~SystemControlService()`).

---

## 2. Evidências do Teste Físico

- **Hardware:** AMD Radeon Cezanne, `amdgpu`, RADV RENOIR (`Mesa 26.2.7`), `/dev/dri/card1` (`226:1`), monitor AOC 24B35HM2 (`1920×1080`).
- **Log EGLFS/KMS:**
  - `Found the following video devices: QList("/dev/dri/card1")`
  - `Creating GBM device for file descriptor 6 obtained from "/dev/dri/card1"`
  - `Atomic reported as supported` / `Atomic enabled`
- **Falha:**
  - `[SGE-LOGIND] TakeDevice( 226 , 1 ) falhou: System.Error.EBUSY Device or resource busy`
  - `[SGE-LOGIND] Falha ao adquirir dispositivo DRM primário: "/dev/dri/card1"`
- **Sessão (`loginctl`):** `Id=9`, `VTNr=3`, `Seat=seat0`, `Service=login`, `Type=tty` — sessão legitimamente criada pelo PAM; o `Active=no` observado posteriormente ocorreu apenas após o retorno à TTY2.

---

## 3. Sequência Real de Inicialização Observada

1. `InputDeviceManager::configureProcessPlatformEnvironment()` define `QT_QPA_PLATFORM=eglfs`, `QT_QPA_EGLFS_INTEGRATION=eglfs_kms`, `QT_QPA_EGLFS_KMS_ATOMIC=1`, `QSG_RHI_BACKEND=vulkan`.
2. `QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan)`.
3. **`QGuiApplication app(argc, argv)`** inicializa o plugin QPA `eglfs_kms` → `QEglFSKmsGbmDevice::open()` chama `open64("/dev/dri/card1", O_RDWR | O_CLOEXEC)` → obtém **`fd 6`** e torna-se **DRM master** no kernel.
4. Instanciação de `SystemControlService` (que dispara sondas assíncronas `QProcess` em `refresh()`).
5. `LogindSessionService::initializeSession(false)` obtém `TakeControl(false)` na sessão PAM da TTY3.
6. `LogindSessionService::acquirePrimaryDrmDevice("/dev/dri/card1")` chama `TakeDevice(226, 1)` → **`systemd-logind` falha com `System.Error.EBUSY`** porque `fd 6` já detém o DRM master.
7. `main.cpp` chama `releaseControl()` e retorna `1`, destruindo `SystemControlService` enquanto os `QProcess` de `refresh()` ainda rodavam.

---

## 4. Causa Raiz Comprovada

Inspeção binária (`nm -D` e `strings`) em `/usr/lib64/libQt6KmsSupport.so.6`, `/usr/lib64/libQt6EglFsKmsSupport.so.6`, `/usr/lib64/libQt6EglFsKmsGbmSupport.so.6` e `/usr/lib64/libQt6Gui.so.6` do Qt 6.11.2 (Fedora 44) confirmou:
- **Ausência total** de chamadas a `org.freedesktop.login1` (`TakeControl`, `TakeDevice`), `libseat` ou passagem externa de descritor DRM (`fd`).
- `QEglFSKmsGbmDevice::open()` usa exclusivamente `open64()` diretamente sobre o nó `/dev/dri/card*`.

Portanto:
- **Causa raiz do `EBUSY`:** posse prévia do DRM master pelo `fd 6` aberto pelo próprio `eglfs_kms` antes de `LogindSessionService::acquirePrimaryDrmDevice()`.
- **Impedimento estrutural:** `eglfs_kms` do Qt 6.11.2 padrão não aceita o `fd` alugado pelo `systemd-logind`. Qualquer tentativa de manter ambos ativos gera **dois proprietários DRM concorrentes** ou falha de permissão (`-EACCES`) nas operações KMS.

---

## 5. Arquitetura de Gerenciamento DRM — Avaliação de Alternativas

1. **Opção A (Recomendada para produção Wayland):** Adotar um backend QPA DRM/KMS + GBM/Vulkan próprio ou baseado em `libseat`, onde **um único proprietário** recebe o descritor DRM via `LogindSessionService::TakeDevice` (ou `libseat`) e o entrega ao `gbm_create_device(fd)` / `drmModeAtomicCommit(fd, ...)`, com `PauseDevice`/`ResumeDevice` atuando diretamente sobre o mesmo `fd`.
2. **Opção B (Não portável no Fedora padrão):** Patchear `qtbase` (`QEglFSKmsDevice::open()`) para usar `logind`/`libseat`.
3. **Opção C (Rejeitada / proibida):** Contornar `TakeDevice` para DRM ou usar `SGE_ALLOW_NO_LOGIND`, quebrando a segurança e a troca de VT pelo `logind`.

Seguindo a diretriz de não implementar soluções que contornem o `logind` nem criar dois proprietários DRM concorrentes, o caminho bare-metal `eglfs_kms` foi mantido **fail-closed** (`return 1`) até decisão arquitetural pela Opção A.

---

## 6. Correções Realizadas Nesta Etapa

- **Encerramento ordenado de processos auxiliares (`QProcess`):**
  - Adicionado destrutor `SystemControlService::~SystemControlService()` em [`src/services/system/SystemControlService.hpp`](../../src/services/system/SystemControlService.hpp) e [`src/services/system/SystemControlService.cpp`](../../src/services/system/SystemControlService.cpp), interrompendo timers e encerrando (`terminate()` / `waitForFinished(150)` / `kill()`) todos os `QProcess` filhos antes da destruição de `QObject`.
  - Adicionado teste de regressão `systemControlDestructorStopsRunningProbesCleanly()` em [`tests/integration/tst_ShellStateRegression.cpp`](../../tests/integration/tst_ShellStateRegression.cpp) usando `QTest::failOnWarning`.

---

## 7. Arquivos Modificados

- [`src/services/system/SystemControlService.hpp`](../../src/services/system/SystemControlService.hpp)
- [`src/services/system/SystemControlService.cpp`](../../src/services/system/SystemControlService.cpp)
- [`tests/integration/tst_ShellStateRegression.cpp`](../../tests/integration/tst_ShellStateRegression.cpp)
- [`docs/engineering/ETAPA_03_2_4_1_DRM_LOGIND_EBUSY.md`](ETAPA_03_2_4_1_DRM_LOGIND_EBUSY.md) *(novo)*

---

## 8. Testes Automatizados Executados (após a última edição)

1. **Configuração limpa Release (`-Werror`):** `-DSGE_HAVE_LIBINPUT=1 -DSGE_HAVE_LIBUDEV=1 -DSGE_HAVE_XCB=1`.
2. **Testes específicos primeiro (`QT_QPA_PLATFORM=offscreen`):**
   - `tst_input_hardware_backend`, `tst_logind_integration`, `tst_shell_state_regression` — **3/3 aprovados**.
3. **Suíte completa CTest (`QT_QPA_PLATFORM=offscreen`):**
   - **48/48 aprovados** (`49.64 s`, `0` falhas).
4. **Smoke tests (`systalia-session`, cwd `/tmp`, saída por `SIGTERM`):**
   - Headless (`offscreen`): `sigterm_rc=0`, `qproc_warn=False`.
   - Nested Wayland (`wayland`): `sigterm_rc=0`, `qproc_warn=False`.
5. Diretórios temporários `/tmp/sge-build-03241` e `/tmp/sge-install-03241` removidos ao final.

---

## 9. Riscos Remanescentes

- **Bloqueador P0 bare-metal:** enquanto `SessionBareMetalKms` depender do plugin `eglfs_kms` padrão do Qt 6.11.2, `TakeDevice(226, 1)` continuará conflitando com o `open64()` interno do Qt.

---

## 10. Procedimento Seguro para Repetir o Teste A (Após Implementação da Opção A)

Não repetir o Teste A bare-metal sobre `eglfs_kms` padrão enquanto a integração única de descritor DRM (`libseat` ou backend QPA/KMS próprio) não for implementada. Após essa implementação:
1. Instalar em prefixo isolado (`$HOME/.local/systalia-homolog`).
2. Alternar para TTY3 (`Ctrl+Alt+F3`), autenticar e executar `systalia-session` com `QT_LOGGING_RULES` e log persistente em `$HOME/.local/state/systalia/` (sem `SGE_ALLOW_NO_LOGIND` e sem `SGE_ALLOW_NO_INPUT`).
3. Confirmar `TakeControl`, `TakeDevice(226, 1)` com sucesso, criação de superfície GBM/Vulkan e renderização da shell VALO V2.

---

## 11. Critérios para Continuar os Testes B–D

1. Teste A concluído com aquisição única do descritor DRM via `logind`/`libseat` e renderização VALO V2 ativa na TTY3.
2. Entrada física `libinput` (teclado/ponteiro) validada no Teste B.
3. Troca de VT (`PauseDevice`/`ResumeDevice`) validada sem tela preta ou travamento no Teste C.
4. Encerramento limpo e retorno intacto à sessão Plasma da TTY2 validados no Teste D.
