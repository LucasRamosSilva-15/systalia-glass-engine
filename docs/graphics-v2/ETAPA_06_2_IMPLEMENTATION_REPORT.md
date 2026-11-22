# Etapa 06.2 — Header, resize e backdrop compartilhado

**Estado: PARTIAL** — branch `valo-graphics-2.0`, 9 de novembro de 2026. O backdrop de clientes externos não foi comprovado e continua com fallback wallpaper-only. As verificações de interação foram instrumentadas no SDE real; ainda é necessária homologação manual prolongada de entrada, aparência e múltiplos monitores.

## Alterações

- `src/qml/WaylandWindowContainer.qml`, `shell/SystaliaFilesWindowV2.qml`, `shell/SystaliaFilesWindow.qml` e `shell/SystaliaMaterialWindow.qml`: removidos os textos dos badges dos cabeçalhos, inclusive no fallback V1 do Files e nas decorações V1/V2 dos clientes. A API genérica de badge do `SystaliaWindowHeader` permanece disponível; sem texto, o espaço fica livre para o título. O texto institucional da TopBar não é cabeçalho de janela e foi preservado. No Material, título e margem das ações seguem o contrato da etapa 06.1: Inter Medium 21 px, 50 px de altura, botões 28 px, intervalo 10 px e margem direita 18,5 px.
- `src/qml/main.qml`: componente reutilizável `ShellResizeHandles`, com bordas de 8 px, oito direções e cursores correspondentes. Coordenadas são calculadas no pai da janela, com a borda oposta fixa. O resize é desabilitado durante maximização ou snap; ao terminar atualiza a geometria de restauração. O caminho próprio de resize dos clientes não foi alterado.
- `shell/SystaliaFilesWindowV2.qml` e `shell/SystaliaMaterialWindow.qml`: larguras internas responsivas e mínimos de 610 × 530 e 610 × 525 px, respectivamente. Esses mínimos mantêm busca, grade e controles acessíveis. As alturas mínimas são próximas das alturas originais porque os controles ainda têm posições verticais fixas; tornar esses painéis verticalmente refluíveis é trabalho separado.
- `graphics/v2/ValoSingleWindowBackdropV2.qml`, `main.qml` e `CMakeLists.txt`: composição limitada de wallpaper com uma janela interna para Dock/TopBar. O piloto original de duas janelas continua independente.
- `tests/integration/tst_ShellStateRegression.cpp`, `tests/unit/tst_ValoOpticalContractV2.cpp`, `src/diagnostics/ValoInternalResizeProbe.hpp`, `src/diagnostics/ValoStabilityProbe.hpp` e `tools/validate_valo_stage062.py`: regressões e diagnósticos opt-in para o executável principal.

UBOs, tokens, compositor, protocolos, renderer V1, shaders ópticos, partículas e Dual-Kawase não foram modificados. A tonalização padrão de 50% permanece.

## Backdrop e segurança

O grafo novo é `wallpaper → janela interna produtora → captura/composição → provider da Dock ou TopBar`. A captura usa apenas a janela produtora, nunca um ancestral comum, a própria consumidora ou `ShellSurfaceItem`; usa `hideSource: false`, sem readback de produção. O provider consumidor aponta ao provider da produtora para preservar a ordem já existente de preparação GPU. Não foi alterado o agendamento QRhi nem introduzida destruição manual de recursos.

O caminho só é habilitado quando exatamente uma das duas janelas internas está visível, pertence ao workspace ativo, não há clientes externos, transição, snap preview ou splitter ativo, há no máximo uma tela, a transformação da produtora é compatível, Dock e TopBar não se sobrepõem e a janela intersecta a consumidora. Fora dessas condições, cada consumidora usa wallpaper-only. Quando duas janelas internas estão visíveis, o piloto Files/Material pode continuar ativo, mas Dock/TopBar retornam ao wallpaper. Não há captura de janelas à frente nem expansão para um grafo genérico de N produtores.

A captura de clientes externos não foi habilitada: uma captura de `ShellSurfaceItem` não demonstra por si só retenção/liberação segura de buffers, sincronização nem ausência de realimentação. O compositor continua apresentando os buffers diretamente. O defeito de conteúdo preto XWayland permanece reservado à Etapa 07 e não foi investigado aqui.

## Evidência e testes

