# ETAPA 06.1 — Alinhamento dos cabeçalhos

Branch: `valo-graphics-2.0`. Data: 2026-11-09. Qt 6.11.2.

## Resultado e causa

**Implementação concluída; validação PARTIAL.** O alinhamento visual e a área útil foram confirmados no SDE principal Vulkan. A matriz manual completa de digitação, menus, subsurfaces e múltiplos monitores/DPR ainda não foi homologada.

O cabeçalho dos clientes passa a usar a mesma referência de **50 px** do Files V2. A diferença confirmada era geométrica: os mesmos elementos de `SystaliaWindowHeader` eram centralizados em alturas de 36 e 50 px, produzindo deslocamento vertical de **7 px** no título, ícone, badge e botões.

Não foi reproduzida diferença de família, peso, tamanho ou hinting entre os títulos. O SDE real resolveu ambos para **Inter Medium, 21 px, peso 500**, com `PreferDefaultHinting`. Não houve mudança global de fontes, ajuste individual de botões ou alteração da tipografia da shell.

O conteúdo preto do XWayland real permanece reservado à Etapa 07. Essa investigação não foi retomada.

## Implementação e arquivos

- `src/qml/shell/SystaliaWindowHeader.qml`: `implicitHeight: 50` como referência compartilhada; nomes de objetos para regressões e medição dos elementos existentes. Família, tamanho, peso e regras de truncamento preservados.
- `src/qml/shell/SystaliaFilesWindowV2.qml`: cabeçalho usa `implicitHeight`; recebe identificação para comparação automatizada. A altura efetiva continua 50 px.
- `src/qml/WaylandWindowContainer.qml`: `titleBarHeight` deriva da referência do cabeçalho; fallback de restauração deriva da altura útil de 500 px; altura mínima externa preserva o mínimo útil anterior de 168 px. Retirada a exceção `badgeWidth: 0`, herdando o mesmo mínimo de 120 px do Files.
- `tests/integration/tst_ShellStateRegression.cpp`: nova regressão de cabeçalho cliente versus Files, incluindo fonte, baseline, dimensões, margens, espaçamento e títulos longos.
- `tests/tst_XdgResize.cpp`: expectativas atualizadas para a moldura mais alta; afirmações explícitas sobre viewport e buffer útil preservados.
- `tests/integration/tst_XdgShellDecoration.cpp`: nova altura externa mínima com o mesmo mínimo útil anterior.
- `src/diagnostics/ValoHeaderAlignmentProbe.hpp`, `src/diagnostics/ValoStabilityProbe.hpp`: sonda opt-in no SDE principal para capturas, coordenadas e resolução real da fonte.
- `tools/validate_valo_header.py`: launcher reproduzível do SDE e Alacritty em runtime Wayland privado.

`SystaliaWindowAction.qml` foi inspecionado e preservado: controles de 28 × 28 px, estados, acessibilidade e ações continuam existentes. `ShellSurfaceItem`, apresentação direta, clipping e fórmulas de entrada/popups foram preservados. Não há captura do cliente, `hideSource`, nova camada gráfica ou readback por frame em produção.

## Coordenadas reais antes/depois

Coordenadas lógicas locais ao cabeçalho, obtidas no **SDE principal Vulkan**, com Files e Alacritty lado a lado e largura de 701 px para ambos:

| Medida | Files V2, antes/depois | Alacritty antes | Alacritty depois |
|---|---:|---:|---:|
| Altura do cabeçalho | 50 | 36 | 50 |
| Centro vertical dos botões | 25 | 18 | 25 |
| Topo dos botões | 11 | 4 | 11 |
| Topo/centro do ícone | 11 / 25 | 4 / 18 | 11 / 25 |
| Topo do título | 12 | 5 | 12 |
| Baseline do título | 32.34375 | 25.34375 | 32.34375 |
| Topo/altura do badge | 12 / 25 | 5 / 25 | 12 / 25 |
| Largura efetiva do badge | 138.484375 | 138.484375 | 138.484375 |
| Margem esquerda do ícone | 18 | 18 | 18 |
| Margem esquerda do título | 50 | 50 | 50 |
| Margem direita dos botões | 18.5 | 18.5 | 18.5 |
| Espaçamento entre botões | 10 | 10 | 10 |

O badge não tinha divergência efetiva nessa largura: o cálculo compartilhado já considerava a largura do texto. A remoção da exceção torna o contrato uniforme, sem alterar arbitrariamente sua largura. Seu centro em 24.5 px decorre do arredondamento de anchors de um elemento de altura ímpar e é idêntico nos dois cabeçalhos.

Os JSON registram família solicitada e resolvida por `QFontInfo`, estilo, peso, tamanho, hinting, ascent, baseline e coordenadas. O ascent dos títulos é 20.34375 px. O fontconfig também identifica `/usr/share/fonts/rsms-inter-fonts/Inter-Medium.ttf`; a confirmação de família/estilo no aplicativo foi feita com `QFontInfo`, sem substituir a fonte. O título longo mantém `Text.ElideRight` e o mesmo espaço reservado ao badge e aos controles.

