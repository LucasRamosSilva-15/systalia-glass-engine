# ETAPA 05.4 — Dual-Kawase e animação óptica

Data: 2026-11-09. Branch: `valo-graphics-2.0`. Baseline: 47/47 CTest.

**Classificação: PARTIAL.** Implementação, testes GPU Vulkan/OpenGL e execução do SDE principal com capturas reais foram realizados. As capturas foram inspecionadas e mostram diferenças de blur e evolução temporal. Ainda falta homologação humana interativa contínua no SDE, especialmente a avaliação perceptiva de suavidade/cintilação, suspensão pelo gerenciador de janelas e retomada. O aplicativo principal continua selecionando Vulkan; OpenGL foi validado pelos testes RHI e de integração, não por uma execução completa do binário principal em OpenGL. Não se declara homologação visual manual integral.

## Causas comprovadas

1. **Entrega inicial da textura filtrada sem notificação ao consumidor.** A primeira criação do wrapper agendava um frame da janela, mas não a atualização do nó do material. Com produtor estático e sem animação incidental, o nó podia continuar usando a textura original. No experimento de controle removendo somente a nova notificação, o teste GPU encontrou Performance e Balanced com a mesma energia de gradiente (0.13420557), `filteredMaterial=false`, e falhou. Restaurando a notificação, Balanced passa a usar o resultado QRhi e a energia cai para 0.00121329. O log do controle negativo foi preservado; sua falha é esperada, não pertence à suíte final.
2. **Consulta da textura dinâmica depois da decisão de cache.** O retorno antecipado quando o blur estava limpo podia impedir `QSGDynamicTexture::updateTexture()` de revelar uma atualização do produtor. Agora a textura dinâmica é atualizada antes da decisão; mudança de conteúdo, tamanho ou ponteiro nativo invalida o blur. A regressão com troca de fundo conhecido verifica a propagação após o primeiro blur.
3. **Performance ignorava a preparação do produtor.** Corrigir o teste para vincular o perfil ao próprio item revelou que a linha “raw” anterior era sobrescrita pelo perfil padrão Quality. Performance precisa atualizar a captura dinâmica upstream, embora execute zero passes Dual-Kawase. A preparação foi preservada para esse perfil; o teste agora confirma a mudança real de cor no fundo sem blur.
4. **Congelamento após cinco segundos era política explícita do item.** O timer de inatividade congelava `m_accumulatedTime`, independentemente de a superfície continuar visível em Balanced/Quality. Não era uma falha da fórmula de cáusticas. O teste de regressão aguarda mais de cinco segundos e compara frames do passe I.
5. **Pausa e intensidade estavam acopladas.** Reduced Motion/economia podiam zerar a intensidade enviada ao material. Agora esses estados controlam o avanço de fase; a intensidade selecionada continua presente estaticamente. Intensidade explicitamente zero continua zero.
6. **TopBar não acompanhava o seletor global.** Files, Material Window e Dock já recebiam o perfil; a TopBar permanecia em seu preset Quality. Foram adicionados vínculos para a barra principal e suas réplicas.

Não foi comprovado defeito no kernel Dual-Kawase: quando a textura correta já era consumida, o pipeline produzia resultados distintos. Não se aumentaram raio, offsets ou intensidade para disfarçar a integração. Um wallpaper suave e a tonalização volumétrica de 85% tornam diferenças menos óbvias; a grade de diagnóstico evita essa ambiguidade.

## Fluxo e gerenciamento de recursos

`Performance | Balanced | Quality` → `materialWindow.blurProfile` (0/1/2) → `ValoMaterialSurfaceV2.blurProfile` → `ValoGlassItemV2` → `ValoBackdropProviderV2` → snapshot do perfil na sincronização → preparação/execução QRhi em `beforeRendering` → wrapper `QSGTexture` → `ValoGlassMaterialV2` → shader óptico.

Performance usa a captura original, 0 passes. Balanced usa 2 down + 2 up. Quality usa 4 down + 4 up. O primeiro wrapper filtrado emite `textureChanged()`, que solicita `updatePaintNode()` do consumidor. Os perfis filtrados mantêm a identidade do wrapper e substituem somente seu ponteiro nativo. Performance pode selecionar a textura original sem destruir o wrapper filtrado.

Foram preservados o estágio de execução, a exigência de command buffer válido antes de reconstrução, a liberação no render thread e a atualização segura do wrapper da Etapa 05.3.1. Em `beforeRendering`, a origem é resolvida pelo provider já sincronizado: chamar o caminho que aposenta um fallback nessa fase poderia invalidar uma textura já entregue ao material naquele frame. Falha de preparação agora registra aviso e estado `unavailable`, em vez de parecer blur filtrado. Não há readback no caminho normal.

Inspeção de `ValoGpuBlurPipelineV2`, `ValoGlassMaterialV2`, `ValoMaterialSurfaceV2.qml` e `SystaliaMaterialWindow.qml` confirmou o kernel, sampler e vínculos existentes. Esses arquivos não precisaram ser modificados. `props.time` continua alimentando `u_Time` no upload integral da UBO V2; `u_CausticIntensity` recebe a intensidade independente do congelamento. Os frames isolados I e os frames H do SDE confirmam contribuição animada apresentada, não apenas um contador no CPU.

