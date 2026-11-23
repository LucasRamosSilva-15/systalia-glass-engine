# SYSTALIA — VALO Graphics 2.0 — Etapa 05.3.1

Data: 08/11/2026. Branch: `valo-graphics-2.0`.

**Status: PARTIAL para homologação manual.** Correções implementadas e verificadas no executável principal real, incluindo stress de perfis e observação de 60 segundos em Vulkan. A interação manual externa com a sessão não pôde ser realizada: a ferramenta de UI não disponibiliza aplicativos. Não se declara homologação manual completa nem se inicia a Etapa 06.

## 1. Crash de perfil: reprodução, causa e correção

A regressão carrega o `main.qml` da shell com Files, Material e Dock abertas e alterna Performance/Balanced/Quality. Antes da correção, GDB capturou SIGSEGV em `QSGRenderThread`:

```
#0  0x0000000000000000 in ??? ()
#1  ValoGlassMaterialShaderV2::updateSampledImage(...)
#2  QSGBatchRenderer::Renderer::updateMaterialDynamicData(...)
#3  QSGBatchRenderer::Renderer::prepareRenderUnmergedBatch(...)
#4  QSGBatchRenderer::Renderer::prepareRenderPass(...)
```

Backtrace completo: `artifacts/graphics-v2-stage-05-3-1/crash-before-backtrace.txt`. A reprodução inicial sob debugger usa o teste da shell real; não foi uma captura do binário principal anterior à correção. O binário principal corrigido foi executado e testado separadamente, conforme seção 5.

Sequência causal:

1. Durante sincronização, `blurredTexture()` retorna um wrapper `QSGTexture` ao material.
2. Em `beforeRendering`, `ValoGpuBlurPipelineV2::prepare()` realoca a pirâmide quando perfil/tamanho muda.
3. O provedor destruía o wrapper anterior e criava outro, mas o material do frame corrente ainda guardava o primeiro endereço.
4. Ao preparar os samplers, o renderer chamava um método virtual nesse objeto já destruído.

A correção mantém **identidade estável do wrapper** durante mudanças de perfil/resize. Apenas sua referência à textura QRhi de saída é atualizada. O wrapper não possui a textura QRhi: a pirâmide continua pertencendo ao pipeline. A reconstrução da pirâmide permanece na thread de renderização em `beforeRendering`, fora de passes ativos; não foi movida para setters QML/GUI nem para `updatePaintNode`.

A disponibilidade de command buffer agora é verificada **antes** de preparar/reconstruir recursos. Antes de uma possível realocação, o wrapper aponta provisoriamente para a fonte válida; se a alocação falhar, não fica apontando para uma textura da pirâmide destruída.

O perfil é copiado durante sincronização para uso no frame, e dirty é atômico. A liberação pelo destrutor GUI é encaminhada à thread gráfica com `scheduleRenderJob(AfterRenderingStage)` quando o Scene Graph continua vivo. A invalidação do Scene Graph libera diretamente na thread proprietária e rearma dirty para a próxima inicialização.

`ValoGpuBlurPipelineV2` foi inspecionado: `prepare()` reconstrói recursos por tamanho/perfil/RHI e `execute()` grava Dual-Kawase no frame ativo. A causa reproduzida estava no wrapper do provedor; o algoritmo e a implementação do pipeline não foram modificados. Os mecanismos de liberação nativa do QRhi continuam sendo usados; não foi introduzido waitIdle, blur CPU ou recriação na GUI.

## 2. Seletor discreto

O slider de perfil foi substituído por três botões na mesma área do painel:

**Performance (0) | Balanced (1) | Quality (2)**

`ButtonGroup` exclusivo; checked acompanha o inteiro `blurProfile`. Os cinco sliders restantes mantêm propriedades e funções. Não há valor contínuo ou mapeamento de porcentagem para perfil.

Balanced é o padrão. Não foi encontrado armazenamento persistido de preferência de perfil na shell atual; não foi criado um novo serviço de persistência. O seletor encaminha o perfil a Material, Files e Dock pelos vínculos existentes. TopBar e o painel de sistema independente mantêm seus próprios perfis; isso não é deriva espontânea.

## 3. Aparência em repouso

A causa identificada no código era a distinção incorreta entre congelar e remover cáusticas. `onIdleTimeout()` para o relógio após cinco segundos, mas `updatePaintNode()` também zerava `effectiveCaustics` quando `m_causticFrozen` era verdadeiro. A iluminação desaparecia no timeout, embora os sliders, opacidade e perfil não mudassem.

