# Consolidação 06.1 — Dock e TopBar

Base `0642672`, merge da Consolidação 06 na branch `valo-graphics-2.0`. A árvore inicial estava limpa.

| Contrato | Resultado da auditoria |
|---|---|
| QML e QRC | Os nomes antigos seguem registrados em `qt_add_qml_module` com os mesmos caminhos. `main.qml` usa os tipos V2, incluindo o delegate da TopBar secundária. A ferramenta de captura da shell e os testes anteriores referenciam V2. A busca por nomes e URLs em código, CMake, testes e ferramentas não identifica consumidores externos ou URLs montadas fora do repositório. |
| Dock | O adaptador herda sinais, integração com `ProcessLauncher`, consultas de apps e lançadores. Mantém perfil Balanced, valores públicos antigos e as escalas antigas de IOR e dispersão por perfil. `activate(action)`, dimensões dinâmicas e estado observável de teclado são compartilhados com V2. O componente de item V2 expõe os estados de hover e pressão usados no item anterior. |
| TopBar | O adaptador herda sinais, telemetria, relógio, ações acessíveis e workspace. O raio padrão antigo de 22 é aplicado somente ao nome antigo. A shell segue com os mesmos controles, serviços e raio da V2. |
| Compatibilidade profunda | Na Dock antiga, `valoGlassInstance` expunha um objeto `ValoGlass` com API própria, incluindo propriedades graváveis, filhos e `glassItem`. O adaptador expõe o contrato reduzido já existente na Dock V2. Chamadores que manipulam a instância antiga precisam ser identificados e migrados antes de declarar paridade. `screenResolution` e `animationTime` continuam expostos, mas a V2 calcula resolução a partir da janela e administra seu próprio relógio óptico. A aparência do nome antigo passa a seguir a geometria e os efeitos V2. |

As implementações visuais antigas nos dois arquivos foram substituídas por adaptadores sem chassis próprio. Nenhum caminho de produto foi removido. `ValoDockItem.qml` continua empacotado como tipo público separado. `ValoGlass.qml`, itens e material V1, shaders, tema de compatibilidade, decoração V1, std140 e protocolo Wayland permanecem disponíveis.

O teste de regressão instancia os adaptadores pelos caminhos QRC e verifica a quantidade de itens ópticos, sinais, foco, teclado e hover. O modo opt-in `SGE_VALO_SHELL_LAUNCH_PROBE` da sonda de clientes aciona o lançamento no SDE pelos caminhos de produção.

## Validação e limites

**PARTIAL.** As duas famílias têm uma implementação visual V2 e mantêm os nomes antigos. A compatibilidade integral de `valoGlassInstance` e a equivalência visual dos clientes externos do tipo legado ainda não foram demonstradas.

- Builds Debug e Release em diretórios inicialmente vazios, com `-Werror`, e recompilação das revisões finais. CTest manteve **47/47** em [Debug](evidence-06-1/Debug-ctest.log) e [Release](evidence-06-1/Release-ctest.log).
- O [teste QRC](evidence-06-1/focus.log) confirmou uma superfície óptica V2 e zero itens ópticos V1 por adaptador. Verificou foco, Tab, Return e hover na Dock, e foco e Space na TopBar. A primeira tentativa buscava os delegados pela hierarquia QObject e falhou. A versão final percorre `childItems()`.
- Comparação da shell ativa com Alacritty Wayland em 1662 × 975. A captura de referência usou o binário existente, cuja revisão exata não foi recompilada para esta comparação. A captura posterior usou o build Release da etapa. A diferença média absoluta RGB em escala 0–255 foi 0,033/0,154/0,210 na Dock e 2,818/2,568/2,366 na TopBar. Relógio, telemetria e animações não foram congelados. Esses frames mostram a shell ativa, que já usava os nomes V2. [Antes](evidence-06-1/sde-antes.png) · [Depois](evidence-06-1/sde-depois.png)
- O SDE real abriu um novo Alacritty Wayland pelo lançamento da [Dock](evidence-06-1/17-dock-terminal.png) e outro pela [TopBar](evidence-06-1/19-topbar-terminal.png). A sonda acionou o método da Dock e o sinal da TopBar. As interações de teclado e ponteiro foram verificadas no teste QRC. XWayland, múltiplos monitores físicos e clientes externos dos tipos legados ainda não foram homologados nesta etapa.
