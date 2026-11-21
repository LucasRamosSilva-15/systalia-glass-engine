# SYSTALIA — Dossiê de Continuidade e Visão Estratégica 2027

**Versão do documento:** 1.0 — consolidação da Etapa 01  
**Data de referência:** 17 de novembro de 2026  
**Situação:** Etapa 01 (25/25 decisões) concluída; Etapa 02 ainda não iniciada  
**Uso recomendado:** transferência de contexto para outra IA, referência de planejamento, base para documentação Git/Mintlify  
**Idioma principal:** português brasileiro

> **Instrução a agentes de IA que lerem este documento:** este arquivo contém decisões estratégicas expressamente escolhidas durante uma sessão de 25 perguntas. **Não substitua essas decisões por suas próprias preferências.** Diferencie sempre o que está decidido, o que é proposta técnica derivada e o que exige inspeção/testes. Consulte o repositório e as evidências atuais antes de declarar algo implementado ou homologado. Não inicie uma nova rodada de perguntas sobre as 25 decisões já tomadas.

---

## 1. Resumo executivo

O **SYSTALIA** é um projeto experimental de **ambiente desktop para Linux**, baseado em um **compositor Wayland próprio**, uma **desktop shell** com componentes próprios e um motor de efeitos visuais chamado **VALO Graphics / VALO Glass**. Sua intenção não é se tornar uma nova distribuição Linux, um sistema operacional independente ou competir com KDE Plasma e GNOME em volume de funcionalidades. Quer oferecer uma experiência **visual diferenciada, acessível e produtiva**, sobretudo para **desenvolvedores e estudantes de tecnologia**, com um núcleo especializado e relativamente leve.

**Visão para 2030, decidida pelo responsável pelo projeto:** um ambiente desktop **utilizável**, de identidade **VALO reconhecida**, sustentado por uma **comunidade e pequena equipe**, capaz de realizar **contribuições técnicas ao ecossistema**, sem competir pela quantidade de recursos.

**Primeira entrega pública desejada:** **SYSTALIA Alpha 0.1**, uma versão **utilizável para desenvolvedores**, com **Wayland nativo priorizado** e escopo intencionalmente limitado. O estado atual do software **não deve ser confundido** com essa meta de lançamento.

**Princípios que orientam as escolhas:** qualidade verificável em estabilidade, acessibilidade, desempenho e consistência visual; privacidade local por padrão; perfis de acessibilidade; workspaces por atividade; documentação e decisões versionadas em Git; IA como ferramenta de desenvolvimento e como **extensão opcional** para usuários, nunca dependência obrigatória da shell.

### 1.1 Terminologia

- **SYSTALIA / Systalia Desktop Environment (SDE):** produto desktop completo desejado, em desenvolvimento.
- **Systalia Glass Engine (SGE):** projeto de compositor/shell gráfica e implementação do motor óptico, relacionado ao ambiente SDE.
- **VALO:** linguagem visual identificada pelos princípios **Vibrant, Adaptive, Lucid, Organic**.
- **VALO Glass / VALO Graphics 2.0:** sistema de materiais gráficos, shaders, blur e efeitos ópticos no Qt/Scene Graph/QRhi.
- **VALO Theme:** tokens e definições centralizadas de aparência.
- **VALO Controls:** biblioteca/estilo planejado para controles QML coerentes com a identidade VALO.
- **SYSTALIA Quick Actions:** proposta de menu de ações comuns (consultas e comandos Linux) com evolução incremental.
- **SYSTALIA AI:** extensão opcional futura com possibilidade de IA local; **1.1.0 é intenção de versão, não garantia de entrega**.

---

## 2. Registro integral das 25 decisões da Etapa 01

Cada subseção registra **a decisão efetivamente escolhida**, seu significado, implicações e limites. Ideias de implementação são identificadas como **propostas**, não como decisões aprovadas automaticamente.

### Decisão 01 — Público-alvo: desenvolvedores e estudantes de tecnologia

**Decisão aprovada:** o público principal do SYSTALIA são **desenvolvedores e estudantes de tecnologia**.

**Implicações:** estudar e programar são fluxos de referência: editor, terminal, navegador, documentação, pesquisa e multitarefa. Facilidade de descoberta e acessibilidade importam tanto quanto atalhos avançados, porque o público inclui iniciantes e pessoas com diferentes habilidades e necessidades.

**Limite:** esse foco **não** torna o ambiente exclusivo para programadores; apenas orienta quais problemas são resolvidos primeiro. Não pressupor que todo desenvolvedor utiliza o mesmo editor, shell ou distribuição.

### Decisão 02 — Problema prioritário: conforto visual e acessibilidade

