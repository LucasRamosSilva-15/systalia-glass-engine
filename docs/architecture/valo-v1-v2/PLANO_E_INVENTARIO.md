# Auditoria e plano de consolidação VALO V1 → V2

**Estado: em andamento; V2 única ainda não demonstrada.** Referência de código: branch `valo-graphics-2.0`, commit `86d2d7e` (antes desta auditoria); árvore originalmente limpa. Data: 2026-11-17. Esta fase registra evidências e decisões antes de excluir ou mover implementações.

## Inventário antes

O Git registra 1.326 arquivos no baseline, dos quais 169 em `src`, 49 em `tests`, 18 em `tools`, 21 em `shaders`, um protocolo e dois arquivos em `data`; 696 estão em `docs` e 359 em `artifacts`. Esses números contam arquivos rastreados, não linhas de código, e incluem evidências, imagens e arquivos gerados versionados. O [inventário de caminhos de produto/testes/ferramentas](inventario-arquivos-baseline.txt) tem 261 entradas; não inclui documentação e evidências. [Referências de símbolos, recursos e caminhos V1](referencias-v1-baseline.txt) e [referências QML complementares](referencias-qml-complementares-baseline.txt) guardam caminho e linha no baseline. Elas são busca textual reproduzível, não prova de ausência de chamadas dinâmicas, consumidores externos ou dependências binárias.

| Item | Evidência | Tratamento necessário antes de retirar |
|---|---|---|
| `SystaliaGlassItem` / `SystaliaGlassMaterial` | `src/main.cpp` registra o primeiro no módulo `Systalia.Engine 1.0`; `src/gfx` implementa ambos; QML, testes e ferramentas os referenciam. | Migrar os consumidores efetivos e contratos públicos; testar eventuais adaptadores sobre V2, ciclo de vida e API antes de retirar implementação. |
| `ValoGlass.qml` | Usado por `decorationV1`, Dock e Files legados; incluído no módulo QML de `CMakeLists.txt`. | Migrar chamadas e fallback, incluindo instanciação dinâmica; preservar nome público por adaptação se necessário. |
| `decorationV1` | `WaylandWindowContainer.qml` alterna via `useGraphicsV2`; testes verificam ambos. | Provar V2 com Wayland e XWayland, popups, resize, foco e buffers; resolver política de compatibilidade antes de excluir. |
| Shaders `shaders/valo_optical.vert` / `.frag` | Compilados em `sge_shaders`; `SystaliaGlassMaterial.cpp` usa `:/shaders/valo_optical.*.qsb`. | Rastrear consumidores, preservar qrc enquanto houver API dependente; testar QSB e framebuffer depois da troca. |
| `SystaliaTheme.qml` | Singleton QML de compatibilidade, ainda usado em `main.qml` e cabeçalho, inclusive por componentes V2; alguns valores são literais próprios. | Mapear cada propriedade e sua semântica visual, preservar singleton ou adaptador público até migração validada. |
| `SystaliaG2Frame.qml` | Usado em `SystaliaWindowHeader.qml`, além de componentes legados. | Não confundir primitiva compartilhada com renderer V1; auditar instâncias e bordas antes de qualquer substituição. |
| ABI CPU/GPU | `SystaliaSurfaceProps.hpp`: matriz 64 bytes, props V1 128 e uniformes de renderização 144. `SystaliaSurfacePropsV2.hpp`: 160 e inclui o cabeçalho V1 por `Matrix4x4_std140`. | Extrair eventual tipo comum preservando nome, alinhamento, offsets e headers públicos; executar testes ABI e validar QSB antes de remover qualquer declaração. |
| Protocolo `z_systalia_surface_v1.xml` | Geração de servidor em `CMakeLists.txt`; testes de protocolo e clientes. | Preservar a versão e os contratos. `v1` aqui identifica protocolo, não renderer legado. |

`main.qml` instancia Files, Dock, TopBar e Control Center V2; Flow Menu, notificações, Task Switcher e Snap Preview já declaram superfícies V2. Isso não prova que arquivos V1 empacotados, caminhos de fallback ou tipos públicos não sejam utilizados por outros consumidores. O conteúdo dos clientes usa `ShellSurfaceItem`; mudanças devem preservar apresentação direta, subsurfaces, sincronização e liberação de buffers.

