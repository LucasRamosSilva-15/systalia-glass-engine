# Etapa 06.3 — Backdrop limitado

**Classificação: PARTIAL.** Branch `valo-graphics-2.0`; 9 de novembro de 2026. A seleção de três entre quatro produtores foi testada em framebuffer QRhi com superfícies equivalentes. O SDE contém apenas duas janelas internas reais elegíveis: Files e Material. As duas foram integradas à Dock e à TopBar e exercitadas no executável principal. Não foi comprovada a inclusão segura de clientes externos, nem a composição de três janelas reais no SDE. Não se trata de suporte universal.

## Arquitetura e alterações

- `src/qml/graphics/v2/ValoLimitedBackdropV2.qml` seleciona até três itens explicitamente fornecidos. Descarta a própria consumidora, itens invisíveis, com opacidade zero, fora do workspace, sem interseção ou acima da consumidora. Ordena por `z`, usa a ordem dos irmãos para empate e retém os três mais próximos; compõe do fundo para a frente sobre o wallpaper. Itens excedentes não são ocultados nem modificados na cena real. Geometria, opacidade, visibilidade e Z são observados; o deslocamento do viewport é uma dependência explícita.
- `src/qml/main.qml` usa esse componente para Dock e TopBar quando Files e Material estão visíveis. Cada consumidora tem seleção e textura próprias; a preparação do provider acompanha o produtor mais próximo, e os dois providers internos são encadeados na ordem Z quando ambos são capturados. Com uma única janela permanece o caminho da Etapa 06.2. Files e Material ainda usam entre si o piloto de duas janelas anterior; portanto a integração entre superfícies internas ainda não é um contrato genérico de três produtores.
- Os três objetos de captura por instância permanecem estáveis durante movimento e mudança de seleção; slots não usados não têm fonte nem atualização ativa. A composição intermediária alimenta o **provider já existente da consumidora**, que executa o Dual-Kawase conforme seu perfil; não foi criado um pipeline de blur por camada. Os providers das próprias janelas seguem necessários para renderizá-las. Para manter os UVs baseados em coordenadas de viewport do shader existente, as capturas continuam no tamanho do viewport. Isso tem custo considerável e ainda não é uma solução otimizada por região.
- `CMakeLists.txt` registra o componente. `tests/unit/tst_ValoOpticalContractV2.cpp` inclui teste de pixels e seleção com quatro produtores. `tests/integration/tst_ShellStateRegression.cpp` cobre seleção das duas janelas reais e ordem dos providers. `src/diagnostics/ValoStabilityProbe.hpp` e `tools/validate_valo_stage063.py` adicionam cenários opt-in e registram as produtoras selecionadas no SDE.

Nenhuma mudança foi feita nas UBOs, nos 124 tokens, nos shaders e geometria L2/G2, no Dual-Kawase, nos protocolos/compositor, no renderer V1, nas partículas ou nas regras Adaptive/Lucid. A tonalização configurável de 50% permanece.

## Segurança, saídas e fallbacks

Só são capturadas janelas internas explicitamente conhecidas; não se captura um ancestral comum, a consumidora, janelas superiores ou buffers de `ShellSurfaceItem`. As capturas usam `hideSource: false`; não há readback por frame na produção. Nenhuma gestão de recursos QRhi ou estágio de renderização foi alterado. A dependência interna é acíclica, da janela inferior para a superior e desta para a consumidora.

O caminho integrado volta para wallpaper-only durante transição de workspace, presença de cliente externo, snap preview, splitter ativo, múltiplas telas ou sobreposição entre Dock e TopBar. Uma produtora com transformação incompatível é ignorada; se nenhuma produtora elegível restar, a consumidora usa wallpaper-only. A seleção de um único output é uma restrição conservadora; não foi validada seleção por output individual em ambiente multimonitor. Quando não há interseção, a respectiva consumidora usa wallpaper. Não se generalizou a captura de clientes: ainda faltam evidências de retenção, liberação, sincronização dos buffers e ausência de realimentação. Apresentação direta Wayland/XWayland foi preservada, e o problema de conteúdo preto XWayland continua reservado à Etapa 07.

O limite de três é testado com quatro itens de diagnóstico, porém o encadeamento de providers de três janelas VALO reais e o ciclo de vida de seus recursos ainda não foram demonstrados. Uma integração futura precisará provar a ordem de preparação de todos os produtores, inclusive quando eles não se intersectarem entre si. Não habilitar candidatos adicionais apenas adicionando-os à lista sem essa prova.

## Testes e capturas