**Decisão aprovada:** o problema que o SYSTALIA deseja resolver primeiro é **conforto visual e acessibilidade**.

**Implicações:** priorizar legibilidade, hierarquia, contraste, escalabilidade, previsibilidade, redução de distrações e controles acessíveis. Os efeitos do VALO são meios, não justificativa para comprometer leitura e interação. O conforto deve ser investigado com pessoas reais e critérios mensuráveis; não há uma solução única para todos.

**Limite:** não tratar “acessibilidade” como sinônimo de uma paleta bonita ou de um efeito de blur reduzido; ela inclui navegação, estados, foco, dimensões de alvo, preferências e formas de interação.

### Decisão 03 — Abordagem: perfis de acessibilidade

**Decisão aprovada:** a abordagem principal para conforto visual e acessibilidade será baseada em **perfis de acessibilidade**, em vez de depender apenas de configuração manual extensa ou adaptação totalmente automática.

**Exemplos propostos, não escopo confirmado:** Leitura/Estudo, Concentração, Alta Legibilidade, Movimento Reduzido. Perfis podem ajustar contraste, tipografia, transparência, intensidade de animações e elementos de distração.

**Requisitos derivados:** permitir ajustes individuais; preservar preferências ao trocar perfis; operar mesmo quando os efeitos GPU avançados estiverem desativados; não impor mudanças visuais inesperadas. Ainda é necessário pesquisar necessidades reais e verificar diretrizes de acessibilidade aplicáveis.

### Decisão 04 — Principal diferencial: experiência visual VALO Glass

**Decisão aprovada:** o principal diferencial perceptível do SYSTALIA será a **experiência visual com VALO Glass**.

**Implicações:** materiais com profundidade e reflexos, identidade própria e consistência entre shell, janelas e componentes. O usuário deve perceber coesão mesmo quando os efeitos ópticos estiverem reduzidos.

**Limite:** a aparência não tem permissão de superar legibilidade, acessibilidade, desempenho ou estabilidade. Não declarar vantagem sobre outros desktops sem testes de usuário e benchmarks.

### Decisão 05 — Arquitetura: híbrida e especializada

**Decisão aprovada:** manter **compositor Wayland, shell e VALO próprios**, mas reutilizar **serviços, bibliotecas e aplicativos Linux existentes**.

**Implicações:** não reconstruir navegador, gerenciador de pacotes, terminal, stack de áudio/rede ou aplicativos comuns sem motivo comprovado. Os componentes próprios existem para viabilizar diferenciais de interface, organização e acessibilidade.

**Limite:** desenvolver um compositor próprio permanece uma responsabilidade alta: buffers, sincronização, protocolos, input, renderização e compatibilidade exigem manutenção continuada.

### Decisão 06 — Primeira versão pública: Alpha utilizável para desenvolvedores

**Decisão aprovada:** a primeira versão pública será uma **Alpha utilizável para desenvolvedores**, e não apenas demonstração visual nem promessa de desktop completo.

**Intenção:** permitir sessões reais de programação e estudo com aplicações externas homologadas, janelas e workspaces funcionais, VALO Glass e recursos básicos de acessibilidade.

**Limite:** “Alpha” permite limitações documentadas, mas não justifica apresentar funções críticas quebradas como prontas. A data de lançamento **não** está definida. A sessão independente, integração com entrada e demais requisitos ainda precisam de critérios verificáveis.

### Decisão 07 — Produtividade: workspaces por atividade

**Decisão aprovada:** o recurso de produtividade prioritário será **workspaces organizados por atividade**.

**Exemplos:** Programação, Estudos, Pesquisa. Na Alpha, uma proposta mínima é permitir nomes e organização de janelas por workspace, mudança de área e movimentação entre áreas. Vincular perfis de acessibilidade ou restaurar sessões automaticamente são evoluções propostas, não obrigatórias na primeira versão.

**Limite:** evitar automações que alterem o contexto do usuário sem aviso e não transformar o recurso em um organizador complexo que dependa de IA.

### Decisão 08 — Compatibilidade: Wayland nativo primeiro, XWayland progressivo

**Decisão aprovada:** homologar aplicativos **Wayland nativos primeiro**; ampliar **XWayland** progressivamente.

**Implicações:** estabelecer matriz pequena e realista de ferramentas de desenvolvimento e estudo; testar Qt/GTK, buffers, entrada, popups, diálogos, clipboard e ações da shell. Compatibilidade X11/XWayland continua no roadmap, sem bloquear toda a Alpha se o escopo público for transparente.

**Limite:** “um Alacritty abre” não comprova compatibilidade geral Wayland. Não declarar XWayland pronto enquanto houver defeitos de apresentação conhecidos.

### Decisão 09 — Hardware: ampla variedade com perfis gráficos adaptativos

