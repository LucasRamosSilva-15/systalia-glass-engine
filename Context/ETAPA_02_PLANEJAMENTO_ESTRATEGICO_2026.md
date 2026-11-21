# SYSTALIA — Etapa 02: Planejamento Estratégico aprovado

**Versão documental:** 1.0 — consolidação pós-aprovação  
**Data das aprovações:** 21 de novembro de 2026  
**Estado:** **Etapa 02 concluída no planejamento (02.1–02.7)**  
**Fonte de autoridade:** escolhas explícitas do responsável pelo SYSTALIA nas conversas de planejamento.  
**Alcance desta consolidação:** decisões de produto/engenharia, não certificação de código, testes ou instalação.  
**Etapa seguinte:** 03 — Planejamento Técnico da Alpha 0.1 (03.1 auditoria inicial, 03.2 próxima).

> **Para IA sucessora:** este documento atualiza o estado do dossiê histórico `SYSTALIA_VISION_2027_HANDOFF.md` (17/11/2026). O handoff original contém 25 decisões da Etapa 01, ainda válidas. Seu aviso de que a Etapa 02 não começou era uma fotografia daquela data. **Não reabrir decisões aprovadas nem presumir funcionalidades homologadas.**

---

## 0. Vocabulário obrigatório e hierarquia de evidências

- **SYSTALIA / SDE:** ambiente desktop Linux desejado.
- **SGE:** Systalia Glass Engine, base de compositor Wayland, shell e VALO.
- **VALO:** Vibrant, Adaptive, Lucid, Organic. A legibilidade, previsibilidade e acessibilidade precedem beleza e complexidade do material.
- **VALO Graphics V2:** renderer óptico atual, diferente das APIs históricas do renderer V1.
- **ADRs:** registros de decisões arquiteturais relevantes. **SDPs:** registros/propostas relevantes de produto e design.
- **Aprovado:** o responsável escolheu a política; **implementado:** há código que a concretiza; **testado:** há evidência de execução sob condições identificadas; **homologado:** o conjunto de critérios foi cumprido no ambiente e hardware especificados. Nenhuma dessas palavras substitui a outra.
- **Hierarquia:** aprovações explícitas do responsável → decisões versionadas atualizadas → código/contratos efetivos para afirmações técnicas → relatórios com data/ambiente/evidências. Documentos históricos não revogam decisões posteriores.

## 1. Etapa 02.1 — Missão, visão e posicionamento

**Estado: APROVADA em 21/11/2026.** A redação de missão e a visão propostas na sessão foram aprovadas expressamente pelo responsável.

**Missão aprovada (redação consolidada):**

> O SYSTALIA é um ambiente desktop Linux experimental, especializado em experiência visual, acessibilidade e produtividade para desenvolvedores e estudantes. Com seu compositor Wayland, shell e linguagem VALO, procura oferecer materiais visualmente expressivos sem sacrificar legibilidade, desempenho, privacidade e previsibilidade. Uma arquitetura modular e o reaproveitamento de serviços Linux sustentam o projeto; decisões documentadas, testes e colaboração orientam sua evolução.

**Visão 2030 aprovada:** tornar o SYSTALIA um desktop Linux utilizável e reconhecível por sua identidade VALO, sustentado por comunidade e equipe pequena, com contribuições técnicas ao ecossistema Linux. Não competir com KDE/GNOME em quantidade de funcionalidades.

**Posicionamento:** experiência diferenciada em conforto visual, acessibilidade e produtividade por atividades, voltada inicialmente a desenvolvedores e estudantes, sem excluir outros públicos. **Não** criar distribuição Linux própria. Compositor/shell/VALO próprios, serviços Linux existentes reutilizados.

**Princípios:** Vibrant (expressividade útil), Adaptive (perfis e capacidade), Lucid (clareza e legibilidade como limites), Organic (transições controláveis e redutíveis). Inspiração Aero/skeuomorphism é referência histórica, não obrigação de replicação.

## 2. Etapa 02.2 — Requisitos e limites

**Estado: APROVADA no planejamento.** Cinco decisões expressas:

