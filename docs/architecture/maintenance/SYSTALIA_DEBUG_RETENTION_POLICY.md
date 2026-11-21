# SYSTALIA — Política de Retenção e Limpeza de Artefatos de Diagnóstico

**Status:** ADOTADA na Consolidação 06.6A (branch `valo-graphics-2.0`).  
**Escopo:** logs, screenshots, dumps, intermediários gerados e evidências de execução de ferramentas/agentes; **não** abrange código-fonte (`src/`), testes (`tests/`), shaders-fonte (`shaders/`), protocolos (`protocols/`), configurações (`config/`, `data/`, `cmake/`) ou documentação arquitetural (`docs/architecture/`, ADRs).

---

## 1. Regras Obrigatórias

1. **Diretório temporário exclusivo fora do Git:** Ferramentas Python (`tools/sge_evidence.py`) e geradores C++ (`tools/sge_artifact_dir.hpp`) criam diretórios exclusivos por execução em `${TMPDIR:-/tmp}/systalia-runs/<tool>-<run-id>/` (via `mkdtemp` / `QTemporaryDir`), evitando colisões em execuções simultâneas e nunca gravando automaticamente em `docs/` ou `artifacts/` rastreados.
2. **Inspeção, sucesso vs. falha/interrupção:** Em execuções bem-sucedidas, o diretório temporário próprio é removido automaticamente no bloco `finally`/destrutor. Em falhas ou interrupções (`SIGINT`/`SIGTERM`), as evidências são preservadas com `run-status.json` e prazo de retenção automático de **24 horas** (`prune_expired_runs`).
3. **Preservação explícita e proteção de diretórios externos:** `SGE_KEEP_EVIDENCE=1`, `SGE_KEEP_EVIDENCE_DIR=<dir>` ou `--artifact-dir <dir>` preservam os artefatos. Diretórios fornecidos pelo usuário ou externos ao subdiretório próprio em `systalia-runs/` **nunca** são removidos automaticamente.
4. **Remoção segura de rastreados:** Exclusões em Git exigem inventário prévio, verificação de referências em Markdown/CMake/QML/testes e uso de `git rm` explícito (nunca `find ... -delete` amplo ou reescrita de histórico).
5. **Proteção de integridade:** Não excluir `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaSurfaceProps`, shaders ou testes V1/V2 sob pretexto de limpeza de artefatos.