O CMake declara `SGE_GRAPHICS_V2`, mas a busca no código e nos testes encontrou somente a declaração: não há evidência de que `OFF` desligue V2. Não tratar essa opção como chave de migração ou fallback funcional sem acrescentar verificação específica.

Na busca textual de `SystaliaTheme.<propriedade>` em `src`, `tests` e `tools`, apareceram `fontFamily` (4), `ocean` (2), `textMuted` (1) e `valo` (1); algumas ocorrências são testes. O singleton expõe outras propriedades públicas, que não se tornam removíveis apenas por ausência de referências internas. O índice textual também não detecta acesso indireto por nome calculado ou consumidores externos.

## Baseline e limitações

- Build limpo isolado na configuração padrão, com Ninja, GCC 16.2.1 e Qt 6.11.2: **437/437 ações concluídas**. [Configuração](baseline-configure.log) e [build](baseline-build.log). Binário limpo SHA-256 `5adbc1f0ce98917befc742d1b6356ffb6e54931a44e8dbd9cf4fa29ac9611202`.
- Uma configuração limpa **Release** com GCC 16.2.1, Qt 6.11.2 e `-Werror` falhou no teste `tst_LiquidTiling.cpp`: `-Werror=array-bounds` em código inline `QQmlPrivate::QQmlElement<SystaliaGlassItem>` de `qqmlprivate.h`, durante otimização. Não está demonstrado se é diagnóstico falso ou defeito real. Não se deve esconder a falha removendo `-Werror` sem investigação. [Log Release](baseline-release-build.log).
- CTest do build limpo: **47/47**, 34,14 s, com `QT_QPA_PLATFORM=offscreen` e dois processos: [log](baseline-ctest.log).
- O executável existente `build/systalia_glass_engine`, SHA-256 `d68d546266e79d959a3daf65051b2f7f3aa37946a48ff02c00a365b18777d23e`, foi executado em uma sessão Wayland real; a sonda abriu Alacritty e registrou capturas e estados em [evidências do baseline](../../graphics-v2/evidence/etapa-06-4/clients-consolidacao-baseline/). O [binário do build limpo](evidence/clients/) também foi executado no SDE real com Alacritty; os estados e capturas registram o par, fallback e overlays. Os testes foram instrumentados, não uma homologação manual. Não comprova outros clientes, XWayland real, múltiplos outputs, acessibilidade ou paridade visual total. A sonda não utiliza gerador de desktop.
- A etapa 06.4 permanece PARTIAL para captura de clientes externos e registra problema de conteúdo preto XWayland não solucionado. Isso impede declarar paridade total das decorações por mera compilação.

## Sequência proposta, com commits pequenos

1. **Baseline e ADRs:** preservar os inventários, os resultados positivos e negativos e capturas reais; identificar versão do compilador/Qt, configuração e executável. Nenhum arquivo de produto é excluído nesta fase.
2. **Isolar contratos de dados:** separar a matriz std140 compartilhada do cabeçalho que também declara V1, mantendo includes e nomes públicos e todos os asserts de 64/128/144/160 bytes. Verificar shaders, UBO e testes existentes. Não remover o contrato V1 apenas porque V2 deixou de incluí-lo.
3. **Migrar consumidores e testes por família:** começar pelas instâncias comprovadamente ativas, depois fallbacks, componentes empacotados e ferramentas. Para cada componente, demonstrar equivalência de sinais, propriedades, input, acessibilidade e renderização. Mapear e preservar chamadas dinâmicas e APIs QML públicas, inclusive `SystaliaTheme` e `useGraphicsV2` enquanto forem contrato de compatibilidade.
4. **Decorações e clientes:** manter `ShellSurfaceItem` e as interfaces do compositor intactos; testar clientes Wayland, popups/subsurfaces, XWayland quando disponível, movimentação, resize, snap, maximização, escala, foco e vida útil de buffers. O fallback só pode ser aposentado após demonstrar equivalência e resolver a política dos consumidores públicos.
5. **Retirar o renderer V1:** somente após rastreamento novamente zerado ou adaptado, excluir em pequenos commits `SystaliaGlassItem`, `SystaliaGlassMaterial`, `ValoGlass.qml` e shaders legados que não tenham consumidores. Separar contratos públicos/ABI ainda necessários do código de implementação; atualizar testes que verificam comportamento, sem remover cobertura simplesmente por mencionar V1. Registrar cada arquivo removido e a substituição.
6. **Modularizar CMake e layout:** primeiro dividir arquivos de configuração preservando `sge_core`, `sge_shaders`, URI e qrc; em seguida separar alvos por fronteiras e mover arquivos gradualmente para `src/app`, `src/compositor`, `src/graphics`, `src/shell`, `src/controls`, `src/services`. Manter includes, aliases de recursos, imports relativos e instalação até migração coordenada; testar instalação em prefixo temporário. Não mover protocolos/UBOs por correspondência de nomes.
7. **Homologação final:** build limpa com configurações documentadas, CTest completo, QSB, ABI, instalação, Vulkan/OpenGL quando disponíveis e screenshots reais de Wayland. Repetir inventário e referências; registrar arquivos removidos/migrados, métricas antes/depois, regressões e limitações. Não declarar V2 única enquanto houver renderer legado ativo, superfícies não migradas ou validação insuficiente.