**Decisão aprovada:** buscar **ampla variedade de hardware** por meio de **perfis gráficos adaptativos**.

**Base existente:** modos gráficos Performance, Balanced e Quality no VALO; o alcance de hardware real depende da GPU, drivers, backends e testes.

**Implicações:** investigar escolha inicial de perfil, orçamento CPU/GPU, memória, resolução e efeitos. O usuário deve ter possibilidade de controle; adaptações automáticas precisam ser previsíveis. Acessibilidade nunca deve ser reduzida em nome de desempenho.

**Limite:** não afirmar “leve em qualquer computador” nem compatibilidade com hardware antigo até efetivamente testar os backends e estabelecer requisitos mínimos.

### Decisão 10 — Desenvolvimento e manutenção: IA + comunidade + equipe pequena

**Decisão aprovada:** utilizar um **modelo híbrido**, combinando agentes de IA, contribuições open source e uma equipe humana pequena, sob liderança técnica.

**Implicações:** IA acelera implementação, testes e documentação, mas não detém autoridade de aprovação. Revisões independentes, CI, testes e documentação são fundamentais para permitir manutenção além de um único agente ou ferramenta comercial.

**Limite:** não pressupor colaboradores já disponíveis; formar equipe e comunidade é objetivo progressivo. Código gerado com IA deve ser compreensível por humanos.

### Decisão 11 — Distribuição: pacotes Linux + testes reproduzíveis

**Decisão aprovada:** disponibilizar **pacotes para distribuições Linux existentes** e **ambiente de testes reproduzível**.

**Implicações:** não criar distribuição Linux própria. Começar com distribuição oficialmente homologada, depois ampliar; padronizar build, dependências, scripts e evidências. Permitir instalação como sessão desktop quando houver integração real com login e dispositivos.

**Limite:** um compositor aninhado não é automaticamente uma sessão independente. Não prometer suporte genérico a todas as distribuições nem equivalência com hardware real sem testes.

### Decisão 12 — Filosofia de código: aberto para aprendizado e experimentação; licença a definir

**Decisão aprovada:** orientar o projeto ao **aprendizado, experimentação e colaboração aberta**, deixando a licença definitiva para análise posterior.

**Implicações:** publicar decisões, relatórios, testes e guias acessíveis; acolher contribuições educacionais. Priorizar clareza técnica dos artefatos.

**Ponto crítico:** repositório público sem licença explícita não concede automaticamente direitos amplos de redistribuição/modificação. Definir licença e política de contribuições antes de aceitar contribuições significativas ou distribuir formalmente a Alpha. Verificar licenças das dependências.

### Decisão 13 — Filosofia visual: materialidade + profundidade + clareza + acessibilidade

**Decisão aprovada:** equilibrar **materialidade, profundidade, clareza e acessibilidade**.

**Implicações:** vidro, volume, iluminação e hierarquia devem melhorar a compreensão espacial. Preservar tipografia, foco, contraste e controle dos efeitos. A identidade gráfica precisa persistir nos perfis leves e de alta legibilidade, com geometria, cor e componentes coerentes.

**Regra:** nenhum efeito visual deve prejudicar leitura, interação ou desempenho necessário à tarefa. A inspiração Windows 7 Aero é referência histórica, não autorização para copiar a interface nem para reproduzir seus problemas.

### Decisão 14 — Qualidade: requisitos mínimos em todas as dimensões

**Decisão aprovada:** buscar **qualidade equilibrada com critérios mínimos obrigatórios** em **estabilidade, acessibilidade/usabilidade, desempenho e consistência visual**.

**Implicações:** boa pontuação visual não compensa falha crítica de teclado, buffers ou acessibilidade. Cada alteração relevante requer validação proporcional ao risco; cenários reais e ensaios prolongados complementam testes automatizados.

**Ainda pendente:** definir métricas e limiares concretos com base no hardware de referência e em pesquisa com usuários. Não inventar números de RAM/FPS nem considerar testes unitários prova de homologação.

### Decisão 15 — Controle de complexidade: especialização com arquitetura modular

**Decisão aprovada:** **núcleo leve, escopo especializado e extensões opcionais**.

**Implicações:** manter responsabilidades separadas entre compositor, shell, motor gráfico e serviços; recursos fora da missão devem, de preferência, ser fornecidos por aplicativos existentes ou módulos opcionais. Não construir uma plataforma genérica de plugins antes de haver demanda suficiente.

**Limite:** modularidade também tem custo; preferir interfaces simples e isolamentos bem definidos a abstrações gigantescas.

### Decisão 16 — Evolução orientada por evidências + consistência dos controles Qt

