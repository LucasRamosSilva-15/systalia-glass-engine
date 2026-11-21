# Consolidação 06.2 — Files e Control Center

Base `8db660b`, branch `valo-graphics-2.0`, árvore inicial limpa. Os ADRs 0001 e 0002 orientam a manutenção dos nomes e dos recursos QML durante a migração.

| Família | Contrato auditado | Resultado |
|---|---|---|
| Files | `controller`, `waylandServer`, estado de janela, snap, geometria de restauração, seleção, busca, título e ações de minimizar/maximizar/fechar. | O nome antigo herda a implementação V2, que usa o mesmo cabeçalho compartilhado e encaminha ações ao controlador. O controlador da shell continua responsável pelas ações de resize e snap externas ao componente. |
| Files óptico | O antigo `ValoGlass` recebia IOR, dispersão, tempo, resolução e raios. `caustics` e `surfaceOpacity` tinham valores públicos próprios, mas não eram ligados ao chassis V1. | O adaptador conserva os valores públicos, deriva IOR e dispersão pela escala Balanced antiga e usa a geometria e a política de cáusticas V2. O controle de opacidade usa a semântica V2 de transmissão/tingimento. Tempo e resolução externos continuam expostos, com cálculo interno V2. Não há alias público antigo para o chassi óptico nos dois componentes. |
| Control Center | `opened`, `open()`, `close()`, `toggle()`, Escape, clique externo, confirmação de energia e `operationError`. Serviços de Wi-Fi, Bluetooth, volume, brilho e silencioso. | O adaptador herda as propriedades e handlers V2. O layout e os controles continuam na implementação V2. O perfil Balanced do adaptador usa o blur V2, cujo resultado não equivale ao acabamento V1. |
| Entrada e acessibilidade | O Files usa edição de texto, ações compartilhadas de cabeçalho e handlers de seleção por ponteiro. O painel usa botões Qt Quick Controls, sliders com handlers e Escape. | A migração reutiliza os mesmos controles V2 da shell. A regressão cobre entrada de texto, foco, clique de fechamento, abertura do painel, modo silencioso e Escape. A auditoria não comprova paridade com tecnologias assistivas nem cada ação de energia e hardware. |
| Referências QML | A shell principal usa `SystaliaFilesWindowV2` e `SystaliaControlCenterV2`. Os geradores históricos e os testes V2 carregam os nomes V2. A busca em CMake, QML, ferramentas e testes não encontrou outro chamador operacional interno pelos nomes antigos antes da migração. | Os quatro tipos continuam em `qt_add_qml_module`. Os imports relativos e as URLs QRC dos consumidores existentes continuam disponíveis. Consumidores dinâmicos externos e dependências de filhos privados ficam fora da comprovação. |

## Inventário

- Migrados no lugar: `src/qml/shell/SystaliaFilesWindow.qml` e `src/qml/shell/SystaliaControlCenter.qml`. Cada um delega ao tipo V2 correspondente sem criar outro chassi.
- Ajustado: `src/qml/shell/SystaliaFilesWindowV2.qml` expõe a superfície óptica por alias somente leitura para o adaptador.
- Validação: `tests/unit/tst_ValoShellV2.cpp` e `src/diagnostics/ValoStabilityProbe.hpp`.
- Preservados: entradas CMake e QRC, UI V2 do Control Center, `decorationV1`, `ValoGlass.qml`, renderer C++ V1, shaders antigos, contratos std140, `SystaliaTheme.qml` e protocolo Wayland.
- Arquivos de produto removidos: nenhum.

`SystaliaControlCenter` aqui é o painel de serviços do sistema. `SystaliaMaterialWindow` é a janela de configuração óptica e fica fora desta migração.

## Evidências e classificação

**PARTIAL.** Os dois nomes legados têm uma única implementação visual por família. A paridade do acabamento com o renderer anterior, a semântica integral de parâmetros ópticos públicos e a homologação com consumidores externos ainda não foram demonstradas.

- Builds limpos com `-Werror` em [Debug](evidence-06-2/Debug-build.log) e [Release](evidence-06-2/Release-build.log). CTest completo passou com 47/47 em [Debug](evidence-06-2/Debug-ctest.log) e 47/47 em [Release](evidence-06-2/Release-ctest.log).
- A [regressão QRC](evidence-06-2/qrc.log) encontrou exatamente um item óptico V2 e zero itens V1 por adaptador. Exercitou foco e digitação na busca, fechamento por clique, abertura do painel, clique que atualiza `SystemControl.doNotDisturb` e Escape. Os testes usam o fallback de backdrop sintético porque a fixture não fornece uma fonte de cena. A evidência visual vem do SDE real.
- No SDE Release, a sonda abriu o painel e carregou dinamicamente os adaptadores com Alacritty Wayland mapeado. Comparações na mesma sessão: Files [nome V2](evidence-06-2/11-files-v2.png) e [nome legado](evidence-06-2/13-files-legacy-name.png). Control Center [nome V2](evidence-06-2/12-control-center-v2.png) e [nome legado](evidence-06-2/14-control-center-legacy-name.png). As instâncias legadas usam os defaults dos adaptadores e papel de parede como backdrop. Assim, a comparação demonstra montagem e apresentação das duas entradas, sem equivalência pixel a pixel com a implementação anterior.
- A [sonda](evidence-06-2/render.log) terminou com código 0. Ela executa o modo `client-interactions` de `tools/validate_valo_stage064.py` com `SGE_VALO_FILES_CONTROL_PROBE=1`, apontando o script ao binário Release e a um diretório isolado. A rodada visual usou Alacritty Wayland. XWayland real, múltiplos outputs e ações dependentes de hardware ou privilégios ficam fora dessa homologação.
