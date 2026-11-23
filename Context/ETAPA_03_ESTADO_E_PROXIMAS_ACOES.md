# SYSTALIA — Continuidade da Etapa 03: Planejamento Técnico da Alpha 0.1

**Referência:** auditoria inicial 03.1 conduzida em 21/11/2026 por inspeção **parcial** de arquivos da branch `valo-graphics-2.0` e dos relatórios existentes.  
**Status:** auditoria inicial concluída como planejamento; **não** equivale a revisão integral de segurança, testes executados nesta sessão ou homologação.

## 1. Situação herdada

Os compromissos de produto encontram-se em `Context/ETAPA_02_PLANEJAMENTO_ESTRATEGICO_2026.md`. A Etapa 03 deve converter esse escopo em entregas técnicas.

**Códigos/artefatos observados (verificar novamente no HEAD):**
- `src/app/main.cpp` inclui detecção de execução aninhada versus direta; instancia services de input, sessão, outputs, workspaces, notificações e shell.
- `src/services/input/InputDeviceManager.cpp` contém integração opcional libinput/udev e configuração de ambiente EGLFS/KMS.
- `src/services/session/LogindSessionService.cpp` oferece comunicação D-Bus/logind e operações de dispositivos.
- `cmake/SgeCore.cmake` mantém o núcleo `sge_core` com compositor, VALO V2, sistema e protocolos.
- `cmake/SgeQml.cmake` declara `Systalia.Engine 1.0` e aliases QRC para preservar imports após 06.6E.
- `cmake/SgeInstall.cmake` instala executável e arquivos de sessão Wayland.
- `tests/CMakeLists.txt` lista testes gráficos, protocolos, Wayland e integração.
- `docs/architecture/valo-v1-v2/CONSOLIDACAO_06_6E.md` relata builds Debug/Release, **48/48 entradas CTest aprovadas**, 25 arquivos `.qsb`, prefixo temporário e execução de cliente Alacritty no socket Wayland. Isso é **resultado reportado em documento**, não reproduzido nesta auditoria.

## 2. Lacunas e bloqueadores para homologação Alpha 0.1

| Prioridade | Área | Estado identificado | O que falta demonstrar |
|---|---|---|---|
| P0 | Sessão Fedora independente | Código EGLFS/KMS, logind, scripts e desktop entry presentes | Sessão de login real, dispositivos, saída gráfica, alternância/encerramento, recuperação |
| P0 | Clientes Wayland nativos | XDG, buffers, foco e testes registrados | Aplicativos reais, buffers acelerados, teclado/mouse, menus/clipboard/resize em Fedora |
| P0 | Compatibilidade obrigatória | Infraestrutura existe | Matriz Qt Quick, Qt Widgets e GTK com núcleo selecionado e registros por versão/driver |
| P0 | Acessibilidade | Controles/tokens e elementos QML existentes | Perfis básicos, foco, contraste, escala, redução de movimento, avaliações humanas |
| P0 | Desempenho e hardware | Testes/benchmarks disponíveis em parte | Dois equipamentos/categorias definidos, medidas CPU/GPU/RAM/frametimes, fallback |
| P0 | Instalação | CMake instala `systalia_glass_engine`, `systalia-session` e `systalia.desktop` | Instalação real na edição/versão Fedora selecionada, caminhos e integração com login |
| P1 | Documentação | Relatórios 06.6E recentes existem | README e blueprints de outubro contêm caminhos anteriores à reorganização |
| P1 | XWayland | Código e testes mencionados | Clientes X11 reais e clipboard entre protocolos — sem prometer abrangência na Alpha |

## 3. Observações técnicas específicas

- `data/systalia-session` usa caminho fixo `/usr/bin/systalia_glass_engine`; `data/systalia.desktop` usa `/usr/bin/systalia-session`. `CMAKE_INSTALL_PREFIX` / `GNUInstallDirs` permitem outros destinos. **Risco de empacotamento** a validar, não prova de falha numa instalação oficial em `/usr`.
- A configuração `config/eglfs_kms.json` em caminhos relativos precisa ser verificada em contexto de instalação e login reais; não presumir resolvida.
- `SGE_MASTER_BLUEPRINT.md` e `SGE_IMPLEMENTATION_PLAN.md` de outubro ainda contêm caminhos históricos `src/gfx`, `src/qml`, `src/system`. Servem como evidências do desenho anterior, não como inventário do HEAD.
- Testes existentes não substituem homologação de sessão física e avaliação humana. A cobertura de XWayland e multi-monitor físico é declarada limitada no relatório 06.6E.
- A inspeção 03.1 **não leu exaustivamente todos os fontes**, não compilou o projeto e não mediu dispositivos reais.

## 4. Sequência técnica sugerida — respeitar escopo aprovado

1. **03.2:** escolher uma versão/edição Fedora e criar matriz reprodutível de validação de sessão independente; verificar DRM/KMS/logind/input e inicialização; não declarar homologação prematura.
2. **03.3:** exercitar compositor, lifecycle de clientes, buffers acelerados e workspaces numa sessão real.
3. **03.4:** completar/verificar perfis acessíveis, keyboard-only, foco, redução de movimento e coerência VALO.
4. **03.5:** homologar núcleo de aplicativos nativos por toolkit (Qt Quick, Qt Widgets, GTK), com cenários obrigatórios e falhas não críticas documentadas.
5. **03.6:** benchmarks e categorias básico/recomendado baseadas em testes reais.
6. **03.7:** validação híbrida — testes automatizados, sessão real e usuários — e decisão de liberação.

## 5. Política para agentes de IA

**Não retomar perguntas da Etapa 01/02; não refatorar novamente a 06.6E só por estética de diretórios; não modificar o código sem autorização.** Ao iniciar uma tarefa de implementação, identificar branch/commit, dono do contrato, impacto no compositor, fallback e provas. A próxima atividade é 03.2, não criação de funcionalidades fora da Alpha.

**Este arquivo é um handoff e roteiro; não é relatório de homologação.**
