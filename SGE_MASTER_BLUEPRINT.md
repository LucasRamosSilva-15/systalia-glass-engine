# SYSTALIA GLASS ENGINE (SGE) & COMPOSITOR WAYLAND — MASTER ENGINEERING BLUEPRINT (v2.0)

**Agentes Executores Alvo:** GPT SOL 6.1 / GPT-6 Luna Extra-Alto  
**Linguagem & Padrões:** C++23 | Qt 6.7+ (QRhi Vulkan Nativo, QtQuick SceneGraph, QtWaylandCompositor) | GLSL 450 (SPIR-V) | CMake + Ninja + CTest  
**Diretriz Operacional Inegociável:** Desenvolvimento estritamente atômico, determinístico, compilação livre de avisos (`-Wall -Wextra -Werror` onde aplicável) e validação obrigatória no `ctest` a cada sub-etapa antes de qualquer commit ou avanço de tag.

---

## 1. VISÃO GERAL E FILOSOFIA DE ENGENHARIA

O **Systalia Glass Engine (SGE)** é um **Compositor Wayland Independente** e motor óptico nativo concebido para o **Systalia Desktop Environment (SDE)**. Ele não é um plugin ou tema para o KWin/Mutter; ele substitui o KWin por completo na sessão final.

### Fundamentos Técnicos Centrais:
1. **Linguagem Visual VALO Oceanic Glass (Frutiger Aero Moderno):** A superfície de cada elemento atua como água límpida tensionada por coesão superficial: menisco convexa periférico de $28.0\text{ px}$, refração de Snell em Screen-Space (`gl_FragCoord.xy / u_ScreenResolution`), dispersão cromática de Cauchy ($B \approx 0.040$) e cáusticas subaquáticas procedurais moduladas no tempo (`u_Time`).
2. **Geometria Analítica G2 Contínua (Superelipse de 4ª Ordem):**
   $$d_{\text{corner}}(\mathbf{q}) = \left(q_x^4 + q_y^4\right)^{\frac{1}{4}} - r$$
3. **Anti-Aliasing Analítico Subpixel:** Borda suavizada via derivada espacial $\text{fw} = \max(\text{fwidth}(d), 0.75)$, com descarte imediato (`discard`) na GPU quando $\alpha_{\text{mask}} \le 0.001$, eliminando aliasing e o efeito de borrado de acrílico com vazamento de canto (*blur bleed*).
4. **Layout de Memória Estrito `std140`:** O bloco uniforme `SystaliaSurfaceProps` é cravado em **exatos 128 bytes** (`static_assert(sizeof(SystaliaSurfaceProps) == 128)`). Nenhuma alteração pode corromper essa assinatura de alinhamento binário.

---

## 2. INVENTÁRIO DO QUE JÁ ESTÁ CONCLUÍDO E HOMOLOGADO

O agente **NÃO DEVE** refazer ou alterar as bases já validadas que sustentam a compilação atual:

* **Fase 1 (Motor Óptico Vulkan & UI Canônica) — `[CONCLUÍDA / TAGGEADA]`:**
  * Pipeline de shaders offline SPIR-V (`valo_optical.vert` e `valo_optical.frag`).
  * Implementação de `SystaliaGlassMaterial`, `SystaliaGlassItem` e `SystaliaSurfaceProps` (128 bytes).
  * Menisco líquido em vetor normal exterior ($\mathbf{+norm}$), refração límpida e cáusticas ativas.
  * Validações aprovadas: `tst_systalia_engine`, `tst_sdf_geometry`, `tst_optical_physics`.
