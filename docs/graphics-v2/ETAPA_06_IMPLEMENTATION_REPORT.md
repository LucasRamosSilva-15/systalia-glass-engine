# ETAPA 06 — Decorações Wayland/XWayland e overlays

Branch: `valo-graphics-2.0`. Stack utilizada: Qt/QtTest 6.11.2, QML, Scene Graph/QRhi, Vulkan e OpenGL. Data: 2026-11-09.

**Classificação: PARTIAL.** A migração dos chassis e dos quatro overlays está implementada; falta completar a homologação manual. Por orientação do usuário, o problema de conteúdo preto do XWayland foi adiado para a Etapa 07, sem continuidade de investigação nesta etapa. Há execução instrumentada do SDE principal e testes reais de apresentação, mas não homologação manual completa de popups, subsurfaces, DPR múltiplo e monitores físicos. O conteúdo X11 real permanece preto; o bloqueio e as hipóteses refutadas estão documentados abaixo.

## Contrato e implementação

Foram consultados `ETAPA_00_1_HANDOVER_AI_PLANNER.md`, `ETAPA_05_5_BACKDROP_PARTICLES_REPORT.md`, a arquitetura e os contratos de apresentação existentes.

### Decoração P0

`src/qml/WaylandWindowContainer.qml` usa um `Loader` para escolher **um único chassi**, V2 por padrão, ou `ValoGlass` V1 com `useGraphicsV2: false`. A troca não recria o viewport nem o cliente.

O V2 utiliza `ValoMaterialSurfaceV2`, G2, sombra analítica, borda especular única, refração e o provider Dual-Kawase existente. Os metadados `ior`, `slant`, dispersão e quatro raios chegam diretamente ao material. Os raios fundidos de snap continuam no contrato original. A tonalização é 0.50 na janela ativa e 0.42 na inativa; não é alpha do cliente.

`ShellSurfaceItem` e `clientViewport` permanecem irmãos acima do chassi, com as mesmas coordenadas, clipping, geometria comprometida pelo cliente e encaminhamento de entrada. Foram retirados somente `clientTextureCapture` e sua limpeza: o buffer nativo não participa do shader óptico, não é capturado e não é ocultado por `hideSource`. Não há readback por frame em produção.

O caminho comum Wayland/XWayland, os handlers de popup, foco, resize, snap, workspace, roteamento de output, configuração assíncrona e desligamento do `ShellSurfaceItem` foram preservados. Não houve alteração do compositor ou de protocolos.

Uma falha descoberta na própria migração foi corrigida: `active` no item V2 controla a existência do nó gráfico, não a ativação de teclado. Ligá-lo a `isWindowActive` fazia o chassi desaparecer ao perder foco. Agora acompanha a visibilidade; o estado ativo/inativo continua modulando a tonalização. A regressão verifica isso com um cliente real.

### Overlays P1

| Arquivo | Mudança gráfica |
|---|---|
| `SystaliaFlowMenu.qml` | Vidro, frame e gloss externos substituídos por uma superfície G2 V2. Pesquisa e conteúdo mantidos. |
| `SystaliaNotificationHost.qml` | Uma superfície V2 por cartão; modelo, descarte, timeout e conteúdo mantidos. |
| `SystaliaTaskSwitcher.qml` | Um chassi V2 externo; thumbnails `WaylandQuickItem` e seleção preservados. |
| `SnapPreviewOverlay.qml` | Três camadas externas substituídas por um chassi V2 com borda única. Geometria e animações de snap mantidas; partículas desativadas nesse indicador. |

`main.qml` conecta perfis e preferências ópticas de contraste/Reduced Motion da Material Window às novas superfícies e aos containers criados dinamicamente. O default continua Balanced. As demais propriedades e conteúdos de Files, Material, Dock e TopBar não foram alterados. O clipping preexistente do painel Flow foi mantido, inclusive sua limitação para sombra fora do painel.

### Auditoria P2

- `SystaliaWindowHeader.qml`: instanciado pelos containers e janelas internas; estes usam `glassSurface: false`, evitando um segundo chassi. Preservado.
- `SystaliaWindowAction.qml`: controles vetoriais, acessibilidade, estados e sinais preservados; seus contornos são dos botões.
- `TileSplitter.qml`: instanciado em `main.qml`, sem chassi V1 a substituir. Preservado.
- `WaylandClientView.qml`: não foi encontrada instanciação no QML ativo; preservado como legado/fallback.

## Backdrop e integridade

Clientes externos recebem exclusivamente `backdropGrab`, cuja fonte continua `wallpaperBackdropRoot`. As amostras `*-compositor.json` registram `wallpaper-only`. O piloto seguro Files/Material não foi ampliado; seu fallback conservador quando há clientes externos permanece.

Não foram modificados renderer V1, UBOs de 128/160 bytes, os 124 tokens, kernels Dual-Kawase, agendamento QRhi, identidade do wrapper de textura, serviços, gerenciamento de buffers ou compositor Wayland/XWayland. A composição corrigida na Etapa 05.2 e as partículas da Etapa 05.5 continuam compartilhadas no V2.

