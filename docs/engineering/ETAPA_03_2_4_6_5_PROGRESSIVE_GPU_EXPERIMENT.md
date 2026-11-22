# SYSTALIA — Etapa 03.2.4.6.5: Experimento Offscreen Progressivo, Segurança de Sincronização e Diagnóstico do Primeiro Frame

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente de referência:** Fedora Linux 44, Qt 6.11.2, C++23, Vulkan/RADV, GBM, libdrm e QtWaylandCompositor  
**GPU de referência:** AMD Radeon Cezanne/Renoir (`/dev/dri/card1`)  
**Baseline de regressão:** 59/59 CTest (`38 unit`, `20 integration`, `1 e2e`)

---

## 1. Auditoria de `poll()` (`observeSyncFdCompletion` / `isSyncFdSignaled`)

Foi auditada a observação de conclusão GPU via `SYNC_FD` em `src/platform/drm/VulkanQuickRenderer.cpp`:

- **Timeout finito e seguro:** `timeoutMs` passa a ser limitado (`timeoutMs >= 0`), impedindo espera indefinida em chamadas com valores negativos.
- **Tratamento de `EINTR`:** Em caso de interrupção por sinal (`errno == EINTR`), o `poll()` é repetido utilizando apenas o tempo restante dentro da janela original de timeout.
- **Rejeição explícita de condições de erro/inválidas:** `POLLERR | POLLNVAL | POLLHUP` são explicitamente rejeitados e não são tratados como sinalização de conclusão GPU.
- **Apenas `POLLIN` limpo indica conclusão:** Somente retorno positivo com `POLLIN` (e sem flags de erro/hangup) transiciona o estado do `GpuSubmissionTracker` para `GPU_EXECUTION_COMPLETED`, registrando `slotId` e `gpuLatencyUs`.
- **Caminhos de falha:** FD inválido (`< 0`), FD fechado, timeout expirado e `VK_ERROR_DEVICE_LOST` não produzem falso positivo de conclusão GPU.

---

## 2. Auditoria da Quarentena de Semáforos

Foi auditado o ciclo de vida dos semáforos binários Vulkan em `src/platform/drm/VulkanQuickRenderer.cpp`:

- **`quarantineSemaphore()` e destruição diferida:** Semáforos associados a frames/submissões em voo são colocados em quarentena antes de qualquer reutilização ou destruição, evitando destruição prematura enquanto a GPU ainda pode sinalizar ou aguardar o objeto.
- **`flushQuarantinedSemaphores()`:** Realiza a limpeza ordenada dos semáforos em quarentena após garantia de ociosidade do dispositivo ou encerramento seguro.
- **Tratamento de `VK_ERROR_DEVICE_LOST` em `destroySharedPoolSlots()` e `shutdown()`:** Em `destroySharedPoolSlots()`, o retorno de `df->vkDeviceWaitIdle(m_device)` é verificado explicitamente; caso retorne `VK_ERROR_DEVICE_LOST`, `m_deviceLost` é marcado como `true` em vez de assumir sucesso silencioso antes da execução de `flushQuarantinedSemaphores()`.

---

## 3. Correções Aplicadas

| Arquivo | Alteração realizada |
| --- | --- |
| `src/platform/drm/VulkanQuickRenderer.cpp` | Endurecimento de `observeSyncFdCompletion()` e `isSyncFdSignaled()` (`timeoutMs >= 0`, retry `EINTR` com tempo restante, rejeição explícita de `POLLERR \| POLLNVAL \| POLLHUP`, transição `GPU_EXECUTION_COMPLETED` apenas em `POLLIN` limpo com log de `slotId` e `gpuLatencyUs`); verificação de `df->vkDeviceWaitIdle(m_device) == VK_ERROR_DEVICE_LOST` em `destroySharedPoolSlots()` antes de `flushQuarantinedSemaphores()`. |
| `tools/diagnostics/sge_vulkan_dmabuf_harness.cpp` | Atualização de `specForId("C")` (`128×128`, DMA-BUF, `ShellRoot.qml`) para `highRisk = true`, garantindo que todos os cenários `C–H` retornem `[BLOCKED]` (código de saída `3`) sem `--allow-gpu-risk` e `SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1`. |
| `tools/homologation/run_offscreen_experiment.sh` | Novo script executor progressivo de cenário único (`--scenario <A..H> [--frames N] [--authorize]`), com recusa a rodar como `root`, gravação de log em `$HOME/systalia-homolog-logs/offscreen-exp-<scenario>-<timestamp>.log`, ativação de `SGE_GRAPHICS_DIAGNOSTICS=1` e bloqueio explícito (saída `3`) para `C–H` sem `--authorize`. |
| `tests/unit/tst_VulkanDmabufAudit.cpp` | Extensão de `submissionTrackerDistinguishesSubmittedTimedOutAndRealGpuCompletion()` cobrindo `POLLHUP` (extremidade de escrita de pipe fechada sem dados), `POLLNVAL` (FD fechado) e FD `-1`, verificando que nenhum deles transiciona para `Completed`. |

