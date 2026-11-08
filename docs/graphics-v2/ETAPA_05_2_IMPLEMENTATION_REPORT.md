# SYSTALIA — VALO Graphics 2.0 — Etapa 05.2

Data: 08/11/2026. Branch: `valo-graphics-2.0`.

**Status: PARTIAL.** Implementação e regressões automatizadas aprovadas; falta homologação visual no SDE principal. Não há comprovação de fidelidade estética, equivalência visual integral entre backends ou aprovação para a Etapa 06.

## Contrato de composição corrigido

Antes, o shader calculava `mix(backdropRefratado, volume, s)` e aplicava novamente alpha aproximadamente `s` sobre o wallpaper visível. Isso produzia pesos `s(1-s)` para o backdrop refratado, `s²` para o volume e `1-s` para o fundo original, permitindo imagem fantasma.

Agora H e K resolvem a transmissão e a tonalização uma única vez:

```
body = (1-s) * backdropRefratado + s * volume
rgb = composição do body com efeitos e contorno
alpha = coberturaSDF * opacidadeHerdada
saída = vec4(rgb * alpha, alpha)
```

`surfaceOpacity` (`s`) continua sendo o controle existente de tonalização/opacidade óptica: 0 transmite o backdrop sem tonalização; 0,4 mistura 60% do backdrop com 40% de volume; 1 seleciona o volume. Não é mais utilizado como alpha de uma segunda composição do wallpaper. Vidro com `s=0` pode continuar refratando e refletindo. Transparência total do item é controlada por `QQuickItem::opacity`; zero produz contribuição nula. Durante fades, a interpolação com o destino é a composição normal da opacidade herdada.

A mistura óptica não é mais multiplicada por `RenderState::opacity()`. A cobertura e o RGBA final recebem essa opacidade; o piso de alpha 0,15 foi removido. O campo de cobertura SDF e as funções das silhuetas permanecem iguais.

## ABI e transporte interno

UBO V1: 128 bytes, sem alterações. UBO V2: 160 bytes, mesmos membros, tipos e offsets C++.

Para transportar o estado sem ampliar a UBO nem alterar a interface pública, somente a cópia enviada pelo material V2 usa dois contratos internos:

- Offset 124: o campo C++ legado `titleBarHeight`, não consumido pela geometria dos shaders V2, transporta `RenderState::opacity()`. Nos shaders ópticos ele se chama `u_InheritedOpacity`. O valor guardado no material não é alterado.
- Offset 88: resolução física do viewport; na cópia enviada, o sinal de Y indica `QRhi::isYUpInFramebuffer()`. H, K e E usam o módulo da resolução e corrigem a orientação vertical quando necessário.

Isso preserva a ABI binária, mas documenta uma mudança de semântica no upload desses slots. Novos shaders V2 devem respeitar esse contrato; não devem interpretar o slot 124 do material de vidro como altura de título.

## Menisco e reflexão

O perfil senoidal tinha valor zero no contorno, máximo 12 unidades para dentro e zero em 24. Foi substituído por `1-smoothstep(0,24,physicalDepth)`: máximo na borda e decaimento suave até zero.

A profundidade usada pelo menisco é `max(-dist,0)/max(length(grad),0.05)`. Trata-se de correção euclidiana local de primeira ordem para o campo L4, não de uma distância exata global. Não muda a silhueta nem a avaliação do antialiasing. As unidades são pixels lógicos do item; o DPR converte essa largura para pixels físicos de tela.

A reflexão direcional usa um termo amplo linear, com contribuição perimetral base e modulação angular limitada. Mantém as cores e a fase iridescente existentes. O limite entre alinhamento máximo e direção oposta passa a ser aproximadamente 1,75 no pico do perfil, em vez de 10,6. Não foi usado um substituto estático: normais, menisco e dispersão continuam sendo calculados em shader.

## Diagnósticos I/J