- Build do projeto e recursos QML: [build.log](evidence/etapa-06-2/build.log). CTest completo: **47/47**, 24,49 s, [ctest.log](evidence/etapa-06-2/ctest.log).
- Testes de integração despacharam eventos de mouse nas oito direções das duas janelas, verificaram lados fixos, mínimos, cursores, maximize/snap e restauração. O executável principal repetiu os 16 arrastos com sucesso: [resultados](evidence/etapa-06-2/resize/resize-results.json), [frame real](evidence/etapa-06-2/resize/internal-windows.png). Os eventos foram instrumentados; isso não substitui uma sessão humana completa de arraste e foco.
- No SDE Vulkan, [Alacritty, Files e Material](evidence/etapa-06-2/clients/02-v2.png) foram abertos; Alacritty aparece mapeado, com conteúdo visível e decoração V2, e o diagnóstico registra fallback wallpaper-only com cliente presente. Após fechamento registra zero clientes e retorno do piloto interno. O ensaio também exercitou maximização/restauração, snap, workspace e perfis via instrumentação. Não comprova digitação, popups, DPR diferente de 1 ou interação manual completa. A [captura anterior da etapa 06.1](evidence/etapa-06-1/before/01-aligned.png) é a referência real antes da remoção dos badges.
- Sobreposição interna: 18 transições instrumentadas de perfil e ordem Z sem crash no SDE, com [registro](evidence/etapa-06-2/pair-stress/stress.json) e frames reais. O teste de framebuffer também redimensiona a fonte enquanto alterna ordem e perfis. A [Dock](evidence/etapa-06-2/dock/sde-0.png) e a [TopBar](evidence/etapa-06-2/topbar/sde-0.png) foram exercitadas separadamente sobre Files; os respectivos estados registram `dockComposite` e `topBarComposite`. O [controle wallpaper-only](evidence/etapa-06-2/control/state-0.json) e a presença de cliente confirmam os fallbacks.
- Teste diagnóstico real de framebuffer QRhi com contraste conhecido: Vulkan e OpenGL passaram, inclusive alternância de perfis, resize da produtora, fallback, destruição e frames seguintes. Contraste médio das bordas Performance/Balanced/Quality: **0,09760 / 0,00547 / 0,00194**, em ambos os backends. Ver [Vulkan](evidence/etapa-06-2/overlay-vulkan.log) e [OpenGL](evidence/etapa-06-2/overlay-opengl.log). Estes frames de teste são apenas diagnóstico GPU, não prova de fidelidade visual do desktop. O SDE principal foi executado em Vulkan; OpenGL não foi homologado no SDE principal.
- Em [0, 5, 15, 30 e 60 segundos](evidence/etapa-06-2/dock/), a Dock manteve a composição ativa, perfil Balanced e opacidades efetivas Files/TopBar/Dock de 0,50/0,42/0,34; o tempo óptico avançou de aproximadamente 0,92 a 59,60 s. A luminância amostrada do Files variou de 0,105 a 0,136 e a da Dock de 0,187 a 0,226. Isso não demonstra sozinho ausência de mudança visual inesperada: as capturas reais devem ser revisadas manualmente no ambiente de destino.
- [Reduced Motion](evidence/etapa-06-2/policy/) foi ativado e desativado pela sonda: Files, TopBar e Dock pararam o timer em aproximadamente 3 s e retomaram após a liberação, com intensidade preservada. É uma verificação instrumentada, não uma auditoria completa de acessibilidade.

## Custo observado

Medições do SDE Vulkan em 1662 × 975, AMD Radeon Vega, Balanced, após aquecimento de 2 s. CPU é o intervalo entre os sinais de início/fim do frame, não uso total de um núcleo; GPU é o último tempo de frame concluído informado pelo QRhi. Amostras e durações diferem e os cenários de TopBar/Dock têm posições distintas; não interpretar a diferença como benchmark causal isolado.

| Cenário | CPU média / p95 (ms) | GPU média / p95 (ms) | Amostras CPU |
|---|---:|---:|---:|
| Wallpaper-only | 0,63 / 0,90 | 1,41 / 1,68 | 182 |
| TopBar composta | 4,96 / 13,04 | 2,14 / 3,50 | 406 |
| Dock composta, 60 s | 4,89 / 12,67 | 2,04 / 3,34 | 3704 |

Os dados brutos estão nos arquivos `frame-timings.json` de cada cenário. A composição adiciona duas texturas RGBA de tela cheia, aproximadamente 12,4 MiB em 1662 × 975 e DPR 1, além dos recursos e passes existentes; o custo cresce com o quadrado do DPR. Ela só é ativada quando há interseção, mas o custo medido justifica acompanhar frame pacing e consumo em hardware mais lento.

## Reprodução e pendências

Com sessão Wayland disponível, compile o projeto e execute `python3 tools/validate_valo_stage062.py` com `resize`, `control`, `topbar`, `dock`, `policy`, `clients` ou `pair-stress`. As sondas são opt-in, usam o executável `build/systalia_glass_engine` e gravam capturas reais nesta pasta de evidências. Execute `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j 2` para a suíte.

Antes da homologação, repetir manualmente as oito direções, foco, snap/maximização/restauração e controle dos perfis; revisar frames aos 60 s, uso prolongado, múltiplos monitores/DPR e OpenGL no aplicativo. Demonstrar retenção e sincronização dos buffers seria pré-requisito para qualquer backdrop de clientes externos; até então manter wallpaper-only. A etapa 07 não foi iniciada.
