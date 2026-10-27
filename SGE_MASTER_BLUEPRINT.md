# Systalia Glass Engine — Blueprint Mestre de Engenharia v3.0

**Data de revisão:** 27 de outubro de 2026
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

`src/qml/main.qml` usa o wallpaper VALO empacotado em `src/qml/assets/figma/valo-ocean-atmosphere.png`, em `Image.PreserveAspectFit`, alinhado ao canvas de referência de 1662 × 975 px. O PNG de 3324 × 1950 é renderizado em alta resolução a partir da reconstituição SVG local para que funcione em runtimes Qt sem o plugin SVG. Os demais ícones também são PNGs 4× gerados das reconstituições locais. Os URLs temporários dos assets originais do Figma responderam HTTP 403; a imagem Carina continua versionada na raiz, mas deixou de ser a fonte do desktop.

`backdropGrab` continua a capturar o wallpaper para que as superfícies de vidro possam refratá-lo. O custo de captura em resoluções altas deve ser medido durante a reformulação.

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

O frame principal do redesign está em `src/qml/`: duas janelas de vidro, TopBar e dock usam um canvas baseado em 1662 × 975 px, que também é o tamanho inicial da janela. Build e captura de runtime foram feitos nesse tamanho via Xvfb/Vulkan. A captura confirmou a composição e a geometria do frame; a exportação dos vetores originais segue bloqueada pelo HTTP 403. O quick panel de sistema, launcher, notificações, task switcher e compositor mantêm seus serviços e interações existentes.

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
| 3.1 | TopBar e telemetria | Componentes e serviço presentes; geometria e tipografia-base conferidas em captura 1662 × 975. |
| 3.2 | Dock e lançamento de processos | Componente e serviço presentes. |
| 3.3/3.4 | Integração de clientes, foco e lançador de aplicações | Handshake, catálogo, filtro, foco e testes presentes; a apresentação de buffers GPU segue como risco aberto. |
| 3.5 | Notificações D-Bus e controles rápidos | Serviço, modelo e componentes presentes; conflito com outro dono do serviço D-Bus é uma condição de execução. |
| 3.6 | Alternador de tarefas | Modelo MRU, atalhos e QML presentes; correções recentes de foco e dimensão do frame aguardam validação no ambiente alvo. |
| 3.7 | Revisão, ASan/UBSan e benchmark | Pendente como auditoria completa; a expansão de testes não substitui profiling ou análise de leaks. |
| Redesign VALO/SDE | Nova linguagem visual a partir do Figma | Composição principal implementada e capturada em 1662 × 975. Wallpaper/ícones são reconstituições locais em PNG de alta resolução; comparação em outras escalas e validação no host alvo seguem pendentes. Control Center de material separado do quick panel. |
| Fases 4–6 | Workspaces, tiling, multi-monitor, sessão autônoma, áudio e terminal | Roadmap; não implementado integralmente. |

### Risco de execução conhecido

Em uma execução reportada pelo usuário, QtWaylandCompositor/Mesa registrou falta de `EGL_WL_bind_wayland_display` e falha ao obter informação do dispositivo. Isso é uma limitação da integração de buffers EGL/driver no ambiente de teste e pode impedir a exibição de clientes acelerados por GPU. Não se deve tratar como aviso cosmético nem afirmar que Alacritty está homologado até que o buffer apareça e aceite input em uma sessão representativa.

SVG não é decodificado pelo runtime Qt mínimo usado nesta captura; por isso os recursos visuais locais são empacotados como PNG de alta resolução. A captura também reportou sessão D-Bus indisponível e falha ao inicializar o display EGL do compositor Wayland. Isso não bloqueou a shell principal, mas buffers Wayland acelerados continuam sem validação. O nome D-Bus já ocupado deve ser diagnosticado sem tomar silenciosamente o serviço do desktop hospedeiro.

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