* **Fase 2 (Infraestrutura Wayland & Protocolos) — `[CONCLUÍDA / TAGGEADA]`:**
  * **Etapa 2.1:** Servidor Wayland nativo herdando de `QWaylandCompositor`, abrindo socket Unix dedicado `wayland-systalia-0` em `$XDG_RUNTIME_DIR`. Teste `tst_wayland_socket` aprovado.
  * **Etapa 2.2A & 2.2B:** Suporte a `QWaylandXdgShell` (`xdg_wm_base`) e imposição obrigatória de Server-Side Decorations via `QWaylandXdgDecorationManagerV1` (`zxdg_decoration_manager_v1`). Clientes externos rodam envelopados sem barras CSD.
  * **Etapa 2.2C:** Redimensionamento interativo pelas 4 bordas e 4 cantos do `WaylandWindowContainer.qml` com troca dinâmica de cursores e envio assíncrono de `sendConfigure` com estado `Resizing`. Teste `tst_xdg_resize` aprovado.
  * **Etapa 2.3:** Protocolo customizado proprietário `z_systalia_surface_v1.xml` com compilação automatizada via `qt6_generate_wayland_protocol_server_sources` e wrappers C++ para controle de raios G2, IOR e slant. Teste `tst_systalia_protocol` aprovado.
* **Fase 3 (Shell Básico) — `[PARCIALMENTE CONCLUÍDA]`:**
  * **Etapa 3.1:** Barra Superior Canônica (`SystaliaTopBar.qml`) ativa com leitura de telemetria Linux real em C++ (`/proc/stat` e `/proc/meminfo`), data/hora e monitor de FPS real do QRhi Vulkan.
  * **Etapa 3.2:** Superbar/Dock Flutuante Inferior (`SystaliaDock.qml`) em chassi óptico com ícones canônicos, animação fluida de escala ao hover e serviço C++ assíncrono `ProcessLauncherService` com injeção de `WAYLAND_DISPLAY=wayland-systalia-0`.

---

## 3. ESTADO ATUAL E PONTO DE INTERVENÇÃO IMEDIATO: ETAPA 3.3

### Diagnóstico de Tela Atual:
O compositor sobe a TopBar com telemetria viva, a Superbar responde no rodapé e o terminal `Alacritty` abre dentro da moldura `SGE SSD`. Contudo, **o conteúdo interno do terminal aparece transparente (apenas o vidro é renderizado), sem prompt ou texto**.

* **Causa Raiz:** O cliente Wayland não aloca buffers gráficos nem começa a desenhar até que receba o handshake inicial `sendConfigure` do compositor definindo dimensões válidas, e o `WaylandQuickItem` no QML necessita de vinculação explícita da propriedade `surface` com dimensões calculadas.
* **Ação:** A **Etapa 3.3** é a etapa de execução imediata.

---

## 4. ROTEIRO COMPLETO DE ENGENHARIA PASSO A PASSO

---

### ETAPA 3.3: Handshake de Configuração, Apresentação do Buffer Cliente e Foco/Z-Order `[ETAPA ATUAL]`

#### 1. Objetivos Técnicos
1. Finalizar o handshake `xdg_surface.configure` garantindo que o cliente receba dimensões iniciais e comece a enviar frames para o buffer.
2. Garantir que o `WaylandQuickItem` dentro do `WaylandWindowContainer.qml` exiba o buffer de pixels com suporte completo a eventos de teclado/mouse do `QWaylandSeat`.
3. Implementar elevação de janela (*raise*) e política de foco de ativação por clique (*click-to-focus*).

#### 2. Especificação de Implementação
* **Em `src/compositor/SystaliaCompositor.cpp`:**
  - No slot `onToplevelCreated`:
    - Enviar configuração inicial imediatamente e conectar ao sinal de primeira confirmação:
      ```cpp
      toplevel->sendConfigure(QSize(800, 500), {QWaylandXdgToplevel::ActivatedState});
      ```
    - Configurar foco de assento (`defaultSeat()->setKeyboardFocus(toplevel->surface())`).
