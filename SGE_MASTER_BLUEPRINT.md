# Systalia Glass Engine — Blueprint Mestre de Engenharia v3.0

**Data de revisão:** 26 de outubro de 2026
**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
**Branch de desenvolvimento:** `feat/system-telemetry-topbar`
**Plano complementar:** `SGE_IMPLEMENTATION_PLAN.md`
**Stack alvo:** C++23, Qt 6.7+, QtQuick/Scene Graph, QtWaylandCompositor, Vulkan/QRhi, GLSL 450, CMake, Ninja e CTest.

Este documento descreve o estado técnico observado no repositório, fixa os contratos que não podem regredir e define a direção do Systalia Glass Engine (SGE). Ele substitui o roteiro v2.0, cujo ponto de intervenção estava desatualizado e ainda classificava recursos implementados como tarefas futuras. A reformulação visual do Systalia — VALO está prevista; os valores visuais finais dependem da leitura dos frames e componentes atuais do Figma e não são inventados neste documento.

## 1. Produto e escopo

O SGE é o compositor Wayland do Systalia Desktop Environment (SDE), integrado ao motor óptico VALO Oceanic Glass. O projeto fornece:

- Um compositor Wayland baseado em `QWaylandCompositor`, com suporte a clientes XDG, decorações do lado do servidor, foco, empilhamento e redimensionamento.
- Uma shell QtQuick com desktop, barra superior, dock, lançador, notificações, controlos rápidos, alternador de tarefas e uma janela demonstrativa Systalia Files.
- Um motor gráfico de superfícies refrativas, baseado em buffers uniformes `std140` e shaders GLSL compilados para SPIR-V pelo CMake.
- Serviços de sistema em C++ para telemetria, lançamento e indexação de aplicações, notificações D-Bus e controlos de volume/brilho.
- Integração opcional de metadados de superfície por meio do protocolo Wayland proprietário `z_systalia_surface_v1`.

A execução atual hospeda a shell QtQuick como aplicação e inicia um servidor Wayland próprio. A sessão autónoma sobre DRM/KMS, logind e entrada direta de dispositivos continua no roadmap de produção; o projeto ainda não deve ser descrito como sessão bare-metal pronta.

## 2. Contratos de engenharia inegociáveis

### 2.1 Layout de GPU

`SystaliaSurfaceProps` mantém exatamente 128 bytes, alinhamento de 16 bytes e estes offsets:

| Campo | Offset | Tamanho |
|---|---:|---:|
| `matrix` | 0 | 64 bytes |
| `radii` | 64 | 16 bytes |
| `windowSize` | 80 | 8 bytes |
| `screenResolution` | 88 | 8 bytes |
| `slant` | 96 | 4 bytes |
| `ior` | 100 | 4 bytes |
| `chromaticDispersion` | 104 | 4 bytes |
| `causticIntensity` | 108 | 4 bytes |
| `opacity` | 112 | 4 bytes |
| `time` | 116 | 4 bytes |
| `hasClientTexture` | 120 | 4 bytes |
| `titleBarHeight` | 124 | 4 bytes |

Os `static_assert` em `src/gfx/SystaliaSurfaceProps.hpp` e os testes de alinhamento são a autoridade para esse contrato.

### 2.2 Geometria e shader

A superfície VALO usa a superelipse analítica `sdRoundedBox4G2` de quarta ordem, raios independentes, recorte com derivadas `fwidth`, amostragem do fundo em screen-space, menisco com normal na direção `+norm`, transparência e cáusticas animadas por `u_Time`. A reformulação de QML não pode alterar esses comportamentos nem remover o recorte do conteúdo cliente sem uma substituição validada.

O shader principal é `shaders/valo_optical.frag`; seus recursos de vértice e o material C++ estão em `shaders/valo_optical.vert`, `src/gfx/SystaliaGlassMaterial.*` e `src/gfx/SystaliaGlassItem.*`.

### 2.3 Wayland e protocolo

