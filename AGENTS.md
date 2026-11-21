# SYSTALIA Glass Engine — Instruções para Agentes (Codex / Geral)

1. **Política de Artefatos de Diagnóstico:** Leia e siga [`docs/architecture/maintenance/SYSTALIA_DEBUG_RETENTION_POLICY.md`](docs/architecture/maintenance/SYSTALIA_DEBUG_RETENTION_POLICY.md).
2. **Saída temporária obrigatória:** Nunca gere capturas, dumps JSON ou logs de execução diretamente em `docs/` ou `artifacts/` rastreados. Utilize `${TMPDIR:-/tmp}/systalia-runs/<run-id>/` (padrão já configurado nos scripts `tools/validate_valo_*.py` e geradores `tools/generate_*.cpp`).
3. **Ciclo de execução:** `gerar em /tmp/systalia-runs/<id>` → `analisar/inspecionar` → `registrar resumo no relatório` → `limpar temporários da própria execução`.
4. **Modo de depuração explícito:** Para manter evidências temporárias após uma execução bem-sucedida, defina `SGE_KEEP_EVIDENCE=1` (ou `SGE_KEEP_EVIDENCE_DIR=<caminho>` / `--artifact-dir <caminho>`).
5. **Bug aberto / regressão:** Se uma execução reproduzir uma falha ainda aberta, preserve apenas o conjunto mínimo necessário para reprodução e registre a justificativa no relatório.
6. **Proteção do repositório:** Não altere ou exclua `src/`, `tests/`, `shaders/`, `protocols/`, `cmake/`, ADRs ou componentes VALO V1/V2 durante rotinas de limpeza.