As decisões [ADR 0001](../adr/0001-valo-v2-como-renderer-oficial.md) e [ADR 0002](../adr/0002-limites-de-modulos-e-recursos.md) são Markdown versionável por Git e podem ser publicados no Mintlify. A topologia final dos targets é proposta, não uma alteração já implementada.

## Inventário depois desta fase

Nenhum arquivo de produto removido ou migrado nesta fase de auditoria. Os arquivos de inventário, ADRs, logs e capturas de baseline são adições. A consolidação funcional e a reorganização ainda não foram concluídas; qualquer resultado posterior deve atualizar esta seção por commit e preservar o baseline histórico.

## Fase incremental: módulo CMake de shaders

`CMakeLists.txt` passou a incluir `cmake/SgeShaders.cmake`, com o mesmo conteúdo anterior de `sge_shaders`, incluindo shaders V1, V2 e de diagnóstico. É uma extração de configuração, sem remover arquivos ou criar uma implementação óptica. O `sge_core`, a URI QML e as regras de instalação não mudaram. O caminho final por responsabilidade e a divisão de targets continuam pendentes.

- Compilação limpa isolada: 437/437 ações concluídas; [configuração](modular-configure.log), [build](modular-build.log).
- 27 arquivos QSB gerados: hashes SHA-256 iguais aos do build limpo de baseline. Os três manifestos qrc de shaders são idênticos após normalizar os diretórios absolutos de build. Isso verifica conteúdo e mapeamento dos recursos, não todos os aspectos visuais.
- CTest completo: **47/47**, 34,28 s: [log](modular-ctest.log). Instalação em prefixo temporário: [log](modular-install.log); os três caminhos instalados (`bin/systalia_glass_engine`, `bin/systalia-session`, `share/wayland-sessions/systalia.desktop`) coincidem com o baseline.
- Binário do build modular SHA-256 `8704c0d0ca9fee48b3940f7980c2d501df6b0f2a6500a3cdafd9d79cd573b7ee` executado no SDE real com Alacritty: [capturas e estados](evidence-cmake-modular/clients/). A comparação de hashes dos executáveis não demonstra identidade binária: os builds em diretórios distintos produziram hashes diferentes. A sonda testa cenários instrumentados, não toda a interação manual ou clientes XWayland.

**Arquivos de produto removidos ou migrados:** nenhum. **Arquivo adicionado:** `cmake/SgeShaders.cmake`; **arquivo alterado:** `CMakeLists.txt`. A migração V1 → V2, remoção do fallback e reorganização física permanecem pendentes, e a falha da configuração Release registrada no baseline continua sem correção.

## Fase incremental: contrato std140 compartilhado