**Decisão aprovada:** orientar evolução por **feedback, testes de usabilidade, desempenho e capacidade de manutenção**, alinhados à missão do projeto. Além disso, corrigir a inconsistência de elementos internos Qt que ainda usam estilos do sistema: **botões, barras e outros controles QML devem ganhar linguagem VALO**.

**Implicações:** testar se novos componentes melhoram experiência; implementar biblioteca QML coerente com tokens e estados acessíveis. Não tratar um botão bonito como solução suficiente: foco, teclado, estados e escala precisam funcionar.

**Limite:** o compositor não pode simplesmente reestilizar os elementos internos de qualquer aplicativo externo. Qt Quick, Qt Widgets e GTK possuem mecanismos distintos.

### Decisão 17 — VALO Controls: padronização progressiva

**Decisão aprovada:** primeiro padronizar **interfaces próprias**; depois oferecer **estilo Qt Quick reutilizável**; futuramente pesquisar integração com **Qt Widgets e GTK**.

**Implicações:** separar VALO Controls do renderer óptico; desenvolver componentes de Button, TextField, Slider, ScrollBar, CheckBox, Switch, ComboBox, Menu, Dialog etc. apenas conforme necessidade comprovada. Um controle simples não deve exigir blur GPU avançado.

**Limite:** integração entre toolkits é uma pesquisa futura, não promessa de tema universal na Alpha.

### Decisão 18 — Interação: equilíbrio mouse/teclado + menu de comandos

**Decisão aprovada:** funcionalidades essenciais acessíveis por **mouse e teclado**, incluindo um **menu discreto com atalhos de terminal úteis** para estudantes e desenvolvedores.

**Exemplos citados:** atualização de pacotes com apt/dnf, além de outras ações comuns que poderão ser selecionadas depois. O menu deve apresentar descrição legível, suporte a foco por teclado e não exigir memorização de comandos.

**Limite:** não supor distribuição específica; comandos e operações têm semânticas diferentes (por exemplo, atualizar índices não é instalar atualizações).

### Decisão 19 — Quick Actions: terminal preparado na Alpha; GUI futura

**Decisão aprovada:** a **visão final** é executar tarefas por **interface gráfica**, mas na **Alpha 0.1** pode-se **abrir um terminal com o comando preparado**, sem executar automaticamente.

**Implicações:** usuário revisa e confirma operações. Suportar terminais homologados explicitamente; prever alternativa de copiar o comando. Para implementação gráfica futura, usar integração adequada ao gerenciador de pacotes, confirmação, autenticação e feedback de erros/progresso.

**Limite:** não executar `sudo` automaticamente, não construir comandos com entrada não confiável e não assumir opções universais de linha de comando dos terminais.

### Decisão 20 — Privacidade: local por padrão, sem telemetria automática

**Decisão aprovada:** **preferências, workspaces e dados de uso permanecem locais por padrão; nenhuma telemetria é enviada automaticamente**.

**Implicações:** logs locais controláveis, compartilhamento de relatórios por iniciativa explícita do usuário, compatibilidade com uso offline nas funções que não exigem rede. Dados usados por funcionalidades adaptativas não devem ser enviados sem permissão.

**Limite:** armazenamento local não resolve sozinho proteção de arquivos, permissões e segurança das integrações.

### Decisão 21 — IA como extensão opcional e IA local futura (meta 1.1.0)

**Decisão aprovada:** o SYSTALIA funciona sem IA. Uma **extensão opcional** poderá oferecer **IA local** capaz de compreender **informações autorizadas sobre o PC**, prevista como objetivo para **versão 1.1.0**.

**Implicações:** modelo local conforme hardware; permissões explícitas para dados de janelas, aplicativos, workspaces e demais informações; confirmação humana para ações sensíveis; botão de desativação efetiva. Eventuais provedores em nuvem não foram escolhidos como dependência.

**Limite:** **1.1.0 não é cronograma aprovado**; pesquisa, segurança, consumo de recursos e consentimento ainda precisarão ser detalhados. Não transformar a IA em monitoramento permanente não autorizado.

### Decisão 22 — Testadores: grupo pequeno e diverso

**Decisão aprovada:** a primeira Alpha será avaliada por **um grupo misto de desenvolvedores, estudantes e pessoas com diferentes necessidades de acessibilidade**.

**Implicações:** testar tarefas reais, discoverability, teclado, conforto visual e estabilidade; coleta de feedback voluntária e acessível. Um grupo inicial de 6–9 pessoas foi **sugerido**, mas **não foi ratificado** como tamanho obrigatório.

**Limite:** não substituir testes internos por exposição de usuários a bugs críticos previsíveis; participação deve ser voluntária e informada.

### Decisão 23 — Governança: liderança técnica e decisões colaborativas

