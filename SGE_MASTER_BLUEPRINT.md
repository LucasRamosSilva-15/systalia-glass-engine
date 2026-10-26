# Systalia Glass Engine — Blueprint Mestre de Engenharia v3.0

**Data de revisão:** 26 de outubro de 2026
**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
**Branch de desenvolvimento:** `feat/system-telemetry-topbar`
**Plano complementar:** `SGE_IMPLEMENTATION_PLAN.md`
**Stack alvo:** C++23, Qt 6.7+, QtQuick/Scene Graph, QtWaylandCompositor, Vulkan/QRhi, GLSL 450, CMake, Ninja e CTest.

Este documento descreve o estado técnico observado no repositório, fixa os contratos que não podem regredir e define a direção do Systalia Glass Engine (SGE). Ele substitui o roteiro v2.0, cujo ponto de intervenção estava desatualizado e ainda classificava recursos implementados como tarefas futuras. Foi feita uma auditoria inicial do frame de desktop no Figma e os valores observados estão registados na seção 5. Estados e componentes ausentes, bem como divergências com a implementação atual, continuam identificados como decisões abertas.

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

A reformulação integral do design deve substituir ou ajustar a apresentação sem duplicar serviços nem remover interações já implementadas. O inventário inicial e as divergências estão registados na seção 5; os valores observados são referência, enquanto tokens, estados e comportamento finais dependem de fechar as decisões abertas.

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
| Redesign VALO/SDE | Nova linguagem visual a partir do Figma | Auditoria inicial do frame principal registada na seção 5; implementação visual pendente de resolver fundo, mapeamento do Control Center, estados ausentes e escala-alvo. |
| Fases 4–6 | Workspaces, tiling, multi-monitor, sessão autônoma, áudio e terminal | Roadmap; não implementado integralmente. |

### Risco de execução conhecido

Em uma execução reportada pelo usuário, QtWaylandCompositor/Mesa registrou falta de `EGL_WL_bind_wayland_display` e falha ao obter informação do dispositivo. Isso é uma limitação da integração de buffers EGL/driver no ambiente de teste e pode impedir a exibição de clientes acelerados por GPU. Não se deve tratar como aviso cosmético nem afirmar que Alacritty está homologado até que o buffer apareça e aceite input em uma sessão representativa.

Avisos de tema de ícones ausente ou de escala de SVG são questões de ambiente/recursos e devem ser tratados separadamente do contrato Wayland. O nome D-Bus já ocupado deve ser diagnosticado sem tomar silenciosamente o serviço do desktop hospedeiro.

## 5. Redesign visual: inventário Figma e decisões abertas

