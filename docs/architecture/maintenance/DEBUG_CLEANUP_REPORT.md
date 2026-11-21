# Relatório de Limpeza e Governança de Artefatos de Debug (Consolidação 06.6A)

**Branch:** `valo-graphics-2.0`  
**Base inicial:** `32af651` (Consolidação 06.5)  
**Política adotada:** [`SYSTALIA_DEBUG_RETENTION_POLICY.md`](SYSTALIA_DEBUG_RETENTION_POLICY.md)

---

## 1. Inventário Antes × Depois

| Métrica | Antes (`32af651`) | Depois da Limpeza | Redução Efetiva |
|---|---|---|---|
| **Arquivos rastreados (Git)** | 2.619 | 642 (+4 arquivos de política/relatório neste commit) | **−1.977 arquivos** |
| **Tamanho no checkout (rastreados)** | 273,36 MiB (286.635.555 bytes) | 70,93 MiB (74.374.633 bytes) | **−202,43 MiB (−212.260.922 bytes)** |
| **Links Markdown relativos quebrados** | 0 / 173 | 0 / 173 | **0 quebrados** |

### Distribuição por diretório (Antes → Depois)
- `docs/architecture/valo-v1-v2/`: 1.242 arquivos (132,90 MiB) → **167 arquivos (26,37 MiB)**
- `docs/graphics-v2/evidence/`: 712 arquivos (84,09 MiB) → **142 arquivos (25,12 MiB)**
- `artifacts/`: 359 arquivos (47,20 MiB) → **27 arquivos (10,81 MiB)** (mantidos os arquivos representativos de `graphics-v2-stage-05-4` e `graphics-v2-stage-05-5` referenciados como diretórios)
- `src/` (170), `tests/` (49), `shaders/` (21), `tools/` (18), `docs/graphics-v2/` (30), `docs/architecture/adr/` (3), `data/` (2), `cmake/` (1), `config/` (1), `protocols/` (1): **100% preservados**.

---

## 2. Lotes de Exclusão Executados (Commits Reversíveis)

1. **`776cb2c` — `chore(cleanup): remove regenerable stage artifacts from artifacts/`**
   - **164 arquivos removidos (14,06 MiB):** artefatos regeneráveis não referenciados em Markdown de etapas concluídas (`artifacts/raw-render-tests/`, `artifacts/dock_v2_*.png`, `artifacts/graphics-v2-stage-04-2/`, `artifacts/graphics-v2-stage-05/`, `artifacts/graphics-v2-stage-05-1/`, `artifacts/graphics-v2-stage-05-3-1/`).
2. **`fff376b` — `chore(cleanup): prune redundant burst frames in stage 05.4 and 05.5 artifacts`**
   - **168 arquivos removidos (22,87 MiB):** sequências intermediárias de frames em `artifacts/graphics-v2-stage-05-4/` e `artifacts/graphics-v2-stage-05-5/`, preservando `sde-0.png`, `sde-60.png`, `frame-timings.json` e `sde.log`.
3. **`b0bc80c` — `chore(cleanup): prune redundant intermediate evidence dumps in docs/`**
   - **1.645 arquivos removidos (165,49 MiB):** dumps intermediários não referenciados (`*-providers.json`, `*-compositor.json` exceto `16-closed-compositor.json`, e capturas intermediárias `03..15` / `sde-10..50` / `sde-70..120`) em `docs/architecture/valo-v1-v2/evidence*` e `docs/graphics-v2/evidence/`.
   - **Preservados:** todos os **102** arquivos diretamente referenciados por links Markdown, todos os arquivos de reprodução do bug aberto `xwayland-xmessage` (`00-native-default.png`, `01-v1-fallback.png`, `02-v2.png`, `02-v2.json`, `xwayland-display.txt`, `sde.log`) e frames representativos em todas as pastas referenciadas como diretório.

---

## 3. Política Permanente e Prevenção de Reacúmulo (`e033513`)

1. **Ferramentas Python (`tools/validate_valo_stage06*.py`, `tools/validate_valo_header.py`):**
   - Alteradas para gravar em `${TMPDIR:-/tmp}/systalia-runs/<run-id>/` por padrão e remover o diretório temporário ao concluir.
   - Adicionado suporte explícito de depuração via `SGE_KEEP_EVIDENCE=1` ou `SGE_KEEP_EVIDENCE_DIR=<dir>`.