## Testes executados

- Build completo: `cmake --build build -j 4`, aprovado. Não houve mudança de shader nem de seus contratos; os recursos QSB existentes foram vinculados pelo build.
- CTest completo: **47/47**, zero falhas, 23.38 s nesta execução. Log: [ctest.log](evidence/etapa-06/ctest.log).
- Cliente/Scene Graph sobre Wayland, Vulkan: **7 resultados aprovados**, incluindo init/cleanup, zero falhas/skip, 3.877 s. [Log](evidence/etapa-06/client-vulkan.log).
- Mesmo conjunto em OpenGL: **7 resultados aprovados**, zero falhas/skip, 4.022 s. [Log](evidence/etapa-06/client-opengl.log).

Novas regressões em testes existentes:

1. Container V2 com G2, ausência de captura e renderer V1 sobreposto, 30 mudanças de propriedade de perfil e retorno reversível ao V1.
2. Framebuffer nativo exatamente igual em 18 trocas de perfil, incluindo texto; perda de ativação sem remover o material; teclado encaminhado pelo compositor provoca novo buffer verde e mudança real no framebuffer.
3. Quatro overlays com um vidro e uma sombra V2, nenhum `SystaliaGlassItem` dentro deles, perfis e preferências compartilhados.
4. Fixtures de resize, SSD, apresentação e lifecycle agora registram os tipos V2. Continuam verificando clipping, dimensões comprometidas, resize contínuo, conexão, buffer real, desconexão e liberação do container.

Arquivos de teste alterados: `tests/unit/tst_ValoDecorations.cpp`, `tests/unit/tst_LiquidTiling.cpp`, `tests/tst_XdgResize.cpp`, `tests/integration/tst_XdgShellDecoration.cpp`, `tests/integration/tst_ClientSurfaceVisibility.cpp`, `tests/integration/tst_ShellStateRegression.cpp` e `tests/e2e/tst_WaylandClientLifecycleE2E.cpp`.

Durante o desenvolvimento, falhas das fixtures foram corrigidas: registro somente V1; destruição diferida do Loader; procura de delegates pela árvore QObject em vez da árvore visual; evento de teste sem scan code; repaint do cliente `QRasterWindow` sem região suja. Não são falhas pendentes do renderer.

O teste existente chamado `xwaylandWindowPresentsClientSurface` associa uma superfície Wayland real a um wrapper X11. Ele valida o caminho comum do container, **não substitui** um cliente conectado ao servidor XWayland real. Backdrops sintéticos usados por fixtures sem fonte não são prova de fidelidade visual.

## Execução do SDE principal e evidências

Foi executado `build/systalia_glass_engine` em Vulkan, aninhado na sessão Wayland real, com runtime privado e D-Bus isolado. Foram lançados Alacritty, cliente Wayland Qt de padrão conhecido, GTK/Zenity e `xmessage` X11. Nenhum gerador de desktop foi utilizado.

A sonda é opt-in em `src/diagnostics/ValoStabilityProbe.hpp`; o launcher reproduzível está em `tools/validate_valo_stage06.py`. Readbacks e alterações programáticas de cenário só acontecem com a variável de diagnóstico. Sem ela, a sonda permanece inerte.

A sequência registra: V1 de fallback e V2 na mesma sessão, maximizar/restaurar, snap/unsnap, minimizar/reativar, workspace 1/0, 30 trocas de perfil, Flow, Alt+Tab/Task Switcher, Snap Preview, notificação e fechamento dos clientes. São ações instrumentadas no aplicativo real, não equivalem a homologação manual de todos os controles por mouse.

| Evidência real | Conteúdo |
|---|---|
| [01-v1-fallback.png](evidence/etapa-06/01-v1-fallback.png) | Comparação com chassi V1 reversível; não é captura do commit anterior com a antiga captura de cliente. |
| [02-v2.png](evidence/etapa-06/02-v2.png) | Chassis V2 e conteúdo nativo dos clientes. |
| [04-restored.png](evidence/etapa-06/04-restored.png) | Restauração após maximização. |
| [11-flow.png](evidence/etapa-06/11-flow.png) | Flow Menu V2. |
| [12-switcher.png](evidence/etapa-06/12-switcher.png) | Task Switcher V2 com previews nativos. |
| [13-snap-preview.png](evidence/etapa-06/13-snap-preview.png) | Indicador de snap V2. |
| [14-notification.png](evidence/etapa-06/14-notification.png) | Cartão de notificação V2. |
| [16-closed-compositor.json](evidence/etapa-06/16-closed-compositor.json) | Contadores após fechamento. |

Na região exposta do cliente Wayland de padrão conhecido, a comparação entre as capturas V1 de fallback e V2 encontrou **81.600 pixels idênticos (100%)**, incluindo texto. É uma verificação de preservação do conteúdo, não uma prova de fidelidade do material ao Figma. [Métrica](evidence/etapa-06/native-client-v1-v2-pixels.json).