* **Em `src/qml/WaylandWindowContainer.qml`:**
  - Garantir que o componente que abriga o cliente possua geometria declarada e vínculo com a superfície:
    ```qml
    WaylandQuickItem {
        id: clientItem
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        anchors.margins: 8
        surface: toplevel ? toplevel.surface : null
        paintMode: WaylandQuickItem.DirectToBuffer
        focus: true
    }
    ```
  - Adicionar `TapHandler` na moldura para trazer a janela para o topo da lista (`z = ++rootWorkspace.topZIndex`) e transferir o foco do teclado.
* **Teste Automatizado (`tests/tst_ClientHandshake.cpp`):**
  - Simular conexão de um toplevel, verificar se `sendConfigure` é despachado com dimensões válidas e se o assento recebe o foco.
* **Critério de Aceitação:** Ao rodar `WAYLAND_DISPLAY=wayland-systalia-0 alacritty`, o prompt do terminal (texto, shell e cursor) é visível dentro da janela, recebe digitação do teclado e a janela sobe para o topo ao ser clicada.
* **Tag:** `sge-v0.3.3`

---

### ETAPA 3.4: Menu Global de Aplicações "Systalia Flow" & Varredura XDG

#### 1. Objetivos Técnicos
Implementar o painel flutuante de aplicativos em overlay (acionado pelo botão `SYSTALIA` da TopBar ou atalho `Super`/`Meta`), exibindo os programas instalados no sistema operacional.

#### 2. Especificação de Implementação
* **Serviço C++ (`src/system/AppRegistryService.hpp/.cpp`):**
  - Leitura assíncrona de arquivos `.desktop` em `/usr/share/applications` e `~/.local/share/applications`.
  - Exposição de um `QAbstractListModel` com os campos `name`, `execCommand`, `iconName` e `categories`.
  - Filtragem em tempo real com `QSortFilterProxyModel` para busca instantânea.
* **Componente QML (`src/qml/shell/SystaliaFlowMenu.qml`):**
  - Chassi de vidro óptico com curvatura contínua G2, campo de pesquisa com foco automático e grade de ícones com física de realce ao passar o mouse.
  - Execução via `ProcessLauncherService` com injeção automática de `WAYLAND_DISPLAY=wayland-systalia-0`.
* **Teste Automatizado (`tests/tst_AppRegistry.cpp`):**
  - Leitura de diretório temporário com arquivos `.desktop` simulados e validação da filtragem por string.
* **Tag:** `sge-v0.3.4`

---

### ETAPA 3.5: Central de Notificações D-Bus & Controles de Hardware "Abyss Control"

#### 1. Objetivos Técnicos
Implementar o painel retrátil no canto superior direito da tela para controles rápidos de sistema (áudio, rede, brilho) e suporte ao servidor de notificações Linux.

#### 2. Especificação de Implementação
* **Serviço C++ D-Bus (`src/system/NotificationServer.hpp/.cpp`):**
  - Implementar o serviço D-Bus `org.freedesktop.Notifications`.
  - Método `Notify`: intercepta aplicativo remetente, título, corpo, ícone e urgência.
* **Toasts Translúcidos (`src/qml/shell/SystaliaToast.qml`):**
  - Notificações em cápsula de vidro líquido com animação elástica (*spring physics*), empilhamento ordenado e auto-remoção com temporizador configurável.
* **Quick Sliders:**
  - Controles de volume integrados via PipeWire/PulseAudio (`libpulse` ou D-Bus) com resposta refrativa em tempo real.
* **Teste Automatizado (`tests/tst_NotificationServer.cpp`):**
  - Disparo de notificações via D-Bus mock e validação de recepção na fila do servidor.
* **Tag:** `sge-v0.3.5`

---

### ETAPA 3.6: Alternador de Tarefas Visuais & Visão Geral Fluida (Alt+Tab / Exposé)

#### 1. Objetivos Técnicos
Permitir visualização panorâmica de todas as superfícies abertas em tempo real e alternância cíclica via teclado.

#### 2. Especificação de Implementação
* **Modelo C++ de Janelas (`src/compositor/WindowStackModel.hpp/.cpp`):**
  - Gerenciamento ordenado da pilha Z de clientes ativos.
