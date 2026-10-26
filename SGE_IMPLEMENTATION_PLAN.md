# Plano Técnico de Implementação — Redesign Systalia — VALO

**Documento alinhado a:** `SGE_MASTER_BLUEPRINT.md` v3.0
**Projeto Figma:** `SYSTALIA — VALO | Desktop Concept`
**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
**Estado:** Primeira implementação visual do frame principal feita em QML em 26 de outubro de 2026; build e validação em runtime ainda pendentes.

## 1. Objetivo

Traduzir a reformulação visual completa do Systalia Desktop Environment para a shell QtQuick e o compositor existentes. A implementação deverá seguir o Figma como fonte de verdade para aparência e interação, preservando os contratos de GPU, Wayland, input e serviços definidos no `SGE_MASTER_BLUEPRINT.md`.

As medidas e estilos observados estão registados na seção 3 como evidência do frame estático; ainda não são tokens de produção aprovados. Onde o Figma não mostra estados, componentes ou comportamento, o plano marca a lacuna em vez de inferir uma solução.

## 2. Estado técnico de partida

O branch já contém:

- Item de vidro óptico, shader GLSL 450, recorte Squircle G2 e `SystaliaSurfaceProps` de 128 bytes.
- Compositor XDG, SSD, resize, protocolo `z_systalia_surface_v1`, foco MRU e Alt+Tab.
- `WaylandWindowContainer.qml` com apresentação do cliente e controlos da janela.
- TopBar, dock, Flow Launcher, control center, host D-Bus de notificações e task switcher.
- Telemetria, catálogo de aplicações, launcher de processos e controlos de sistema.
- Wallpaper Carina Nebula 3840×2160 na raiz, com `PreserveAspectCrop` e captura para refração.
- 23 entradas declaradas no CTest, entre unitárias, integração e E2E.

O estado de código não prova homologação gráfica. Logs de execução reportaram ausência de `EGL_WL_bind_wayland_display` e falha de identificação de dispositivo Mesa. A visibilidade de aplicações aceleradas precisa ser validada em uma sessão representativa. O nome D-Bus de notificações também pode pertencer a outro daemon no barramento do desktop hospedeiro.

Após a auditoria, a janela principal passou a usar o wallpaper VALO local descrito na seção 3.4; a imagem Carina permanece no repositório como recurso legado.

## 3. Inventário visual obrigatório antes de implementar

Para cada frame do Figma que faça parte do redesign, registrar:

| Item | O que levantar |
|---|---|
| Tela e contexto | Nome do frame, resolução, escala e estado da aplicação |
| Layout | Ancoragens, dimensões, alinhamento, margens e comportamento responsivo |
| Tokens | Cores, tipografia, pesos, espaçamentos, raios, bordas, opacidade, sombras e materiais |
| Componentes | Variações, estados normal/hover/focus/pressed/disabled e conteúdo |
| Interação | Atalhos, abertura/fechamento, navegação, foco, drag, resize e transições |
| Assets | Ícones, imagens, formatos, nomes e licenças/origem quando aplicável |
| Acessibilidade | Contraste, navegação por teclado, foco visível e escalonamento |

O inventário deve cobrir pelo menos desktop/wallpaper, TopBar, dock, launcher, painel de controlos, notificações, task switcher, moldura SSD, janela Systalia Files e estados com clientes externos. Se um componente não aparecer no Figma, marcar como decisão aberta em vez de lhe atribuir um visual presumido.

### 3.1 Auditoria realizada — `Page 1`