- O socket de desenvolvimento padrão é `wayland-systalia-0` dentro de `XDG_RUNTIME_DIR`.
- A interface base para clientes externos é `xdg_wm_base`; o compositor impõe SSD por `zxdg_decoration_manager_v1`.
- A geração das interfaces servidoras de `z_systalia_surface_v1.xml` é feita pelo CMake via `qt6_generate_wayland_protocol_server_sources`.
- Recursos de superfície precisam ser invalidados quando a superfície ou o cliente é destruído.
- Operações de janela devem passar pelo compositor/assento e respeitar a superfície XDG viva. O QML não deve chamar métodos inexistentes em proxies de cliente.

### 2.4 Fundo e captura

A imagem de fundo oficial atualmente versionada na raiz é `carina-nebula-star-formation-astronomy-astrophysics-stars-3840x2160-4312.jpg`. `src/main.cpp` procura o arquivo na pasta pai do executável e no diretório de execução; `src/qml/main.qml` usa `Image.PreserveAspectCrop`. O fundo procedural de gradientes, grelha, barras, orbes e partículas foi removido.

`backdropGrab` continua a capturar a imagem para que as superfícies de vidro possam refratá-la. Essa captura não é um cenário sintético; deve ser preservada enquanto houver componentes que a consumam. O custo de captura em resoluções altas deve ser medido durante a reformulação.

## 3. Arquitetura e inventário atual

### 3.1 Build e organização

`CMakeLists.txt` cria `sge_shaders`, `sge_core` e o executável `systalia_glass_engine`. Os QML da shell são empacotados no módulo `Systalia.Engine`. A compilação usa C++23 e `-Wall -Wextra -Werror` para C++.

Áreas principais:

- `src/gfx/`: estrutura uniform, material, item gráfico e geometria de recorte.
- `src/compositor/`: compositor, extensão Wayland, modelo de janelas e foco MRU.
- `src/system/`: serviços de telemetria, processos, catálogo de aplicações, notificações e controlos de sistema.
- `src/qml/`: janela raiz, contêiner de cliente Wayland e componentes da shell em `shell/`.
- `shaders/`: shaders VALO.
- `tests/`: testes unitários, integração, ciclo de vida E2E e testes históricos do motor.

### 3.2 Motor gráfico

`SystaliaGlassItem` publica as propriedades de curvatura, slant, IOR, dispersão, intensidade de cáusticas, opacidade, tempo, fundo e textura cliente. `SystaliaGlassMaterial` atualiza o bloco uniforme e as texturas do Scene Graph. O pipeline de shaders é compilado pelo Qt ShaderTools.

### 3.3 Compositor Wayland e janelas

`SystaliaCompositor` inicializa o socket, XDG Shell, manager SSD, assento padrão, handshake inicial, superfície de protocolo e rastreamento de clientes. `WindowStackModel` mantém janelas em ordem MRU, dá suporte ao ciclo Alt+Tab/Alt+Shift+Tab e publica a seleção para QML.

`WaylandWindowContainer.qml` apresenta clientes, mantém a moldura SSD, redimensiona pelas bordas e cantos, fornece minimizar/maximizar/fechar e associa metadados ópticos. A integração gráfica de buffers de clientes precisa ser validada no tipo de sessão e driver alvo.

### 3.4 Serviços do sistema

- `SystemTelemetryService` lê `/proc/stat` e `/proc/meminfo` e publica CPU, memória, hora e FPS.
- `ProcessLauncherService` inicia aplicações com ambiente Wayland dedicado e acompanha processos ativos.
- `AppRegistryService` varre arquivos `.desktop`, prepara ícones, categorias e filtragem.
- `NotificationServer` exporta `org.freedesktop.Notifications` no barramento da sessão e mantém um modelo com timers de expiração. O nome D-Bus é exclusivo: se outro daemon já o possui, o SGE informa o conflito e não o substitui automaticamente.
- `SystemControlService` lê e aplica volume/brilho por ferramentas disponíveis, e mantém os toggles de estado da shell.

### 3.5 Shell e composição visual

