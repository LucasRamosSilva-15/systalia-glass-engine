# Etapa 03.2.1 — Fundação da Sessão Fedora Independente

- **Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
- **Branch:** `valo-graphics-2.0`
- **Base auditada:** `c8e4912`
- **Estado:** **Implementada e validada localmente (instalação, CTest, modos headless e aninhado)**; **não homologada** em login/DRM/KMS/VT físico no Fedora (escopo da Etapa 03.2.2).

---

## 1. Diagnóstico e Causas dos Problemas Encontrados

1. **Caminhos absolutos `/usr/bin/` fixos nos artefatos de sessão:**
   - `data/systalia-session` executava `exec /usr/bin/systalia_glass_engine` fixo e `data/systalia.desktop` apontava `Exec=/usr/bin/systalia-session` fixo.
   - **Causa/impacto:** qualquer instalação em prefixo diferente de `/usr` (como `/usr/local`, `/opt` ou prefixos temporários de validação) falhava ao localizar o executável ou invocava binários errados.
2. **`config/eglfs_kms.json` não instalado e dependente do CWD:**
   - [`cmake/SgeInstall.cmake`](../../cmake/SgeInstall.cmake) não instalava `config/eglfs_kms.json`, e [`src/app/main.cpp`](../../src/app/main.cpp) passava o caminho relativo `"config/eglfs_kms.json"` para `InputDeviceManager::configureProcessPlatformEnvironment()`.
   - **Causa/impacto:** ao iniciar a sessão a partir de SDDM/TTY (onde o diretório de trabalho é `$HOME` ou `/`), `QFile::exists("config/eglfs_kms.json")` falhava silenciosamente e `QT_QPA_EGLFS_KMS_CONFIG` não era exportado.
3. **Exportação fixa de `QT_QPA_PLATFORM="wayland;eglfs"` no script de sessão:**
   - `data/systalia-session` definia `QT_QPA_PLATFORM="wayland;eglfs"` antes de lançar o compositor. Em sessão independente sem `WAYLAND_DISPLAY`/`DISPLAY`, `InputDeviceManager` precisava sobrescrever para `eglfs` e clientes filhos dependiam de `ProcessLauncherService` para receber `wayland`.
4. **Tratamento silencioso de falhas de `eglfs_kms.json` e `logind`:**
   - Ausência de aviso diagnóstico quando nenhum `eglfs_kms.json` era localizado ou quando `LogindSessionService::initializeSession(false)` falhava em modo `SessionBareMetalKms`.

---

## 2. Arquivos Modificados e Justificativas

| Arquivo | Alteração | Justificativa |
|---|---|---|
| [`data/systalia-session.in`](../../data/systalia-session.in) *(renomeado de `data/systalia-session`)* | Template CMake com resolução de `systalia_glass_engine` via `$SCRIPT_DIR`, `@CMAKE_INSTALL_FULL_BINDIR@` ou `$PATH`, repasse de `"$@"` e remoção de `QT_QPA_PLATFORM` fixo | Eliminar dependência de `/usr/bin` e preservar detecção automática de modo em `InputDeviceManager` |
| [`data/systalia.desktop.in`](../../data/systalia.desktop.in) *(renomeado de `data/systalia.desktop`)* | `Exec=@CMAKE_INSTALL_FULL_BINDIR@/systalia-session` e `TryExec=@CMAKE_INSTALL_FULL_BINDIR@/systalia-session` | Garantir registro válido do `.desktop` em qualquer `CMAKE_INSTALL_PREFIX` |
| [`cmake/SgeInstall.cmake`](../../cmake/SgeInstall.cmake) | `configure_file(... @ONLY)` para os templates de sessão e instalação de `config/eglfs_kms.json` em `${CMAKE_INSTALL_DATADIR}/systalia` | Instalar todos os artefatos necessários à sessão independente |
| [`cmake/SgeCore.cmake`](../../cmake/SgeCore.cmake) | Definição `SGE_DATA_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/systalia"` | Permitir localização do `eglfs_kms.json` instalado independentemente do CWD |
| [`src/services/input/InputDeviceManager.hpp`](../../src/services/input/InputDeviceManager.hpp) e [`.cpp`](../../src/services/input/InputDeviceManager.cpp) | Adição de `resolveKmsConfigPath()` (precedência: `QT_QPA_EGLFS_KMS_CONFIG` → `preferredPath` → `extraSearchDirs` → `$XDG_CONFIG_HOME/systalia/` → `<exe>/config` e `<exe>/../share/systalia/` → `SGE_DATA_INSTALL_DIR` → `/etc/systalia/` → `/usr/share/systalia/`) + `qWarning()` quando ausente | Tornar resolução de `eglfs_kms.json` determinística, relocável e testável |
| [`src/app/main.cpp`](../../src/app/main.cpp) | `qWarning()` explícito se `logindSessionService.initializeSession(false)` falhar em `SessionBareMetalKms` | Visibilidade diagnóstica de falhas na inicialização bare-metal |
| [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) e [`tests/unit/tst_LogindIntegration.cpp`](../../tests/unit/tst_LogindIntegration.cpp) | Definição `SGE_BINARY_DIR` e validação dos templates `.in` e dos artefatos gerados no build | Validar geração sem `/usr/bin` fixo |
| [`tests/unit/tst_InputHardwareBackend.cpp`](../../tests/unit/tst_InputHardwareBackend.cpp) | Adição de `kmsConfigResolutionAcrossInstallPrefixesAndEnv()` | Cobrir resolução por prefixo instalado e override por `QT_QPA_EGLFS_KMS_CONFIG` |