2. **Geradores C++ (`tools/generate_dock_v2_artifacts.cpp`, `tools/generate_shell_v2_artifacts.cpp`, `tools/generate_stage_05_1_artifacts.cpp`, `tools/raw_render_diagnostics/raw_render_diagnostics.cpp`):**
   - Diretório padrão alterado de `artifacts/` no repositório para `/tmp/systalia-runs/...` (mantendo `--artifact-dir` para sobrescrita explícita).
3. **`.gitignore`:** adicionados `systalia-runs/`, `artifacts/tmp-*`, `docs/**/tmp-*` e `*.tmp`, sem ignorar extensões úteis (`.png`, `.json`, `.log`) globalmente.
4. **Instruções para agentes:** criados [`AGENTS.md`](../../../AGENTS.md) e [`GEMINI.md`](../../../GEMINI.md) na raiz do repositório.

---

## 4. Validação Pós-Limpeza

- **Verificação de links Markdown:** **173/173** links relativos em `docs/` válidos (**0 quebrados**).
- **Build + CTest QRC/QML/Shaders:** compilação limpa do target completo em `build/` e aprovação de **7/7** testes críticos de recursos e renderização (`tst_systalia_engine`, `tst_valo_glass`, `tst_valo_decorations`, `tst_valo_glass_shader_v2`, `tst_valo_glass_rendering_v2`, `tst_valo_shell_v2`, `tst_shell_state_regression`).
- **Pendências / Observação:** O histórico Git anterior não foi reescrito (conforme política); objetos históricos continuam no packfile local até eventual política futura de manutenção de repositório.

---

## 5. Endurecimento da Limpeza Automática (Consolidação 06.6A.1 — COMPLETE)

**Commit de implementação:** `6c64f04` (+ commit de documentação/política).

### Lacunas corrigidas
1. `tools/validate_valo_stage06.py` agora utiliza `EvidenceRun` (`tools/sge_evidence.py`) e remove seu diretório temporário próprio ao concluir com sucesso, preservando apenas se `[output-directory]` explícito ou `SGE_KEEP_EVIDENCE=1` for usado.
2. Todos os validadores Python (`validate_valo_stage06.py`, `validate_valo_header.py`, `validate_valo_stage062.py`, `validate_valo_stage063.py`, `validate_valo_stage0631.py`, `validate_valo_stage064.py`) executam todo o ciclo de subprocessos dentro de `with EvidenceRun(...)` + `try/finally` com tratamento de `SIGINT`/`SIGTERM`:
   - **Sucesso:** remove exclusivamente o diretório temporário próprio criado por `mkdtemp` dentro de `${TMPDIR:-/tmp}/systalia-runs/`.
   - **Falha / Interrupção:** preserva o diretório próprio com `run-status.json` (`retention_hours: 24`) e executa poda automática (`prune_expired_runs`) de execuções expiradas com mais de 24h.
   - **Proteção de caminhos:** diretórios fornecidos pelo usuário (`--artifact-dir`, argumento posicional, `SGE_KEEP_EVIDENCE_DIR`) ou externos a `systalia-runs/` nunca são removidos.
3. Os 4 geradores C++ (`generate_dock_v2_artifacts.cpp`, `generate_shell_v2_artifacts.cpp`, `generate_stage_05_1_artifacts.cpp`, `raw_render_diagnostics.cpp`) foram padronizados via [`tools/sge_artifact_dir.hpp`](../../../tools/sge_artifact_dir.hpp) para criar diretórios exclusivos por execução (`QTemporaryDir` em `${TMPDIR:-/tmp}/systalia-runs/<tool>-XXXXXX`), eliminando colisões e retenção indefinida.

### Validação 06.6A.1
- **Testes automatizados (`tests/unit/tst_EvidenceCleanup.py`, registrado como `tst_evidence_cleanup` no CTest):** **7/7** cenários aprovados (sucesso, falha de subprocesso + poda TTL, interrupção `SIGTERM`, retenção explícita `SGE_KEEP_EVIDENCE=1`, diretório customizado, 4 execuções simultâneas sem colisão e proteção contra remoção de diretórios externos).
- **Build limpo Debug (`-Werror`) + CTest completo:** **48/48 aprovados** (49,93 s).
- **Build limpo Release (`-Werror`) + CTest completo:** **48/48 aprovados**.
