# SYSTALIA Glass Engine — Instruções para Agentes Gemini / Antigravity

Estas diretrizes aplicam-se a todas as execuções de agentes Gemini (incluindo Gemini 4 Hydrogen) neste repositório:

1. **Política Oficial de Retenção:** Siga [`docs/architecture/maintenance/SYSTALIA_DEBUG_RETENTION_POLICY.md`](docs/architecture/maintenance/SYSTALIA_DEBUG_RETENTION_POLICY.md).
2. **Proibição de acúmulo em Git:** Não grave logs extensos, dumps JSON intermediários ou sequências de frames em `docs/` ou `artifacts/` versionados. As ferramentas em `tools/` usam `tools/sge_evidence.py` e `tools/sge_artifact_dir.hpp` para gerar diretórios exclusivos em `${TMPDIR:-/tmp}/systalia-runs/<tool>-<id>/` com limpeza automática em sucesso e retenção de 24h em falhas/interrupções.
3. **Fluxo obrigatório por tarefa:** **gerar → inspecionar → resumir no relatório → confirmar limpeza automática**. Diretórios externos (`--artifact-dir`, `SGE_KEEP_EVIDENCE_DIR`) nunca sofrem remoção automática.
4. **Preservação explícita:** Use `SGE_KEEP_EVIDENCE=1` ou `SGE_KEEP_EVIDENCE_DIR=<dir>` quando precisar reter evidências para inspeção.
5. **Integridade V1/V2 e testes:** Nunca remova componentes VALO V1/V2, shaders, contratos ABI ou testes durante tarefas de limpeza de artefatos.
6. **Ordem obrigatória de execução de testes:** Ao modificar ou criar qualquer componente, execute **primeiro** o(s) teste(s) específico(s) do(s) componente(s) mexido(s) ou criado(s) (ex.: `ctest -R <teste_especifico> --output-on-failure`) e, após aprovação, execute **todos** os testes da suíte completa (`ctest --output-on-failure`).
7. **Regra permanente do backend Waylib (Etapa 03.2.5.2+):** Toda compilação, execução de testes e validação de regressão deve usar exclusivamente `-DSGE_ENABLE_WAYLIB_BACKEND=ON`, em build incremental único `Release` (`build/waylib`) com `-Werror`. Não compilar com `OFF` em etapas normais, não duplicar matrizes ON/OFF nem Debug/Release sem justificativa concreta, e reutilizar o diretório de build incrementalmente.