Fonte auditada em 26 de outubro de 2026: [SYSTALIA — VALO | Desktop Concept](https://www.figma.com/design/kax6aiYKgM2Fk7dllyZRRF/SYSTALIA-%E2%80%94-VALO-%7C-Desktop-Concept). O arquivo tem uma página, `Page 1`, com uma composição principal `SYSTALIA / VALO — Main Desktop` (node `2:2`) de **1662 × 975 px**. A auditoria documenta o estado estático visível; o frame não fornece variantes nem protótipo de interação.

### 5.1 Composição observada

As posições abaixo são relativas ao frame `2:2`.

| Elemento | Node | Geometria | Conteúdo e aparência observados |
|---|---|---:|---|
| Atmosfera oceânica | `2:3` | 1662 × 975, tela toda | Fundo vetorial azul profundo com halos ciano, raios volumétricos descendentes e partículas bioluminescentes. Não é uma fotografia. |
| Top System Bar | `2:81` | 1646 × 45 em (8, 4), raio 22 | Cápsula azul-marinho translúcida; marca SYSTALIA, `SDE-KWIN \| VALO`, CPU/RAM/FPS, relógio e seis glifos de status. |
| Systalia Files | `2:88` | 701 × 534 em (113, 193), raio 20 | Janela de vidro com navegação lateral, Home ativo, breadcrumb, pesquisa e grade de seis itens; badge `ABYSSUS VES` e ações de janela. |
| Systalia Control Center | `2:134` | 700 × 534 em (864, 193), raio 20 | Janela de ajustes `VALO & SOAR Engine`; navegação lateral, três opções e área `Live Material Controls` com um toggle e seis sliders. |
| Floating Dock | `2:194` | 555 × 125 em (555, 817), raio 47 | Cápsula de vidro com cinco ícones: pasta, áudio, terminal, código e brilho magenta. Cada slot de ícone mede 78 × 78 px. |

As janelas usam contorno branco de 1,5 px com 79% de opacidade, sombra ciano e preta e camadas translúcidas azul-petróleo. O frame inclui reflexos e assets vetoriais VALO: prisma cáustico interno (`12:10`, `12:11`), fita espectral (`24:2`, `24:4`), refração violeta/ciano (`24:3`, `24:5`), glints e folhas acrílicas para os arquivos, e ícones customizados para a dock. O fundo usa um grupo vetorial próprio (`2:3`), distinto da imagem Carina versionada no repositório.

### 5.2 Tokens visuais lidos dos estilos do frame

`get_variable_defs` não retornou variáveis para a composição. Os valores abaixo vêm dos estilos aplicados às camadas; ainda precisam ser consolidados num tema QML depois das decisões abertas.

| Token observado | Valor/uso no Figma |
|---|---|
| Gradiente base do desktop | `#0a2542` para `#061426`, com halos azuis/cianos e partículas. |
| Superfície da TopBar | `rgba(7,22,40,0.86)`; contorno `rgba(116,213,238,0.24)`; sombra `rgba(9,186,237,0.2)`. |
| Texto | Família Inter. Marca 19 px; telemetry 17 px; nomes de janela 20–21 px; navegação 16 px; labels dos sliders 16 px. |
| Cores de texto | `#c6e5f8`, `#9cbad3`, `#d5e2ee`, `#f4fcff`; seleção ciano `#42e9ee`. |
| Trilha dos sliders | Progresso em gradiente `#14c4e7` → `#17f5df`; trilha `rgba(115,186,218,0.26)`; labels de valor `#d9ecf7`. |
| Material das janelas | Blur de fundo de 18 px, gradientes ciano/azul translúcidos, gloss superior, borda clara e sombras internas/externas. |

O estado capturado mostra `Home` selecionado em Systalia Files e `Control Center` selecionado no painel de ajustes. A chave do material está ligada; os sliders mostram 65%, 100, 75%, `On`, 10 e 100%. Estes são valores da ilustração, não valores iniciais aprovados para o serviço.

### 5.3 Cobertura, estados e diferença para o branch

O frame é um único estado composto. Não há frames separados de launcher, notificações, alternador de tarefas, moldura SSD ou clientes Wayland externos. Também não aparecem estados hover/focus/pressed/disabled, menus abertos, animações, fluxo de teclado, comportamento responsivo ou acessibilidade. Esses elementos ficam sem especificação visual; não se deve inferir uma aparência a partir do código atual.

| Área | Referência Figma | Implementação observada no branch | Questão pendente |
|---|---|---|---|
| Wallpaper | Atmosfera vetorial luminosa, com raios e partículas. | Foto Carina 3840 × 2160 com `PreserveAspectCrop`. | Confirmar se o redesign substitui a foto ou se Carina continua como fonte do desktop. |
| TopBar | Cápsula 1646 × 45, margem 8/4 e raio 22; mostra `SDE-KWIN \| VALO` e seis glifos. | `SystaliaTopBar.qml`: barra full-width de 40 px, gradiente e linhas ciano; mostra `SDE \| VALO` e quatro botões. | Confirmar marca, semântica dos seis glifos, geometria e conteúdo final; preservar bindings reais da telemetria. |
| Janelas desktop | Systalia Files 701 × 534 e janela de ajustes 700 × 534, lado a lado. | `main.qml`: janela demo de vidro 920 × 600; Control Center QML é um painel superior de 358 × 508. | Definir layout/resolução-alvo e relação entre a janela de ajustes do Figma e o quick panel atual. |
| Control Center | Navegação General/Status/Control Center/Software/Drivers/About e ajustes do material VALO/SOAR. | `SystaliaControlCenter.qml`: Wi-Fi, Bluetooth, Não incomodar, volume e brilho; `main.qml` tem uma gaveta de depuração com quatro parâmetros ópticos. | Determinar se a janela do Figma substitui a gaveta, o quick panel, ambos ou é uma aplicação independente. |
| Dock | Cápsula 555 × 125 com slots 78 × 78 e ícones ilustrados. | `SystaliaDock.qml`: 64 px de altura, tiles 44 × 44 e cinco ações reais. | Confirmar escala, margens e correspondência funcional dos cinco ícones. |
| Launcher, notificações, task switcher e clientes | Não mostrados nesta página. | Componentes QML/serviços já existem. | Obter frames/estados de referência ou confirmar que ficam fora deste redesign. |

O frame Figma tem 1662 × 975 px e o shell atual inicia em 1280 × 800 px. Não há especificação de escala ou layout responsivo; não escalar essas medidas por suposição. Os nomes e estados observados são evidência visual, mas ainda não definem comportamento.

### 5.4 Decisões para fechar antes de substituir o visual

1. Escolher a direção do fundo: atmosfera vetorial do Figma ou wallpaper Carina atual.
2. Confirmar se a janela `VALO & SOAR Engine` é o novo painel de configurações da shell, uma janela independente ou outra superfície.
3. Confirmar resolução e estratégia responsiva, incluindo proporção e escala de referência.
4. Fornecer ou aprovar estados de launcher, notificações, task switcher, SSD e clientes externos que não aparecem no frame.
5. Confirmar a semântica dos glifos e dos cinco ícones da dock, além dos nomes `SDE-KWIN`, `ABYSSUS VES` e `SOAR Engine`.
6. Verificar a origem/licença dos assets vetoriais antes de os exportar para o produto.
7. Consolidar os estilos observados em tokens compartilhados somente após resolver as decisões acima.

## 6. Roadmap de engenharia

### 6.1 Próximo trabalho: redesign Systalia — VALO

1. Fechar as decisões listadas na seção 5.4 e obter referências para os estados/componentes ausentes.
2. Consolidar os estilos aprovados em tokens e definir o layout responsivo sem alterar contratos Wayland/GLSL.
3. Reestruturar o desktop e os componentes shell segundo a especificação visual aprovada.
4. Validar os estados das janelas externas e da aplicação demo.
5. Corrigir/documentar importação de buffers GPU no ambiente alvo.
6. Executar CTest, inspeção visual e profiling; atualizar o plano técnico e registar discrepâncias.

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
