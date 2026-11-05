# Revisão de C++, QML e interface em src/

Data: 04/11/2026. Base: commit `b2e541c`, sem alterações locais anteriores à revisão.

A revisão encontrou 22 problemas: 5 de prioridade P1 e 17 de prioridade P2. P1 indica risco de perda de acesso a janelas, instabilidade ou quebra de um fluxo principal. P2 indica comportamento incorreto em uma interação ou configuração específica. Os achados descrevem a base auditada; as correções posteriores estão registradas ao final deste documento.

## Validação e limites

- `cmake --build build -j2`: passou.
- `ctest --test-dir build --output-on-failure -j2`: **31/31 passaram**, executados fora do sandbox para permitir sockets Wayland e D-Bus locais. As falhas iniciais de permissão no sandbox não foram classificadas como bugs.
- `cmake --build build --target all_qmllint -j2`: terminou com sucesso, mas emitiu **180 avisos**. Parte decorre dos módulos registrados dinamicamente em `main.cpp`, que o analisador não resolve; a contagem não representa 180 defeitos.
- Um executável temporário, ligado à biblioteca existente, carregou o QML completo em modo `offscreen` com renderização por software, D-Bus isolado e um cliente Wayland real da suíte. Foram inspecionados estados, hierarquia e uma captura diagnóstica do layout. O verificador não foi incorporado ao projeto.
- As reproduções dinâmicas abaixo são explicitamente identificadas. Os demais achados vêm da análise estática de fluxos e contratos de API. Não foi validada a aparência final dos shaders Vulkan, nem o funcionamento em DRM/KMS, troca de VT ou múltiplos monitores físicos. O modo software não homologa o material óptico.

## Achados prioritários — P1

### 1. O botão fechar destrói a apresentação antes de o aplicativo aceitar o fechamento

Local: `src/qml/WaylandWindowContainer.qml:399` e `:550`.

Depois de enviar `closeToplevel()`, o código inicia um timer de 220 ms que chama `disposeContainer()` incondicionalmente. Um editor com confirmação de salvamento, um aplicativo que cancela o fechamento ou um cliente lento continua vivo, mas perde seu container e a entrada correspondente em `trackedClientContainers`. A superfície e a entrada C++ podem continuar existindo. Selecioná-la no alternador não recria a apresentação.

**Correção:** encerrar o container quando a superfície/toplevel realmente terminar. Um timeout pode oferecer uma ação explícita para aplicativo sem resposta, mas não deve remover silenciosamente a interface. Validar com um cliente que rejeita o evento de fechamento e outro que mostra confirmação.

Evidência: análise estática; a suíte atual usa um cliente que aceita o fechamento.

### 2. Suspender, reiniciar e desligar escolhem uma ferramenta sem esses subcomandos

Local: `src/system/SystemControlService.cpp:112`, `:122` e `:132`.

Quando `loginctl` existe, os três métodos executam `loginctl poweroff`, `loginctl reboot` e `loginctl suspend`. Esses comandos não constam da interface da ferramenta instalada, verificada por `loginctl --help`. O fallback para `systemctl` depende da ausência do executável, não do resultado. Os callbacks descartam o erro, enquanto o painel fecha como se a ação tivesse sido executada.

**Correção:** usar a API adequada de `org.freedesktop.login1.Manager` ou os comandos correspondentes do `systemctl`, propagando falha à interface. Testar com ferramentas simuladas; não executar ações reais de energia na suíte.

Evidência: código e ajuda da ferramenta local; nenhuma ação de energia foi executada.

### 3. Texturas do scene graph são destruídas diretamente pela thread da interface

Local: `src/gfx/SystaliaGlassItem.cpp:15` e `:223`.

O destrutor chama `releaseResources()`, que executa `delete` nas texturas de fallback criadas durante a renderização. A mesma implementação é usada quando o Qt solicita liberação de recursos ao retirar um item da cena. Em renderização com thread separada, isso permite destruir recursos gráficos na thread errada e sem coordenar sua utilização pelos materiais.