---

## 4. Fidelidade da Matriz (`A–H` vs. Frame 1 na TTY3)

A matriz progressiva no harness `sge_vulkan_dmabuf_harness` isola incrementalmente as variáveis do primeiro frame observado no hang da TTY3 (`amdgpu: ring gfx timeout, signaled seq=820468, emitted seq=820469`, antes do primeiro Atomic Commit KMS):

| Cenário | Escopo / Carga | Risco configurado | Estado atual |
| --- | --- | --- | --- |
| **A** | Baseline reduzido (`128×128`), sem `ShellRoot.qml` completo / carga mínima | Seguro | Executado e aprovado (`[PASS]`) |
| **B** | `128×128` com pipeline DMA-BUF / `SYNC_FD` sem carga completa de `ShellRoot.qml` + VALO V2 | Seguro | Executado e aprovado (`[PASS]`) |
| **C** | `128×128`, DMA-BUF, `ShellRoot.qml` (com efeitos VALO V2) | Alto risco (`highRisk = true`) | Bloqueado sem autorização (`exit 3`) |
| **D–H** | Progressão em `1920×1080` aproximando-se integralmente da composição e carga do Frame 1 na TTY3 | Alto risco (`highRisk = true`) | Bloqueados sem autorização (`exit 3`) |

---

## 5. Executor Experimental (`tools/homologation/run_offscreen_experiment.sh`)

O executor `tools/homologation/run_offscreen_experiment.sh` implementa controle estrito para execução assistida:

- Aceita apenas um cenário por invocação: `--scenario <A..H> [--frames N] [--authorize]`.
- Recusa execução como `root`.
- Habilita `SGE_GRAPHICS_DIAGNOSTICS=1`.
- Registra timestamp, commit, cenário e código de saída em `$HOME/systalia-homolog-logs/offscreen-exp-<scenario>-<timestamp>.log`.
- Exige `--authorize` para cenários `C–H`; sem essa flag, retorna código `3` com `[BLOCKED]`.
- Validado com `bash -n` e teste de bloqueio sem `--authorize` (`./tools/homologation/run_offscreen_experiment.sh --scenario C` → saída `3`, `[BLOCKED] Cenário C requer --authorize explícito após aprovação humana.`).

---

## 6. Testes Seguros Executados (Após Edições Finais)

1. **Build Release com `-Werror`:** Compilação limpa em diretório temporário isolado (removido após validação).
2. **Testes específicos DRM/Vulkan:** `11/11` aprovados em `2.23 s` (incluindo `tst_vulkan_dmabuf_audit` em `0.29 s`).
3. **Suíte completa CTest:** `59/59` aprovados (`100% tests passed, 0 tests failed out of 59`) em `50.90 s` (`38 unit`, `20 integration`, `1 e2e`).
4. **Harness seguro (Cenários A e B, 2 frames cada, `SGE_GRAPHICS_DIAGNOSTICS=1`):**
   - `[PASS] Cenário A concluiu 2 frames.`
   - `[PASS] Cenário B concluiu 2 frames.`
