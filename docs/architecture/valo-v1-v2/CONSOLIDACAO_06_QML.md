# Consolidação 06 — Aposentadoria controlada do QML V1

**Estado: PARTIAL — preparação verificada, remoções ainda não autorizadas.**
**Base:** `valo-graphics-2.0` em `82b49ba` (2026-11-21).
**Escopo:** somente os componentes QML V1 cuja substituição pela shell V2 pode ser demonstrada. Não retirar o renderizador C++ V1, shaders legados, `ValoGlass.qml`, `decorationV1`, `SystaliaTheme`, `ShellSurfaceItem`, contratos std140 nem o protocolo `z_systalia_surface_v1.xml` nesta fase.

## Evidência já disponível

- `main.qml` instancia `SystaliaDockV2`, `SystaliaTopBarV2`, `SystaliaFilesWindowV2` e `SystaliaControlCenterV2`; isso demonstra o caminho principal, **não** ausência de consumidores dos nomes V1.
- `CONSUMIDORES_V1.md` identifica `SystaliaDock.qml`, `SystaliaFilesWindow.qml`, `SystaliaTopBar.qml` e `SystaliaControlCenter.qml` ainda empacotados pelo módulo `Systalia.Engine 1.0`.
- `WaylandWindowContainer.qml` conserva fallback `decorationV1` e `useGraphicsV2`.
- O baseline atualizado tem Debug/Release com **47/47 CTest** e Alacritty Wayland visível em ambas as decorações; XWayland real ainda sem validação.

## Matriz de aposentadoria

| Família | V1 no módulo | Implementação ativa V2 | Estado | Condição de retirada |
|---|---|---|---|---|
| Dock | `src/qml/shell/SystaliaDock.qml` | `SystaliaDockV2.qml` | PENDENTE | Buscar instâncias QML estáticas/dinâmicas, testes, ferramentas, QRC e consumidores externos; decidir adaptador |
| TopBar | `src/qml/shell/SystaliaTopBar.qml` | `SystaliaTopBarV2.qml` | PENDENTE | Mesma auditoria; verificar sinais e telemetria |
| Files | `src/qml/shell/SystaliaFilesWindow.qml` | `SystaliaFilesWindowV2.qml` | ADAPTADOR V2 | Paridade visual do antigo chassis e semântica óptica externa pendentes |
| Control Center | `src/qml/shell/SystaliaControlCenter.qml` | `SystaliaControlCenterV2.qml` | ADAPTADOR V2 | Homologação completa dos serviços e da acessibilidade pendente |
| Componentes compartilhados | `ValoGlass.qml`, `SystaliaG2Frame.qml`, `SystaliaWindowHeader.qml` | Sem equivalência integral demonstrada | PRESERVAR | Não excluir por associação com V1 |
| Decoração de clientes | `decorationV1` | `decorationV2` | PRESERVAR | Exige fase própria de compatibilidade e clientes reais |

## Execução por família

1. Rastrear todas as referências por caminho e símbolo em `src`, `tests`, `tools`, `cmake`, `CMakeLists.txt` e scripts; procurar `Qt.createComponent`, `Loader.source`, `Loader.sourceComponent`, aliases `qrc`, `QQmlComponent` e URLs construídas dinamicamente. A busca textual não comprova inexistência de clientes externos.
2. Inventariar propriedades, sinais, métodos, tipos QML públicos e identificadores de recurso do componente V1. Comparar com o substituto V2.
3. Para cada família escolher e justificar: **(A)** manter como adaptador leve V2; **(B)** deprecar mantendo compatibilidade; **(C)** remover após prova de ausência de consumidores e decisão explícita de quebra de API. Evitar duplicar o renderizador dentro de um adaptador.
4. Quando pronto, efetuar apenas uma família por commit. Atualizar `qt_add_qml_module(QML_FILES)`, imports, referências QRC, testes e documentação.
5. Verificar build limpa Debug e Release com `-Werror`; CTest completo (47 testes de referência), instalação em prefixo temporário; cliente Wayland real e screenshots da shell para identificar regressões. Registrar limitações sem declarar XWayland homologado.
6. Atualizar `CONSUMIDORES_V1.md`, `PLANO_E_INVENTARIO.md` e ADRs quando necessário. Registrar status, evidências, arquivos removidos/migrados e commit.

## Critérios de bloqueio

**Não retirar um componente** se existir referência ativa, contrato público não resolvido, risco de quebra de QML dinâmico, regressão de renderização, ou impossibilidade de validar as alterações. A compilação bem-sucedida, isoladamente, não autoriza a exclusão.

## Resultado deste primeiro commit

Esta alteração cria exclusivamente o roteiro de execução e a matriz por família. **Nenhum componente de produção foi modificado; nenhum teste, build ou sessão gráfica foi executado neste commit.** A fase continua PARTIAL.

## Continuação 06.1

Dock e TopBar agora delegam aos componentes V2 pelos mesmos nomes QML e URLs QRC. A etapa permanece **PARTIAL** pela compatibilidade profunda da Dock antiga. O relatório registra a auditoria, os resultados de 47/47 em ambas as configurações e as capturas com lançamentos Wayland reais. [Consolidação 06.1](CONSOLIDACAO_06_1_DOCK_TOPBAR.md)

## Continuação 06.2

Files e Control Center usam adaptadores V2 pelos nomes anteriores. [Relatório e evidências](CONSOLIDACAO_06_2_FILES_CONTROLCENTER.md)