Os JSON adjacentes preservam perfis, visibilidade, geometria, DPR, estado ativo, número de clientes e fallback. Os `*-providers.json` registram passes/status efetivos do provider; os getters foram lidos em C++, sem ampliar API QML/UBO.

## Benchmarks e limitações

A execução instrumentada terminou normalmente, após 30 trocas de perfil e os testes de overlays. O último contador registrou `windows: 0` e `xManaged: 0` após o fechamento; isso comprova a retirada dos clientes do compositor, não a aprovação do conteúdo X11 preto. Os exits de Zenity/Xmessage ao fechar o diálogo foram 1; não foram classificados como crash do SDE.

As métricas da execução Vulkan estão em [frame-timings.json](evidence/etapa-06/frame-timings.json): tempo CPU de frame e timestamps GPU do swapchain, aquecimento de 2 s. Incluem a cena inteira, transições, capturas diagnósticas e clientes; não representam custo isolado da decoração nem comparação controlada por perfil. Não há benchmark GPU do SDE principal em OpenGL: o binário mantém sua seleção Vulkan; OpenGL foi verificado nos testes de cliente/Scene Graph reais.

| SDE Vulkan, cena inteira | Média | P95 | Amostras |
|---|---:|---:|---:|
| CPU de frame | 2.545 ms | 10.069 ms | 2007 |
| GPU, frame concluído | 2.460 ms | 3.438 ms | 2024 |

Os providers alcançados pela árvore QObject da sonda registraram `filtered` e 4 passes em Balanced, 8 em Quality. Esse inventário não enumera integralmente os containers dinâmicos de clientes; não constitui medição individual do blur de cada decoração. As trocas incluindo Performance foram exercitadas sem crash; a ausência de alteração dos pixels do cliente foi verificada separadamente. Não foi necessário readback em produção.

Uma primeira tentativa X11 usou `:1`, também utilizado pela sessão hospedeira. Não foi contabilizada como teste XWayland do SDE. A sonda passou a usar `:91`, sem alterar o compositor de produção. O contador `xManaged` e o root X11 foram registrados para confirmar o destino correto.

**Bloqueio visual XWayland não resolvido:** o `xmessage` conectado ao servidor real foi associado (`xManaged: 1`, quatro clientes simultâneos), mas seu conteúdo fica preto, mesmo com `-bg white -fg black`. A captura [anterior ao resize](evidence/etapa-06/xwayland-initial-check/00-native-default.png) e a [comparação V1](evidence/etapa-06/xwayland-initial-check/01-v1-fallback.png) também mostram o problema. Desativar Glamor apenas no diagnóstico não o removeu ([experimento](evidence/etapa-06/xwayland-no-glamor/02-v2.png)); portanto a hipótese inicial de importação GPU não foi confirmada. Também foi refutada a hipótese de origem global X11 aplicada ao buffer: as amostras registram `geometryX/Y = 0`, e o wrapper já normaliza a origem em `setSurface`, resize e configure. Uma alteração experimental de coordenadas foi retirada, sem mudar esse contrato. Não há causa completa comprovada para o conteúdo preto, e **a apresentação visual XWayland não está aprovada**. Nenhum código do compositor, wrapper C++ ou gerenciamento de buffers foi alterado para contornar a falha. A passagem do teste de wrapper não resolve esse bloqueio do servidor real.

Permanecem sem homologação manual completa: popups e subsurfaces reais, interação do cliente X11, drag pelo mouse, múltiplos monitores físicos e escalas DPR diferentes de 1. Os testes virtuais de output/escala continuam passando, mas não substituem essa validação. GTK emitiu avisos de escala e `VK_ERROR_SURFACE_LOST_KHR` no ambiente aninhado; sua apresentação final deve ser conferida nas capturas, sem classificar esses avisos como resolvidos.

## Reprodução e critérios para Etapa 07

Em uma sessão Wayland com Alacritty, Zenity e Xmessage instalados:

```sh
cmake --build build -j 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j 2
python3 tools/validate_valo_stage06.py /tmp/valo-stage06-review
```

Para repetir os testes essenciais OpenGL:

```sh
QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=opengl build/tests/tst_client_surface_visibility
```

No SDE principal sem a sonda, abrir os mesmos clientes; confirmar leitura e digitação, clicar nos controles, arrastar, redimensionar continuamente, usar snap, workspaces, menus e popups, alternar repetidamente os três perfis e testar monitores/escalas distintos. Nenhum conteúdo pode desaparecer, duplicar ou perder entrada.

Pendências transferidas para a Etapa 07 por orientação do usuário: investigar e corrigir o conteúdo preto do XWayland real, confirmar sua entrada e popups, completar a homologação manual de subsurfaces e múltiplos monitores/DPR, e esclarecer os avisos GTK. As capturas V1/V2, os contadores do servidor isolado e os experimentos refutados ficam como ponto de partida. A ausência de aprovação visual XWayland não deve ser confundida com o resultado 47/47 do CTest.

A Etapa 06 fica encerrada como PARTIAL. Não foi iniciado trabalho da Etapa 07 nem realizado merge.
