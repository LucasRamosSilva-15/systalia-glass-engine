# Diagnóstico Release do Liquid Tiling

A falha foi reproduzida a partir de `ebaed25` em um diretório Ninja vazio com `-DCMAKE_BUILD_TYPE=Release`, GCC 16.2.1 (Red Hat 16.2.1-3), Qt 6.11.2 e as flags originais `-O3 -DNDEBUG -Wall -Wextra -Werror`. O alvo isolado foi `tests/CMakeFiles/tst_liquid_tiling.dir/unit/tst_LiquidTiling.cpp.o`. O [diagnóstico integral da etapa que falhou](evidence-release/diagnostico-build-limpo.log) inclui o comando e a cadeia de inlining.

O GCC aponta para `qqmlprivate.h:112` no destrutor de `QQmlPrivate::QQmlElement<SystaliaGlassItem>`, seguindo `std::default_delete<QQuickItem>` e `unique_ptr::reset` na linha 200 do teste original. A nota de alocação aponta para `make_unique<QQuickItem>` na linha 186 e informa 32 bytes. As duas janelas desse cenário são objetos `QQuickItem` simples. O serviço recebe ponteiros emprestados e o teste verifica a limpeza ao destruir uma janela. A instanciação do wrapper de `SystaliaGlassItem` vem do registro manual em `initTestCase` e não da alocação que o diagnóstico cita.

A classificação é **provável falso positivo de otimização** no caminho diagnosticado. O experimento recompila a unidade original e a alterada com o mesmo comando de `compile_commands.json`, trocando apenas o caminho do fonte original e o arquivo de saída. Desabilitar desvirtualização e desvirtualização especulativa somente na execução experimental elimina o diagnóstico no original. A correção elimina a instanciação redundante na unidade sem mudar flags, registro gerado, tempo de vida dos objetos ou comportamento testado. A investigação não estabelece ausência geral de UB e não identifica um número de bug upstream.

O registro manual era uma dependência V1 real **de compilação**. O módulo já registra o tipo gerado via `qml_register_types_Systalia_Engine`, e os componentes instanciados pelo teste são Snap Preview e Tile Splitter. A correção deixa somente o registro do módulo neste teste. A [matriz de consumidores](CONSUMIDORES_V1.md) distingue essa situação do fallback, dos recursos QSB e da ABI que ainda têm dependências.

## Resultados

| Verificação | Resultado | Evidência |
|---|---|---|
| A/B da unidade de tradução | Original 1, original sem desvirtualização 0, corrigido com flags originais 0 | [Status](evidence-release/ab.log) e [diagnóstico A/B original](evidence-release/diagnostico-ab.log) |
| Debug | Build de diretório novo, CTest 47/47 em 34,49 s | [Build](evidence-release/debug-build.log), [CTest](evidence-release/debug-ctest.log) |
| Release | Build do mesmo diretório criado vazio para reproduzir a falha, retomado após a correção. CTest 47/47 em 36,58 s | [Build](evidence-release/release-build.log), [CTest](evidence-release/release-ctest.log) |
| SDE Release em Wayland | Alacritty real com conteúdo visível em V1 e V2. A sonda também percorreu maximização, snap, workspaces e fechamento e encerrou com código 0 | [V1](evidence-release/01-v1-fallback.png), [V2](evidence-release/02-v2.png), [estado final](evidence-release/16-closed-compositor.json), [execução](evidence-release/wayland-run.log) |

Ambas as suítes incluem testes de protocolo, resize XDG, visibilidade e ciclo de vida de clientes. A sonda usou o modo `client-interactions` de `tools/validate_valo_stage064.py`, com caminhos temporários ajustados para o binário Release e a pasta de evidências. SHA-256 do executável testado: `33a74b1419095ff7911e91103262d2a010c2d90cf40c5d3c034da2f764c3c4bb`. A execução foi instrumentada com Alacritty Wayland. O servidor XWayland iniciou com zero clientes X gerenciados e fica fora da validação visual desta etapa.

A alteração está restrita a `tests/unit/tst_LiquidTiling.cpp`, commit `881eeea`. Os ADRs 0001 e 0002 seguem aplicáveis. A remoção de um registro redundante do teste não introduz uma decisão arquitetural. O fallback `decorationV1`, as APIs e o pipeline de clientes permanecem presentes.