- I usa o mesmo limiar de ativação e ganho 0,10 de H; intensidade zero gera RGBA zero. Foram removidos intensidade mínima 0,15, ganho 0,55 e piso de alpha.
- J usa o mesmo perfil e energia de H, incluindo a atenuação pelo contorno especular que cobre a reflexão em H.
- Ambos aplicam cobertura e opacidade herdada ao RGBA pré-multiplicado.
- O alpha representa a energia do efeito isolado: sobre preto o RGB exibe essa energia; sobre um fundo arbitrário ocorre source-over, com atenuação do destino. Portanto, sobrepor I/J a K não é uma decomposição aditiva exata de H. Para medir contribuição aditiva, comparar o RGB sobre preto, antes de saturação/clipping de H.
- A separação entre medição isolada e composição real é feita pelo fundo de teste; não foram adicionadas propriedades ou modos QML.

## Backdrop: defeito reproduzido e limites

Um backdrop assimétrico, vermelho em cima e azul embaixo, foi amostrado invertido em OpenGL tanto no perfil Performance quanto Quality. A correção de orientação usa a convenção real do framebuffer informada por QRhi. Vulkan/Wayland passou com a mesma implementação, sem inversão adicional.

A regressão usa ShaderEffectSource dinâmico, superfície deslocada do canto da janela e pipeline GPU Quality real. Após o primeiro blur, troca o topo para verde e move a superfície. A textura atualiza e as posições permanecem corretas. Não foi reproduzida falha de dirty neste caminho. **ValoBackdropProviderV2.cpp e o agendamento QRhi não foram alterados.** O retorno antecipado por dirty continua existindo; isso não equivale a prometer suporte a provedores que mudem silenciosamente sem sinalização.

Não há nova garantia para fontes em atlas, sourceRect arbitrário, transformações de fonte ou render targets intermediários. Esses domínios não fazem parte da regressão atual e requerem validação específica antes de qualquer correção adicional.

## Arquivos alterados

- `shaders/v2/valo_optical_v2.frag`: composição, alpha, perfil, reflexão e orientação UV.
- `shaders/v2/valo_diag_basic.frag`: K acompanha composição, perfil e UV de H.
- `shaders/v2/valo_diag_caustics.frag`: energia e alpha de I.
- `shaders/v2/valo_diag_reflection.frag`: perfil, energia e alpha de J.
- `shaders/v2/valo_diag_blur.frag`: orientação de E acompanha H/K.
- `shaders/v2/valo_optical_v2.vert`: nome do slot de opacidade no bloco GLSL.
- `src/gfx/v2/ValoGlassMaterialV2.cpp`: upload separado de opacidade e orientação.
- `src/gfx/v2/SystaliaSurfacePropsV2.hpp`: comentários do contrato interno, sem mudança de layout.
- `tests/unit/tst_ValoOpticalContractV2.cpp`: nova regressão de pixels usando QSBs reais e backdrop conhecido.
- `tests/unit/tst_ValoGlassRenderingV2.cpp`: borda passa a ser verificada por contraste RGB e cobertura, sem exigir alpha maior que o preenchimento resolvido.
- `tests/CMakeLists.txt`: registro do novo teste com backend RHI explícito.
- Este relatório.

Compositor Wayland/XWayland, renderer V1, fallback, tokens, layout/funcionalidades QML e implementação Dual-Kawase não foram editados. Não houve merge, commit automático ou início da Etapa 06.

## Validação executada

Build: `cmake --build build -j 4`, incluindo geração dos QSBs, concluído com sucesso.

| Execução final | Resultado |
|---|---|
| `ctest --test-dir build --output-on-failure` com acesso aos recursos locais | **47/47 aprovados**; 46 existentes + nova regressão |
| Nova regressão, OpenGL/offscreen, DPR padrão | **27 casos aprovados** (incluindo init/cleanup) |
| Nova regressão, Vulkan/Wayland, DPR padrão | **27 casos aprovados** |
| Nova regressão, OpenGL/Wayland, `QT_SCALE_FACTOR=2` | **27 casos aprovados** |

A regressão verifica:

1. H/K com `surfaceOpacity` 0, 0,4 e 1 sobre destinos vermelho, preto e branco; cor esperada do backdrop/volume, sem influência indevida do destino.
2. Refração de uma linha branca em K: a linha aparece deslocada, sem permanecer na posição original.
3. Opacidade herdada 0 e 0,5 em H/I/J/K; I desativado com RGBA nulo em toda a imagem.
4. Alpha de I/J em fundos preto/branco e resposta linear à opacidade herdada.
5. Cortes laterais e diagonais L2/G2: decaimento, término da faixa, diferença de meia largura até 3 pixels lógicos e concentração angular limitada.
6. Contribuição RGB de I/J em relação a H/K em pontos sem saturação, sobre preto.
7. Orientação, atualização após primeiro blur e deslocamento de superfície nos perfis Performance/Quality.

Os primeiros testes dentro do sandbox falharam por acesso gráfico/sockets; a suíte completa foi repetida fora dessa restrição. Sem `QT_QUICK_BACKEND=rhi`, o offscreen podia não desenhar o material: o novo teste exige RHI para evitar falso positivo.

Limitações ambientais observadas:

- Vulkan/offscreen: plugin não suporta `createPlatformVulkanInstance`; houve falha de inicialização. Vulkan foi testado com sucesso via Wayland.
- OpenGL/offscreen com DPR 2: leituras de framebuffer falharam. A mesma regressão passou em Wayland com DPR 2; esse resultado offscreen permanece registrado como limitação não resolvida, sem alteração especulativa do renderer.
- O SDE principal foi invocado com `QT_QPA_PLATFORM=wayland timeout 15s build/systalia_glass_engine` fora do sandbox; o processo terminou pelo limite de tempo, sem log útil. Não foi possível observar sua janela por ferramenta de UI (inventário sem apps disponíveis). Isso **não comprova que o desktop tenha renderizado corretamente**.

Logs desta execução em `/tmp/valo52-build.log`, `/tmp/valo52-ctest-final.log`, `/tmp/valo52-vulkan-final.log`, `/tmp/valo52-dpr2-wayland.log` e `/tmp/valo52-sde.log`. São temporários, não evidências visuais homologadas.

## Homologação manual pendente no SDE principal

1. Compilar e executar `build/systalia_glass_engine` na sessão gráfica real; não usar o gerador de desktop como substituto. O main atual seleciona Vulkan explicitamente.
2. Na janela Files V2, verificar H e K sobre detalhes contrastantes do wallpaper, movendo e redimensionando a janela. Confirmar uma única imagem refratada, sem traço original residual ou salto de UV.
3. Usar os controles/propriedades existentes `debugPass` para H, I, J e K em uma sessão de depuração. I deve desaparecer com cáusticas desativadas; J deve acompanhar a reflexão de H. Para medir energia isolada, observar I/J sobre preto; para avaliar composição, restaurar o wallpaper.
4. Variar `surfaceOpacity` entre 0, 0,4 e 1: conferir transmissão/tonalização. Variar `opacity` do item/ancestral entre 0, 0,5 e 1: conferir fade uniforme e ausência total do material em zero. A sombra é um item separado; para isolar vidro, desativar `showShadow`.
5. Inspecionar o decaimento nas laterais e nos cantos G2, incluindo raios pequenos e slant, e confirmar equilíbrio/iridescência em movimento. A correção local de distância ainda exige julgamento visual nesses extremos.
6. Alterar o wallpaper ou acionar o parallax após o primeiro blur; verificar atualização e alinhamento em DPR 1 e 2.
7. Comparar com OpenGL numa execução de depuração que selecione esse backend. Não assumir que somente `QSG_RHI_BACKEND=opengl` substitui o `setGraphicsApi(Vulkan)` do main. Não modificar o compositor para essa comparação.

Nenhuma captura artificial foi usada como prova de fidelidade. Os padrões procedurais dos testes são entradas controladas para medições de pixel. A etapa permanece **PARTIAL** até a avaliação visual no SDE real.