**Decisão aprovada:** **liderança técnica preserva a visão**, mas decisões relevantes passam por **propostas documentadas, revisão e discussão colaborativa**.

**Implicações:** utilizar processos leves de proposta (por exemplo, SDP — SYSTALIA Design Proposal) e ADR (Architecture Decision Record) proporcionalmente ao impacto. Mudanças de arquitetura, segurança, buffers, shaders e APIs merecem revisão técnica e justificativa rastreável.

**Limite:** não criar burocracia para ajustes triviais; agentes de IA não aprovam decisões estratégicas por conta própria.

### Decisão 24 — Garantia de consistência: padrões + componentes + testes + Mintlify/Git

**Decisão aprovada:** combinar **diretrizes documentadas**, **VALO Theme/Controls**, **testes automatizados** e **revisão humana**; instituir **Mintlify como central de documentação**, com **Git como sistema de versionamento e fonte de verdade**. **Decisões relevantes da IA devem ser registradas e acrescentadas à documentação versionada.**

**Implicações:** áreas de produto, arquitetura, design system, build/testes, governança, decisões e qualidade. Fazer alterações documentais por commits/PRs; preservar histórico, alternativas, impactos, autoria/revisores, estado do trabalho e vínculo com código/evidências.

**Limite:** registrar **decisões materialmente relevantes**, não todo token ou tentativa descartada de um agente. Um agente não deve inventar que uma decisão foi aprovada, que um benchmark passou ou que o site Mintlify está publicado. A integração e implantação do Mintlify ainda precisam ser realizadas/verificadas.

### Decisão 25 — Visão 2030: utilidade, VALO, comunidade e contribuição

**Decisão aprovada:** até 2030, buscar **desktop utilizável**, **identidade VALO reconhecida**, **comunidade sustentável** e **contribuições técnicas**, **sem competir pela quantidade de funcionalidades**.

**Implicações:** medir sucesso por utilidade, qualidade, sustentabilidade e contribuição; progresso por marcos de produto, não por linhas de código. Diferenciação em experiência visual e acessibilidade; cooperação com ecossistema Linux sempre que possível.

**Limite:** 2030 é horizonte de visão, não garantia de maturidade ou promessa de números de usuários.

---

## 3. Declaração de missão sugerida (texto editorial, aguardando aprovação formal)

> O SYSTALIA é um ambiente desktop Linux experimental, especializado em experiência visual, acessibilidade e produtividade para desenvolvedores e estudantes. Com seu compositor Wayland, shell e linguagem VALO, procura oferecer materiais visualmente expressivos sem sacrificar legibilidade, desempenho, privacidade e previsibilidade. Uma arquitetura modular e o reaproveitamento de serviços Linux sustentam o projeto; decisões documentadas, testes e colaboração orientam sua evolução.

**Posicionamento:** não é uma nova distribuição, não é um concorrente funcional abrangente do KDE/GNOME e não pretende substituir bibliotecas/serviços maduros sem justificativa. Seu diferencial percebido é o **VALO Glass**; seu valor de produto é **conforto + acessibilidade + produtividade por atividades**.

**Frase para público leigo:** “Estou desenvolvendo a interface gráfica de um ambiente desktop para Linux — a parte do computador com janelas, barra, menus e organização das tarefas — com um visual próprio e recursos de acessibilidade.”

---

## 4. Princípios VALO e política de design

- **Vibrant:** riqueza e expressividade dos materiais, mas apenas quando útil à hierarquia visual.
- **Adaptive:** ajustar/perfilar a experiência segundo preferências e capacidade gráfica; nunca surpreender o usuário com mudanças constantes.
- **Lucid:** legibilidade, contraste, clareza e previsibilidade prevalecem sobre efeitos decorativos.
- **Organic:** transições coerentes, controláveis e passíveis de redução/desativação.

**Política de produto derivada:** preservar identidade mesmo sem blur/refração; usar estados de foco/hover/pressed/disabled distintos; permitir navegação por teclado; fornecer perfis; impedir que efeitos atrapalhem código, texto e controles. A regra “melhor efeito visual” não deve substituir avaliação de usabilidade.

**Referências históricas:** Windows 7 Aero e skeuomorphism moderno servem de inspiração de materialidade; não determinam fidelidade de aparência nem arquitetura idêntica ao Windows.

---

## 5. Contexto técnico herdado — verificar no código antes de usar

> Esta seção registra o **estado conhecido em conversas e relatórios**, não certifica o estado atual do branch. Uma IA sucessora deve inspecionar commits, testes e evidências antes de modificar o projeto.

### 5.1 Tecnologias e estrutura conhecidas