A definição existente de `Matrix4x4_std140` e seus asserts foram extraídos, sem alteração textual, para `src/gfx/ValoStd140Matrix.hpp`. O cabeçalho público anterior inclui o novo arquivo, preservando o nome e as declarações transitivamente disponíveis; `SystaliaSurfacePropsV2.hpp` continua incluindo o cabeçalho V1 por compatibilidade. Portanto, a dependência transitiva de V2 no cabeçalho V1 **ainda existe**; a extração prepara uma separação posterior sem retirar a API anterior. Nenhuma estrutura de UBO, offset, shader ou renderer foi removido.

- Compilação incremental e testes: [build](contrato-std140-build.log), [CTest 47/47](contrato-std140-ctest.log), 24,70 s. Um arquivo independente que inclui somente `ValoStd140Matrix.hpp` compilou com asserts de tamanho 64 e alinhamento 16; os testes existentes continuam verificando 128/160 e offsets; o `static_assert` do bloco de renderização preserva os 144 bytes.
- O executável reconstruído tem o mesmo SHA-256 `8704c0d0ca9fee48b3940f7980c2d501df6b0f2a6500a3cdafd9d79cd573b7ee` do build modular anterior. Foi executado com Alacritty no [SDE real](evidence-cmake-modular/clients-abi/); cenário instrumentado, sem homologação geral de clientes externos.
- Arquivos adicionados: `src/gfx/ValoStd140Matrix.hpp` e [inventário após as duas mudanças](inventario-apos-contratos.txt). Arquivos alterados: `src/gfx/SystaliaSurfaceProps.hpp` e `CMakeLists.txt`. Nenhum arquivo removido. O inventário de produto/testes/ferramentas passou de 261 para 263 caminhos com a adição desse cabeçalho e de `cmake/SgeShaders.cmake`; não é uma medição de linhas de código.

## Fase incremental: usos internos de tema

Os quatro acessos diretos de produção a `SystaliaTheme` foram migrados: `main.qml` usa `ValoTheme.primitiveNavy950` e `SystaliaWindowHeader.qml` usa `ValoTheme.fontFamily` em três locais. Os valores anteriores eram aliases diretos desses mesmos campos. `SystaliaTheme.qml`, sua inscrição no módulo e todas as propriedades públicas permanecem para compatibilidade; a busca textual passou de quatro acessos diretos em `src/qml` para zero, sem considerar usuários externos ou acessos indiretos. O teste de compatibilidade compara agora as propriedades antigas e novas, além de preservar as expectativas anteriores.

- [Build](tema-v2-build.log), [compilação final dos testes](tema-v2-build-test.log) e [CTest 47/47](tema-v2-ctest.log), 24,74 s.
- O SDE principal foi executado com Alacritty após a alteração: [capturas e estados](evidence-cmake-modular/clients-tema-v2/). As capturas `00-no-client` antes e depois têm resolução 1662 × 975 e diferença média absoluta RGB de 0,139/0,139/0,131 em escala 0–255. Foram feitas em momentos distintos com animações e relógio; essa métrica não demonstra equivalência pixel a pixel nem substitui homologação manual.
- Arquivos alterados: `src/qml/main.qml`, `src/qml/shell/SystaliaWindowHeader.qml` e `tests/unit/tst_ValoTheme.cpp`. Nenhum arquivo de produto removido ou movido. O inventário de caminhos segue em 263; os módulos CMake adicionais e a matriz compartilhada são as duas adições sobre o inventário inicial de 261.

A consolidação ainda é parcial: `SystaliaGlassItem`, `SystaliaGlassMaterial`, `ValoGlass.qml`, shaders antigos e `decorationV1` permanecem compilados ou expostos e ainda têm referências; V2 não é a única implementação existente. A eliminação depende da migração e validação descritas neste plano, em particular contratos públicos, interfaces QML, Release e apresentação real de clientes externos. O singleton de compatibilidade não foi removido.

## Fase incremental: ponto de entrada

`src/main.cpp` foi movido para `src/app/main.cpp`; seu conteúdo tem SHA-256 `2fcc09537c6dc813ec584b5ce95fb5a6e611e0c5fb2806048fd556083670d0cb` antes e depois. O alvo do executável em `CMakeLists.txt` e a referência no blueprint corrente foram atualizados. Relatórios históricos permanecem com os caminhos vigentes quando foram escritos. A busca textual por `src/main.cpp` no código, ferramentas e configuração não encontrou outras referências operacionais. Nenhum renderer ou contrato público foi removido.