| ID | Política aprovada | Implicação e limite |
|---|---|---|
| **02.2-A** | **Qualidade equilibrada com requisitos mínimos obrigatórios** | Nenhuma dimensão compensa falhas críticas de outra; estabilidade, acessibilidade, desempenho e consistência visual exigem mínimos independentes. Privacidade e segurança são transversais. |
| **02.2-B** | **Fedora** como distribuição inicial de referência da Alpha 0.1 | Edição/versão exata a definir; outras distribuições podem ser adicionadas após testes. Não declarar Fedora homologado. |
| **02.2-C** | **Duas categorias de hardware: básico e recomendado** | Equipamentos, drivers, RAM e métricas não escolhidos; perfis Performance e Balanced/Quality são candidatos, não certificações. |
| **02.2-D** | **Matriz ampliada por toolkit desde a Alpha, Wayland nativo prioritário** | Abrangência inicial por tecnologia, não promessa universal de aplicativos. |
| **02.2-E** | **Núcleo obrigatório por toolkit, com limitações não críticas documentadas** | Fluxos essenciais selecionados precisam funcionar em cada toolkit obrigatório; falhas não críticas de aplicativos adicionais podem ser documentadas. |

### 2.1 Requisitos funcionais e prioridades do planejamento

| ID | Requisito | Prioridade e escopo |
|---|---|---|
| RF-01 | Instalar e executar sessão Fedora independente | P0 |
| RF-02 | Abrir, mover, redimensionar e fechar janelas Wayland | P0 |
| RF-03 | Foco, empilhamento, maximização e restauração | P0 |
| RF-04 | Workspaces organizados por atividade | P0 |
| RF-05 | Dock e TopBar funcionais | P0 |
| RF-06 | Perfis básicos de acessibilidade e ajustes individuais | P0 |
| RF-07 | VALO Glass adaptativo com degradação dos efeitos | P0 |
| RF-08 | Controles internos VALO coerentes e acessíveis | P0 |
| RF-09 | Uso essencial por teclado e mouse | P0 |
| RF-10 | Quick Actions: comando preparado no terminal **sem autoexecução** | P1, **opcional** na Alpha |
| RF-11 | Núcleo de aplicativos selecionados por toolkit sob Wayland nativo | P0 |
| RF-12 | Expansão da compatibilidade XWayland | P1, **posterior** à Alpha 0.1; base experimental pode existir |
| RF-13 | Estilo VALO Qt Quick reutilizável | P1; evolução progressiva |
| RF-14 | Extensão opcional de IA local | P2, **fora da Alpha** |

**Importante:** prioridades RF/RNF são a especificação operacional consolidada no planejamento, sujeita a detalhamento técnico e matrizes de aceitação. Não há autorização para introduzir recursos além do escopo aprovado.

### 2.2 Requisitos não funcionais

- **RNF-01 Estabilidade:** nenhum defeito crítico conhecido no conjunto de fluxos homologados; ciclo de vida e recuperação testados.
- **RNF-02 Acessibilidade:** uso por teclado, foco, legibilidade, contraste, escala e redução de movimento.
- **RNF-03 Desempenho:** CPU, GPU, RAM, frametimes e latência medidos em hardware de referência; valores determinados por dados reais.
- **RNF-04 Consistência VALO:** componentes, interação e estados respeitam tokens e perfis, inclusive sem blur avançado.
- **RNF-05 Privacidade:** dados locais por padrão; sem envio automático de telemetria.
- **RNF-06 Compatibilidade:** fluxo de trabalho real no núcleo homologado de cada toolkit.
- **RNF-07 Reprodutibilidade:** build, instalação, testes e documentação reproduzíveis.

### 2.3 Cobertura de aplicativos e cenários

Toolkits obrigatórios definidos: **Qt Quick (Qt 6), Qt Widgets (Qt 6) e GTK (GTK 3 e/ou GTK 4 conforme matriz final)**, todos em Wayland nativo nos cenários obrigatórios. Terminais, editores e navegadores são categorias de fluxo, não homologação automática de marcas específicas.

Para cada aplicativo selecionado, testar quando aplicável: inicializar/encerrar, renderizar e atualizar buffers, teclado/mouse, foco e empilhamento, mover/resize/maximizar/restore, menus/popups/diálogos, clipboard dentro/entre clientes, workspaces e ciclo de vida. XWayland é progressivo e não precisa ser universal na primeira Alpha.

**Política de homologação 02.2-E:** requisitos de um **núcleo por toolkit** devem passar; limitações não críticas de aplicativos adicionais podem constar em lista pública de problemas conhecidos. Definir posteriormente quais aplicativos, versões, conjunto de cenários e gravidade dos defeitos constituem o núcleo; não inventar números.

### 2.4 Gate de lançamento