## Política temporal

| Estado | Comportamento |
|---|---|
| Quality visível/ativo, permitido | Avança continuamente; timer de 16 ms |
| Balanced visível/ativo, permitido | Avança continuamente; timer de 33 ms |
| Economia moderada | Quality 33 ms; Balanced 50 ms |
| Performance | Fase estática, iluminação preservada |
| Reduced Motion / contexto proíbe animação | Fase estática, iluminação preservada |
| Item oculto, inativo, janela oculta/minimizada | Timer próprio suspenso |
| Retomada | Reinicia medição do delta, preserva fase acumulada; não integra tempo oculto |

O delta continua limitado a 100 ms para evitar saltos após travamento do event loop. Os intervalos são pedidos ao timer, não garantia de FPS do compositor. Outras animações da UI podem produzir frames. Os critérios de visibilidade usam o estado Qt do item/janela; não implementam detecção de oclusão por outras janelas.

## Métricas GPU

Teste `gpuBackdropProfileSharpness`: produtor QSG estático sem notificações incidentais, checker/texto contrastante, material real, diagnóstico E sem iluminação animada. Leitura do framebuffer somente no teste. Não usa `filterOffline`. A observação da textura consumida ocorre no render thread, comparando o ponteiro nativo do sampler com o da origem.

Energia = média de `(L(x+1,y)-L(x,y))² + (L(x,y+1)-L(x,y))²`, com RGB codificado normalizado e pesos 0.2126/0.7152/0.0722. Região interior documentada no teste. Menor energia indica menor nitidez nesse padrão; não equivale a uma porcentagem perceptiva de blur. A luminância média também é verificada para rejeitar uma imagem preta/vazia como falso resultado.

| Perfil | Passes | Energia Vulkan | Energia OpenGL | L média | Sampler filtrado |
|---|---:|---:|---:|---:|---|
| Performance | 0 | 0.134205570 | 0.134205570 | 0.486377 | Não |
| Balanced | 4 | 0.001213294 | 0.001213294 | 0.485962 | Sim |
| Quality | 8 | 0.000003277 | 0.000003277 | 0.480551 | Sim |

Sequência 0 → 1 → 2 → 0 → 2 → 1 repetiu os mesmos resultados, verificando estabilidade do wrapper entre perfis filtrados. As comparações assertivas são relativas, não esses números específicos. Backends confirmados nos logs; fabricante/modelo da GPU não foi registrado, portanto não se extrapola desempenho para outro hardware.

## SDE principal: evidências reais

Todos os PNG abaixo vêm de `build/systalia_glass_engine`, com Files e Material Window abertas, Dock e TopBar presentes, em Vulkan. O gerador de desktop não foi usado. A sonda existente foi ampliada, opt-in por variáveis de ambiente, para automatizar seleção, pausas e captura da janela real.

Evidências em [artifacts/graphics-v2-stage-05-4](../../artifacts/graphics-v2-stage-05-4/):

- `sde-profiles-H/`: grade/texto injetados como **estímulo de backdrop**, renderização H de produção; 72 transições, saída normal, `stress.json` confirma Vulkan e contagem. As capturas Performance mostram a grade nítida através do vidro; Balanced/Quality a filtram. O conteúdo da janela e os controles continuam reais.
- `sde-profiles-E/`: mesma comparação com a contribuição de blur isolada e intensidade animada zero. `pattern-config.json` confirma que a origem do backdrop foi efetivamente substituída pelo padrão. Uma tentativa anterior sem atribuição efetiva foi descartada e não está nestas evidências.
- `sde-quality/` e `sde-balanced/`: capturas H e JSON em 0, 5, 15, 30 e 60 segundos, sem interação. Todos os quatro itens visíveis mantiveram timer ativo, perfil correto, estado `filtered` e 8/4 passes. Opacidade, Adaptive e intensidade permaneceram estáveis; o tempo e a iluminação evoluíram.
- `sde-policies-I/`: passe I isolado. Fase Files ~0.932 no início, ~2.862 ao ativar Reduced Motion, inalterada nas amostras 4 e 6 (pausa/ocultação), ~4.769 após retomada em 8. Intensidade Files/Material/Dock 0.85 e TopBar 0.18 preservada; ocultação não acumula tempo para um salto posterior.
- `sde-performance/`: inicialização direta em Performance sobre a grade, amostras 0/2/4/6/8, fase zero, `bypass`, zero passes. Confirma que o produtor fica utilizável mesmo sem timer óptico.
- `temporal-metrics.json`: regiões e diferenças RGB das capturas reais; os recortes excluem relógio/telemetria da TopBar. Diferenças também incluem conteúdo da UI, portanto são apoio, não prova isolada de cáusticas. O teste I separa a contribuição óptica.

Tempo óptico de Files nos instantes nominais:

| Perfil | 0 s | 5 s | 15 s | 30 s | 60 s |
|---|---:|---:|---:|---:|---:|
| Quality | 0.913 | 5.616 | 15.130 | 29.679 | 58.735 |
| Balanced | 0.914 | 5.682 | 15.374 | 30.161 | 59.673 |