- Build limpo do estado após o movimento: 437/437 ações concluídas; [configuração](app-configure.log), [build](app-build.log). Os 27 QSB continuam com hashes idênticos ao baseline.
- CTest completo: **47/47**, 34,33 s; [log](app-ctest.log). Instalação em prefixo temporário com os três caminhos anteriores: [log](app-install.log).
- Binário limpo SHA-256 `f6702f129808a23a573fffdc3bcbe09377c4040b7acabc4b8af7f8405d36e124` executado com Alacritty no [SDE real](evidence-app/clients/), com capturas e estados instrumentados. Não é homologação manual completa nem validação do XWayland real.
- [Inventário após o movimento](inventario-apos-app.txt): 263 caminhos de produto/testes/ferramentas. Sobre o baseline de 261, houve duas adições (`cmake/SgeShaders.cmake`, `src/gfx/ValoStd140Matrix.hpp`) e um arquivo movido (`src/main.cpp` → `src/app/main.cpp`); nenhum arquivo de produto foi excluído.

**Situação final desta entrega: PARTIAL.** V1 ainda está compilado e exposto; a reorganização dos demais diretórios e targets, a investigação da falha Release e a migração e validação de clientes e fallback permanecem pendentes. Não é seguro declarar V2 como única implementação ou excluir os arquivos V1 com as evidências atuais.

## Investigação Release e consumidores V1

O [diagnóstico Release](DIAGNOSTICO_RELEASE.md) registra a reprodução limpa, a classificação e a correção isolada no teste Liquid Tiling. A [matriz de consumidores V1](CONSUMIDORES_V1.md) complementa o inventário com caminhos verificáveis para os consumidores diretos, dependências de build e contratos públicos. A retirada de `decorationV1`, a migração dos consumidores públicos restantes e a validação de XWayland real continuam como bloqueadores da consolidação.

A correção foi validada com CTest completo em Debug e Release (47/47 em cada) e com Alacritty no SDE Release, com capturas de ambas as decorações. A falha de compilação histórica continua registrada como baseline. O próximo passo da consolidação é migrar os consumidores ainda ativos e auditar a compatibilidade dos tipos públicos, preservando a paridade dos clientes e do fallback até validação apropriada.

## Consolidação 06.1

Os caminhos antigos `src/qml/shell/SystaliaDock.qml` e `src/qml/shell/SystaliaTopBar.qml` foram migrados no lugar para adaptadores V2. Nenhum caminho do inventário de produto foi adicionado, movido ou excluído. Os componentes V2 concentram a renderização, com ajustes no contrato da Dock e uma regressão QRC. O fallback de clientes e os contratos V1 protegidos permanecem disponíveis. O estado é **PARTIAL** pela API profunda de `valoGlassInstance` e pela validação pendente dos consumidores externos. [Auditoria e evidências](CONSOLIDACAO_06_1_DOCK_TOPBAR.md)

## Consolidação 06.2

Files e Control Center foram migrados no lugar para adaptadores V2. Os caminhos do inventário do produto continuam os mesmos. A consolidação desta etapa é **PARTIAL** porque o acabamento V1 e a semântica de todos os parâmetros ópticos externos não têm paridade comprovada. Os contratos V1 protegidos seguem disponíveis. [Inventário da etapa, testes e capturas](CONSOLIDACAO_06_2_FILES_CONTROLCENTER.md)

## Consolidação 06.3

Foram eliminadas a dependência transitiva de `SystaliaSurfacePropsV2.hpp` no cabeçalho V1 (`SystaliaSurfaceProps.hpp`), os registros manuais redundantes de `SystaliaGlassItem` em `src/app/main.cpp` e em seis suítes de teste, e o uso de `SystaliaGlassItem` em `tst_SystaliaProtocol.cpp` (migrado para `ValoGlassItemV2`). `decorationV1`, `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaGlassMaterial` e os shaders legados permanecem preservados até homologação completa de clientes e XWayland. Estado: **PARTIAL**. [Relatório e evidências](CONSOLIDACAO_06_3.md)