Bloqueadores propostos/ratificados em princípio pela aprovação dos critérios da 02.4: falhas críticas de sessão, entrada, buffers, acessibilidade, privacidade, compatibilidade do núcleo ou desempenho inviável no equipamento de referência. Um resultado visual excepcional **não** compensa falha em qualquer eixo obrigatório.

### 2.5 Hardware e ambiente: pendências legítimas

- Fedora aprovado como referência, **sem versão/edição fechada**.
- Hardware **básico e recomendado** aprovado **apenas como categorias**. Nenhum dispositivo, GPU, CPU ou configuração foi homologado.
- Perfis gráficos candidatos Performance/Balanced/Quality; não declarar FPS, latência, RAM ou VRAM mínimos antes dos benchmarks.
- Diferenciar testes em compositor **aninhado** e sessão **independente**. Uma captura/execução em Xvfb/Wayland host não prova sessão Fedora autônoma.

## 3. Etapa 02.3 — Arquitetura estratégica

**Estado: APROVADA no planejamento.** Cinco decisões expressas:

| ID | Política aprovada | Consequência |
|---|---|---|
| **02.3-A** | **Núcleo modular integrado, separação progressiva** | Sem obrigação inicial de transformar compositor, shell e graphics em processos isolados. |
| **02.3-B** | **Contratos explícitos e dependências unidirecionais, integração preservada** | Interfaces claras, sem acoplamentos circulares de implementação. Manter `sge_core` enquanto conveniente. |
| **02.3-C** | **Propriedade única de estado por domínio, comunicação orientada a eventos** | Fonte autoritativa por domínio; consumidores solicitam mudanças e recebem eventos confirmados. |
| **02.3-D** | **Compatibilidade controlada por maturidade** | Proteção de protocolos, segurança e ABIs; APIs experimentais evoluem com rastreabilidade e migração proporcional. |
| **02.3-E** | **Extração orientada por evidências e necessidade técnica** | Separar bibliotecas/processos se benefícios demonstrados superarem custos. |

### 3.1 Domínios e proprietários lógicos — proposta de contrato

| Domínio | Dono lógico | Não deve assumir |
|---|---|---|
| Compositor | Superfícies, buffers, foco, input, outputs | Políticas de aparência do VALO |
| Shell | Apresentação, Dock, TopBar, ações de UI | Propriedade direta dos buffers de clientes |
| VALO Graphics V2 | Recursos ópticos, shaders, materiais, composição | Ciclo de vida de processos da sessão |
| VALO Theme/Controls | Tokens, estados visuais, estilos e perfis | Autoridade sobre superfícies/seat |
| Services | Aplicações, notificações, sistema e sessão | Renderização óptica |
| Extensões futuras | Recursos opcionais e permissionados | Dependências mandatórias do núcleo |

A distribuição exata dos estados de **workspaces** e **preferências de acessibilidade** entre serviços/modelos ainda requer inspeção de código e definição de interfaces. Fonte de verdade única não exige processo independente.

### 3.2 Contratos de interação (CA-01 a CA-07)

1. **CA-01 Superfícies Wayland:** compositor responde por criação, buffer, apresentação e destruição.
2. **CA-02 Foco:** shell solicita; compositor valida, altera e publica confirmação.
3. **CA-03 Workspaces:** serviço/modelo de workspaces mantém organização lógica, coordenando superfícies e foco com o compositor.
4. **CA-04 Renderização:** VALO Graphics controla recursos e propriedades ópticas.
5. **CA-05 Aparência/acessibilidade:** preferências centralizadas alimentam Theme, Controls, Shell e efeitos gráficos.
6. **CA-06 Aplicações/processos:** services lançam e acompanham processos por interfaces explícitas.
7. **CA-07 Apresentação:** shell controla estados de seus componentes próprios.

**Estado:** contratos conceituais derivados de decisões. Não presumir que o código os implemente integralmente. Fontes de verdade, ciclo de vida, ownership e sinais devem ser auditados antes de refatoração.

### 3.3 Regras de comunicação

- C++ direto/interfaces bem definidas para operações locais síncronas.
- Sinais, propriedades Qt/QML e eventos para comunicar alterações confirmadas de estado.
- Operações assíncronas e IPC/D-Bus somente onde fronteiras, privilégios, custo/latência ou terceiros justificarem.
- Observação/evento **não significa** aplicar estado antes da confirmação, nem autoriza ciclos de dependência de implementação.
- Não criar comunicação entre processos apenas para aparentar modularidade.