- Projeto compilado; suíte completa **47/47 CTest**, 24,60 s: [build](evidence/etapa-06-3/build.log), [testes](evidence/etapa-06-3/ctest.log).
- Teste GPU de quatro retângulos conhecidos, com leitura de framebuffer exclusivamente diagnóstica: os três mais próximos foram compostos na ordem correta e o quarto permaneceu fora da amostra; alterações de Z, opacidade e posição mudaram a seleção e a presença de cliente externo acionou fallback. Os caminhos limitado, único e par passaram em Vulkan e OpenGL: [Vulkan](evidence/etapa-06-3/framebuffer-vulkan.log), [OpenGL](evidence/etapa-06-3/framebuffer-opengl.log). Esses elementos de teste não são evidência de fidelidade visual do desktop.
- SDE principal Vulkan: [TopBar com Files e Material](evidence/etapa-06-3/limited-topbar/sde-0.png), [Dock com Files e Material](evidence/etapa-06-3/limited-dock/sde-60.png). Estados registram `Files,Material` e a consumidora ativa. A [regressão de integração](evidence/etapa-06-3/ctest.log) verifica inversão da ordem Z e os providers. O [estresse no SDE](evidence/etapa-06-3/limited-stress/stress.json) fez 18 transições instrumentadas de perfil/ordem Z com as duas janelas atrás da Dock sem encerramento inesperado. Não equivale a interação humana prolongada ou quatro janelas reais.
- A sonda do SDE abriu e fechou [Alacritty Wayland](evidence/etapa-06-3/clients/02-v2.png), e registrou wallpaper-only enquanto havia cliente externo e retorno do piloto interno após seu fechamento. Também exercitou estados de snap, restauração e workspace via instrumentação. Não comprova digitação, ciclo repetido de abertura/fechamento nem múltiplos outputs reais.
- [0, 5, 15, 30 e 60 s](evidence/etapa-06-3/limited-dock/): seleção permaneceu `Files,Material`; opacidades efetivas Files/Material/TopBar/Dock permaneceram 0,50/0,50/0,42/0,34; tempo óptico avançou aproximadamente 0,90 → 60,41 s. Luminância amostrada Files 0,344–0,350, Material 0,449–0,452 e Dock 0,217–0,240. Essas medidas e capturas são evidência instrumentada, não substituem avaliação visual humana de cintilação, legibilidade e alterações sutis.
- [Reduced Motion](evidence/etapa-06-3/policy/) parou os timers observados por volta de 2,9 s e os retomou após liberação. Foi ensaio instrumentado com uma janela interna; não é auditoria completa de acessibilidade. O SDE principal foi executado em Vulkan; OpenGL foi testado no framebuffer QRhi, não homologado no SDE principal.

## Desempenho

Medição no SDE Vulkan, Radeon Vega, 1662 × 975, DPR 1, perfil Balanced, aquecimento de 2 s. CPU mede o intervalo entre sinais do frame, não o uso total de um núcleo; GPU usa o último tempo de frame concluído informado pelo QRhi. Os cenários e durações diferem, logo não são comparação causal isolada.

| Cenário | CPU média / p95 ms | GPU média / p95 ms | Amostras CPU |
|---|---:|---:|---:|
| Controle wallpaper-only, etapa 06.3 | 0,58 / 0,75 | 1,43 / 1,69 | 204 |
| TopBar, duas janelas | 7,07 / 15,31 | 3,50 / 5,70 | 529 |
| Dock, duas janelas, 60 s | 7,08 / 14,60 | 3,31 / 5,50 | 5588 |
| TopBar, uma janela, etapa 06.2 | 4,96 / 13,04 | 2,14 / 3,50 | 406 |
| Dock, uma janela, etapa 06.2 | 4,89 / 12,67 | 2,04 / 3,34 | 3704 |

Dados brutos: `frame-timings.json` nas pastas de cada cenário. Na observação longa houve cerca de 96 callbacks de CPU por segundo após aquecimento; esse número não representa necessariamente quadros apresentados. Com duas produtoras, estimam-se três texturas RGBA de viewport por consumidora ativa, cerca de **18,5 MiB** em DPR 1, além do piloto interno, buffers e pyramid de blur. Com três produtoras seriam cerca de 24,7 MiB por consumidora; é estimativa, não medição de alocação do driver. O custo cresce com DPR² e duas consumidoras ativas podem duplicar os intermediários. As medições indicam aumento significativo; o projeto não forneceu orçamento de frame configurado, e não foi implementada degradação silenciosa ou limite arbitrário. É necessário definir orçamento e medir alocações, latência e frame pacing em hardware de destino antes de ampliar o número de produtores reais.

## Reprodução e pendências

Em sessão Wayland, `python3 tools/validate_valo_stage063.py limited-dock`, `limited-topbar`, `limited-stress`, `control`, `policy`, `clients` ou `pair-stress` executa o SDE real e grava evidências. A suíte é `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j 2`.

Ainda faltam três ou mais janelas internas VALO reais com providers independentes, testes prolongados de abertura/fechamento, resize e foco manuais, comparação em múltiplos workspaces/outputs físicos, OpenGL no SDE e homologação visual de legibilidade/continuidade. Até provar esses casos e a segurança de buffers externos, manter os fallbacks; não foi iniciado trabalho da Etapa 07.