A amostra inicial ocorre após assentamento de aproximadamente um segundo. Tempo óptico não é cronômetro de parede: há custo de captura, agendamento e limite de delta. Não há platô após cinco segundos. Aos 60 s, diferenças médias RGB em bytes contra t0: Files 4.026/4.792, Material 2.887/3.705, Dock 6.301/2.532 e TopBar 0.609/0.215 (Quality/Balanced). A TopBar mantém o preset luminoso mais discreto de Balanced.

## Arquivos alterados

- `src/gfx/v2/ValoBackdropProviderV2.{cpp,hpp}`: atualização upstream, invalidação/cache, notificação inicial, diagnóstico de status/passes; preservação do wrapper e do ciclo QRhi.
- `src/gfx/v2/ValoGlassItemV2.{cpp,hpp}`: política de animação por perfil/visibilidade/contexto, fase contínua, intensidade estática preservada, rejeição de perfil intermediário e getters diagnósticos C++.
- `src/qml/main.qml`: perfil da TopBar principal e réplicas ligado ao seletor existente.
- `src/diagnostics/ValoStabilityProbe.hpp`: estímulo verificável, amostras temporais, políticas e capturas opt-in do SDE real.
- `tests/unit/tst_ValoOpticalContractV2.cpp`: regressões GPU de nitidez/sampler/wrapper, fonte dinâmica e animação além de cinco segundos, Reduced Motion/ocultação/retomada.
- `tests/unit/tst_ValoRefractionOpticsV2.cpp`: item sem janela não deve iniciar animação invisível.
- `tests/integration/tst_ShellStateRegression.cpp`: perfil global também verificado na TopBar.
- Este relatório e artefatos de execução.

## Compilação e testes

- `cmake --build build -j4`: aprovado, incluindo projeto e recursos QSB existentes.
- `ctest --test-dir build --output-on-failure`: **47/47 aprovados**, 38.97 s. Regressões adicionadas aos executáveis existentes; não se infla a contagem de CTest.
- `build/tests/tst_valo_optical_contract_v2`, funções `gpuBackdropProfileSharpness` e `profileAnimationAndStaticEnergy`: Vulkan e OpenGL, 4 resultados aprovados em cada execução (incluindo init/cleanup).
- `build/tests/tst_shell_state_regression repeatedGraphicsProfiles`: Vulkan e OpenGL, 3 resultados aprovados por backend, com trocas por clique, resize e invalidação/retomada já cobertos pela regressão anterior.
- SDE principal: 72 trocas, grade H/E, duas sequências de 60 s e teste de política com capturas; sem crash observado.
- `git diff --check`: aprovado.

Os logs de integração ainda contêm avisos preexistentes de EGL_WL_bind_wayland_display, buffers SVG e encerramento do Xwayland auxiliar de teste. A suíte aprova, mas isso não constitui validação de clientes Wayland/XWayland externos. Nenhum código do compositor foi alterado.

## Reprodução e homologação pendente

Em uma sessão gráfica Wayland, execute o binário principal normalmente e abra Files e Systalia Control Center. Compare os três botões, aguarde 60 s sem interação e observe as bordas/volume em Files, Material, Dock e TopBar. Use um wallpaper de alta frequência para a comparação de blur. Teste Reduced Motion no contexto, oculte/revele e minimize/restaure a janela host, verificando ausência de cintilação e salto. A homologação humana dessa sequência continua pendente.

Para repetir as capturas automatizadas do aplicativo real, use um diretório de saída novo:

```bash
SGE_VALO_STABILITY_PROBE_DIR=/tmp/valo54-quality \
SGE_VALO_OPTICAL_TIMELINE=1 SGE_VALO_OPTICAL_PROFILE=2 \
build/systalia_glass_engine
```

Use perfil 1 para Balanced. Para comparação sobre grade: `SGE_VALO_PROFILE_STRESS=12 SGE_VALO_BLUR_PATTERN=1` com o diretório de saída, sem timeline; a sonda salva `profile-{0,1,2}.png` e encerra após a sequência. `SGE_VALO_OPTICAL_PASS=E` isola blur, `I` isola cáusticas; omitir mantém H. Para RM/ocultação, combine timeline, perfil e `SGE_VALO_POLICY_PROBE=1`. A sonda bloqueia entrada na sequência temporal e encerra ao terminar; não usar essas flags na sessão de uso normal.

Nesta execução foi usado `dbus-run-session`, runtime temporário privado e `WAYLAND_DISPLAY=/run/user/1000/wayland-1` absoluto para preservar a sessão/compositor já abertos. Esses caminhos são específicos deste ambiente. Temas/portal foram desativados somente nas execuções de diagnóstico. Não são mudanças do produto.

UBOs V1 128 bytes/V2 160 bytes, compositor Wayland/XWayland, 124 tokens, renderer V1, kernel Dual-Kawase, fórmulas GLSL e composição corrigida na Etapa 05.2 permaneceram inalterados. Não houve merge nem início da Etapa 06.
