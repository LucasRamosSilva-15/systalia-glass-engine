# SYSTALIA — Política de Retenção e Limpeza de Artefatos de Diagnóstico

**Status:** ADOTADA na Consolidação 06.6A (branch `valo-graphics-2.0`).  
**Escopo:** logs, screenshots, dumps, intermediários gerados e evidências de execução de ferramentas/agentes; **não** abrange código-fonte (`src/`), testes (`tests/`), shaders-fonte (`shaders/`), protocolos (`protocols/`), configurações (`config/`, `data/`, `cmake/`) ou documentação arquitetural (`docs/architecture/`, ADRs).

---

## 1. Regras Obrigatórias

1. **Diretório temporário fora do Git:** Ferramentas e sondas devem gravar arquivos temporários em `${TMPDIR:-/tmp}/systalia-runs/<run-id>/` por padrão, nunca automaticamente dentro de `docs/` ou `artifacts/` rastreados.
2. **Inspeção e resumo antes de descartar:** O agente analisa os logs/capturas necessários, registra métricas, status e comandos executados no relatório correspondente, e descarta os artefatos temporários da própria execução ao final.
3. **Preservação explícita para depuração:** Quando houver necessidade de depurar ou preservar evidência (ex.: falha nova, regressão ou bug aberto), utilizar `SGE_KEEP_EVIDENCE=1`, `SGE_KEEP_EVIDENCE_DIR=<dir>` ou `--artifact-dir <dir>` para reter o conjunto mínimo reproduzível.
4. **Remoção segura de rastreados:** Exclusões em Git exigem inventário prévio, verificação de referências em Markdown/CMake/QML/testes e uso de `git rm` explícito (nunca `find ... -delete` amplo ou reescrita de histórico).
5. **Proteção de integridade:** Não excluir `ValoGlass.qml`, `SystaliaGlassItem`, `SystaliaSurfaceProps`, shaders ou testes V1/V2 sob pretexto de limpeza de artefatos.