* **Carrossel QML (`src/qml/shell/SystaliaTaskSwitcher.qml`):**
  - Exibição de miniaturas ao vivo utilizando as texturas de buffer do compositor.
  - Interceptação de atalhos globais (`Alt+Tab`, `Alt+Shift+Tab`) no compositor.
  - Efeito óptico de profundidade: janelas inativas recebem distorção refrativa acentuada enquanto a selecionada recebe realce luminoso ciano.
* **Teste Automatizado (`tests/tst_TaskSwitcher.cpp`):**
  - Validação da ordem de alternância e transferência do foco de ativação.
* **Tag:** `sge-v0.3.6`

---

### ETAPA 3.7: [REVISÃO & REFATORAÇÃO 1] Auditoria de Código, Limpeza e Benchmarking de Concha

#### 1. Objetivos Técnicos
Realizar auditoria estrita da base de código acumulada nas Fases 1 a 3 antes de iniciar recursos de múltiplos monitores e tiling.

#### 2. Itens de Auditoria:
* **Detecção de Memory Leaks:** Execução com AddressSanitizer (`-fsanitize=address,undefined`) para garantir destruição limpa de superfícies Wayland, nós de cena e processos fechados.
* **Estabilidade do Alinhamento `std140`:** Verificação de que nenhum novo membro quebrou o alinhamento da struct de 128 bytes.
* **Profiling de CPU/GPU:** Medição da taxa de quadros (60+ FPS constante) e tempo de frame ($< 16.6\text{ ms}$) com 5 janelas clientes simultâneas abertas.
* **Refatoração de Includes:** Aplicação do princípio de Includes Mínimos (IWYU) e eliminação de código redundante no QML.
* **Tag:** `sge-v0.3.7-review`

---

### FASE 4: WORKSPACES DINÂMICOS, TILING & MULTI-MONITOR

---

### ETAPA 4.1: Espaços de Trabalho Virtuais Contínuos (Virtual Workspaces)

#### 1. Objetivos Técnicos
Adicionar suporte a múltiplos desktops virtuais com transição horizontal contínua de câmera e paralaxe óptico do fundo oceânico.

#### 2. Especificação Técnica:
* O compositor gerencia uma coleção de workspaces contendo listas isoladas de `WaylandWindowContainer`.
* Atalhos `Super + Ctrl + Esquerda/Direita` animam a transição com curva de aceleração cúbica.
* Janelas podem ser transferidas entre workspaces via arrastar e soltar ou atalho de teclado (`Super + Shift + Esquerda/Direita`).
* **Teste Automatizado (`tests/tst_Workspaces.cpp`):**
  - Comprovar isolamento de superfícies entre espaços e preservação de texturas offscreen.
* **Tag:** `sge-v0.4.1`

---

### ETAPA 4.2: Snapping Magnético & Tiling Hidrodinâmico ("Liquid Tiling")

#### 1. Objetivos Técnicos
Implementar ancoragem inteligente de janelas em meia-tela (split 50/50) ou quadrantes (25%), com adaptação contínua da curvatura física do menisco líquido.

#### 2. Especificação Técnica:
* **Fusão de Bordas:** Quando duas janelas são acopladas lado a lado, a aresta adjacente achata para raio $0.0\text{ px}$, mantendo a curvatura externa de $28.0\text{ px}$.
* **Divisor Central Interativo:** Uma barra transparente permite redimensionar simultaneamente ambas as metades divididas.
* **Preview Orgânico:** Ao aproximar uma janela das bordas da tela, uma gota de vidro translúcida expande-se indicando a zona de fixação magnética (*drop zone*).
* **Teste Automatizado (`tests/tst_Tiling.cpp`):**
  - Validação das equações de geometria acoplada e reconfiguração de raios via `z_systalia_surface_v1`.