`src/qml/main.qml` contém o desktop, wallpaper, captura do backdrop, janela demo Systalia Files, camada de clientes e pontos de integração dos componentes:

- `SystaliaTopBar.qml` — marca, hora, telemetria e controlos.
- `SystaliaDock.qml` — dock inferior e atalhos.
- `SystaliaFlowMenu.qml` — lançador e pesquisa de aplicações.
- `SystaliaControlCenter.qml` — ajustes rápidos.
- `SystaliaNotificationHost.qml` — toasts do servidor D-Bus.
- `SystaliaTaskSwitcher.qml` — miniaturas e seleção MRU.

A reformulação integral do design deve substituir ou ajustar a apresentação sem duplicar serviços nem remover interações já implementadas. O inventário visual final precisa ser conciliado com os frames atuais do Figma antes de definir cores, tipografia, medidas e estados exatos.

## 4. Estado das etapas

“Implementado” nesta tabela descreve código presente; não equivale a homologação visual ou validação em todos os drivers. Os logs de execução e o resultado mais recente do CTest devem acompanhar qualquer decisão de release.

| Fase/etapa | Escopo | Situação observada |
|---|---|---|
| Fase 1 | Motor óptico, shader, item QSG e layout 128 bytes | Implementado; invariantes e testes dedicados presentes. |
| 2.1 | Servidor Wayland e socket | Implementado; `tst_wayland_socket`. |
| 2.2A/B | XDG Shell e SSD | Implementado; testes de shell e decorações. |
| 2.2C | Resize de bordas/cantos | Implementado; `tst_xdg_resize` cobre geometria e interação. |
| 2.3 | Protocolo de metadados ópticos | XML, geração CMake, extensão e testes presentes. |
| 2.4 | Composição/recorte de conteúdo de cliente | Shader, viewport e teste de geometria presentes; rendering real ainda requer confirmação no ambiente do usuário. |
| 3.1 | TopBar e telemetria | Componentes e serviço presentes; validação visual permanece necessária. |
| 3.2 | Dock e lançamento de processos | Componente e serviço presentes. |
| 3.3/3.4 | Integração de clientes, foco e lançador de aplicações | Handshake, catálogo, filtro, foco e testes presentes; a apresentação de buffers GPU segue como risco aberto. |
| 3.5 | Notificações D-Bus e controles rápidos | Serviço, modelo e componentes presentes; conflito com outro dono do serviço D-Bus é uma condição de execução. |
| 3.6 | Alternador de tarefas | Modelo MRU, atalhos e QML presentes; correções recentes de foco e dimensão do frame aguardam validação no ambiente alvo. |
| 3.7 | Revisão, ASan/UBSan e benchmark | Pendente como auditoria completa; a expansão de testes não substitui profiling ou análise de leaks. |
| Redesign VALO/SDE | Nova linguagem visual a partir do Figma | Planejado; frames, medidas e componentes do redesign ainda não foram incorporados a uma especificação técnica verificável. |
| Fases 4–6 | Workspaces, tiling, multi-monitor, sessão autônoma, áudio e terminal | Roadmap; não implementado integralmente. |

### Risco de execução conhecido

Em uma execução reportada pelo usuário, QtWaylandCompositor/Mesa registrou falta de `EGL_WL_bind_wayland_display` e falha ao obter informação do dispositivo. Isso é uma limitação da integração de buffers EGL/driver no ambiente de teste e pode impedir a exibição de clientes acelerados por GPU. Não se deve tratar como aviso cosmético nem afirmar que Alacritty está homologado até que o buffer apareça e aceite input em uma sessão representativa.

Avisos de tema de ícones ausente ou de escala de SVG são questões de ambiente/recursos e devem ser tratados separadamente do contrato Wayland. O nome D-Bus já ocupado deve ser diagnosticado sem tomar silenciosamente o serviço do desktop hospedeiro.

## 5. Redesign visual: requisitos para atualizar este blueprint