A correção conserva intensidade e fase das cáusticas congeladas. Redução de movimento e política de energia continuam podendo suprimi-las explicitamente. Não foram removidas animações previstas, o congelamento em idle ou o contexto Adaptive.

A regressão de pixel `idleFreezePreservesCausticEnergy` força o timeout no item real, exige energia não nula no passe I e verifica que reduced motion ainda zera sua contribuição. Essa asserção detecta o comportamento anterior de desligar a luz.

Execuções preliminares sem bloqueio de entrada mostraram divergências e uma delas registrou movimento da Material Window (x=810 no início, cerca de x=881 depois). Elas não atendem à condição sem interação e foram preservadas em `concurrent-observation/` e `interrupted-by-input/`. Não é possível atribuir cada diferença dessas execuções ao renderer apenas pelos valores dos sliders. A medição qualificável foi repetida sem testes gráficos concorrentes e com entrada bloqueada somente na janela de observação. As regiões de pixel da comparação final estão explicitadas em `pixel-comparison.json`; as médias da tabela usam as regiões proporcionais da sonda, não necessariamente os mesmos retângulos. O SDE normal e os testes de botões não bloqueiam entrada.

O executável `build/systalia_glass_engine` foi medido aos tempos nominais 0, 10, 30 e 60 s, sem alterações de propriedades/input durante a sequência. t=0 ocorre após um segundo de acomodação inicial. Capturas PNG vêm de `QQuickWindow::grabWindow()` do aplicativo principal renderizado, não de um gerador de desktop ou reconstrução artificial.

| Tempo | Files | Material | Dock | Cáusticas congeladas |
|---|---:|---:|---:|---|
| 0 s | 0.146474 | 0.282236 | 0.212847 | Não |
| 10 s | 0.119357 | 0.277951 | 0.204746 | Sim |
| 30 s | 0.119357 | 0.277951 | 0.204746 | Sim |
| 60 s | 0.119357 | 0.277951 | 0.204746 | Sim |

Luminância média de RGB codificado em sRGB, em escala 0–1, nas faixas inferiores internas definidas pela sonda; não é uma medida fotométrica em nits.

As mudanças de 0 até o congelamento correspondem à fase animada inicial; as amostras não afirmam ausência de variação nos primeiros cinco segundos. Entre 10/30/60 s, as regiões comparadas de Files, Material, Dock e wallpaper não têm pixels diferentes. Relógio e telemetria do header não entram nessa comparação.

Nas três superfícies controladas, opacity/effectiveOpacity ficaram em 0,85/0,85 (Files e Material) e 0,34/0,34 (Dock); `itemOpacity=1`, ambient=0,35, adaptiveOffset=0, energy=1. A fase exata e a exposição da janela também constam nos JSONs; na medição sem input, fase congelada e posições das janelas permanecem constantes após o timeout. Perfil 1 e dirty=false permaneceram constantes nas amostras. A identidade e as dimensões 1662×975 da textura de backdrop foram registradas no ponto de sincronização e permaneceram constantes. A região de wallpaper observada também permaneceu idêntica. Isso não é uma garantia sobre todo provedor arbitrário sem sinalização de mudança.

O painel de sistema oculto aparece com dirty=true nos JSONs, por não ter sido renderizado; ele não é evidência de deriva das janelas abertas. O teste não separa I/J em quatro capturas adicionais a cada tempo: isso mudaria a cena. A preservação da contribuição de cáusticas é coberta pela regressão de pixel; reflexos e demais parcelas não têm dependência temporal além das já existentes.

## 4. Decorações Wayland permanecem V1 — análise para Etapa 06

Confirmado em `src/qml/WaylandWindowContainer.qml:478`: o chassi usa `ValoGlass`, com `clientTextureSource: clientTextureCapture`, raios, título, escala, metadados ópticos e relógio legados.

`ShellSurfaceItem` permanece em dois pontos (popups em torno da linha 420; superfície principal em torno da linha 511). A superfície principal fica em `clientViewport`, com geometria do cliente, clipping, foco, input, shellSurface/surface e movimentação associados ao container.

Antes de uma futura migração, será necessário preservar esse contrato de buffers/composição do cliente, premultiplicação, escalonamento/DPR, viewport, popups, subsurfaces, clipping, foco, input, resize e decorações/snap. `ValoMaterialSurfaceV2` não é uma substituição cega de `ValoGlass` nesse caminho: captura de cliente e decoração precisam ser avaliadas juntas. **Nenhum desses arquivos ou buffers foi migrado nesta etapa.**

## 5. Testes e evidências