---

## 3. Versão Fedora de Referência Provisória e Dependências

- **Referência técnica provisória observada no host:** **Fedora Linux 44** (`VERSION_ID=44`, `SUPPORT_END=2027-05-19`), escolha operacional sujeita a confirmação definitiva na Etapa 03.2.2+.
- **Dependências verificadas via `rpm -q` no ambiente atual:**
  - `qt6-qtbase-devel-6.11.2-3.fc44.x86_64`
  - `qt6-qtdeclarative-devel-6.11.2-3.fc44.x86_64`
  - `qt6-qtwayland-devel-6.11.2-1.fc44.x86_64`
  - `qt6-qtshadertools-devel-6.11.2-1.fc44.x86_64`
  - `wayland-devel-1.26.0-1.fc44.x86_64`
  - `systemd-devel-259.10-1.fc44.x86_64` (`systemd-udev-259.10-1.fc44.x86_64`)
  - `libxcb-devel-1.17.0-7.fc44.x86_64`
  - `vulkan-loader-devel-1.4.341.0-1.fc44.x86_64`
  - `xorg-x11-server-Xwayland-24.1.14-2.fc44.x86_64`
  - `libinput-1.31.3-1.fc44.x86_64` (**atenção:** `libinput-devel` **não** está instalado neste host, portanto `SGE_HAVE_LIBINPUT` não foi habilitado no build atual).

---

## 4. Resultados Reais dos Testes

1. **Build limpo Release (`-Wall -Wextra -Werror`) e CTest completo (`QT_QPA_PLATFORM=offscreen`):**
   - **48/48 testes aprovados** (`48,98 s`), incluindo `tst_input_hardware_backend` (com o novo caso `kmsConfigResolutionAcrossInstallPrefixesAndEnv`) e `tst_logind_integration`.
2. **Instalação em prefixo customizado (`/tmp/sge-install-0321`):**
   - Instalados: `bin/systalia_glass_engine`, `bin/systalia-session`, `share/systalia/eglfs_kms.json`, `share/wayland-sessions/systalia.desktop`.
   - Verificado que `systalia.desktop` aponta `Exec=/tmp/sge-install-0321/bin/systalia-session` e `TryExec=/tmp/sge-install-0321/bin/systalia-session`.
3. **Regressão do modo headless a partir de `/tmp` (`QT_QPA_PLATFORM=offscreen`):**
   - Executável instalado carregou QML/QRC (`Systalia.Engine 1.0`), inicializou Wayland (`wayland-systalia-0`), XDG Shell, XDG Decoration, Viewporter e XWayland `:1` sem depender do diretório da árvore fonte.
4. **Regressão do modo aninhado via script instalado (`/tmp/sge-install-0321/bin/systalia-session`, `cwd="/tmp"`):**
   - Iniciado com `dbus-run-session` + `alacritty` no socket `wayland-systalia-0`, encerrando com `exit 0` e limpeza automática de evidências via `EvidenceRun`.
   - Diretórios temporários `/tmp/sge-build-0321` e `/tmp/sge-install-0321` removidos após a validação.

---

## 5. Riscos e Bloqueadores Restantes

- **Sessão DRM/KMS e `logind` em hardware real:** não testados nesta etapa (requer login SDDM/TTY fora de sessão aninhada).
- **`libinput-devel` ausente no host:** o backend real `libinput` (`SGE_HAVE_LIBINPUT=1`) precisa ser compilado e validado com `libinput-devel` instalado na etapa 03.2.2.
- **Empacotamento RPM / caminhos de sistema:** embora `cmake --install` suporte qualquer prefixo, a descoberta automática pelo SDDM/GDM no Fedora exige instalação em `/usr/share/wayland-sessions/` (ou configuração explícita do display manager).

---

## 6. Critérios de Entrada para a Etapa 03.2.2

1. Instalar `libinput-devel` no ambiente Fedora 44 alvo para compilar com `SGE_HAVE_LIBINPUT=1`.
2. Instalar ou empacotar o SYSTALIA no ambiente de teste Fedora (VM ou hardware dedicado) com `systalia.desktop` visível ao SDDM/TTY.
3. Executar roteiro real de login independente, aquisição DRM/KMS (`eglfs_kms` + Vulkan), entrada `libinput`, troca de VT (`PauseDevice`/`ResumeDevice`) e encerramento limpo com coleta de logs.
