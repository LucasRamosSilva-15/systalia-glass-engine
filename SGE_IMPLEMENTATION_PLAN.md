# Plano Técnico de Implementação — Redesign Systalia — VALO

**Documento alinhado a:** `SGE_MASTER_BLUEPRINT.md` v3.0
**Projeto Figma:** `SYSTALIA — VALO | Desktop Concept`
**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`
**Estado:** Planejamento; a especificação visual dos frames e tokens ainda precisa ser extraída do Figma.

## 1. Objetivo

Traduzir a reformulação visual completa do Systalia Desktop Environment para a shell QtQuick e o compositor existentes. A implementação deverá seguir o Figma como fonte de verdade para aparência e interação, preservando os contratos de GPU, Wayland, input e serviços definidos no `SGE_MASTER_BLUEPRINT.md`.

O plano não fixa cores, tamanhos, espaçamentos ou novas telas sem evidência dos frames. Essa informação será preenchida na etapa de auditoria visual.

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

## 4. Mapeamento do design para o código

| Área do design | Implementação principal | Regras |
|---|---|---|
| Tokens compartilhados | Novo módulo QML de tema, por exemplo `src/qml/theme/SystaliaTheme.qml` | Valores transcritos do Figma uma vez; componentes não repetem literais. |
| Desktop e wallpaper | `src/qml/main.qml` | Usar a imagem Carina existente; manter recorte proporcional e captura necessária ao vidro. |
| Barra superior | `src/qml/shell/SystaliaTopBar.qml` | Preservar bindings de telemetria e abertura dos painéis. |
| Dock | `src/qml/shell/SystaliaDock.qml` | Preservar atalhos, estados de processos, hover e ações. |
| Flow Launcher | `src/qml/shell/SystaliaFlowMenu.qml` | Preservar busca, modelo de aplicações e execução. |
| Control center | `src/qml/shell/SystaliaControlCenter.qml` | Preservar volume, mute, brilho, rede, Bluetooth e DND. |
| Notificações | `src/qml/shell/SystaliaNotificationHost.qml` | Preservar o modelo D-Bus, temporizadores, ações de fechar e ordem visual. |
| Alternador | `src/qml/shell/SystaliaTaskSwitcher.qml` | Preservar seleção MRU, Alt+Tab, Alt+Shift+Tab e confirmação ao soltar Alt. |
| Janelas clientes | `src/qml/WaylandWindowContainer.qml` | Preservar buffer, SSD, foco, resize, minimizar, maximizar e fechar. |
| Motor óptico | `src/gfx/` e `shaders/` | Só alterar parâmetros visuais depois de aprovar o design; nunca quebrar o ABI `std140` nem a equação G2. |

O nome do módulo QML de tema é uma proposta de implementação. Se o inventário indicar que o tema deve ser produzido como singleton C++ ou outro recurso, documentar a decisão antes de espalhar tokens pelos componentes.

## 5. Sequência de implementação

### Etapa A — Auditoria do Figma e especificação

1. Acessar o arquivo e identificar páginas, frames e versões válidas.
2. Separar telas de referência de estudos antigos ou componentes rejeitados.
3. Produzir a tabela da seção 3, com medidas observadas e estados.
4. Marcar cada elemento como existente, substituído, novo ou removido.
5. Confirmar os pontos de interação que afetam serviços e Wayland.

**Saída:** especificação visual preenchida e lista de discrepâncias do código atual. Não iniciar a substituição visual antes dessa saída.

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

- **Design ainda não transcrito:** os links do Figma foram fornecidos, mas as medidas e os frames do redesign ainda não estão disponíveis neste plano. Essa é a dependência inicial.
- **Buffers GPU:** a falta da extensão EGL reportada pode tornar uma shell visualmente correta incapaz de mostrar determinados clientes Wayland.
- **D-Bus compartilhado:** um daemon de notificações hospedeiro pode ocupar o nome padrão.
- **Captura de wallpaper 4K:** a captura live do backdrop pode aumentar custo de GPU/memória; medir antes de alterar a arquitetura.
- **Shell demo vs. aplicações finais:** Systalia Files é parte da apresentação atual; a função e escopo dessa janela devem ser confirmados no redesign.

## 9. Atualização dos documentos

Quando a especificação visual for extraída:

1. Atualizar as seções 5 e 6 do `SGE_MASTER_BLUEPRINT.md` com decisões observadas.
2. Substituir as dependências abertas por frames, tokens e critérios verificáveis.
3. Atualizar este plano com o mapeamento final e a sequência aprovada.
4. Anotar cada mudança de escopo, decisão e resultado de validação nos dois documentos.
