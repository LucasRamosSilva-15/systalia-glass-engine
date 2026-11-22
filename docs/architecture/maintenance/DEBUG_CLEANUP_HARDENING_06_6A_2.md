# Consolidação 06.6A.2 — Proteção Definitiva do Ciclo de Vida de Evidências

- **Branch:** `valo-graphics-2.0`
- **Commit base:** `f609579`
- **Commit de implementação:** `6144ff4` (`fix(cleanup): add flock/metadata ownership checks and remove hardcoded brain paths`)
- **Política atualizada:** [`SYSTALIA_DEBUG_RETENTION_POLICY.md`](./SYSTALIA_DEBUG_RETENTION_POLICY.md)

## 1. Riscos Residuais Corrigidos

1. **Propriedade e estado de retenção explícitos:** `prune_expired_runs()` em [`tools/sge_evidence.py`](../../../tools/sge_evidence.py) e `pruneExpiredRuns()` em [`tools/sge_artifact_dir.hpp`](../../../tools/sge_artifact_dir.hpp) passaram a exigir `managed_by == "sge-evidence-v1"` em `run-status.json`. Diretórios históricos sem metadados ou desconhecidos são preservados.
2. **Proteção contra colisão, execuções ativas e reutilização de PID:** Cada execução adquire lock exclusivo não-bloqueante `flock(LOCK_EX | LOCK_NB)` sobre `.sge-run.lock`. Como o kernel libera o lock automaticamente em encerramentos ou `SIGKILL`, o mecanismo é imune à reutilização de PID pelo sistema operacional. Diretórios ativos não sofrem poda mesmo se ultrapassarem o TTL de 24 horas.
3. **Escopo restrito de TTL (24h):** O prazo de retenção de 24 horas é aplicado apenas a diretórios gerenciados e encerrados com `status` em `failed` ou `interrupted` sem `explicit_keep: true`. Execuções preservadas explicitamente (`SGE_KEEP_EVIDENCE=1`, `SGE_KEEP_EVIDENCE_DIR`, `--artifact-dir`) permanecem protegidas.
4. **Proteção contra symlinks e travessia de caminho:** As rotinas validam que o alvo é filho direto de `systalia-runs/` e não é symlink (`_is_direct_child_non_symlink()` em Python e verificação canônica em C++).
5. **Remoção de caminhos automáticos para `~/.gemini/antigravity/brain/...`:** Os 5 geradores C++ atualizados passaram a usar `ScopedArtifactRun` ou flags explícitas (`--artifact-dir` / `--brain-dir`), sem gravar automaticamente no diretório do Antigravity e sem remover quaisquer arquivos pessoais ou existentes:
   - `tools/bench_and_artifacts.cpp`
   - `tools/generate_dock_v2_artifacts.cpp`
   - `tools/generate_shell_v2_artifacts.cpp`
   - `tools/generate_stage_05_1_artifacts.cpp`
   - `tools/raw_render_diagnostics/raw_render_diagnostics.cpp`

## 2. Validação e Testes Executados

### Testes unitários de ciclo de vida (`tests/unit/tst_EvidenceCleanup.py`)
Comando: `python3 -m unittest -v tests/unit/tst_EvidenceCleanup.py`
Resultado: **10/10 aprovados** (`Ran 10 tests in 0.296s — OK`):
- `test_active_run_with_old_directory_is_never_pruned`
- `test_concurrent_runs_do_not_collide`
- `test_custom_directory_is_never_deleted`
- `test_explicit_retention_survives_beyond_24h_ttl`
- `test_external_directory_protection`
- `test_failed_subprocess_preserves_evidence_and_prunes_expired`
- `test_interruption_preserves_evidence_with_interrupted_status`
- `test_successful_run_cleans_up_automatically`
- `test_symlink_and_path_traversal_protection`
- `test_unknown_directory_without_metadata_is_preserved`

### Builds limpos e CTest completo (`QT_QPA_PLATFORM=offscreen`)
- **Debug (`-Werror`)** no commit `6144ff4`: **48/48 testes aprovados** (50.03s), diretório temporário removido após a execução.
- **Release** no commit `6144ff4`: **48/48 testes aprovados** (50.21s), diretório temporário removido após a execução.

## 3. Escopo e Limitações

- Nenhuma alteração foi feita no renderer VALO V1/V2, shaders, contratos ABI ou protocolos Wayland, nem houve remoção de arquivos V1.
