# Plano Técnico de Implementação — Redesign Systalia — VALO

**Documento alinhado a:** `SGE_MASTER_BLUEPRINT.md` v3.0
**Projeto Figma:** `SYSTALIA — VALO | Desktop Concept`
**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
**Estado:** Inventário inicial do frame principal extraído do Figma em 26 de outubro de 2026; implementação visual pendente de resolver as decisões abertas das seções 3 e 8.

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
| Wallpaper | Atmosfera vetorial luminosa. | Carina Nebula 3840 × 2160, `PreserveAspectCrop`. | Resolver qual arte deve ser aplicada; manter a divergência documentada até essa decisão. |
| TopBar | Cápsula 1646 × 45 em (8, 4), raio 22; seis glifos. | `SystaliaTopBar.qml`: full-width, 40 px, quatro botões; string `SDE | VALO`. | Confirmar dimensões, identidade `SDE-KWIN | VALO` e significado dos glifos; manter telemetria real. |
| Desktop e janelas | Duas janelas lado a lado, 701 × 534 e 700 × 534. | `main.qml`: chassis demo 920 × 600; shell inicia em 1280 × 800. | Definir resolução de referência, layout e comportamento responsivo antes de substituir a composição. |
| Systalia Files | Navegação + grade de arquivos em janela editável. | Janela demo de vidro e conteúdo em `main.qml`. | Mapear a janela e preservar ações/estado existentes; ajustar geometria após definir escala. |
| Control Center | Janela de ajustes ópticos VALO/SOAR com seis sliders. | `SystaliaControlCenter.qml` é quick panel de Wi-Fi/volume/brilho; `main.qml` tem gaveta de depuração óptica com quatro sliders. | Definir se Figma representa uma aplicação nova, a gaveta óptica ou uma substituição do quick panel. |
| Dock | 555 × 125; slots 78 × 78 e cinco ícones ilustrados. | `SystaliaDock.qml`: 64 px de altura e tiles 44 × 44, com cinco ações reais. | Preservar ações, confirmar correspondência dos glifos e adaptar escala. |
| Launcher, notificações, task switcher e janelas externas | Sem referências no frame disponível. | Componentes e serviços já existem. | Não redesenhar sem referências ou decisão explícita de escopo. |
| Material VALO | Gloss, refrações e assets vetoriais visíveis. | Material/shader nativos com ABI `std140` de 128 bytes e geometria G2. | Manter contratos; mapear parâmetros visuais somente depois de validar o design e os valores do motor. |

O frame mede 1662 × 975 px e não mostra uma escala alternativa. As dimensões são referência do artboard, não uma declaração de pixels físicos nem de layout responsivo.

## 4. Mapeamento do design para o código

| Área do design | Implementação principal | Regras |
|---|---|---|
| Área | Implementação principal | Regra e divergência observada |
|---|---|---|
| Tokens compartilhados | Novo módulo QML de tema, por exemplo `src/qml/theme/SystaliaTheme.qml` | A consulta ao node `2:2` não retornou variáveis vinculadas ao frame; consolidar os valores inline registados na seção 3 depois de fechar arte e escala. |
| Desktop e wallpaper | `src/qml/main.qml` | Figma mostra atmosfera vetorial; branch usa foto Carina. Manter `backdropGrab` onde há refração e aguardar decisão sobre a arte. |
| Barra superior | `src/qml/shell/SystaliaTopBar.qml` | Figma: cápsula 1646 × 45. Código: full-width 40 px. Preservar bindings de telemetria e abertura dos painéis; confirmar glifos e rótulos. |
| Dock | `src/qml/shell/SystaliaDock.qml` | Figma: 555 × 125 com cinco slots 78 × 78. Código: altura 64 px e tiles 44 × 44. Preservar ações e estados dos processos. |
| Systalia Files | `src/qml/main.qml` | Figma: janela 701 × 534 com navegação e grade. Código: chassis demo 920 × 600. Preservar operações e ajustar o layout depois de definir a escala-alvo. |
| Control Center VALO/SOAR | `src/qml/shell/SystaliaControlCenter.qml` e gaveta em `main.qml` | O Figma mostra ajustes de material; o quick panel atual mostra controles do sistema e a gaveta tem quatro parâmetros ópticos. A correspondência entre as três superfícies ainda está aberta. |
| Flow Launcher | `src/qml/shell/SystaliaFlowMenu.qml` | Não aparece no frame. Preservar busca/modelo/execução; solicitar referência antes de mudar aparência. |
| Notificações | `src/qml/shell/SystaliaNotificationHost.qml` | Não aparece no frame. Preservar D-Bus, timers, fechar e ordem; solicitar referência antes de mudar aparência. |
| Alternador | `src/qml/shell/SystaliaTaskSwitcher.qml` | Não aparece no frame. Preservar seleção MRU e atalhos; solicitar referência antes de mudar aparência. |
| Janelas clientes | `src/qml/WaylandWindowContainer.qml` | Sem frame ou estado externo no Figma auditado. Preservar buffer, SSD, foco, resize e ações da janela. |
| Motor óptico | `src/gfx/` e `shaders/` | Figma usa gloss, refração e assets vetoriais. Só alterar parâmetros após aprovação visual; manter ABI `std140` e geometria G2. |