* **Tag:** `sge-v0.4.2`

---

### ETAPA 4.3: Suporte a Múltiplos Monitores & Escalonamento Fracionário

#### 1. Objetivos Técnicos
Implementar o protocolo `wl_output`, `xdg_output_unstable_v1` e suporte ao protocolo de escala fracionária `wp_fractional_scale_v1`.

#### 2. Especificação Técnica:
* Cada display físico recebe uma instância de renderização própria sincronizada no mesmo loop gráfico Vulkan.
* A TopBar ancora no display primário com opção de espelhamento; o dock adapta-se à resolução de cada monitor.
* Filtros analíticos subpixel no shader impedem perda de nitidez em fatores de escala 125%, 150% e 175%.
* **Teste Automatizado (`tests/tst_MultiOutput.cpp`):**
  - Adição e remoção simulada de monitores em runtime com verificação de migração de janelas.
* **Tag:** `sge-v0.4.3`

---

### ETAPA 4.4: [REVISÃO & REFATORAÇÃO 2] Validação de Protocolos e Robustez IPC

#### 1. Objetivos Técnicos
Auditoria profunda do subsistema Wayland para garantir conformidade estrita com o padrão XDG Shell e resiliência contra clientes maliciosos ou instáveis.

#### 2. Itens de Auditoria:
* **Tratamento de Crash de Clientes:** Garantir que quando um cliente externo trava (`SIGSEGV` / `SIGKILL`), o compositor limpa todos os recursos, texturas e contêineres sem congelar o loop gráfico.
* **Teste de Fuzzing de IPC:** Envio de sequências anômalas de `sendConfigure` e redimensionamentos extremos para garantir que nenhum ponteiro nulo seja acessado.
* **Tag:** `sge-v0.4.4-review`

---

### FASE 5: SESSÃO AUTÔNOMA BARE-METAL (DRM/KMS) & PRODUÇÃO

---

### ETAPA 5.1: Backend Nativo DRM/KMS e Libinput (Sessão Direta sem KWin)

#### 1. Objetivos Técnicos
Permitir que o binário do Systalia Glass Engine seja iniciado a partir de um console puro (TTY) sem nenhum servidor X11 ou Wayland intermediário rodando.

#### 2. Especificação Técnica:
* Configuração do pipeline gráfico do Qt para rodar sobre `eglfs_kms` ou extensão direta Vulkan Display (`VK_KHR_display`).
* Integração de `libinput` e `libudev` para captura direta de ponteiros de mouse, teclado e touchpads a partir de nós `/dev/input/event*`.
* Suporte nativo a aceleração de cursor por hardware (*hardware planes* KMS) para latência de ponteiro zero.
* **Teste Automatizado (`tests/tst_DrmKmsBackend.cpp`):**
  - Verificação de enumeração de nós DRM e compatibilidade com buffers GBM.
* **Tag:** `sge-v0.5.1`

---

### ETAPA 5.2: Integração com `systemd-logind` e Entrada de Sessão SDDM

#### 1. Objetivos Técnicos
Disponibilizar a sessão gráfica do Systalia na tela de login do Linux (SDDM / GDM) com alternância segura de sessões via console virtual (VT switching).

#### 2. Especificação Técnica:
* Aquisição e liberação segura de dispositivos gráficos restritos via interface D-Bus `org.freedesktop.login1.Session` (`TakeControl`, `ReleaseControl`).
* Criação do arquivo de inicialização de sessão `/usr/share/wayland-sessions/systalia.desktop`.
* Script de bootstrap `systalia-session` configurando variáveis canônicas de ambiente (`XDG_CURRENT_DESKTOP=Systalia`, `MOZ_ENABLE_WAYLAND=1`, `QT_QPA_PLATFORM=wayland`).
* **Critério de Aceitação:** Selecionar "Systalia" no SDDM e fazer login direto no ambiente gráfico líquido nativo.
* **Tag:** `sge-v0.5.2`

