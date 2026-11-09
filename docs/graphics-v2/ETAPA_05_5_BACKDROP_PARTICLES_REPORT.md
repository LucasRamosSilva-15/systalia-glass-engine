# ETAPA 05.5 — Backdrop de janelas, transparência e partículas

Data: 2026-11-09. Branch: `valo-graphics-2.0`. Referência: [Etapa 05.4](ETAPA_05_4_BLUR_ANIMATION_REPORT.md).

**Status: PARTIAL.** O backdrop entre Files e Material Window internas V2 foi implementado e validado no SDE principal Vulkan, com inversões de Z, três perfis e capturas reais até 60 segundos. O padrão de tonalização é 50% e as partículas têm um passe independente. A composição de clientes Wayland/XWayland **não foi implementada**: a presença de qualquer janela externa ativa explicitamente wallpaper-only para o par interno. Não há simulação de conteúdo desses clientes. A generalização também aguarda validar a retenção/importação de buffers externos e reduzir o custo de propagação de frames antes de ampliar o grafo.

## 1. Investigação e arquitetura do backdrop

`backdropGrab` realmente capturava somente `wallpaperBackdropRoot`. Não havia informação das janelas traseiras na textura enviada ao Dual-Kawase; aumentar raio não resolveria essa ausência.

Foi adicionado `ValoInternalWindowBackdropV2.qml`, deliberadamente limitado ao par interno Files/Material. Ele constrói um grafo acíclico:

```text
wallpaper → backdropGrab → material da janela traseira
                         ↘ cópia de wallpaper no compositor intermediário
janela traseira (corpo + conteúdo real) → rearCapture ────────↗
compositor intermediário → compositeGrab → Dual-Kawase da janela frontal
                                         → material óptico frontal
```

A traseira sempre recebe wallpaper. Só a frontal recebe a composição. A seleção usa Z; em empate, Material, criada depois no mesmo pai, fica à frente. A troca de foco atualiza o grafo pelas ligações QML antes da sincronização do Scene Graph. Não se captura o pai comum nem a cena inteira. Não há captura da própria frontal, janela à frente, ou seu conteúdo por realimentação. Os dois `ShaderEffectSource` novos usam `recursive: false` e `hideSource: false`.

A captura da traseira usa um render target do tamanho do viewport e um `sourceRect` deslocado pelo negativo de `mapToItem`, incluindo a margem do workspace. Assim `gl_FragCoord` permanece em coordenadas de tela também ao renderizar o corpo traseiro offscreen. Uma captura do tamanho local da janela foi rejeitada: deslocava o wallpaper dentro do corpo capturado. O compositor intermediário também tem o tamanho do viewport.