- Linux como base; Qt 6.11.2 no ambiente de desenvolvimento relatado; C++23; QML / Qt Quick / Scene Graph; QtWayland Compositor; Vulkan e OpenGL via QRhi; shaders GLSL/SPIR-V.
- Compositor Wayland com XDG Shell, gestão de superfícies, empilhamento, foco, resize e integração experimental XWayland.
- Shell com TopBar, Dock, Files e janela de controle do material gráfico; workspaces, snap e componentes de controle/overlays.
- Motor VALO Graphics V2: `ValoGlassItemV2`, `ValoGlassMaterialV2`, `ValoBackdropProviderV2`, `ValoMaterialSurfaceV2.qml`; GPU Dual-Kawase; geometria SDF L2/G2; partículas discretas.
- Tokens VALO centralizados; histórico de compatibilidade com renderer V1.
- Branch de trabalho histórico: `valo-graphics-2.0` no repositório `LucasRamosSilva-15/systalia-glass-engine`.

### 5.2 Restrições técnicas históricas a preservar até revisão

- ABI UBO V1: 128 bytes; V2: 160 bytes; 124 tokens VALO preservados durante a migração.
- Evitar `hideSource: true` em superfícies de clientes; preservar apresentação dos buffers Wayland e input.
- Não assumir seguro capturar `ShellSurfaceItem` ou compartilhar seus buffers em passes auxiliares sem contrato claro de ciclo de vida e sincronização.
- Nunca usar capturas sintéticas do desktop como evidência de funcionamento no SDE real.
- Perfis Performance/Balanced/Quality e política de Reduced Motion devem permanecer funcionais.

### 5.3 Histórico gráfico relevante

- As etapas VALO Graphics 2.0 migraram componentes principais da shell e decorações Wayland para V2.
- Etapa 06.1: alinhamento dos cabeçalhos internos/clientes; referência comum de 50 px; relatórios indicaram fontes Inter Medium, 21 px e 47/47 testes à época.
- Etapa 06.2: remoção de badges, resize das janelas internas e primeiro backdrop com integração limitada a Dock/TopBar; classificação PARTIAL.
- Etapa 06.3: seleção de até três produtoras internas para backdrop, testada em cenários controlados; integração real limitada; classificação PARTIAL.
- Etapa 06.3.1: corrigido fallback global que era acionado ao abrir clientes Wayland; passou a considerar geometria e ordem Z, mas sem captura segura de clientes externos; classificação PARTIAL.
- Foi discutida uma **possível Etapa 06.4**, inspirada na composição centralizada do Windows 7 Aero/DWM, com política revisada de **somente uma janela inferior relevante por componente**. **Discussão/proposta, não presumir implementação ou aprovação de arquitetura definitiva.**
- Problema conhecido em versões anteriores: **conteúdo preto de clientes XWayland reais**; investigação reservada à Etapa 07, sem confirmação de resolução. Outros itens pendentes: entrada manual, popups/subsurfaces, sessão independente, DPR/multimonitor, testes prolongados, qualidade visual e desempenho.

**Recomendação para a IA sucessora:** antes de continuar VALO Graphics, conferir se a arquitetura/branch mudou depois da pausa estratégica. Não partir do pressuposto de que a Etapa 06.4 foi implementada. A prioridade do usuário passou a ser a **estratégia futura do produto**, não acrescentar imediatamente mais efeitos visuais.

---

## 6. Escopo proposto da Alpha 0.1

### 6.1 Núcleo necessário (proposta de critérios, ainda pendente de homologação)

1. Sessão desktop instalável em pelo menos uma distribuição de referência; modo aninhado utilizável para desenvolvimento, mas distinto de uma sessão autônoma homologada.
2. Aplicações Wayland nativas selecionadas abrem, recebem entrada, apresentam buffers corretamente e fecham sem defeitos críticos.
3. Movimentação, foco, resize, maximize/restore, workspaces por atividade e navegação mouse/teclado funcionam.
4. Dock, TopBar, janelas e VALO Controls internos têm qualidade e comportamento consistente, incluindo perfis de acessibilidade.
5. VALO Glass possui perfis gráficos adaptáveis e degradáveis, sem comprometer legibilidade.
6. Quick Actions pode abrir terminal com comando preparado **sem execução automática**, quando houver integração homologada.
7. Sem telemetria automática; diagnósticos locais e compartilhamento voluntário.
8. Pacotes, instalação, build e testes reproduzíveis; documentação de problemas conhecidos e limites de suporte.

### 6.2 Não prometer na Alpha inicial

- Suporte XWayland amplo e totalmente homologado; tema VALO universal para GTK/Qt Widgets; plugins públicos; IA local integrada; restauração avançada de aplicativos; compatibilidade garantida com qualquer GPU/monitor/distribuição.
- A aparência do conceito Figma não substitui experiência funcional nem testes reais.

