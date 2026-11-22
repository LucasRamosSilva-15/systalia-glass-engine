# SYSTALIA — VALO Graphics 2.0 — Etapa 05.3

Data: 08/11/2026. Branch: `valo-graphics-2.0`.

**Status: PARTIAL — migração e regressões concluídas; homologação visual no executável principal pendente.** Não foi iniciada a Etapa 06.

## Migração do chassi

O único `ValoGlass` V1 de `SystaliaMaterialWindow.qml` foi substituído por `ValoMaterialSurfaceV2`, com Squircle G2, provider de backdrop, Dual-Kawase GPU, refração, dispersão, cáusticas, reflexos e sombra V2. O tint, o stroke padrão, o offset e a intensidade de sombra seguem os parâmetros do chassi Files V2.

O chassi usa `z: -1`, permanecendo atrás dos controles existentes. Não foi adicionado outro preenchimento, borda ou sombra de janela. As decorações locais de botões, painéis, seleção e sliders continuam existindo: não são chassis sobrepostos.

Foram preservados tamanho, posições, âncoras, organização espacial, título, navegação, header, DragHandler, handlers de minimizar/maximizar/fechar, controller, snapState e cálculo de raios fundidos. As propriedades legadas `screenResolution` e `animationTime` continuam disponíveis; V2 obtém viewport e tempo pelo caminho existente do renderer, sem adaptar indevidamente o relógio V1.

O chassi recebe diretamente as propriedades ópticas da janela. `materialActive=false` seleciona transmissão sem tonalização, IOR 1, dispersão/cáusticas zero e perfil sem blur; mantém o acabamento geométrico, assim como a superfície Files existente.

## Vínculos corrigidos

O slider denominado “Backdrop Blur Filter” escrevia em `refractiveIndex`, com uma porcentagem calculada a partir do IOR. Dois sliders tinham apenas valores locais constantes, sem handlers que alterassem o material. Além disso, Files e Dock recebiam algumas propriedades públicas na shell, mas elas não eram encaminhadas até os respectivos itens ópticos V2.

| Posição preservada | Controle final | Origem/efeito |
|---|---|---|
| 1 | Glass Refraction · IOR | `refractiveIndex`, faixa existente 1–1,5; exibição numérica do IOR |
| 2 | Glass Layer Opacity | `surfaceOpacity`, faixa existente 0,2–1; tonalização óptica segundo o contrato 05.2 |
| 3 | Backdrop Blur Filter | antes “Opacity · Secondary”, valor local 0,75; agora `blurAmount`, com os perfis GPU existentes |
| 4 | Chromatic Dispersion | `dispersion`, faixa existente 0–0,08, exibição numérica |
| 5 | Caustic Intensity | `caustics`, faixa existente 0–1,5; exibe intensidade efetiva limitada a 1 pelo renderer |
| 6 | Chromatic Dispersion · Strength | antes valor local 1,0; agora `dispersionStrength`, multiplicador da dispersão |

Os valores iniciais foram mantidos: IOR 1,25; opacity 0,85; dispersion 0,04; caustics 0,85; blur 0,75; strength 1,0. Nenhum valor armazenado foi reescrito para “recalibrar” o usuário. Valores de cáusticas acima de 1 continuam armazenados; a saturação no item C++ já existia.

`effectiveDispersion = dispersion * dispersionStrength`. Blur não altera IOR nem dispersão. O valor contínuo de blur é mantido, mas seleciona os três perfis já suportados: `[0,2; 0,4667)` Off/Performance, `[0,4667; 0,7333)` Balanced, e `[0,7333; 1]` Quality. Isso não representa um raio de blur contínuo nem uma porcentagem física. O valor inicial 0,75 preserva Quality.

### Integração sem editar os componentes já migrados

`main.qml` encontra por tipo o primeiro `ValoGlassItemV2` descendente de Files e Dock e instala bindings de IOR, dispersão e cáusticas. Encaminha também slant para Files e aplica à Dock seu próprio valor de opacidade de tema e high contrast. O controle de opacidade da Material continua chegando a Files; não substitui a opacidade temática própria da Dock.

A descoberta por tipo evita depender da ordem numérica dos filhos. O contrato é que cada uma dessas árvores tenha um único chassi óptico principal. Se no futuro forem introduzidas superfícies V2 aninhadas nessas árvores, esse adaptador deverá ser revisado. A nova regressão exige alvos válidos e verifica os valores nos itens C++ reais.

O perfil de blur de Files acompanha o controle. Para Dock, a shell traduz o perfil para sua propriedade existente `performanceProfile`, mantendo a implementação de seleção de perfis do componente intacta. Não foram editados Files V2, Dock V2, ValoMaterialSurfaceV2 ou o renderer.

## Arquivos alterados