- Build C++/QML e relink: aprovado (`cmake --build build -j 4`). Shaders/UBOs não foram alterados.
- Suíte completa: **47/47 CTest aprovados**; as regressões foram adicionadas a executáveis existentes.
- GDB antes da correção: crash reproduzido e backtrace arquivado.
- GDB após correção: sequência de trocas na shell real concluída sem SIGSEGV.
- Regressão dirigida Vulkan/Wayland: 72 cliques alternando os três botões, seleção exclusiva, resize alternado e hide/releaseResources/show, com frames válidos; aprovada.
- Regressão dirigida OpenGL/Wayland: mesmo cenário aprovado. A comparação OpenGL usa o teste que carrega a shell principal; o executável principal mantém sua seleção explícita de Vulkan.
- **Aplicativo principal real / Vulkan:** 72 mudanças de perfil com um frame válido capturado em cada troca; retorno 0. `stress.json` registra 72 transições. As três superfícies permanecem abertas; capturas de cada perfil arquivadas.
- **Aplicativo principal real / Vulkan:** sequência de 60 segundos sem interação, PNG/JSON nos quatro tempos; retorno 0.

As interações repetidas dos testes são eventos de mouse nos botões reais. O stress do executável usa uma sonda opt-in que altera a configuração da janela no event loop do aplicativo; não se apresenta isso como clique manual humano. A inspeção visual das capturas reais foi realizada, mas a ferramenta de UI retornou inventário sem apps. Permanece pendente a confirmação manual externa das trocas e do repouso no ambiente habitual do usuário.

Evidências em `artifacts/graphics-v2-stage-05-3-1/`: `sde-{0,10,30,60}.png`, `state-*.json`, `profile-{0,1,2}.png`, `stress.json`, comparação de pixels, backtraces e logs CTest/Vulkan/OpenGL. Não são prova de fidelidade ao Figma ou de estabilidade em todo hardware/driver.

## 6. Reprodução no aplicativo principal

A sonda é inerte sem `SGE_VALO_STABILITY_PROBE_DIR`. Foi adicionada ao executável principal para permitir medição reproduzível, sem alterar seus serviços ou substituí-lo por outro desktop.

Na sessão gráfica normal, com o socket do compositor disponível:

```bash
SGE_VALO_STABILITY_PROBE_DIR=/tmp/valo531-observation build/systalia_glass_engine
```

Isso bloqueia input apenas nessa janela de observação, registra os quatro tempos e encerra após 60 s. Para stress:

```bash
SGE_VALO_STABILITY_PROBE_DIR=/tmp/valo531-stress SGE_VALO_PROFILE_STRESS=12 build/systalia_glass_engine
```

Para execução manual, não definir essas variáveis: abrir Files e Material, usar Performance/Balanced/Quality e aguardar 60 s sem interação. Confirmar retorno ao perfil escolhido, seleção exclusiva e ausência de crash, troca abrupta de luz ou alteração de opacidade.

Nesta sessão, a instância habitual já ocupava o socket padrão. As execuções de evidência usaram runtime privado e D-Bus isolado, conectadas ao Wayland hospedeiro por caminho absoluto, sem encerrar outro compositor:

```bash
runtime=$(mktemp -d /tmp/valo531-runtime.XXXXXX)
XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=/run/user/1000/wayland-1 \
QT_QPA_PLATFORM=wayland QT_QPA_PLATFORMTHEME=generic QT_NO_XDG_DESKTOP_PORTAL=1 \
SGE_VALO_STABILITY_PROBE_DIR=/tmp/valo531-observation \
dbus-run-session -- build/systalia_glass_engine
```

O caminho de Wayland é específico desta sessão; adaptar ao ambiente real. Tema de plataforma genérico e bypass do portal foram usados apenas no ambiente de teste para evitar bloqueios de inicialização do portal; não alteram os tokens QML ou o renderer.

## 7. Arquivos e restrições

Alterados: `ValoBackdropProviderV2.cpp/.hpp` (wrapper, snapshot de perfil, dirty e liberação), `ValoGlassItemV2.cpp/.hpp` (idle e leitura diagnóstica do tempo), `SystaliaMaterialWindow.qml` (seletor), `src/main.cpp` e novo `src/diagnostics/ValoStabilityProbe.hpp` (sonda opt-in), testes `tst_ShellStateRegression.cpp` e `tst_ValoOpticalContractV2.cpp`, relatório e evidências.

Preservados: UBOs V1/V2, 124 tokens, serviços, compositor Wayland/XWayland, renderer V1, implementação Dual-Kawase, decorações de clientes e buffers. Sem merge automático. A Etapa 06 permanece não iniciada.
