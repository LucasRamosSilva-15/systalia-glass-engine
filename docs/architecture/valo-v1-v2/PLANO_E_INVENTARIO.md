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