O nome do módulo QML de tema é uma proposta de implementação. Se o inventário indicar que o tema deve ser produzido como singleton C++ ou outro recurso, documentar a decisão antes de espalhar tokens pelos componentes.

## 5. Sequência de implementação

### Etapa A — Auditoria do Figma e especificação

1. Acessar o arquivo e identificar páginas e frames. **Concluído para a página `Page 1` e o frame `2:2`.**
2. Separar telas de referência de estudos antigos ou componentes rejeitados. **Pendente:** há apenas uma página e a validade das camadas como versão final não está identificada.
3. Produzir a tabela da seção 3, com medidas e estados observados. **Concluído para a composição estática visível.**
4. Marcar cada elemento como existente, substituído, novo ou removido. **Concluído inicialmente; correspondências abertas estão na seção 3.3.**
5. Confirmar os pontos de interação que afetam serviços e Wayland. **Pendente:** o frame não documenta interações nem estados alternativos.

**Saída atual:** inventário estático e discrepâncias registados. A Etapa A ainda não está fechada: não substituir o visual antes de resolver as decisões da seção 3.3 e obter os estados/referências que farão parte do escopo.

### Etapa B — Fundação visual

1. Criar tokens compartilhados de cor, tipografia, escala, raios, materiais e animação a partir do Figma.
2. Mapear tokens a propriedades já existentes do glass engine quando houver equivalência.
3. Preparar assets em formatos adequados e atualizar o pipeline de recursos se necessário.
4. Validar wallpaper, contraste e recorte nas resoluções e escalas escolhidas.

**Saída:** tema comum usado por ao menos um componente representativo, sem duplicação de valores.

### Etapa C — Estrutura do desktop e janelas

1. Reorganizar `main.qml` segundo a hierarquia e zonas visuais do Figma.
2. Aplicar o wallpaper e manter `backdropGrab` apenas onde a refração o consome.
3. Atualizar moldura e janela demonstrativa Systalia Files.
4. Atualizar `WaylandWindowContainer.qml` sem alterar contratos de buffer, input, geometria, foco ou estados XDG.
5. Comparar clientes reais, incluindo terminal, com o conteúdo da janela visível.

**Saída:** desktop base e janelas com hierarquia visual consistente.

### Etapa D — Shell e overlays

Implementar nesta ordem, conforme dependências visuais observadas:

1. TopBar e dock.
2. Flow Launcher e estados de pesquisa/seleção.
3. Control center e estados de cada toggle/slider.
4. Host de notificações e empilhamento.
5. Task switcher e seleção MRU.
6. Estados vazios, indisponíveis, foco por teclado e painéis sobre janelas.

A ordem pode mudar se o Figma definir dependências diferentes; registrar a razão.

### Etapa E — Serviços e integração

1. Manter os serviços C++ desacoplados dos detalhes de apresentação.
2. Ligar os novos controles visuais às propriedades QML existentes e testar valores reais.
3. Evitar comandos bloqueantes no render thread; manter a execução assíncrona.
4. Resolver a validação de buffers EGL no ambiente objetivo, distinguindo buffers SHM dos acelerados.
5. Testar notificações em barramento isolado ou com o daemon concorrente identificado; não substituir silenciosamente outro dono D-Bus.

**Saída:** aparência nova com a mesma funcionalidade real, sem controles meramente decorativos.

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
- **Direção do wallpaper:** a composição vetorial do Figma diverge da imagem Carina usada pelo branch. É necessária uma decisão antes de atualizar `main.qml`.
- **Mapeamento de Control Center:** o painel VALO/SOAR do Figma não corresponde diretamente ao quick panel existente; a gaveta óptica de `main.qml` também só cobre parte dos controles mostrados.
- **Tokens e assets:** a consulta do frame não retornou variáveis vinculadas; os estilos estão inline e a origem/licença dos assets precisa ser confirmada antes da exportação.
- **Escala:** existe uma referência de 1662 × 975 px, sem variantes; comportamento responsivo não pode ser inferido.
- **Buffers GPU:** a falta da extensão EGL reportada pode tornar uma shell visualmente correta incapaz de mostrar determinados clientes Wayland.
- **D-Bus compartilhado:** um daemon de notificações hospedeiro pode ocupar o nome padrão.
- **Captura de wallpaper 4K:** a captura live do backdrop pode aumentar custo de GPU/memória; medir antes de alterar a arquitetura.
- **Shell demo vs. aplicações finais:** Systalia Files é parte da apresentação atual; a função e escopo dessa janela devem ser confirmados no redesign.

## 9. Atualização dos documentos

Quando as decisões e referências pendentes forem resolvidas:

1. Atualizar as seções 5 e 6 do `SGE_MASTER_BLUEPRINT.md` com decisões observadas.
2. Substituir as dependências abertas por frames, tokens e critérios verificáveis.
3. Atualizar este plano com o mapeamento final e a sequência aprovada.
4. Anotar cada mudança de escopo, decisão e resultado de validação nos dois documentos.
