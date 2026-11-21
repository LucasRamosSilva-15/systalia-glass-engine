# SYSTALIA Glass Engine — Instruções para Agentes Gemini / Antigravity

Estas diretrizes aplicam-se a todas as execuções de agentes Gemini (incluindo Gemini 4 Hydrogen) neste repositório:

1. **Política Oficial de Retenção:** Siga [`docs/architecture/maintenance/SYSTALIA_DEBUG_RETENTION_POLICY.md`](docs/architecture/maintenance/SYSTALIA_DEBUG_RETENTION_POLICY.md).
2. **Proibição de acúmulo em Git:** Não grave logs extensos, dumps JSON intermediários (`*-providers.json`, `*-compositor.json`) ou sequências completas de frames em `docs/` ou `artifacts/` versionados. As ferramentas em `tools/` gravam por padrão em `${TMPDIR:-/tmp}/systalia-runs/<run-id>/`.
3. **Fluxo obrigatório por tarefa:** **gerar → inspecionar → resumir no relatório → limpar `/tmp/systalia-runs/<id>`**. Ao versionar evidências em `docs/`, inclua apenas capturas representativas ou reproduções mínimas de bugs abertos.
4. **Preservação explícita:** Use `SGE_KEEP_EVIDENCE=1` ou `SGE_KEEP_EVIDENCE_DIR=<dir>` quando precisar reter evidências para inspeção.
5. **Integridade V1/V2 e testes:** Nunca remova componentes VALO V1/V2, shaders, contratos ABI ou testes durante tarefas de limpeza de artefatos.