---

### ETAPA 5.3: Camada de Compatibilidade Legada XWayland

#### 1. Objetivos Técnicos
Permitir que aplicativos legados ou jogos que não suportam Wayland nativo (Steam, softwares baseados em X11) funcionem normalmente dentro do SGE.

#### 2. Especificação Técnica:
* Inicialização sob demanda do executável `Xwayland` associado a um display Unix dedicado (`:0`).
* Mapeamento automático de janelas X11 para instâncias de `WaylandWindowContainer` com aplicação do chassi refrativo e decorações SSD correspondentes.
* **Teste Automatizado (`tests/tst_XWaylandBridge.cpp`):**
  - Confirmação do ciclo de vida e mapeamento de janelas X11 simuladas.
* **Tag:** `sge-v0.5.3`

---

### FASE 6: REFINAMENTO MULTIMÍDIA E APLICAÇÕES NATIVAS SDE

---

### ETAPA 6.1: Pipeline de Áudio PipeWire e Modulação Acústica das Cáusticas

#### 1. Objetivos Técnicos
Conectar o motor óptico à camada de áudio do sistema operacional, permitindo que a luz cáustica e a dispersão reajam de forma sutil à reprodução sonora.

#### 2. Especificação Técnica:
* Módulo C++ conectado à API de streams do PipeWire (`libpipewire-0.3`).
* Cálculo de FFT (Transformada Rápida de Fourier) com 64 bandas espectrais em tempo real.
* Modulação do parâmetro `u_CausticIntensity` no fragment shader a partir de frequências graves (20Hz a 120Hz).
* **Tag:** `sge-v0.6.1`

---

### ETAPA 6.2: Systalia Terminal Nativo (Acelerado por Vulkan)

#### 1. Objetivos Técnicos
Substituir a necessidade de emuladores externos com o desenvolvimento do terminal proprietário do ecossistema, desenhado para tirar proveito total das capacidades de transparência interna e recorte G2 sem buffers intermediários.

#### 2. Especificação Técnica:
* Parser de sequências ANSI e renderizador de grid de glifos acelerado via texturas de atlas na GPU.
* Conexão interna direta com o compositor, alcançando taxas de atualização de mais de 144 FPS com latência de entrada imperceptível.
* **Tag:** `sge-v0.6.2`

---

### ETAPA 6.3: [REVISÃO FINAL DE PRODUÇÃO] Homologação Geral v1.0

#### 1. Objetivos Técnicos
Bateria completa de validação de lançamento da versão 1.0.0 estável do **Systalia Desktop Environment**:
* Execução de 100% da suíte CTest com zero falhas em build de Release (`-O3 -DNDEBUG`).
* Validação de consumo de memória em repouso ($< 250\text{ MB}$ de RAM total para o compositor e shell completos).
* Empacotamento para distribuições Linux baseadas em RPM (Fedora) e Arch Linux (PKGBUILD).
* **Tag:** `sge-v1.0.0-gold`

---

## 5. PROTOCOLO OPERACIONAL OBRIGATÓRIO PARA O AGENTE A CADA TURNO

1. **Auditoria Prévia:** Antes de escrever código, leia os arquivos que serão impactados na etapa para evitar regressões nas etapas concluídas.
2. **Código Completo:** **NUNCA** utilize placeholders, snippets incompletos ou instruções como `// ... restante do código`. Entregue sempre os arquivos integrais.
3. **Criação de Teste Unitário:** Toda etapa requer um teste unitário automatizado correspondente (`tests/tst_*.cpp`) registrado em `tests/CMakeLists.txt`.
4. **Verificação Mandatória:**
   ```bash
   cmake --build build -j$(nproc)
   ctest --test-dir build --output-on-failure
   ```
5. **Relatório de Saída:** Ao finalizar, reporte a saída real do `ctest`, os arquivos criados/modificados e forneça o comando de commit semântico e a tag Git correspondente.