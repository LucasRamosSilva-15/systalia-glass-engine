# SYSTALIA — Contexto canônico para agentes de IA

**Última consolidação de planejamento:** 21/11/2026  
**Produto:** SYSTALIA Desktop Environment (SDE)  
**Implementação:** Systalia Glass Engine (SGE)  
**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch técnica de referência para a auditoria:** `valo-graphics-2.0`

> **Leia antes de editar:** esta pasta é um pacote de transferência de contexto entre IAs. Não constitui evidência de build, de testes realizados nem de homologação. Informações de implementação podem ficar obsoletas e devem ser verificadas contra o commit atual.

## Ordem de leitura para qualquer IA

1. **[ETAPA_02_PLANEJAMENTO_ESTRATEGICO_2026.md](ETAPA_02_PLANEJAMENTO_ESTRATEGICO_2026.md)** — registro aprovado e atualizado das sete fases da Etapa 02; inclui missão aprovada, requisitos, arquitetura, qualidade, escopo Alpha e roadmap. **Prevalece sobre o handoff histórico quando houver divergência de estado das decisões.**
2. **[SYSTALIA_VISION_2027_HANDOFF.md](SYSTALIA_VISION_2027_HANDOFF.md)** — texto original **integral**, datado de 17/11/2026, contendo as **25 decisões da Etapa 01** e amplo histórico técnico/estratégico. **Preservado sem reescrever as decisões.**
3. **[ETAPA_03_ESTADO_E_PROXIMAS_ACOES.md](ETAPA_03_ESTADO_E_PROXIMAS_ACOES.md)** — situação do planejamento técnico 03.1, evidências consultadas, lacunas para a Alpha e limites da auditoria.
4. **Documentação técnica no repositório:** `docs/README.md`, `docs/architecture/adr/`, `docs/architecture/valo-v1-v2/CONSOLIDACAO_06_6E.md`, `cmake/`, `tests/` e código real.
5. **Históricos anteriores:** `SGE_MASTER_BLUEPRINT.md` e `SGE_IMPLEMENTATION_PLAN.md`. Foram escritos em outubro de 2026, antes da reorganização 06.6B–06.6E. Conservam decisões e contexto históricos, mas seus caminhos `src/gfx/`, `src/qml/`, `src/system/` e afirmações de status não devem ser tomados como topologia atual.

## Linha do tempo e precedência

| Período | Fato | Estado |
|---|---|---|
| 17/11/2026 | Handoff original da Etapa 01, 25/25 escolhas | Histórico preservado |
| 21/11/2026 | Etapa 02.1–02.7 concluída com aprovações expressas do responsável | **Decisões estratégicas aprovadas no planejamento** |
| 21/11/2026 | Etapa 03 iniciada, auditoria 03.1 efetuada por leitura parcial do repositório | **Auditoria inicial, não homologação** |
| A seguir | 03.2: sessão Fedora independente; 03.3+: compositor, compatibilidade, acessibilidade, desempenho | Planejado, **não executado por esta PR** |

**Nota sobre o documento histórico:** `SYSTALIA_VISION_2027_HANDOFF.md` afirma corretamente, *em 17/11*, que a Etapa 02 ainda não havia iniciado e que a missão textual era uma proposta. As decisões de 21/11 **substituem apenas esses estados antigos**: a missão e a visão foram aprovadas, e a Etapa 02 foi encerrada. **Não alterar retroativamente a fonte histórica.**

## Regras obrigatórias para agentes de IA

- **Não perguntar novamente** as 25 decisões da Etapa 01 ou as escolhas aprovadas da Etapa 02, salvo pedido explícito do responsável.
- Diferenciar rigorosamente **proposta**, **aprovada no planejamento**, **implementada no código**, **testada** e **homologada em hardware real**.
- Não declarar Fedora, Qt/GTK, XWayland, DRM/KMS, GPUs ou perfis de acessibilidade homologados sem evidências diretas.
- **Git como fonte de verdade; Mintlify como portal planejado**, não presumir que o portal foi publicado.
- Preservar os contratos atuais necessários: `Systalia.Engine 1.0`, protocolo `z_systalia_surface_v1`, aliases QRC e ABIs/shaders conforme o código **atual**, sem inferir ABI VALO V2 a partir de documentos V1.
- **Wayland nativo prioritário**; XWayland progressivo; experiência acessível e legível acima dos efeitos gráficos.
- Não enviar telemetria automaticamente; dados locais por padrão; IA local futura é opcional e depende de consentimento.
- Qualquer mudança substancial deve indicar motivo, dono do contrato, impactos, testes e ADR/SDP quando cabível; revisão humana permanece necessária.
- **Não executar alterações de código por iniciativa própria** ao receber estes arquivos. Pedir autorização para a tarefa específica; PR documental não homologa o projeto.
- **Não inventar** métricas de FPS/RAM, hardware aprovado, data de lançamento, equipe, licença, publicação Mintlify ou resultados de testes.
- Escrever em português brasileiro; ser objetivo ao pedir decisões e evitar questionários repetitivos.

## Estado de decisões versus evidências

| Item | Decisão de produto | Evidência de implementação |
|---|---|---|
| Fedora como referência | Aprovada | Sessão independente ainda não homologada |
| Hardware básico e recomendado | Aprovada como **categorias** | Equipamentos e limites quantitativos indefinidos |
| Qt Quick, Qt Widgets e GTK | Núcleo por toolkit exigido | Matriz real de homologação pendente |
| VALO Graphics V2 | Renderer oficial no projeto | Relatórios e testes existem; validar no host alvo |
| IA local | Extensão opcional futura | Fora do escopo Alpha 0.1 |
| Licença | Ainda não escolhida | Não presumir licença open source concedida |

## Atualização deste contexto

Em cada marco relevante, atualizar **primeiro o documento correspondente à etapa**, depois o índice de continuidade. Não substituir uma decisão aprovada sem registrar sua revogação. Documentar a evidência com data, branch e commit. Não reescrever arquivos históricos para esconder divergências: criar uma nota de atualização ou nova versão e referenciar a anterior.
