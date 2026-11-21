# ADR 0002 — Limites de módulos e recursos

- **Estado:** aceita e implementada nas Consolidações 06.6C e 06.6D (árvore hierárquica por responsabilidade e versão, CMake modular, preservação de `sge_core`, `sge_shaders` e aliases QRC)
- **Data:** 2026-11-17
- **Contexto:** `sge_core` agrupa compositor, serviços, renderers e módulo QML; `sge_shaders` agrupa shaders de produção e diagnósticos. Uma separação apressada pode quebrar inicialização QML, recursos estáticos, geração de protocolo e includes públicos.

## Decisão proposta

Organizar gradualmente `src/app`, `src/compositor`, `src/graphics`, `src/shell`, `src/controls` e `src/services`, com `tests`, `tools`, `protocols`, `assets` e `docs` separados. Primeiro modularizar os arquivos CMake preservando alvos e comportamento; depois introduzir alvos de responsabilidade clara e dependências explícitas, mantendo `sge_core` como fachada durante a transição quando necessário. Preservar URI `Systalia.Engine`, versão, singleton, paths qrc efetivamente usados, Qt resources, geração de protocolo, dependências privadas de QRhi e instalação da sessão. A futura separação de shaders de diagnóstico deve manter os testes e ferramentas que os consomem.

Movimentos de QML precisam de aliases de recurso ou atualização coordenada de imports relativos, chamadas `Qt.createComponent`, testes e qrc. Criar temporariamente encaminhamentos de includes para interfaces públicas, se necessário. Cada fase exige build limpa, teste da instalação em prefixo temporário e validação de execução da shell real. Não alterar comportamento gráfico como efeito colateral da reorganização.

## Consequências

A topologia final de targets é uma proposta a validar contra ciclos de dependências e inicialização estática Qt; não é prova de que arquivos podem ser movidos mecanicamente. O plano e o inventário correspondem ao commit de baseline e devem ser atualizados em cada fase.