### 6.3 Critérios de qualidade a formalizar na Etapa 02

- **Estabilidade:** critérios para crashes, falhas graves, input, ciclo de vida dos clientes e sessões de uso.
- **Acessibilidade:** keyboard-only, foco, legibilidade, contraste, redução de movimento, escala e testes com pessoas de necessidades diversas.
- **Desempenho:** baseline CPU/GPU/RAM, hardware de referência e requisitos mínimos; medição de latências e frametimes, sem números arbitrários.
- **Consistência:** controles VALO, estados, perfis, geometria, composição de vidro, fallback gráfico.
- **Compatibilidade:** matriz explícita por aplicativo/protocolo/toolkit/driver/sessão.

Todos os critérios mínimos devem ser cumpridos: média de qualidade não mascara bloqueador crítico.

---

## 7. Roadmap orientativo (não aprovado como calendário)

| Horizonte | Objetivo | Observações |
|---|---|---|
| Planejamento 2027 / Etapa 02 | Formalizar visão, requisitos, arquitetura estratégica, ADRs, limites e critérios da Alpha | Próxima etapa ainda não iniciada |
| Alpha 0.1 | Experiência utilizável para desenvolvedores, Wayland nativo primeiro, perfis acessíveis e workspaces | Data não definida; depende de homologação |
| Alpha posterior | Melhorar XWayland, integrações, Quick Actions e organização de atividades com evidências | Escopo dependerá da demanda e manutenção |
| Beta | Maior estabilidade e cobertura de compatibilidade; serviços/integração de sessão melhorados | Sem promessa de funcionalidades extensas |
| 1.1.0 (objetivo explicitado) | Extensão opcional de IA local com acesso consentido a informações do PC | Não constitui compromisso de cronograma ou viabilidade |
| Até 2030 | Desktop utilizável, VALO reconhecido, comunidade e contribuições técnicas | Visão, não garantia |

---

## 8. Governança e documentação Git/Mintlify

### 8.1 Fonte de verdade

- **Git:** fonte de verdade versionada para decisões aprovadas, documentação, código e testes.
- **Mintlify:** portal de publicação e navegação da documentação, alimentado pelos arquivos Git.
- **Issues e PRs:** discussão, rastreabilidade, revisão e aprovação de mudanças.
- **ADRs:** registram mudanças arquiteturais; **SDPs:** propostas de produto/design relevantes (nomes de mecanismos sugeridos; formalização ainda pendente).

### 8.2 Estrutura documental sugerida

```text
docs/
  vision/
    SYSTALIA_VISION_2027.md
    product-principles.md
    alpha-0.1-scope.md
  architecture/
    overview.md
    compositor.md
    shell.md
    valo-graphics.md
    accessibility.md
    decisions/
      0001-example-adr.md
  design-system/
    valo-principles.md
    valo-theme.md
    valo-controls.md
  engineering/
    building.md
    testing.md
    contributing.md
    ai-development-policy.md
  quality/
    compatibility-matrix.md
    accessibility-test-plan.md
    performance-baseline.md
    known-issues.md
```

**Esta árvore é recomendação editorial, não estrutura já criada.** Ajustar à configuração real do Mintlify e ao repositório existente.

### 8.3 Contrato para decisões de IA

Quando uma IA propuser ou implementar uma mudança materialmente relevante, registrar:

1. Identificador e título da decisão, data, estado: *proposta / aprovada / implementada / homologada / substituída*.
2. Problema real e evidências que motivaram a mudança.
3. Restrições, alternativas consideradas e justificativa.
4. Comportamento esperado e impacto em acessibilidade, desempenho, privacidade, compatibilidade e manutenção.
5. Arquivos/componentes afetados, versão/branch/commit e compatibilidade/fallback.
6. Testes automatizados e manuais realizados, evidências e limitações ainda abertas.
7. Revisão humana e decisão final; link para PR/ADR.
8. Documento Mintlify/Git atualizado **no mesmo fluxo de alteração**, quando houver documentação impactada.

**Não registrar como homologado** algo avaliado apenas por logs gerados por agente, teste mock ou capturas sintéticas. Em mudanças críticas, exigir revisão humana qualificada sempre que disponível.

### 8.4 Política de privacidade para documentos e diagnósticos

- Não publicar sem revisão logs que contenham dados pessoais, segredos, nomes de usuários, caminhos privados ou tokens.
- Relatos de terceiros devem ter consentimento. Logs locais não são enviados automaticamente.
- IA local futura acessa dados estritamente mediante permissão explícita e granular, com opção de desligamento.

---

## 9. Riscos estratégicos e cuidados