O teste bicolor revelou ainda que a projeção de uma camada OpenGL tem orientação diferente da projeção de tela. `ValoGlassMaterialV2` agora considera o sinal Y da projeção ao codificar a orientação na altura assinada existente; usar somente `isYUpInFramebuffer()` invertia o wallpaper uma segunda vez na camada. O shader e o layout da UBO não precisaram mudar. A diferença de projeção também aparece na implementação de [QSGRhiLayer](https://codebrowser.dev/qt6/qtdeclarative/src/quick/scenegraph/qsgrhilayer.cpp.html#428). Vulkan/OpenGL e DPR 2 são cobertos por regressão. Texturas usam DPR da tela Qt. A opacidade própria da traseira é aplicada à cópia, pois o Qt captura a raiz da origem como opaca. Escalas/rotações diferentes da identidade desativam o piloto em vez de produzir coordenadas incorretas. Este piloto não suporta uma lista arbitrária de transformações.

O caminho composto só fica elegível quando as duas janelas estão visíveis, sobrepostas, no mesmo pai e no workspace atual, sem transição. Fora disso, `coverageMode` é `wallpaper-only`. Em caso de qualquer janela externa registrada no compositor, também retorna wallpaper-only, conservadoramente mesmo se o cliente estiver em outro workspace. Dock, TopBar e outras superfícies continuam com seu backdrop de wallpaper; o piloto não promete composição completa do desktop.

O wrapper do provider e os mecanismos de alocação/liberação QRhi da Etapa 05.3.1/05.4 foram preservados. Foi acrescentada uma dependência explícita de preparação entre os providers, descrita abaixo, dentro do mesmo estágio `beforeRendering`. As novas capturas são recursos de Scene Graph administrados pelo Qt; não há criação/destruição manual de QRhi em QML nem mudança do estágio Dual-Kawase. Os passes permanecem 0, 2 down + 2 up e 4 down + 4 up. Não há readback na produção.

A documentação Qt confirma que `ShaderEffectSource` pode compor uma cadeia de origens e que ciclos exigem um recurso extra, que este piloto evita: [ShaderEffectSource](https://doc.qt.io/qt-6/qml-qtquick-shadereffectsource.html). Não se utiliza a opção recursiva como correção para um grafo incorreto.

### Limites de segurança

Não foram alterados `ShellSurfaceItem`, apresentação, retenção ou liberação de buffers de clientes, nem qualquer arquivo do compositor. Os testes internos comprovam a cadeia sob as condições acima; não comprovam que capturar buffers externos seria seguro. Intercalar clientes em Z, suas subsuperfícies, workspaces e aquisição/liberação de buffers exige uma generalização que não foi feita nesta entrega. A expansão foi contida também pelo custo adicional e pela necessidade de provar o ciclo de buffers externos, sem extrapolar o piloto para clientes.

## 2. Tonalização óptica padrão

`SystaliaMaterialWindow.qml` agora declara `property real surfaceOpacity: 0.50`. O slider de 20–100% e seus sinais/vínculos foram preservados. Files segue o valor da janela de material; os testes continuam alterando o slider e verificando os dois materiais.

O valor é o peso de mistura do backdrop refratado com o volume oceânico, não o alpha herdado do item. A cobertura SDF e a opacidade herdada continuam controlando o alpha pré-multiplicado do corpo. As capturas/JSON do SDE mostram Files e Material em 0.50, TopBar em 0.42 e Dock em 0.34, todos com `Item.opacity` independente. As opacidades próprias de Dock, TopBar e outros painéis não foram sobrescritas.

## 3. Origem antiga e novo passe de partículas

A função `waterCaustics` em `shaders/valo_optical.frag` V1 mistura três ondas e aplica potência: não é um sistema de partículas. O wallpaper `valo-ocean-atmosphere.png`, inspecionado diretamente, já contém pontos luminosos estáticos. Não foi encontrado um emissor independente antigo para recuperar. Esses pontos do wallpaper continuam fazendo parte do backdrop; o novo efeito é uma contribuição separada e esparsa, não uma amplificação/duplicação das ondas V1.

Foram adicionados `ValoOpticalParticlesV2.qml` e `valo_particles.frag`, como overlay entre o corpo e o conteúdo. O passe usa um bloco próprio de ShaderEffect; **nenhum campo foi adicionado à UBO óptica V2 de 160 bytes**. As fórmulas H/I/J/K existentes, composição óptica, cáusticas suaves e kernels de blur ficaram intactos.

Características:

- Quality: até seis partículas; Balanced: até três; Performance: overlay desativado.
- Centros com sementes diferentes por instância da superfície, sem grade/tile ou wrap; Dock usa sua própria semente. A semente de cada superfície permanece estável durante sua vida, mas não é persistida entre execuções.
- Deriva contínua em trajetórias limitadas, frequências baixas e sem pulsação de brilho. Núcleo de aproximadamente 1–1.8 pixels lógicos, halo curto; não há grandes discos ciano.
- `particleIntensity` calibrável na superfície (0.35 padrão; Dock 0.25). Não reutiliza nem redefine o slider de intensidade das cáusticas. Não foi acrescentado controle visual à janela.
- SDF L2/G2 e slant recortam a contribuição no interior; a cor de saída é pré-multiplicada por alpha e por `qt_Opacity`. Conteúdo fica acima do overlay para preservar leitura.
- Partículas e cáusticas compartilham a **fase**, não a intensidade. O item expõe `animationTime`; seu timer continua se qualquer um dos efeitos estiver habilitado. Desligar cáusticas não congela as partículas.
- Reduced Motion/economia que proíbe animação congelam a fase, preservando partículas estáticas. Ocultação/inatividade suspendem o timer; a retomada não integra o tempo oculto. Performance continua sem movimento.

Foi corrigida também uma lacuna do construtor C++: um item criado diretamente com pai já associado à janela podia perder o sinal inicial `windowChanged`. Agora observa a janela atual imediatamente, permitindo iniciar o timer ao mostrar a janela. Isso foi reproduzido pelo teste com cáusticas zero e partículas habilitadas.

## 4. Testes de pixels, estabilidade e OpenGL

Regressões adicionadas ao executável óptico existente:

- `internalWindowBackdropPixels`: duas superfícies V2 com patches conhecidos. A frontal recebe a cor da traseira; desativar a composição faz aparecer o wallpaper azul. Mudança do conteúdo traseiro chega ao sampler frontal. Trinta e seis inversões Z alternam os três perfis e confirmam que a cor antiga não fica acumulada. Ocultação retorna a wallpaper. Avisos de recursão/feedback fazem o teste falhar.
- `suspendedParticlePixels`: renderiza o QSB separado sobre alpha conhecido; verifica contribuição esparsa, mudança de fase, estabilidade com fase pausada, Quality/Balanced, zero em Performance, intensidade zero, alpha herdado zero/meio e recorte G2/slant. Salva frames do shader em 0/15/30/60 segundos quando a saída diagnóstica é solicitada.
- `particleSchedulingWithoutCaustics`: cáusticas zero, partículas presentes, avanço de fase, Reduced Motion com intensidade preservada, retomada e Performance sem timer.
- Integração do shell: padrão 50%, mesmos sliders/ações, seleção de backdrop ao inverter Z, fallback por presença externa, ocultação, ausência de interseção e separação entre workspaces.

Mantidos os testes da Etapa 05.4 de nitidez real GPU, textura efetivamente amostrada, wrapper estável, dano upstream e animação após cinco segundos.

### Falha de ordenação reproduzida e corrigida

O primeiro grafo acíclico em QML ainda não impunha a ordem dos callbacks dos providers. Depois de inverter Z, a frontal podia atualizar sua captura da traseira **antes** de o provider traseiro preparar o novo perfil. A captura referenciava a saída anterior; em seguida a reconstrução do blur traseiro podia aposentar recursos já referenciados nos comandos desse frame. Um wrapper estável sozinho não resolve essa dependência entre produtores.

O backtrace do abort OpenGL aponta para `_mesa_DeleteFramebuffers` / `_mesa_reference_renderbuffer_` / `delete_renderbuffer` no worker `gl0`. Desabilitar glthread ou depth parecia ajudar em controles individuais, mas a falha reapareceu na suíte completa. Essas hipóteses não foram consideradas correções e **os workarounds de depth foram removidos**. `tests/CMakeLists.txt` permanece inalterado.

O provider agora tem uma referência opcional `upstreamBlurProvider`, configurada apenas para a frontal e espelhada durante a sincronização. Antes de atualizar a captura composta, prepara a dependência traseira no próprio `beforeRendering`. A chamada posterior normal da traseira reaproveita o estado já preparado. Há guardas para auto dependência, ciclo e dependência entre janelas QRhi diferentes. Não se alterou o estágio de criação/destruição nem a identidade do wrapper; corrigiu-se a ordem de preparação exigida pelo novo grafo.

**Controle causal:** o mesmo teste, com `SGE_VALO_DEPENDENCY_NEGATIVE_CONTROL=1`, desliga somente a dependência no fixture e reproduziu o abort com `free(): invalid pointer` numa execução normal. Sob GDB, esse controle terminou normalmente; a manifestação depende do timing e não foi apresentada como determinística. Com a dependência ativa, a suíte óptica inteira passou **três vezes consecutivas** (32 verificações por execução) em OpenGL offscreen padrão, sem flags de driver/depth. O teste agora faz 36 inversões e confirma por assertiva qual provider depende de qual; o shell verifica também os vínculos reais. Logs positivos, controle negativo e backtrace foram preservados. O controle negativo é deliberadamente inseguro e não faz parte do CTest normal nem da aplicação.

### Resultado final

- Build do projeto e dos novos QSB: aprovado.
- `ctest --test-dir build --output-on-failure`: **47/47 aprovados**, 46.31 s, sem testes ignorados.
- Essenciais Vulkan/Wayland: **7 resultados aprovados**, incluindo init/cleanup; 0 falhas/skip.
- Essenciais OpenGL/Wayland: **7 resultados aprovados**, incluindo init/cleanup; 0 falhas/skip.
- SDE principal Vulkan final: 72 trocas de perfil **e** inversões de Z com as duas janelas sobrepostas; saída normal. A primeira tentativa com limite de 40 s terminou em timeout (124), sem contador final; a repetição com limite de 90 s concluiu as 72. A causa do atraso da primeira tentativa não foi determinada e o timeout não foi contado como aprovação nem como crash.
- Sequências do SDE de 60 s em cada um dos três perfis, com Z invertido em 7/22/45 s; sem crash observado e `coverageMode=internal-pair` nas amostras.
- `git diff --check`: aprovado.

Os testes existentes Wayland/XWayland passaram, mas não homologam a nova captura de clientes, que continua desabilitada. OpenGL foi executado nos testes reais de Scene Graph/QRhi sobre Wayland; o binário principal continua selecionando Vulkan e não foi alterado para forçar OpenGL.

## 5. Latência e custo medidos

### Atualização do conteúdo traseiro

Após mudar a cor traseira no teste GPU, a primeira leitura diagnóstica encontrou a mudança na frontal: **22.197 ms em Vulkan** e **20.620 ms em OpenGL**, uma tentativa em cada. É um limite superior com `qWait(16)`, agendamento e readback incluídos, não uma medição input-to-photon nem um cronômetro exclusivo de GPU. Não foi necessário `filterOffline` ou readback por frame na aplicação.

### Benchmark do SDE principal

Seis execuções seriais, viewport 1662×975, geometria sobreposta constante, scheduling natural, sem outro teste desta tarefa concorrente. Cada execução durou aproximadamente 8 s, descartando os primeiros 2 s. A captura PNG inicial ocorre antes dessa janela de medição. O controle desativa apenas a composição entre janelas, mantendo geometria, perfil e efeitos iguais. O JSON confirma `wallpaper-only` no controle e `internal-pair` no tratamento.

Timestamps foram habilitados apenas com `SGE_VALO_BACKDROP_BENCHMARK=1`; `QRhiCommandBuffer::lastCompletedGpuTime()` mede o frame GPU concluído, incluindo cena/blur/capturas, não apenas o novo passe. O dado pode corresponder ao frame anterior. Ver [configuração de timestamps Qt](https://doc.qt.io/qt-6/qquickgraphicsconfiguration.html). Tempos abaixo são locais, sem extrapolação para outros dispositivos.

| Perfil | GPU média wallpaper (ms) | GPU média par (ms) | Delta (ms) | P95 GPU par (ms) | Amostras GPU controle/par |
|---|---:|---:|---:|---:|---:|
| Performance | 1.347 | 1.518 | +0.172 | 1.871 | 6 / 5 |
| Balanced | 1.821 | 2.560 | +0.739 | 3.912 | 181 / 358 |
| Quality | 2.170 | 3.184 | +1.014 | 3.946 | 356 / 561 |

A amostra Performance é pequena e sua diferença não deve ser extrapolada para carga contínua. Performance estático produz poucas atualizações: sua amostra é pequena, não um percentil robusto de carga contínua. Sem animação não apareceu um ciclo autossustentado de redesenho; as atualizações restantes incluem telemetria/relógio. Em Balanced/Quality, a entrega de dano da traseira pode agendar frames adicionais. O número de amostras evidencia esse custo e impede tratar o delta por frame como custo total por segundo equivalente.

O campo `cpuFrame` nos JSON registra **tempo de parede entre beforeFrameBegin e afterFrameEnd**, incluindo esperas/submissão; não é utilização ativa de CPU. Médias controle/par: Performance 1.987/2.146 ms, Balanced 0.862/6.547 ms, Quality 0.799/10.008 ms. Não somar esse valor ao timestamp GPU como se fossem trabalhos independentes. A cadeia permanece mais cara e precisa de otimização antes de ser generalizada.

Memória adicional mínima estimada em RGBA8/DPR 1: composição de viewport ~6.18 MiB e captura da traseira também em viewport ~6.18 MiB, total ~12.36 MiB, além das pirâmides já existentes, depth/stencil/cache do Qt e eventuais alinhamentos. É uma estimativa de payload, não medição de VRAM. Escala aproximadamente com DPR². Não foi adicionado readback obrigatório ou armazenamento de frames anteriores para simular refração.

## 6. Evidências reais e comportamento temporal

Artefatos em [graphics-v2-stage-05-5](../../artifacts/graphics-v2-stage-05-5/):

- `profile-0`, `profile-1`, `profile-2`: capturas H reais e JSON em 0/5/15/30/60 s do SDE principal. O slider exibe 50%; Files e Material continuam com seu conteúdo e interações originais.
- `profile-1/sde-30.png`: ícones da janela Files traseira aparecem desfocados sob a lateral transparente da Material frontal. Em 15/60 s, Files está à frente; a origem se inverte.
- `stress`: 72 transições e PNGs dos perfis, sem gerador de desktop.
- `gpu-vulkan`, `gpu-opengl`: readback do teste real GPU, latência diagnóstica, nitidez Dual-Kawase e shader de partículas isolado.
- `benchmark-*`: PNG/estado inicial e timestamps do SDE; `benchmark-summary.json` reúne os controles verificados. Tentativas de benchmark com flags incorretas foram descartadas, não incluídas nesta tabela/artefatos.
- Logs de build, suíte final, essenciais, controles e backtrace offscreen.

Tempo óptico de Files nas amostras nominais:

| Perfil | 0 s | 15 s | 30 s | 60 s |
|---|---:|---:|---:|---:|
| Performance | 0.00 | 0.00 | 0.00 | 0.00 |
| Balanced | 0.91 | 15.18 | 30.94 | 60.51 |
| Quality | 0.91 | 14.93 | 30.43 | 59.46 |

A fase é a mesma enviada às partículas. Pausas de captura, timers e limite de delta explicam diferenças contra o relógio de parede. Perfis filtrados continuaram com 4/8 passes e timer ativo, sem congelamento aos cinco segundos. As diferenças de Z foram efetivas e constam nos JSON. As capturas foram inspecionadas; não substituem a avaliação humana contínua de suavidade e legibilidade.


### Partículas isoladas no aplicativo real

`particles-isolated-sde/` contém outra execução de `build/systalia_glass_engine` em Vulkan, sem sobreposição, em 0/5/15/30/60 s. A sonda define a opacidade **do item do corpo óptico** como zero, mantendo seu timer e o overlay irmão; a interface real e o wallpaper continuam presentes. É um diagnóstico de contribuição, não uma captura do acabamento final. As capturas H com acabamento final estão nos diretórios dos perfis.

Files avançou de ~0.928 a ~58.920 segundos ópticos com o corpo oculto; as partículas continuaram visíveis e mudaram de posição. Contra t0, as regiões de Files tiveram 375/339/386 pixels com diferença RGB acima de 2 bytes em 15/30/60 s, de um total de 374.334 pixels na região. Isso é compatível com contribuição esparsa; não são manchas de área grande. Pontos estáticos do wallpaper permanecem nos mesmos lugares e não explicam essas mudanças. O teste QSB com fundo/alpha conhecidos separa ainda mais a contribuição da UI.

`particles-policy-sde/` confirma Reduced Motion na execução real: fase ~3.024 nas amostras 2, 4 e 6, retomando para ~4.771 em 8. Entre as amostras 2 e 4, as regiões Files, Material e Dock tiveram **zero pixels alterados acima de 2 bytes**, sem zerar a intensidade das partículas. Na parte de ocultação essa sonda histórica oculta o item de vidro, não todos os conteúdos da janela; não se usa essa parte como prova de oclusão completa de uma superfície. Visibilidade herdada e desligamento de timer continuam cobertos pelos testes do item e pelas ligações do shell.

## 7. Arquivos alterados

- `src/qml/graphics/v2/ValoInternalWindowBackdropV2.qml`: compositor do par interno, seleção Z/workspace, mapeamento de viewport e fallback explícito.
- `shaders/v2/valo_backdrop_copy.frag`: cópia GPU do wallpaper já capturado no compositor intermediário.
- `src/qml/main.qml`: liga somente Files/Material ao seletor de backdrop; clientes externos desabilitam o piloto.
- `src/qml/shell/SystaliaMaterialWindow.qml`: padrão óptico 50%, sem mudar faixa/sinais do slider.
- `src/qml/graphics/v2/ValoOpticalParticlesV2.qml`, `shaders/v2/valo_particles.frag`: overlay separado e UBO própria de ShaderEffect.
- `src/qml/graphics/v2/ValoMaterialSurfaceV2.qml`, `ValoDockV2.qml`: integração do overlay sob o conteúdo e calibração própria.
- `src/gfx/v2/ValoGlassItemV2.{hpp,cpp}`: fase observável, intensidade de partículas apenas para scheduling e observação inicial da janela C++.
- `src/gfx/v2/ValoBackdropProviderV2.{hpp,cpp}`: dependência de preparação, snapshot na sincronização e guardas contra ciclos/cross-window. Wrapper e liberação permanecem preservados.
- `src/gfx/v2/ValoGlassMaterialV2.cpp`: orientação de UV baseada também na projeção atual da camada OpenGL; layout da UBO preservado.
- `src/diagnostics/ValoStabilityProbe.hpp`, `src/main.cpp`: sobreposição, inversões de Z, isolamento e timestamps opt-in.
- `CMakeLists.txt`: registro dos novos QML/QSB.
- `tests/unit/tst_ValoOpticalContractV2.cpp`, `tests/integration/tst_ShellStateRegression.cpp`: regressões de pixels, dependências e integração. `tests/CMakeLists.txt` não precisou ser alterado na solução final.
- Este relatório e `artifacts/graphics-v2-stage-05-5/`.

Não foram alterados compositor, buffers/clientes Wayland/XWayland, renderer V1, 124 tokens, structs/layouts UBO V1/V2, kernels Dual-Kawase ou fórmulas do shader óptico H e diagnósticos I/J/K. A alteração do upload de orientação não muda seus tamanhos de 128/160 bytes. Não houve merge nem início da Etapa 06.

## 8. Como reproduzir

Na sessão gráfica, execute `build/systalia_glass_engine`, abra Files e Systalia Control Center, sobreponha as janelas e alterne o foco pelo cabeçalho. Em Balanced, ícones/texto da traseira devem ficar reconhecíveis porém desfocados sob as áreas sem controles da frontal. Quality difunde mais; Performance preserva detalhes sem passes Kawase. Inverta a ordem e confira que a janela recém-traseira deixa de consumir o composto. Mova uma janela para outro workspace ou abra um cliente externo: o piloto deve voltar a wallpaper-only.

Verifique slider inicial em 50, faixa 20–100 e independência da opacidade própria do Dock/TopBar. Observe partículas pequenas por 60 s em Balanced/Quality; em Performance não devem aparecer partículas próprias do overlay. Os pontos já desenhados no wallpaper podem continuar presentes. Reduced Motion deve deixá-las estáticas. A homologação humana contínua de foco/movimento, suavidade e legibilidade permanece recomendada; a evidência desta entrega é automatizada e inspecionada por capturas reais.

Para reproduzir a sequência automatizada do SDE principal:

```bash
SGE_VALO_STABILITY_PROBE_DIR=/tmp/valo55-quality \
SGE_VALO_OPTICAL_TIMELINE=1 SGE_VALO_OPTICAL_PROFILE=2 \
SGE_VALO_INTERNAL_OVERLAP=1 build/systalia_glass_engine
```

Use perfis 0/1 nas demais comparações. `SGE_VALO_PROFILE_STRESS=12`, com overlap e sem timeline, faz 72 trocas/inversões. `SGE_VALO_PARTICLES_ONLY=1` isola o overlay no SDE; `SGE_VALO_POLICY_PROBE=1` habilita a sequência curta de Reduced Motion/pausa/retomada. A sonda temporal bloqueia entrada e encerra ao terminar. Não usar essas flags em uma sessão normal.

Benchmark: habilite `SGE_VALO_BACKDROP_BENCHMARK=1`, profile e overlap, sem timeline; encerra em ~8 s. Para o controle, **defina** `SGE_VALO_WALLPAPER_ONLY=1`; para o composto, **remova a variável**, não use valor zero, pois a sonda verifica sua presença. Confira `backdropCoverage` no JSON antes de comparar.

Testes GPU essenciais:

```bash
QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=vulkan \
SGE_VALO_GPU_METRICS_DIR=/tmp/valo55-gpu \
build/tests/tst_valo_optical_contract_v2 internalWindowBackdropPixels \
  suspendedParticlePixels particleSchedulingWithoutCaustics \
  gpuBackdropProfileSharpness profileAnimationAndStaticEnergy
```

Repita com `QSG_RHI_BACKEND=opengl`; para coordenadas HiDPI, `QT_SCALE_FACTOR=2`. Testes não reconfiguram o compositor principal para OpenGL.

Neste ambiente, as execuções principais usaram `dbus-run-session`, um `XDG_RUNTIME_DIR` privado temporário e `WAYLAND_DISPLAY=/run/user/1000/wayland-1` absoluto para não interferir no servidor já aberto. Tema/portal foram desativados só nas execuções diagnósticas. Não se matou nem reconfigurou a sessão do usuário.

## 9. Pendências que mantêm PARTIAL

1. Composição de clientes Wayland/XWayland e de mais de duas superfícies: não implementada. Não há prova de segurança para importação/retenção de seus buffers nesse grafo; fallback conservador permanece ativo.
2. A segurança comprovada é do grafo de duas janelas na mesma janela QRhi. Uma generalização exigirá dependências topológicas e retenção de recursos também para N produtores; os guardas atuais não são um compositor genérico.
3. Propagação de dano pode solicitar frames adicionais em Balanced/Quality. O custo medido e a memória adicional impedem assumir escalabilidade para N janelas sem outro trabalho.
4. Oclusão por janelas externas, transformações arbitrárias, múltiplos monitores/DPR em movimento e HDR não foram homologados. A regressão verifica DPR 2 controlado e Z/workspaces do par interno.
5. Benchmarks são amostras locais de frame completo, com poucos frames em Performance; não são microbenchmarks de partículas, medição de VRAM ou latência de apresentação. Não houve avaliação humana contínua do acabamento final.