## Área útil e operações geométricas

| Caso | Antes | Depois |
|---|---|---|
| Moldura para cliente 800 × 500 | 816 × 552 | 816 × 566 |
| Origem do viewport | (8,44) | (8,58) |
| Área útil desse cliente | 800 × 500 | 800 × 500 |
| Moldura mínima padrão | 320 × 220 | 320 × 234 |
| Área útil mínima padrão | 304 × 168 | 304 × 168 |

A relação continua `frameHeight = clientHeight + titleBarHeight + 2 × frameMargin`. Os 8 px entre o cabeçalho e a origem do cliente já existiam; não foi introduzida uma faixa adicional.

Resize, snap, unsnap, maximização, restauração, popup e migração de output já usam `titleBarHeight`. Foram auditadas essas dependências; a restauração tinha um fallback literal de 552 px que agora deriva da altura útil. Em maximização com área externa fixa, o cliente recebe configure com a nova área disponível e recompromete seu buffer, sem recorte de um buffer antigo.

Na execução real, Alacritty manteve viewport **685 × 400** ao alternar moldura normal e restauração. Na maximização, recebeu **1646 × 755**; no snap, **811 × 755**. Após dez pedidos consecutivos de resize lateral, a área útil passou para **735 × 400** e a altura do cabeçalho permaneceu 50.

## Evidências do aplicativo real

As capturas são de `build/systalia_glass_engine`, em Vulkan sobre a sessão Wayland real, com Alacritty nativo. Não foi utilizado gerador de desktop nem imagem sintetizada.

- [Antes, Files e Alacritty](evidence/etapa-06-1/before/01-aligned.png), [coordenadas antes](evidence/etapa-06-1/before/01-aligned.json).
- [Depois, Files e Alacritty](evidence/etapa-06-1/after/01-aligned.png), [coordenadas depois](evidence/etapa-06-1/after/01-aligned.json).
- [Título longo](evidence/etapa-06-1/after/02-long-title.png).
- [Foco e tentativa de tecla sintética](evidence/etapa-06-1/after/02-typing.png), [estado do foco](evidence/etapa-06-1/after/keyboard.json). A captura não confirmou eco da tecla; não é prova de digitação manual no SDE.
- [Maximização](evidence/etapa-06-1/after/03-maximized.png), [restauração](evidence/etapa-06-1/after/04-restored.png).
- [Snap](evidence/etapa-06-1/after/05-snap.png), [unsnap](evidence/etapa-06-1/after/06-unsnap.png), [resize](evidence/etapa-06-1/after/07-resized.png).

A sonda aplica operações no aplicativo real e mede sua árvore visual, incluindo containers dinâmicos. Não equivale a uma homologação manual completa de todos os gestos e dispositivos.

## Testes e limitações

O build completo foi aprovado ([log](evidence/etapa-06-1/build.log)). CTest completo: **47/47**, zero falhas, **23.42 s** ([log](evidence/etapa-06-1/ctest.log)). O teste de alinhamento novo passou; a primeira tentativa havia usado coordenada local ao pai do título no lugar da coordenada relativa ao cabeçalho, corrigida na própria asserção.

OpenGL sobre Wayland: **7 resultados aprovados**, incluindo init/cleanup, zero falhas/skip, **3.787 s** ([log](evidence/etapa-06-1/opengl.log)). O mesmo conjunto verifica pixels nativos inalterados durante 18 trocas de perfil e o repaint após teclado. As [interações do cliente e do cabeçalho](evidence/etapa-06-1/interaction-results.txt) incluem explicitamente popup, ponteiro, minimizar, maximizar, fechar e resize nas oito bordas.


A referência de 50 px é comum ao container Wayland/XWayland, mas o conteúdo real XWayland não foi homologado nesta etapa. Nenhuma tentativa de resolver seu conteúdo preto foi incorporada. UBOs, 124 tokens, compositor, renderer V1, pipeline GPU e efeitos V2 não foram modificados.

OpenGL é verificado no teste de apresentação de cliente/Scene Graph. O SDE principal mantém sua seleção Vulkan. A inspeção visual foi feita em DPR 1, em um monitor da sessão aninhada; múltiplos monitores físicos e outras escalas não foram homologados manualmente. Popups e entrada foram exercitados por integração com clientes reais; a matriz manual de menus/subsurfaces no SDE permanece uma limitação.

## Reprodução

```sh
cmake --build build -j 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j 2
python3 tools/validate_valo_header.py after
QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=opengl build/tests/tst_client_surface_visibility
```

O launcher requer uma sessão Wayland e Alacritty instalados. O cliente muda o título por OSC para testar truncamento. A sonda tenta uma tecla sem Enter; o estado do foco é registrado, mas essa tentativa não confirmou eco e não substitui digitação manual. Capturas e readbacks só existem com a sonda habilitada.

Não houve merge nem início da Etapa 07.