### 3.4 Maturidade e versionamento de contratos

| Classe | Tratamento |
|---|---|
| **Protegido** | Protocolo Wayland, segurança, layouts GPU/ABI, ciclo de vida de recursos: analisar compatibilidade, versionar/migrar e testar mudanças. |
| **Estável** | Interfaces públicas conhecidas: preferir compatibilidade; depreciação e migração para alterações incompatíveis. |
| **Experimental** | Interfaces internas em desenvolvimento: evoluir com registros e testes proporcionais. |

Contratos que **exigem cautela**: URI QML `Systalia.Engine 1.0`, aliases QRC, `z_systalia_surface_v1.xml`, layouts GPU std140, persistência e interfaces de integração. **Versão da API QML ≠ versão do produto; protocolo V1 ≠ renderer óptico V1.** Verificar tamanho e offsets da ABI **V2** no código, não usar os 128 bytes do antigo blueprint como única fonte.

Mudança incompatível requer: identificar contrato/consumidores → impacto e riscos → preservar/adaptar/versionar/migrar → testar → documentar ADR e aprovação humana proporcionais ao impacto.

### 3.5 Quando extrair módulos

Somente mediante benefício demonstrado em segurança/isolamento de privilégios, isolamento de falhas, performance, reutilização, testes ou manutenção, acompanhado de análise dos custos de IPC, links, carregamento Qt/QML, init, ownership e compatibilidade. Nenhum calendário obrigatório para divisão do `sge_core`.

## 4. Etapa 02.4 — Qualidade e acessibilidade

**Estado: APROVADA no planejamento.**

- **02.4-A:** **Modelo híbrido com três pilares obrigatórios:** testes automatizados, avaliações manuais em sessões reais e testes com usuários (incluindo necessidades distintas de acessibilidade). Nenhum dos três elimina os demais.
- **02.4-B:** **Cinco critérios obrigatórios**, aprovados como pacote:

| Dimensão | Condição aprovada |
|---|---|
| Estabilidade | Nenhuma falha crítica conhecida nos fluxos homologados |
| Acessibilidade | Teclado, foco visível, contraste, escala e redução de movimento |
| Desempenho | Funcionamento demonstrado nas categorias básico/recomendado após testes reais |
| VALO | Efeitos nunca prejudicam leitura ou utilização |
| Compatibilidade | Núcleo selecionado de Qt Quick, Qt Widgets e GTK funciona em Wayland |

**Transversais:** privacidade local, sem telemetria automática; segurança, documentação e instalação reprodutível.

**Pendente:** limiares quantitativos, cenário e duração dos testes, seleção de pessoas, consentimentos, dispositivos e matriz final. Não afirmar que já foram executados testes humanos.

## 5. Etapa 02.5 — Escopo da Alpha 0.1

**Estado: APROVADA no planejamento.**

### Obrigatório

1. Sessão desktop **independente no Fedora**, instalada e inicializável.
2. Compositor Wayland e gerenciamento funcional de janelas.
3. Dock, TopBar e workspaces funcionais.
4. VALO Graphics V2 com perfis adaptáveis e fallback acessível.
5. Perfis básicos de acessibilidade.
6. Aplicativos selecionados **Qt Quick, Qt Widgets e GTK** homologados no núcleo de cada toolkit.
7. Instalação, documentação e testes reproduzíveis.

### Opcional na Alpha 0.1

- Quick Actions com terminal **preparado, sem executar automaticamente**.

### Posterior / não prometer na Alpha 0.1

- Compatibilidade XWayland **ampliada**; a implementação experimental existente não a torna compromisso de suporte integral.
- IA local integrada, plataforma pública de plugins, tema universal GTK/Qt Widgets, restauração avançada de apps, todos os hardwares/monitores/distribuições.

**Sem data de release fixada**, sem hardware homologado, sem meta numérica arbitrária. Licença ainda pendente.

## 6. Etapa 02.6 — Governança

**Estado: APROVADA no planejamento.**

| Área | Política |
|---|---|
| **Git/GitHub** | Fonte de verdade do código, decisões, testes e documentação versionada |
| **Mintlify** | Portal central de documentação planejado; **não** considerar implantado sem comprovação |
| **ADRs** | Decisões relevantes de arquitetura e contratos |
| **SDPs** | Propostas/decisões relevantes de produto e design |
| **IA** | Auxilia na implementação/revisão/documentação; aprovação humana para decisões estratégicas |
| **Testes** | Evidências e contexto de hardware/driver/versão antes de alegações de homologação |
| **Colaboração** | Contribuições abertas com revisão, conforme política/licença futura |