O redesign será conduzido a partir de `SYSTALIA — VALO | Desktop Concept`. Esta revisão não declara que o Figma já foi inspecionado; somente o nome e links do projeto foram fornecidos. Antes de traduzir o design em valores de produção, registrar:

1. Frames de desktop, estados e dimensões de referência.
2. Tokens de cor, tipografia, espaçamento, raio, elevação e material.
3. Componentes reutilizáveis e suas variantes/estados.
4. Comportamentos de hover, foco, teclado, animações, menus e janelas.
5. Uso do wallpaper Carina, contraste e recorte nas proporções alvo.
6. Acessibilidade, escala e comportamento em resoluções distintas.
7. Mapeamento de cada componente visual para QML/C++ existente ou componente novo.

Quando o Figma estiver acessível ou for exportado, atualizar esta seção com os valores e decisões observados, remover os itens pendentes e numerar a etapa de implementação aprovada.

## 6. Roadmap de engenharia

### 6.1 Próximo trabalho: redesign Systalia — VALO

1. Auditar frames e extrair a especificação visual.
2. Consolidar tokens e layout sem alterar contratos Wayland/GLSL.
3. Reestruturar o desktop e os componentes shell segundo a hierarquia do design.
4. Validar os estados das janelas externas e da aplicação demo.
5. Corrigir/documentar importação de buffers GPU no ambiente alvo.
6. Executar CTest, inspeção visual e profiling; atualizar o plano técnico e registrar discrepâncias.

### 6.2 Fase 3.7 — Auditoria técnica

- ASan/UBSan no ciclo de criação/remoção de superfícies e processos.
- Benchmark com várias janelas e medição da captura de backdrop.
- Revisão dos avisos de QML, includes e timers.
- Confirmação dos 128 bytes e de todos os testes.

### 6.3 Fase 4 — Workspaces, tiling e multi-monitor

- 4.1 workspaces virtuais com foco e isolamento de janelas.
- 4.2 snapping e tiling com raios externos/internos coordenados.
- 4.3 outputs múltiplos, escala fracionária e migração de superfícies.
- 4.4 auditoria de IPC, desconexões e clientes malformados.

### 6.4 Fase 5 — Sessão autônoma

- 5.1 backend DRM/KMS, libinput/libudev e integração de dispositivos.
- 5.2 logind/SDDM, scripts de sessão e limpeza de VT.
- 5.3 bridge XWayland sob demanda.

### 6.5 Fase 6 — Ecossistema e release

- 6.1 integração PipeWire e reação óptica a áudio, se mantida no escopo.
- 6.2 terminal nativo do Systalia.
- 6.3 homologação, empacotamento e release 1.0.

## 7. Testes e definição de pronto

O `tests/CMakeLists.txt` registra 23 entradas de CTest no estado observado do branch: testes do motor, geometry/SDF, integração Wayland, serviços, testes unitários expandidos, integração de protocolo e E2E. Um alvo CTest não é sinônimo de um único caso QTest; use o output de CTest e os slots executados para reportar cobertura sem confundir os números.

Comandos canônicos:

~~~sh
cmake -S . -B build -G Ninja
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
~~~

Uma etapa está pronta quando:

- compila com C++23 e sem warnings tratados como erros;
- os testes relacionados e a suíte completa passam no ambiente de build indicado;
- input, lifecycle e apresentação de cliente são testados onde aplicáveis;
- a revisão visual corresponde ao Figma inspecionado;
- diferenças restantes, limitações do ambiente e evidências de validação ficam registradas.

## 8. Procedimento de mudança

1. Identificar os arquivos e contratos afetados.
2. Atualizar o plano técnico para refletir dependências e critérios.
3. Fazer uma mudança vertical testável, sem regressão dos contratos existentes.
4. Rodar build e os testes apropriados; executar a suíte completa antes de marcar uma fase como homologada.
5. Comparar execução com o design, registrar evidência e revisar este blueprint quando status ou decisões mudarem.
6. Só então preparar commit, tag ou PR de acordo com o escopo pedido pelo responsável.