Fonte: [SYSTALIA — VALO | Desktop Concept](https://www.figma.com/design/kax6aiYKgM2Fk7dllyZRRF/SYSTALIA-%E2%80%94-VALO-%7C-Desktop-Concept), página `Page 1`, frame `SYSTALIA / VALO — Main Desktop` (`2:2`), **1662 × 975 px**. As posições são relativas ao frame. O arquivo expõe uma única composição desktop; não foram encontrados frames separados de estados ou protótipo de interação.

| Nó Figma | Posição/tamanho | Conteúdo e tratamento visível |
|---|---:|---|
| `2:3` Vibrant / Luminous ocean atmosphere | (0, 0), 1662 × 975 | Fundo vetorial azul profundo, halos cianos, raios volumétricos e partículas bioluminescentes; não usa a foto Carina. |
| `2:81` Top System Bar | (8, 4), 1646 × 45, raio 22 | Cápsula translúcida `rgba(7,22,40,0.86)`, borda `rgba(116,213,238,0.24)` e sombra ciano. Marca SYSTALIA, `SDE-KWIN | VALO`, CPU 15%, RAM 4.8GB, FPS 100, `MON 19 OCT > 15:30:22` e seis glifos. |
| `2:88` Systalia Files — editable window | (113, 193), 701 × 534, raio 20 | Vidro com blur de 18 px. Badge `ABYSSUS VES`; navegação lateral com Home selecionado; breadcrumb, pesquisa, alternador de vista e grade de seis itens. |
| `2:134` Systalia Control Center — editable window | (864, 193), 700 × 534, raio 20 | Badge `VALO & SOAR Engine`; navegação General/Status/Control Center/Software/Drivers/About; painel `Live Material Controls` com toggle ligado e seis sliders. |
| `2:194` Floating Dock / organic glass | (555, 817), 555 × 125, raio 47 | Cápsula com cinco slots 78 × 78: pasta, alto-falante, terminal, código e estrela magenta. |

**Estilos observados:** Inter; marca da TopBar 19 px, telemetry 17 px, títulos das janelas 20–21 px, navegação 16 px e labels dos sliders 16 px. Texto usa `#c6e5f8`, `#9cbad3`, `#d5e2ee` e `#f4fcff`; seleção ciano `#42e9ee`; progresso de slider `#14c4e7` → `#17f5df`. As janelas combinam contornos claros, sombras ciano/preta, gloss superior e gradientes azul-petróleo. `get_variable_defs` não retornou variáveis para o frame; os estilos estão aplicados diretamente nas camadas.

**Estado estático:** Home e Control Center estão selecionados; a chave do material aparece ligada. Os valores ilustrados são 65%, 100, 75%, On, 10 e 100%. Não os tratar como defaults de runtime. Os assets visíveis incluem grupo vetorial de fundo, prisma/fita/refração VALO, ícones de arquivo acrílicos, controles de janela e ícones customizados da dock. A origem/licença dos SVGs ainda não foi verificada.

### 3.2 Cobertura do frame e lacunas

| Componente/estado | Cobertura no Figma | Resultado da auditoria |
|---|---|---|
| Desktop, wallpaper, TopBar, Files, janela VALO/SOAR e dock | Visíveis em `Page 1` | Medidas e aparência estática registadas acima. |
| Launcher, notificações, task switcher, moldura SSD e clientes externos | Não aparecem | Pedir frames/estados ou confirmar que ficam fora do redesign. |
| Hover, focus, pressed, disabled, teclado, animações e responsividade | Não especificados | Não inferir aparência ou comportamento a partir do código atual. |
| Variáveis e estilos compartilhados | Nenhuma variável devolvida para `2:2` | Consolidar valores em tema QML depois de fechar decisões de arte e escala. |

### 3.3 Correspondência inicial com o branch

| Área | Figma observado | Código atual | Decisão/ação |
|---|---|---|---|
| Wallpaper | Atmosfera vetorial luminosa. | SVG local VALO em `src/qml/assets/figma/valo-ocean-atmosphere.svg`, alinhado ao canvas pelo `PreserveAspectFit`. | Direção vetorial aplicada; o SVG original do Figma não foi acessível pelo proxy (HTTP 403), então o recurso local reconstitui a referência. |
| TopBar | Cápsula 1646 × 45 em (8, 4), raio 22; seis glifos. | `SystaliaTopBar.qml`: medidas e marca `SDE-KWIN \| VALO` aplicadas, telemetria real e quatro ações existentes. | Os dois glifos adicionais não têm semântica identificada; mantida a funcionalidade conhecida. |
| Desktop e janelas | Duas janelas lado a lado, 701 × 534 e 700 × 534. | Canvases em `main.qml` usam escala uniforme baseada em 1662 × 975. | Layout implementado; dimensões/escala aguardam comparação visual em runtime. |
| Systalia Files | Navegação + grade de arquivos em janela editável. | `SystaliaFilesWindow.qml`: Home ativo, busca, seis itens, ações da janela e status Wayland. | Implementação estática da composição; navegação real de arquivos não fazia parte do componente anterior. |
| Control Center | Janela de ajustes ópticos VALO/SOAR com seis sliders. | `SystaliaMaterialWindow.qml` controla seis propriedades reais do `SystaliaGlassItem`; `SystaliaControlCenter.qml` preserva os ajustes de sistema. | Mapeamento separado do quick panel, sem sliders para parâmetros sem suporte. |
| Dock | 555 × 125; slots 78 × 78 e cinco ícones ilustrados. | `SystaliaDock.qml`: dimensões, ícones vetoriais locais e cinco ações reais. | Geometria aplicada; desenhos locais aproximam o conceito e não são os SVGs originais do Figma. |
| Launcher, notificações, task switcher e janelas externas | Sem referências no frame disponível. | Componentes e serviços já existem. | Não redesenhar sem referências ou decisão explícita de escopo. |
| Material VALO | Gloss, refrações e assets vetoriais visíveis. | Material/shader nativos com ABI `std140` de 128 bytes e geometria G2. | Manter contratos; mapear parâmetros visuais somente depois de validar o design e os valores do motor. |

O frame mede 1662 × 975 px e não mostra uma escala alternativa. A implementação usa essas dimensões como artboard e aplica escala uniforme calculada pelo menor fator entre largura e altura da janela. Esse comportamento é uma decisão de implementação, ainda sem evidência de captura em múltiplas resoluções.

## 4. Mapeamento do design para o código

| Área | Implementação principal | Regra e divergência observada |
|---|---|---|
| Tokens compartilhados | `src/qml/theme/SystaliaTheme.qml` | Singleton QML com paleta e tipografia base observadas; revisão de contraste em runtime pendente. |
| Desktop e wallpaper | `src/qml/main.qml` e `src/qml/assets/figma/valo-ocean-atmosphere.svg` | Arte vetorial local e `backdropGrab` para as superfícies de vidro. O SVG é uma reconstituição, pois o proxy bloqueou a exportação dos assets Figma. |
| Barra superior | `src/qml/shell/SystaliaTopBar.qml` | Geometria Figma, marca e bindings reais de telemetria; os quatro botões continuam abrindo o quick panel. |
| Dock | `src/qml/shell/SystaliaDock.qml` | Geometria e slots aproximados do frame; ações preservadas e ícones locais empacotados como recursos QML. |
| Systalia Files | `src/qml/shell/SystaliaFilesWindow.qml` | Janela 701 × 534 com navegação e seis cartões ilustrativos; ações de janela e status Wayland. |
| Control Center VALO/SOAR | `src/qml/shell/SystaliaMaterialWindow.qml` | Janela 700 × 534 ligada a seis propriedades do material. O quick panel de sistema permanece em `SystaliaControlCenter.qml`. |
| Flow Launcher | `src/qml/shell/SystaliaFlowMenu.qml` | Não aparece no frame. Preservar busca/modelo/execução; solicitar referência antes de mudar aparência. |
| Notificações | `src/qml/shell/SystaliaNotificationHost.qml` | Não aparece no frame. Preservar D-Bus, timers, fechar e ordem; solicitar referência antes de mudar aparência. |
| Alternador | `src/qml/shell/SystaliaTaskSwitcher.qml` | Não aparece no frame. Preservar seleção MRU e atalhos; solicitar referência antes de mudar aparência. |
| Janelas clientes | `src/qml/WaylandWindowContainer.qml` | Sem frame ou estado externo no Figma auditado. Preservar buffer, SSD, foco, resize e ações da janela. |
| Motor óptico | `src/gfx/` e `shaders/` | ABI `std140` de 128 bytes, geometria G2 e shader permanecem sem alterações. Contorno e recorte de cliente mantêm `fwidth`/`smoothstep`; QML altera propriedades existentes. |

`SystaliaTheme.qml` é singleton QML e reúne cores e tipografia base observadas. Build, execução e revisão do resultado ainda estão pendentes.

### 3.4 Implementação visual inicial

- `main.qml` organiza o frame em canvases escalados com referência 1662 × 975; clientes Wayland permanecem em coordenadas lógicas da janela raiz.
- `SystaliaFilesWindow.qml` e `SystaliaMaterialWindow.qml` formam as duas janelas lado a lado. O painel Material controla opacidade, IOR, dispersão cromática, cáusticas, raio e slant do glass item.
- `SystaliaTopBar.qml` usa as medidas do frame e mantém CPU/RAM/FPS/hora ligados a `SystemTelemetry`.
- `SystaliaDock.qml` usa as dimensões ilustradas e mantém os cinco launchers existentes.
- `main.cpp` não procura mais o wallpaper Carina; o SVG VALO é recurso do módulo QML. `backdropGrab` continua ativo.
- O shader não foi alterado. A borda G2 e o recorte de cliente mantêm suavização analítica via `fwidth`; retângulos arredondados QML declaram `antialiasing: true`, e imagens usam `smooth`/`mipmap`.
- Os endpoints temporários de asset Figma retornaram HTTP 403 pelo proxy. Os SVGs em `src/qml/assets/figma/` foram reconstituídos localmente e não correspondem aos vetores originais.
- Build, execução e captura visual não foram feitos; validação de runtime segue pendente.

## 5. Sequência de implementação

### Etapa A — Auditoria do Figma e especificação

1. Acessar o arquivo e identificar páginas e frames. **Concluído para a página `Page 1` e o frame `2:2`.**
2. Separar telas de referência de estudos antigos ou componentes rejeitados. **Pendente:** há apenas uma página e a validade das camadas como versão final não está identificada.
3. Produzir a tabela da seção 3, com medidas e estados observados. **Concluído para a composição estática visível.**
4. Marcar cada elemento como existente, substituído, novo ou removido. **Concluído inicialmente; correspondências abertas estão na seção 3.3.**
5. Confirmar os pontos de interação que afetam serviços e Wayland. **Pendente:** o frame não documenta interações nem estados alternativos.

**Saída atual:** frame principal inventariado e usado como referência para a primeira implementação. Estados de launcher, notificações, task switcher, SSD, clientes e acessibilidade seguem sem referência; isso não bloqueia o escopo visual do frame principal.

### Etapa B — Fundação visual

1. Criar tokens compartilhados de cor, tipografia, escala, raios, materiais e animação a partir do Figma.
2. Mapear tokens a propriedades já existentes do glass engine quando houver equivalência.
3. Preparar assets em formatos adequados e atualizar o pipeline de recursos se necessário.
4. Validar wallpaper, contraste e recorte nas resoluções e escalas escolhidas.

**Saída:** tema QML comum criado e usado nos componentes principais; revisão de contraste e escala pendente.

### Etapa C — Estrutura do desktop e janelas

1. Reorganizar `main.qml` segundo a hierarquia e zonas visuais do Figma.
2. Aplicar o wallpaper e manter `backdropGrab` apenas onde a refração o consome.
3. Atualizar moldura e janela demonstrativa Systalia Files.
4. Atualizar `WaylandWindowContainer.qml` sem alterar contratos de buffer, input, geometria, foco ou estados XDG.
5. Comparar clientes reais, incluindo terminal, com o conteúdo da janela visível.

**Saída:** desktop e duas janelas principais implementados em QML; comparação visual e validação Wayland pendentes.

### Etapa D — Shell e overlays

O frame principal está implementado nestes componentes:

1. TopBar e dock.
2. Janela VALO/SOAR e controles de material.
3. Janela Systalia Files.

Flow Launcher, notificações, task switcher e janelas externas continuam com a aparência anterior porque o frame não especifica esses componentes.

### Etapa E — Serviços e integração

1. Manter os serviços C++ desacoplados dos detalhes de apresentação.
2. Ligar os novos controles visuais às propriedades existentes do `SystaliaGlassItem`; ainda falta validar os valores em runtime.
3. Evitar comandos bloqueantes no render thread; manter a execução assíncrona.
4. Resolver a validação de buffers EGL no ambiente objetivo, distinguindo buffers SHM dos acelerados.
5. Testar notificações em barramento isolado ou com o daemon concorrente identificado; não substituir silenciosamente outro dono D-Bus.

**Saída:** controles de material ligados a propriedades reais; validação no ambiente Vulkan permanece pendente.

### Etapa F — Revisão visual, desempenho e acessibilidade

1. Capturar cada frame do SGE nas mesmas dimensões/escala da referência.
2. Comparar posição, tamanho, cores, tipografia, hierarquia, estados e animações com o Figma.
3. Corrigir divergências observadas e registrar diferenças intencionais.
4. Testar teclado, foco, navegação, contraste e escala.
5. Medir FPS, uso de CPU/GPU e custo da captura do wallpaper em idle e com clientes.
6. Eliminar warnings de QML e confirmar que não há loops de binding.

**Saída:** checklist visual com evidência e limitações registradas.

## 6. Critérios de aceitação

O redesign fica pronto quando:

- Cada frame prioritário do inventário tem correspondência visual verificável na aplicação.
- Tokens vêm do Figma observado; nenhum valor visual de produção foi inventado por conveniência.
- TopBar, dock, launcher, painel, toasts, task switcher e janelas externas mantêm as ações reais.
- Cliente externo envia buffer visível e recebe teclado/mouse na sessão alvo.
- `SystaliaSurfaceProps` continua com 128 bytes e todos os offsets definidos no blueprint.
- Build C++23 com Ninja termina sem warnings sob `-Werror`.
- CTest executa todos os alvos disponíveis com 100% de aprovação.
- Não há warnings de binding loop no startup e os serviços que não conseguem adquirir recursos reportam a causa.
- Desempenho e limitações do host gráfico foram medidos e descritos, não presumidos.

## 7. Plano de validação

| Camada | Evidência |
|---|---|
| Build | Configuração Ninja e build completos em ambiente com Qt 6.7+, Vulkan e dependências Wayland. |
| Unitária | Geometria SDF, alinhamento, parsers, modelos e estado do task switcher. |
| Integração | Protocolo de superfície, XDG/SSD/resize, filtro de aplicações, serviços de notificação e composição. |
| E2E | Lifecycle Wayland com conexão, buffer, registro, desconexão e limpeza. |
| Visual | Capturas por frame/estado comparadas com dimensões e escala do Figma. |
| Runtime | Alacritty ou outro cliente GPU acelerado, input, resize, controles e notificações reais. |
| Performance | FPS e custo de frame em desktop vazio e com múltiplas superfícies. |

O número de entradas CTest não é o número de casos QTest. Registrar ambos separadamente nos relatórios.

## 8. Riscos e decisões abertas

- **Cobertura do Figma:** o arquivo auditado expõe um frame desktop estático; launcher, notificações, task switcher, SSD, clientes, estados de input e comportamento responsivo não estão especificados.
- **Direção do wallpaper:** a atmosfera vetorial foi aplicada; a arte local reconstitui o frame e não é o SVG original do Figma, que o proxy bloqueou com HTTP 403.
- **Mapeamento de Control Center:** a janela VALO/SOAR liga seis controles a propriedades reais do material; o quick panel permanece separado.
- **Tokens e assets:** o frame não tem variáveis retornadas; `SystaliaTheme.qml` reúne os tokens base. Os desenhos locais não são os assets exportados do Figma.
- **Escala:** existe uma referência de 1662 × 975 px; o canvas usa escala uniforme, ainda sem comparação de screenshots em outros tamanhos.
- **Buffers GPU:** a falta da extensão EGL reportada pode tornar uma shell visualmente correta incapaz de mostrar determinados clientes Wayland.
- **D-Bus compartilhado:** um daemon de notificações hospedeiro pode ocupar o nome padrão.
- **Captura de wallpaper:** o fundo vetorial e a captura live do backdrop têm custo de GPU/memória ainda não medido.
- **Shell demo vs. aplicações finais:** Systalia Files continua como janela ilustrativa; os seis cartões não representam operações reais de navegação no sistema de arquivos.

## 9. Atualização dos documentos

Quando as decisões e referências pendentes forem resolvidas:

1. Atualizar as seções 5 e 6 do `SGE_MASTER_BLUEPRINT.md` com decisões observadas.
2. Substituir as dependências abertas por frames, tokens e critérios verificáveis.
3. Atualizar este plano com o mapeamento final e a sequência aprovada.
4. Anotar cada mudança de escopo, decisão e resultado de validação nos dois documentos.