Não registrar mensagens ou tentativas descartadas como decisões aprovadas. Evitar burocracia em tarefas simples. Dados/logs privados e segredos não devem entrar em PRs.

**Licença:** não escolhida; avaliar antes de distribuição formal ou política ampla de colaboração.

## 7. Etapa 02.7 — Roadmap aprovado

| Marco | Objetivo | Limites |
|---|---|---|
| **Alpha 0.1** | Desktop Fedora utilizável, Wayland, Shell/VALO V2, acessibilidade e apps homologados | Gated por qualidade; data indefinida |
| **Alpha 0.2** | Correções, compatibilidade ampliada, progressos XWayland, hardware e UX | Detalhes sujeitos a evidências |
| **Beta** | Consolidar experiência, testes humanos, acessibilidade, estabilidade e documentação | Sem promessa de cobertura universal |
| **1.0** | Primeira versão estável após critérios demonstrados | Sem calendário fixo |
| **Pós-1.0** | Ecossistema opcional, produtividade e possível IA local | Viabilidade e consentimento a avaliar |

**IA 1.1.0:** no dossiê histórico havia intenção de IA local opcional na versão `1.1.0`. Essa intenção **não é promessa de data**, não integra a Alpha e **não obriga** que IA seja lançada na primeira versão pós-1.0. Preserve a distinção entre intenção e roadmap de release aprovado.

## 8. Inventário consolidado das aprovações — não reabrir

| Etapa | Identificadores | Resultado |
|---|---|---|
| 01 | 01–25 | 25 decisões estratégicas do handoff de 17/11; preservadas integralmente |
| 02.1 | missão/visão | Aprovadas em 21/11 |
| 02.2 | 02.2-A–E | Requisitos, Fedora, categorias hardware, matriz por toolkit e gate |
| 02.3 | 02.3-A–E | Arquitetura integrada, contratos, estado único, maturidade, evidências |
| 02.4 | 02.4-A–B | Três pilares de validação e cinco critérios obrigatórios |
| 02.5 | escopo Alpha | Obrigatório/opcional/posterior aprovado |
| 02.6 | governança | Git/Mintlify, ADR/SDP, revisão humana |
| 02.7 | roadmap | Alpha 0.1 → Alpha 0.2 → Beta → 1.0 → pós-1.0 |

As subfases 02.6 e 02.7 foram aprovadas em conjunto. **A Etapa 02 encerrou-se no planejamento.**

## 9. Handoff operativo

Ao receber uma tarefa futura, uma IA deve:

1. Ler `Context/README.md`, este documento e o handoff integral da Etapa 01.
2. Respeitar as aprovações existentes; só solicitar novas escolhas quando realmente alterarem a direção ou bloquearem a execução.
3. Inspecionar a branch/commit atuais e `docs/README.md` antes de inferir estado de implementação.
4. Preservar contratos QML, Wayland e GPU segundo o **código atual**, com cuidado especial para alias de recursos.
5. Manter a distinção entre **nativo Wayland**, **XWayland progressivo**, **sessão aninhada** e **sessão Fedora independente**.
6. Propor mudanças técnicas com impacto, testes, evidências e fallback; não executar mudanças não autorizadas.
7. **Prioridade imediata da Etapa 03:** trabalhar nos bloqueadores reais para a Alpha, não iniciar nova reorganização de diretórios nem intensificar efeitos decorativos.
8. Usar a pasta `Context/` como contexto persistente de decisões; manter relatórios técnicos detalhados em `docs/` e vincular suas evidências.

## 10. Questões que continuam abertas (não são decisões ausentes por negligência)

- Versão/edição Fedora e matriz de pacotes/dependências finais.
- Hardware básico/recomendado real, GPUs/drivers, medições e limites mensuráveis.
- Aplicativos/versões/cenários oficiais por toolkit e classificação de defeitos.
- Perfis específicos e parâmetros de acessibilidade, testes de usuários reais.
- Política de licença, participação pública e revisão de contribuições.
- Disponibilização real do Mintlify.
- Data de lançamento da Alpha, dependente de homologação, **não decidida**.

**Fim da consolidação da Etapa 02.**