| Risco | Por que importa | Direção de resposta |
|---|---|---|
| Escopo excessivo | KDE/GNOME são ecossistemas maduros e amplos | Especializar; reutilizar serviços/aplicativos |
| Prioridade excessiva à estética | Pode ocultar problemas de input, buffers e acessibilidade | Critérios mínimos; testes com usuários |
| IA acelerando mudanças sem revisão | Decisões difíceis de reproduzir ou manter | ADR/PR, revisão, testes e documentação Git |
| Compositor próprio complexo | Wayland, QRhi, sincronização, ciclo de vida e dispositivos | Isolamento de subsistemas e homologação progressiva |
| Fragmentação entre distribuições | Qt/drivers/serviços variam | Uma plataforma homologada primeiro; matriz de suporte |
| “Leve” não demonstrado | GPU/blur podem ser custosos | Benchmarks e perfis com fallback |
| Privacidade confundida com ausência de telemetria | Dados locais ainda exigem proteção | Permissões, logs controlados, transparência |
| Dependência da IA local | Excluiria usuários em hardware modesto | Extensão opcional; núcleo independente |
| Mintlify e documentação obsoleta | Perda de rastreabilidade | Atualização junto ao PR, versionamento Git |

---

## 10. Instruções operacionais para transferir a outra IA

### 10.1 Contexto mínimo que a próxima IA deve assumir

1. **A sessão de 25 perguntas foi encerrada.** Não reformular essas decisões sem solicitação do responsável.
2. O **foco atual é estratégia e formalização**, após pausa no desenvolvimento da interface gráfica.
3. A **Etapa 02 ainda não começou**; deve transformar a visão em requisitos, arquitetura e documentação versionada, distinguindo decisões do usuário de recomendações do assistente.
4. O projeto busca **qualidade de experiência**, não competir em funcionalidade com KDE/GNOME.
5. Todo trabalho deve preservar **privacidade local, acessibilidade, documentação rastreável, controle humano e escopo especializado**.
6. **Mintlify + Git** é decisão estratégica. Não assumir que já existe site ou integração publicada.
7. O usuário pretende poder continuar usando diferentes ferramentas de IA sem ficar preso a um modelo ou assinatura.

### 10.2 O que perguntar somente quando necessário

- Quais repositórios e branch são atualmente canônicos, pois podem ter mudado.
- Qual distribuição Linux será a primeira plataforma homologada e qual hardware de referência será usado.
- Qual licença será escolhida antes de aceitar contribuições e distribuir a Alpha.
- Quem revisará mudanças críticas e como será configurada a documentação Git/Mintlify.

**Não repetir:** público, diferencial, perfis, escopo, compatibilidade inicial, visão 2030, governança ou política de privacidade; já foram decididos.

### 10.3 Prompt de inicialização sugerido para outra IA

> Você está assumindo a continuidade estratégica do SYSTALIA Desktop Environment. Leia integralmente `SYSTALIA_VISION_2027_HANDOFF.md`. A Etapa 01 foi concluída com 25 decisões aprovadas. Preserve-as e não invente estados de implementação. Seu trabalho seguinte, somente quando autorizado, é elaborar a Etapa 02 com `SYSTALIA_VISION_2027.md` e arquitetura estratégica, regras de governança, critérios da Alpha 0.1 e estrutura de documentação Git/Mintlify. Distingua **decisão aprovada**, **proposta a validar**, **implementação comprovada** e **pendência**. Consulte o código e relatórios mais recentes antes de qualquer afirmação de implementação. Priorize acessibilidade, qualidade, privacidade e escopo especializado; não retome o desenvolvimento gráfico apenas por iniciativa própria.

---

## 11. Registro de estado e controle de mudanças deste dossiê

- **Fonte da parte estratégica:** escolhas explícitas do usuário nas questões 01 a 25 da Etapa 01, registradas nesta conversa.
- **Fonte da parte técnica:** histórico da conversa, relatórios de implementação VALO Graphics 2.0 e descrição conhecida do repositório. **Deve ser revalidada** ao retomar programação.
- **Propostas não aprovadas automaticamente:** redação exata da missão, exemplos de perfis, estrutura de diretórios Mintlify, metas numéricas, tamanho do grupo de testadores, marcos da Alpha e arquitetura futura de backdrop.
- **Licença:** não definida.
- **Data da Alpha:** não definida.
- **Etapa 02:** não iniciada.
- **Versão 1.1.0 para IA local:** intenção futura sujeita a pesquisa/viabilidade.

### Histórico de versões

| Versão | Data | Alteração |
|---|---|---|
| 1.0 | 2026-11-17 | Consolidação integral das 25 decisões e elaboração de dossiê para transferência de IA |

---

**Fim do dossiê.**