5. **Smoke tests do `systalia-session` instalado:**
   - Modo headless (`QT_QPA_PLATFORM=offscreen`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.
   - Modo nested Wayland (`QT_QPA_PLATFORM=wayland`, `SGE_DISABLE_AUDIO=1`): `poll_before_sigterm=None`, `sigterm_rc=0`.

---

## 7. Cenários Bloqueados (`C–H`)

Nenhum cenário de alto risco foi executado automaticamente nesta etapa:

- `sge_vulkan_dmabuf_harness --scenario C` retornou código `3`:
  `[BLOCKED] Cenário C (128x128) bloqueado por segurança. Requer --allow-gpu-risk e SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1.`
- `./tools/homologation/run_offscreen_experiment.sh --scenario C` (sem `--authorize`) retornou código `3`:
  `[BLOCKED] Cenário C requer --authorize explícito após aprovação humana.`
- Cenários `D–H` permanecem igualmente bloqueados atrás de autorização explícita e **não** foram executados.
- Nenhum teste físico DRM/KMS (`SGE_USE_DRM_BACKEND=1`, `TakeDevice`, troca de VT/TTY3) foi executado.

---

## 8. Evidências de Conclusão GPU

Durante a execução segura do **Cenário B** com `SGE_GRAPHICS_DIAGNOSTICS=1`, foi observado o ciclo real completo por frame:

- **Frame 1:** `frameId=1 slotId=1 size=QSize(128, 128) vkFormat=44 modifier=0x200000000401a01 expectedLayout=UNDEFINED->COLOR_ATTACHMENT_OPTIMAL` → `CPU_COMMAND_RECORDED` → `GPU_SUBMITTED` → `SYNC_FD exported` → `commit submitted` → `GPU_EXECUTION_COMPLETED frameId=1 slotId=1 syncFd=42 gpuLatencyUs=27` → `MOCK_PAGE_FLIP_COMPLETED`.
- **Frame 2:** `frameId=2 slotId=2 ... gpuLatencyUs=10` → `MOCK_PAGE_FLIP_COMPLETED`.

Os cenários `C–H` não foram executados, portanto não há telemetria de latência GPU para eles nesta etapa.

---

## 9. Riscos Restantes

- **GPU compartilhada com a sessão ativa (KDE Plasma):** Como a GPU AMD Radeon Cezanne/Renoir (`/dev/dri/card1`) é compartilhada com o compositor da sessão desktop, um `ring gfx timeout` no kernel AMDGPU provocado por carga problemática nos cenários `C–H` pode congelar ou derrubar a sessão gráfica inteira.
- **Limitação de `timeout`/`SIGKILL`:** Um `timeout` de processo ou `SIGKILL` no espaço de usuário **não** destrava uma fila/ring travada no kernel (`amdgpu: ring gfx timeout`); a recuperação depende do reset de GPU do driver ou reinicialização.

---

## 10. Recomendações para Execução Isolada

1. Executar **um cenário por vez**, começando com **`--frames 1`**, na ordem progressiva **`C → D → E → F → G → H`**, somente com autorização humana explícita para cada cenário.
2. Utilizar o executor controlado:
   `./tools/homologation/run_offscreen_experiment.sh --scenario C --frames 1 --authorize`
3. Inspecionar o log gerado em `$HOME/systalia-homolog-logs/offscreen-exp-<scenario>-<timestamp>.log` antes de autorizar o próximo cenário da sequência.
4. Recomenda-se que o **Cenário C** (`128×128`, `ShellRoot.qml` + VALO V2) seja o primeiro experimento autorizado, por isolar a carga QML/VALO V2 ainda em baixa resolução antes da transição para `1920×1080` (`D–H`).

---

## 11. Critérios de Interrupção

A progressão experimental deve ser **interrompida imediatamente** caso ocorra qualquer um dos eventos abaixo:

- `VK_ERROR_DEVICE_LOST`
- `GPU_COMPLETION_TIMED_OUT`
- Erro de memória / alocação Vulkan ou DMA-BUF
- Congelamento visual, travamento de sessão ou evento `ring gfx timeout` no `dmesg`/journal

---

## 12. Decisão de Segurança

| Categoria | Classificação | Justificativa |
| --- | --- | --- |
| **Testes automatizados (Build Release `-Werror`, CTest 59/59, Harness A–B, Smoke tests)** | **APTO** | Todos os testes seguros passaram após as edições finais (`59/59` CTest em `50.90 s`, `11/11` DRM/Vulkan em `2.23 s`, `A` e `B` aprovados, smoke tests headless e nested com `sigterm_rc=0`). |
| **Cenário C (`128×128`, `ShellRoot.qml` + VALO V2)** | **APTO (primeiro experimento autorizado recomendado)** | Isolado em `128×128`, protegido por `--authorize`, `--allow-gpu-risk` e `SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1`, pronto para execução assistida de 1 frame. |
| **Cenários D–H (`1920×1080`)** | **APTO para progressão controlada um-a-um após C** | Devem ser executados apenas sequencialmente (`D → E → F → G → H`), com `--frames 1` inicial e autorização humana explícita por cenário após aprovação do anterior. |
| **Homologação física TTY3 (`SGE_USE_DRM_BACKEND=1`)** | **NÃO APTO (BLOQUEADA)** | Permanece bloqueada até a conclusão integral e sem falhas dos cenários offscreen `C–H`. |