**Correção:** transferir a limpeza para um trabalho na thread de renderização e tratar a invalidação do scene graph. Cobrir destruição de janelas, remoção de itens e recriação dos recursos. A documentação do [QQuickItem](https://doc.qt.io/qt-6/qquickitem.html#releaseResources) exige agendar essa limpeza com `scheduleRenderJob()`.

Evidência: violação estática do contrato de ciclo de vida; não houve reprodução de crash Vulkan.

### 4. Uma superfície Wayland nativa pode ser associada a uma janela X11

Local: `src/compositor/XWaylandManager.cpp:1211`, especialmente o filtro em `:1217` e o fallback em `:1241`.

`notifySurfaceCreated()` só rejeita uma superfície de outro cliente quando o mapa de janelas X11 está vazio. Se existe uma janela X11 aguardando associação, a superfície de um cliente Wayland nativo pode ser escolhida por coincidência de ID ou pelo fallback que seleciona qualquer wrapper ainda sem superfície. IDs de recursos não são globalmente únicos entre clientes.

**Correção:** exigir que a superfície pertença ao cliente XWayland antes de qualquer busca e associar pelo identificador confirmado pelo protocolo. Evitar associação arbitrária por ordem de chegada. Testar abertura concorrente de clientes nativos e X11.

Evidência: análise estática do filtro de propriedade e dos fallbacks.

### 5. Retomar dispositivos pelo logind invalida descritores ainda usados pelos consumidores

Local: `src/system/LogindSessionService.cpp:502`, especialmente `:513`; integração em `src/system/InputDeviceManager.cpp:320`.

O callback de abertura entrega ao libinput o próprio descritor guardado em `m_devices`. No `ResumeDevice`, o serviço fecha esse descritor e guarda outro número, mas não há consumidor de `deviceResumed` no `src/` que atualize ou recrie o acesso do libinput. O estado do serviço volta a ativo sem restaurar o consumidor. Além disso, os sinais de suspensão de GPU e restauração de swapchain não estão ligados ao backend gráfico.

**Correção:** integrar suspensão/retomada aos consumidores reais, definir a posse dos descritores e só confirmar a pausa após interromper o uso. Para entrada, usar um ciclo coordenado de suspensão/retomada do libinput. Não basta atualizar o mapa e emitir sinais.

Evidência: análise estática da integração; impacto condicionado à sessão nativa e aos eventos de pausa/retomada. Os testes atuais verificam o serviço simulado, não esse ciclo de hardware.

## Renderização, geometria e interação — P2

### 6. Movimento nativo envia coordenadas globais como coordenadas locais da superfície

Local: `src/system/InputDeviceManager.cpp:597`, especialmente `:609`.

`sendMouseMoveEvent(focusView, newPos, newPos)` recebe a posição do desktop nos dois argumentos de posição. Para uma janela deslocada, por exemplo em `(300, 100)`, o ponto local do cliente deve descontar a posição da janela, suas decorações e eventuais transformações. O caminho também reutiliza `mouseFocus()` em vez de determinar qual superfície está sob o cursor.

**Correção:** fazer hit testing na cena e mapear a posição para o item/superfície escolhidos, incluindo mudanças de foco e coordenadas de saída. Testar ponteiro em janelas deslocadas e sobrepostas. Depende do caminho libinput direto; não foi validado em hardware.

### 7. A configuração de blending implementada nunca é habilitada

Local: `src/gfx/SystaliaGlassMaterial.cpp:6` e `:79`.

O shader implementa `updateGraphicsPipelineState()` para selecionar `SrcAlpha`, mas não ativa `UpdatesGraphicsPipelineState`. Portanto o Qt não chama o método. Isso é relevante porque o fragment shader entrega RGB sem multiplicação pelo alfa, enquanto o blending padrão do scene graph pressupõe cores premultiplicadas.

**Correção:** alinhar a saída do shader e o blending, preferencialmente preservando a convenção premultiplicada do Qt. Caso seja necessário estado customizado, habilitar explicitamente a flag. O requisito da flag está documentado em [QSGMaterialShader](https://doc.qt.io/qt-6/qsgmaterialshader.html#updateGraphicsPipelineState).

Evidência: análise estática; os artefatos visuais resultantes precisam de confirmação no backend Vulkan.

### 8. O material ignora a opacidade herdada do QML

Local: `src/gfx/SystaliaGlassMaterial.cpp:25`.

`updateUniformData()` incorpora `state.combinedMatrix()`, mas nunca `state.opacity()`. Assim, opacidade de ancestrais e animações de fade não entram no material. Isso afeta, por exemplo, a abertura/fechamento do launcher, alternador e preview, que animam `Item.opacity`: o vidro pode persistir com intensidade incompatível com seus elementos filhos.

**Correção:** transportar a opacidade efetiva ao shader e aplicá-la corretamente à composição inteira, incluindo a textura do cliente. Apenas multiplicar `surfaceOpacity` não basta se `clientCoverage` continuar forçando alfa elevado. O [contrato de atualização do material](https://doc.qt.io/qt-6/qsgmaterialshader.html#updateUniformData) inclui o estado de opacidade.

Evidência: análise estática; não houve homologação visual Vulkan dos fades.

### 9. A superfície original é desenhada sobre a versão recortada pelo shader

Local: `src/qml/WaylandWindowContainer.qml:470`, `:493` e `:515`.

O `clientViewport` é filho do item de vidro, tem `z: 1` e mantém o `ShellSurfaceItem` pintável. A captura usa `hideSource: false`. O cliente é composto no shader e novamente por cima dele, com recorte apenas retangular. Isso anula o recorte dos cantos internos e o tratamento óptico da primeira passagem, além de duplicar trabalho.

**Correção:** manter a superfície como fonte de textura e alvo de input, ocultando sua pintura direta, ou implementar uma composição única com máscara equivalente. Validar com conteúdo claro e opaco nos cantos inferiores.

Evidência: ordem de desenho e configuração estática da cena.

### 10. Popups do cliente ficam sujeitos ao recorte retangular da janela

Local: `src/qml/WaylandWindowContainer.qml:479` e `:481`.

O `ShellSurfaceItem` está dentro de um viewport com `clip: true`. O tipo cria automaticamente itens para popups, e não existe uma camada alternativa no projeto para apresentá-los fora desse recorte. Menus contextuais e listas que ultrapassam a área do cliente ficam sujeitos a corte e perda de área interativa.

**Correção:** separar a camada de popups da região que recorta o conteúdo principal, preservando coordenadas e grabs. Confirmar com um menu aberto junto à borda inferior/direita. A criação automática é o padrão documentado de [ShellSurfaceItem](https://doc.qt.io/qt-6/qml-qtwayland-compositor-shellsurfaceitem.html#autoCreatePopupItems-prop).

Evidência: análise estática; o cenário de popup externo não foi reproduzido nesta revisão.

### 11. O shader do cliente recebe a resolução do monitor para uma textura do canvas

Local: `src/qml/WaylandWindowContainer.qml:195`, especialmente `:204`; fonte em `src/qml/main.qml:315`.

Ao mapear ou terminar um arraste, `syncOutputRouting()` troca `screenResolution` pela resolução física do monitor. Entretanto, `backdropGrab` captura a janela Qt inteira. O shader divide `gl_FragCoord` por essa resolução, passando a amostrar outro ponto do wallpaper quando o canvas e o monitor têm tamanhos diferentes.

**Correção:** usar as dimensões e o espaço de coordenadas da textura efetivamente amostrada, com transformação explícita quando houver captura por saída. Atualizar ao redimensionar o canvas.

Evidência dinâmica: em um canvas `1662 × 975`, o cliente recebeu `screenResolution = 800 × 800` do monitor offscreen. A divergência de dimensões foi reproduzida; o efeito nos pixels Vulkan foi inferido do shader.

### 12. O registro C++ de workspace diverge da página QML do cliente

Local: `src/qml/main.qml:587` e `src/system/WorkspaceManagerService.cpp:110`.

`onToplevelWorkspaceChanged()` altera `workspaceIndex` e o pai visual, mas não atualiza `m_windowWorkspaces`. A contagem de janelas e o culling usam esse mapa antigo. Mover pelo objeto toplevel pode deixar a janela na página nova e continuar classificando-a no workspace anterior.

**Correção:** centralizar a alteração de workspace para sincronizar objeto, container e pai visual de forma idempotente, sem recursão de sinais.

Evidência dinâmica: depois de mover o toplevel para o workspace 2, o container informou `workspaceIndex = 2` e pai `workspacePage_2`, enquanto `WorkspaceManager.windowWorkspace(container)` continuou em `1`.

### 13. Files e painel de materiais continuam presumindo que pertencem ao workspace zero

Local: `src/qml/main.qml:32`, `:71` e `:172`.

As funções de recuperação sempre mudam para o workspace 0, apesar de `moveActiveWindowToWorkspace()` permitir mover essas janelas. O layout de snap também só as atualiza quando o workspace ativo é 0, e registra seus snaps nesse índice fixo.

**Correção:** consultar o workspace registrado de cada janela e utilizá-lo nas operações de recuperação, snap e redimensionamento. Alternativamente, restringir explicitamente a movimentação se essas janelas forem fixas por decisão de produto.

Evidência dinâmica: após mover Files para o workspace 1 e chamar `raiseFileWindow()`, o workspace ativo voltou a `0`, a janela permaneceu registrada em `1` e sua visibilidade efetiva ficou falsa.

### 14. Minimizar o painel de materiais não preserva o estado de minimização

Local: `src/qml/shell/SystaliaMaterialWindow.qml:175`.

O botão só atribui `visible = false`; `windowOpen` continua `true`. O gerenciador de workspaces recalcula a visibilidade com base em `windowOpen` e reapresenta o painel após uma troca de workspace. Files já usa os dois estados em conjunto.

**Correção:** usar uma operação central de minimização que atualize o estado persistente e deixe a visibilidade ser derivada dele.

Evidência dinâmica: ocultar o painel como faz o botão, trocar de 0 para 1 e retornar a 0 resulta em `visible = true` e `windowOpen = true`.

### 15. O registro de tiling conserva janelas ocultas e o workspace antigo

Local: `src/system/LiquidTilingService.cpp:299` e `:360`; chamadas em `src/qml/WaylandWindowContainer.qml:266`.

O serviço guarda o workspace no momento do snap. Mudanças posteriores de workspace não atualizam a entrada. `hasActiveSplitInWorkspace()` também considera janelas minimizadas/fechadas, sem verificar sua apresentação. Fechar uma janela interna apenas a oculta, sem removê-la do registro de snap.

**Correção:** sincronizar o registro com movimentação, minimização e fechamento, e determinar o divisor a partir das janelas efetivamente apresentadas. Validar minimizar um dos lados e mover um par encaixado entre workspaces.

Evidência: análise estática. Pode deixar o divisor visível e interceptando input sem um par de janelas correspondente.

### 16. Títulos longos não são limitados e o cabeçalho de clientes é baixo demais

Local: `src/qml/shell/SystaliaWindowHeader.qml:91`, `:107` e `:117`; uso em `src/qml/WaylandWindowContainer.qml:525`.

`titleLabel` tem `elide`, mas não tem largura limitada: o título usa a largura implícita e empurra o badge para cima das ações ou para fora da janela. `badgePreferredWidth` é calculado, mas não utilizado. Além disso, o cabeçalho de clientes mede 36 px, enquanto o bloco do título começa em 13 px e mede 34 px, chegando a 47 px; o ícone chega a 51 px. Há invasão vertical da região do cliente.

**Correção:** dimensionar título e badge com limites reais, reservar a área das ações e alinhar o conteúdo à altura efetiva do cabeçalho. Validar títulos grandes e janelas de largura mínima.

Evidência: geometria estática; captura por software foi usada apenas para inspeção geral do layout, sem homologar fontes e material finais.

### 17. O escurecimento por software é aplicado mesmo quando o brilho físico funciona

Local: `src/qml/main.qml:565` e `src/system/SystemControlService.cpp:243`.

O comentário descreve um fallback para monitores sem controle de hardware, mas a camada sempre calcula sua opacidade a partir de `SystemControl.brightness`. Com brilho físico em 50%, a shell ainda recebe uma camada preta de 42,5%. Antes da primeira leitura, o padrão 70 já produz escurecimento de 25,5%. A camada tem `z: 95`, portanto também escurece regiões da cena de modo diferente das barras e painéis acima dela.

**Correção:** expor a capacidade/backend de brilho e ativar o dimmer somente no modo de software, com composição consistente. Evitar escurecimento inicial baseado em um valor ainda não medido.

Evidência: análise estática da fórmula e da integração.

### 18. “Silencioso” muda o botão, mas não suprime notificações

Local: `src/system/SystemControlService.cpp:104`, `src/qml/shell/SystaliaControlCenter.qml:235` e `src/qml/shell/SystaliaNotificationHost.qml:21`.

`setDoNotDisturb()` só altera um booleano. O servidor de notificações e o host visual não consultam esse estado; o host continua apresentando todos os registros. A interface confirma uma preferência sem aplicar seu efeito.

**Correção:** impedir a apresentação de novos popups enquanto o modo estiver ativo, definindo separadamente se devem permanecer em um histórico. Validar o fluxo D-Bus com o modo ligado e desligado.

Evidência: análise estática e busca de todos os consumidores de `doNotDisturb` no `src/`.

### 19. O fallback PulseAudio interpreta a resposta de mute como sempre falsa

Local: `src/system/SystemControlService.cpp:233`.

O resultado completo de `pactl get-sink-mute` é comparado com `yes`. A resposta usual inclui o rótulo, como `Mute: yes`, de modo que a comparação falha. No backend `pactl`, um refresh pode desfazer o indicador de mute, e o próximo toggle volta a solicitar mute em vez de desativá-lo.

**Correção:** analisar o campo correto e fixar o locale do subprocesso, ou usar uma API estruturada. Cobrir respostas com rótulo, mute ligado/desligado e falha de comando.

Evidência: análise estática do parser; depende do uso do fallback `pactl`.

### 20. Hotplug real não usa a migração implementada em removeScreen()

Local: `src/compositor/OutputManagerService.cpp:25`, `:205` e `:325`.

Os sinais de adição/remoção de `QScreen` chamam `syncWithHostScreens()`. Essa função destrói e recria todas as saídas, mas não migra as janelas nem reconcilia `m_windowScreenMap`. A migração existe em `removeScreen()`, que esse fluxo não chama. Os índices antigos podem identificar outra saída ou ultrapassar a nova lista. Além disso, o roteamento só envia `surfaceEnter` se o índice mudar; saídas recriadas com o mesmo índice não recebem essa atualização pelo caminho manual.

**Correção:** reconciliar por identidade da saída, reaproveitando as existentes, migrando janelas das removidas e atualizando os registros/notificações. Testar o caminho dos sinais reais de `QGuiApplication`, além da API de remoção virtual.

Evidência: análise estática; não houve desconexão de monitor físico.

### 21. Aplicativos com Terminal=true são iniciados sem terminal

Local: `src/system/AppRegistryService.cpp:245` e `src/qml/shell/SystaliaFlowMenu.qml:375`.

O parser lê `Exec`, mas não transporta `Terminal=true`. O launcher executa diretamente o comando. Entradas como monitores de processo ou editores de terminal podem iniciar sem uma janela utilizável ou terminar imediatamente, embora apareçam como aplicações disponíveis.

**Correção:** transportar esse requisito no modelo e resolver um emulador de terminal com passagem adequada de argumentos, ou desabilitar essas entradas com indicação clara enquanto o suporte estiver pendente. Cobrir uma entrada `.desktop` com `Terminal=true`.

Evidência: análise estática do parser, modelo e chamada do launcher.

### 22. A animação óptica de clientes recebe apenas uma fotografia do relógio

Local: `src/qml/main.qml:236` e `src/qml/WaylandWindowContainer.qml:467`.

O mapa passado a `createObject()` contém `animTime: animTimer.tick`, que avalia o valor naquele instante. Nenhum binding posterior atualiza essa propriedade, ao contrário das janelas internas declaradas em QML. O shader recebe um tempo constante e suas cáusticas ficam congeladas durante a vida do cliente.

**Correção:** fornecer um `Qt.binding()` para o relógio ou compartilhá-lo por uma propriedade vinculada. Validar que o tempo do cliente acompanha o timer depois de sua criação e ao retornar de um workspace oculto.

Evidência: análise estática da criação dinâmica e dos bindings subsequentes.

## Lacunas funcionais e cobertura recomendada


Files usa uma lista fixa de seis itens. A busca não filtra o modelo; voltar/avançar e alternar grade/lista não têm ação; selecionar uma localização só troca seu nome. No painel de materiais, algumas seções só alteram a seleção e os sliders “Secondary” e “Strength” não atualizam uma propriedade externa. Esses elementos foram tratados como funcionalidades incompletas do protótipo, separadamente dos 22 defeitos acima.

O launcher é predominantemente operado por ponteiro: faltam ações explícitas de ativação do resultado por Enter e uma sequência completa de navegação de teclado. Os controles personalizados de navegação e vários alvos por `TapHandler` também precisam de foco, ações acessíveis e nomes. O filtro global de teclado do compositor merece teste integrado com botões, sliders e painéis focados, pois sua exceção para a shell reconhece apenas entradas de texto.

Priorizar testes integrados de: fechamento cancelado; minimização seguida de troca de workspace; movimentação via toplevel e via container; snap seguido de minimização/movimentação; títulos longos; menu de cliente fora da borda; resolução do backdrop após resize; notificações em modo silencioso; comandos externos simulados; e renderização com opacidade parcial. Para os achados de GPU e logind, os testes precisam incluir o backend e o ciclo de vida reais, além das verificações de estado já existentes.

Uma captura por software e 31 testes aprovados não permitem afirmar que o desktop está visualmente correto em Vulkan ou que a sessão nativa está validada.


## Correções implementadas após a revisão

As alterações abaixo correspondem aos 22 achados. A seção original foi mantida como registro da revisão do commit base; seus números de linha se referem àquele estado.

| Item | Mudança aplicada |
|---|---|
| 1 | Removido o timer que destruía o container antes de o cliente aceitar o fechamento. A remoção acompanha o ciclo de vida real da superfície. |
| 2 | Ações de energia usam `systemctl`; o serviço publica sucesso/falha e o painel apresenta os erros. |
| 3 | A limpeza das texturas é agendada na thread de renderização; a invalidação do scene graph também libera os recursos. |
| 4 | Superfícies de outros clientes são rejeitadas e foram removidas as associações por ordem de chegada. |
| 5 | Consumidores libinput recebem descritores duplicados, mantêm a concessão durante a pausa e reabrem a concessão retomada. A entrada é suspensa antes da confirmação; a janela Qt Quick pausa a apresentação durante a suspensão da sessão. |
| 6 | Eventos do ponteiro passam pelo hit testing do Qt Quick, que aplica empilhamento e transformação para coordenadas locais. |
| 7 | Material e fragment shader adotam blending premultiplicado compatível com o Qt; foi removido o override não habilitado. |
| 8 | A opacidade herdada entra no bloco de estado do renderer e afeta a composição inteira. |
| 9 | A captura oculta a pintura direta do cliente e o material faz a composição única. |
| 10 | Popups de primeiro nível são apresentados como irmãos da captura do cliente; os popups aninhados permanecem fora do recorte principal. |
| 11 | A resolução do backdrop permanece vinculada ao canvas, sem ser substituída pela resolução do monitor. |
| 12 | Alterações do workspace do toplevel sincronizam o registro do container no serviço C++. |
| 13 | Recuperação, snap e redimensionamento das janelas internas consultam seu workspace atual. |
| 14 | Minimizar o painel de materiais atualiza `windowOpen` e a visibilidade. |
| 15 | O registro de tiling acompanha mudanças de workspace; janelas ocultas/minimizadas não formam um divisor ativo. Fechar janelas internas remove seu snap. |
| 16 | Títulos têm largura limitada, badges reservam espaço para as ações e o conteúdo é centralizado na altura efetiva do cabeçalho. |
| 17 | O serviço distingue brilho físico de brilho por software; o dimmer só funciona no segundo modo e afeta a shell inteira. |
| 18 | O modo silencioso impede a apresentação das notificações enquanto estiver ativo. O modelo é preservado, incluindo suas expirações. |
| 19 | O parser aceita respostas `Mute: yes/no`; os comandos de sondagem usam locale C. |
| 20 | A sincronização conserva saídas existentes e remove as ausentes pelo caminho de migração, atualizando índices e consumidores. |
| 21 | O modelo transporta `Terminal=true` e o launcher escolhe um terminal disponível, passando argumentos separadamente, sem interpolação de shell. |
| 22 | O tempo de animação de clientes é um binding para o relógio da shell. |

O contrato óptico de `SystaliaSurfaceProps` continua com 128 bytes. O renderer acrescenta um slot de 16 bytes para opacidade herdada: C++ e a reflexão dos dois shaders usam um bloco de 144 bytes, com o estado da cena no offset 128.

Foram acrescentados testes de regressão para fechamento cancelado, ponteiro em janela deslocada, popup Wayland, recuperação de janelas movidas/minimizadas, registros de workspace, divisor de tiling, modo silencioso, brilho físico simulado, erros de energia simulados, requisitos de terminal, resolução/animação do cliente e conservação das saídas. O teste de logind agora percorre abertura, pausa, fechamento e reabertura usando os callbacks reais de descritores e pipes do kernel.

A validação de comandos de energia usa executáveis simulados; nenhuma operação real de suspender, reiniciar ou desligar foi executada. Os fluxos de DRM/KMS, libinput físico, troca de VT e hotplug de monitores reais ainda precisam de homologação no hardware. A renderização final Vulkan também precisa de inspeção visual no ambiente gráfico de destino; as verificações headless não substituem essa etapa.

Validação final das correções: compilação aprovada, **32/32 executáveis de teste passaram** pelo CTest, sem falhas. `git diff --check` passou. A reflexão dos shaders compilados confirmou o bloco de 144 bytes e o slot da cena no offset 128 em ambos os estágios. O alvo `all_qmllint` terminou com sucesso, mantendo 192 avisos, principalmente de imports/tipos registrados em runtime e acessos dinâmicos; isso não foi tratado como ausência de avisos ou como homologação visual.