`get_variable_defs` não retornou variáveis para a composição. Os valores abaixo vêm dos estilos aplicados às camadas. A paleta e a família tipográfica base estão em `src/qml/theme/SystaliaTheme.qml`; a captura em 1662 × 975 conferiu a composição, enquanto contraste em outros tamanhos e a família Inter instalada no host alvo seguem pendentes.

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
| Wallpaper | Atmosfera vetorial luminosa, com raios e partículas. | PNG 3324 × 1950 gerado da reconstituição local; proporção 1662 × 975. | Capturado no runtime. Os SVGs originais do Figma continuam inacessíveis via URL temporária (HTTP 403). |
| TopBar | Cápsula 1646 × 45, margem 8/4 e raio 22; mostra `SDE-KWIN \| VALO` e seis glifos. | `SystaliaTopBar.qml`: geometria, telemetria real, data e hora; seis glifos abrem o quick panel. | Captura confere a geometria. Os glifos têm semântica parcial no frame e mantêm a ação existente do painel. |
| Janelas desktop | Systalia Files 701 × 534 e janela de ajustes 700 × 534, lado a lado. | `SystaliaFilesWindow.qml` e `SystaliaMaterialWindow.qml` em (113,193) e (864,193), dentro do canvas `min(largura/1662, altura/975)`. | Posições e dimensões conferidas na captura 1662 × 975; outras escalas aguardam comparação. |
| Control Center | Navegação General/Status/Control Center/Software/Drivers/About e ajustes do material VALO/SOAR. | `SystaliaMaterialWindow.qml` apresenta seis sliders ligados a opacidade, IOR, dispersão, cáusticas, raio e slant. `SystaliaControlCenter.qml` continua responsável por Wi-Fi, Bluetooth, Não incomodar, volume e brilho. | Correspondência visual implementada; áreas sem equivalente no motor não foram expostas como sliders decorativos. |
| Dock | Cápsula 555 × 125 com slots 78 × 78 e ícones ilustrados. | `SystaliaDock.qml`: dimensões, raio 47 e cinco ações preservadas; ícones PNG 4× gerados de vetores locais. | Geometria conferida. Os ícones locais aproximam os vetores originais, inacessíveis pelo proxy. |
| Launcher, notificações, task switcher e clientes | Não mostrados nesta página. | Componentes QML/serviços já existem. | Obter frames/estados de referência ou confirmar que ficam fora deste redesign. |

O frame Figma tem 1662 × 975 px. O canvas mantém essa referência e usa escala uniforme para caber na janela do desktop; não há variantes responsivas no arquivo. Posições e proporções em resoluções diferentes ainda aguardam comparação visual.

### 5.4 Implementação visual

- `src/qml/main.qml` usa 1662 × 975 como dimensão inicial e canvas escalado para janelas, TopBar e dock; a camada Wayland permanece em coordenadas lógicas da janela raiz.
- `SystaliaFilesWindow.qml` segue as posições do frame com navegação Home, pesquisa, seis itens sem cartões adicionais e badge `ABYSSUS VES`.
- `SystaliaMaterialWindow.qml` apresenta o painel VALO/SOAR, seis controles que alteram propriedades existentes do `SystaliaGlassItem` e um toggle que ativa/desativa a opacidade do material.
- `SystaliaTheme.qml` guarda a paleta e a família Inter. SVGs locais são as fontes editáveis das reconstituições; PNGs 2×/4× são empacotados porque o runtime Qt usado não decodifica SVG. Os assets exportados do Figma não puderam ser recuperados do proxy (HTTP 403), portanto esses desenhos não são cópias dos vetores originais.
- O shader GLSL não foi alterado: `shaders/valo_optical.frag` mantém `fwidth`/`smoothstep` para o contorno G2 e o recorte do cliente. Retângulos QML arredondados declaram `antialiasing: true`; imagens usam `smooth` e `mipmap`.
- `cmake --build build --target systalia_glass_engine` concluiu. A captura via Xvfb/Vulkan em 1662 × 975 confirmou a renderização dos recursos e a geometria-base. A captura reportou falta de sessão D-Bus e falha de EGL para o compositor; não houve validação de cliente Wayland externo ou de GPU física. CTest não foi executado.

### 5.5 Decisões pendentes

1. Comparar a implementação em escalas menores; a dimensão de referência 1662 × 975 já foi capturada, mas o frame não fornece estados responsivos.
2. Fornecer referências para launcher, notificações, task switcher, SSD, clientes externos e estados de foco/teclado ausentes.
3. Definir o significado dos seis glifos da TopBar; a implementação preserva o quick panel existente como ação para todos eles.
4. Substituir as reconstituições locais pelos SVGs originais do Figma caso seja possível exportá-los e se isso for desejado.
5. Validar em sessão gráfica alvo e medir FPS/custo do `backdropGrab`; a captura atual usa Xvfb com Vulkan/lavapipe.

## 6. Roadmap de engenharia

### 6.1 Próximo trabalho: redesign Systalia — VALO

1. Capturar o desktop em resoluções menores; a captura em 1662 × 975 já confirmou a geometria do frame principal.
2. Fornecer referências para os estados/componentes que não aparecem no frame principal.
3. Validar em sessão Vulkan os controles de material, clientes externos e buffers GPU.
4. Medir custo de composição e `backdropGrab`; corrigir problemas de layout ou avisos QML.
5. Executar CTest e profiling durante a homologação; atualizar o plano com evidências.

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