- `src/qml/shell/SystaliaMaterialWindow.qml`: troca exclusiva do chassi, vínculos e rótulos corretos dos seis controles; nomes de objetos para regressão e alias da superfície.
- `src/qml/main.qml`: adaptador de integração com os itens ópticos existentes e encaminhamento dos perfis.
- `tests/integration/tst_ShellStateRegression.cpp`: duas novas regressões dentro da suíte existente.
- Este relatório.

Sem alterações no compositor Wayland/XWayland, UBOs, 124 tokens, renderer V1, shaders, pipeline de blur ou componentes anteriormente migrados. Sem remoção de controles ou merge automático.

## Validação executada

- `cmake --build build -j 4`: aprovado, incluindo compilação/cache dos QML e relink do executável principal. Não houve mudança de shader nesta etapa.
- `ctest --test-dir build --output-on-failure`: **47/47 aprovados**; as novas regressões foram acrescentadas ao teste existente da shell.
- Teste dirigido com `QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=vulkan QSG_INFO=1 build/tests/tst_shell_state_regression materialWindowUsesOneV2ChassisAndLiveSettings materialWindowSliderInputAndWindowActions`: **4 aprovações, zero falhas**, incluindo init/cleanup. O log confirma `Creating QRhi with backend Vulkan`.

As regressões carregam o `main.qml` real da shell, com seus serviços, sem usar o gerador de desktop. Verificam:

1. Superfície G2 contendo um item de vidro V2, uma sombra V2 e nenhum item V1.
2. Os seis handlers de configuração e seus efeitos nos itens ópticos de Material, Files e Dock.
3. Blur selecionando 0/1/2 sem modificar o IOR.
4. Dispersão combinada com strength, intensidade de cáusticas e supressão pelo toggle de material.
5. Eventos de mouse nos seis sliders reais, não apenas atribuições em propriedades.
6. Ações do controller para maximizar/restaurar, reposicionar, elevar/focar, fechar e reabrir a Material Window.
7. As regressões anteriores de workspace, visibilidade e composição permanecem aprovadas.

Os testes verificam encaminhamento funcional e exercitam RHI/Vulkan; não comprovam julgamento estético ou resposta visual perceptível de cada slider. O movimento foi verificado por reposicionamento e integração do controller; o arrasto manual contínuo e a precisão visual de suas transições continuam pendentes.

Logs temporários: `/tmp/valo53-build.log`, `/tmp/valo53-ctest.log`, `/tmp/valo53-vulkan-shell.log`.

## Executável principal e comparação Figma

A primeira execução de `build/systalia_glass_engine` em Wayland abortou porque `wayland-systalia-0.lock` já estava ocupado. Não foi encerrado outro compositor nem removido seu lock. Foi feita uma segunda tentativa com `XDG_RUNTIME_DIR` privado e `WAYLAND_DISPLAY` absoluto, sem alterar código do compositor; a execução atingiu o timeout de 15 segundos, sem log útil. Isso não comprova que a janela tenha sido desenhada corretamente.

A ferramenta de UI retornou inventário sem aplicativos e host de browser indisponível. Portanto, não foi possível inspecionar manualmente o executável principal nem certificar foco/arrasto/acabamento por observação direta.

A referência oficial foi obtida pelo conector Figma, arquivo `kax6aiYKgM2Fk7dllyZRRF`, nó `2:2`:

https://www.figma.com/design/kax6aiYKgM2Fk7dllyZRRF/SYSTALIA-%E2%80%94-VALO-%7C-Desktop-Concept?node-id=2-2

Foi inspecionado o screenshot real do design (canvas 1662×975, visualização 1024×601). A referência contém título, navegação à esquerda, painel Options e seis sliders. A implementação preserva essa organização e as coordenadas existentes; os rótulos incorretos foram ajustados para expressar a função real. **Não houve comparação visual lado a lado com uma imagem observada do SDE atualizado.** Não é alegada fidelidade de pixels, brilho, cor ou acabamento ao Figma. Nenhuma captura artificial foi usada como evidência.

## Homologação manual pendente

1. Na sessão gráfica real, iniciar o executável principal em Vulkan, sem disputar o socket de outra instância; abrir “Systalia Control Center” pela ação Settings da Dock.
2. Comparar a Material Window com Files e com o nó Figma 2:2: silhueta, borda única, sombra única, acabamento, conteúdo e organização espacial.
3. Mover cada slider e observar Material/Files/Dock: IOR afeta refração; blur seleciona perfis; opacity controla tonalização; dispersion/strength afetam dispersão cromática; caustics altera cáusticas. Não esperar que o slider de opacity mude a opacidade temática independente da Dock.
4. Confirmar toggle, navegação, foco ao clicar/arrastar, movimento, snap, maximização/restauração, minimizar, fechar e reabrir.
5. Observar efeitos em movimento e atualização do wallpaper após alterações do perfil; verificar ausência de chassi ou sombra V1 residual.

A Etapa 05.3 encerra com implementação testada e status **PARTIAL**, aguardando essa homologação visual. Não iniciar a Etapa 06.
