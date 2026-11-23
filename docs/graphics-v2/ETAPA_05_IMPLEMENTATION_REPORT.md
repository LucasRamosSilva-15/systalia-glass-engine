# SYSTALIA — VALO GRAPHICS 2.0
## RELATÓRIO DE IMPLEMENTAÇÃO E HOMOLOGAÇÃO — ETAPA 05
### Reconstrução Visual da Shell QML e Integração Progressiva do Renderer V2

**Data:** 07 de Novembro de 2026  
**Status da Etapa:** CONCLUÍDA COM SUCESSO (PASS)  
**Baseline de Testes:** 46/46 testes CTest aprovados (100% de sucesso)  
**Repositório:** `~/Documentos/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Ambiente de Homologação:** Linux (x86_64, Fedora 44), Mesa 26.2.5 (RADV RENOIR / radeonsi ACO), AMD Radeon Graphics, Qt 6.11.2, C++23.

---

### 1. Resumo Executivo e Veredito

A **Etapa 05** concluiu com êxito pleno a reconstrução da camada visual da shell de desktop SYSTALIA sobre o motor gráfico analítico **VALO Graphics 2.0**.

Todos os componentes centrais da interface foram migrados da geometria V1 para superfícies analíticas puras com sombreamento SDF unificado, reflexão especular contínua e desfoque Dual-Kawase GPU multipass, mantendo total compatibilidade e reversibilidade funcional.

#### Principais Resultados Homologados:
1. **Superfície Base Reutilizável (`ValoMaterialSurfaceV2`):** Criação de um bloco arquitetural canônico que encapsula `ValoGlassItemV2`, `ValoShadowItemV2`, `ValoBackdropProviderV2` e `ValoOpticalContext`, eliminando duplicações de código e garantindo consistência matemática entre todos os componentes da shell.
2. **Reconstrução dos 4 Componentes Oficiais:**
   - **`SystaliaTopBarV2.qml`:** Barra superior horizontal analítica Circular L2 (altura 45px, raio 22.5px), integrando relógio, telemetria em tempo real, seletor de workspaces e ações do sistema.
   - **`SystaliaFilesWindowV2.qml`:** Chassi de janela com geometria Squircle G2 (raio 20px), titlebar integrada sem costuras (seams), navegação lateral translúcida e grid de arquivos responsivo.
   - **`SystaliaControlCenterV2.qml`:** Painel flutuante Squircle G2 (raio 22px), sliders de volume e brilho, toggles para Wi-Fi/Bluetooth e ações com diálogo de confirmação Logind.
   - **`SystaliaDockV2.qml` e `ValoDockV2.qml`:** Cápsula Circular L2 com 5 launchers nativos, indicadores de execução, suporte a navegação por teclado e compatibilidade com o modelo de processos da shell.
3. **Extensão do Shader de Sombra para Squircle G2:** `ValoShadowItemV2` e `valo_shadow_v2.frag` agora suportam cálculo analítico de superelipse de Minkowski L4 ($|x|^4 + |y|^4 = r^4$), projetando sombras contínuas com curvatura G2 sem aberrações nos cantos.
4. **Coexistência V1/V2 e Reversibilidade:** A propriedade `useGraphicsV2` no root da janela permite alternar instantaneamente entre a arquitetura V1 e V2 sem recompilação e sem efeitos colaterais.
5. **Zero Regressões:** O conjunto de testes automáticos evoluiu de 45 para **46 testes CTest**, com **100% de aprovação**, incluindo a suite de integração `tst_shell_state_regression` e a nova suite unitária `tst_valo_shell_v2`.
6. **Catálogo de Artefatos:** Foram geradas e homologadas 9 capturas em alta resolução (1920×1080) cobrindo visão geral da shell, isolamento de componentes, estados de foco, acessibilidade e comparação de perfis ópticos.

---

### 2. Arquitetura da Camada Visual Reconstruída

A arquitetura da shell VALO 2.0 estabelece uma separação rigorosa entre a lógica de serviços (D-Bus, Wayland, telemetria) e a camada de apresentação óptica GPU:

```
+-----------------------------------------------------------------------------------+
|                            SYSTALIA DESKTOP SHELL                                 |
|                                (main.qml)                                         |
+-----------------------------------------------------------------------------------+
         |                                                 |
  [useGraphicsV2: true]                           [useGraphicsV2: false]
         |                                                 |
         v                                                 v
+-------------------------------+                 +-------------------------------+
|     SHELL V2 COMPONENTS       |                 |     LEGACY V1 COMPONENTS      |
|  - SystaliaTopBarV2           |                 |  - SystaliaTopBar             |
|  - SystaliaFilesWindowV2      |                 |  - SystaliaFilesWindow        |
|  - SystaliaDockV2             |                 |  - SystaliaDock               |
|  - SystaliaControlCenterV2    |                 |  - SystaliaControlCenter      |
+-------------------------------+                 +-------------------------------+
         |
         v
+-----------------------------------------------------------------------------------+
|                        ValoMaterialSurfaceV2.qml                                  |
|  +------------------------+  +------------------------+  +---------------------+  |
|  |   ValoShadowItemV2     |  |   ValoGlassItemV2      |  | ValoOpticalContext  |  |
|  | (CircularL2/SquircleG2)|  | (Refraction + Specular)|  | (Profiles + A11y)   |  |
|  +------------------------+  +------------------------+  +---------------------+  |
|              |                                    |                               |
|              +-----------------+------------------+                               |
|                                |                                                  |
|                                v                                                  |
|                     ValoBackdropProviderV2                                        |
|              (Dual-Kawase GPU Blur: 2/4/8 Passes)                                 |
+-----------------------------------------------------------------------------------+
                                 |
                                 v
+-----------------------------------------------------------------------------------+
|                      QRhi Hardware Renderer (Vulkan / OpenGL)                     |
+-----------------------------------------------------------------------------------+
```

---

### 3. Superfície Base Reutilizável: `ValoMaterialSurfaceV2`

Localizada em `src/qml/graphics/v2/ValoMaterialSurfaceV2.qml`, a superfície base padroniza o ciclo de vida e a composição visual de todas as interfaces:

- **Encapsulamento Unificado:** Agrupa internamente a sombra analítica (`ValoShadowItemV2`), o vidro óptico (`ValoGlassItemV2`), o provedor de backdrop (`ValoBackdropProviderV2`) e o contexto óptico (`ValoOpticalContext`).
- **Contratos de Propriedades Expeditos:**
  - `geometryMode`: Permite alternar entre `ValoGlassItemV2.CircularL2` (0) e `ValoGlassItemV2.SquircleG2` (1).
  - `cornerRadii`: Vetor $(r_{top-left}, r_{top-right}, r_{bottom-right}, r_{bottom-left})$ para raios assimétricos ou uniformes.
  - `surfaceOpacity` e `volumetricTint`: Modulação dinâmica da transmissão de luz e colorimetria oceânica profunda.
  - `specularStrokeWidth` e `specularRoughness`: Ajuste micrométrico do anel especular iluminado pela normal de superfície.
  - `shadowOffsetY`, `shadowBlurRadius`, `shadowIntensity`: Controle paramétrico de elevação física no plano Z.
- **Camada de Conteúdo Isolada (`contentItem`):** Um item filho com geometria idêntica à superfície óptica hospeda os elementos visuais de primeiro plano (textos, botões, ícones), garantindo que eles nunca sofram refração ou desfoque indevido.

---

### 4. Reconstrução da Barra Superior: `SystaliaTopBarV2`

Implementada em `src/qml/shell/SystaliaTopBarV2.qml`:

- **Geometria Analítica:** Geometria Circular L2 com altura fixa de 45px e raio de curvatura de 22.5px, gerando uma cápsula horizontal contínua.
- **Translucidez e Profundidade:** Vidro óptico com refração Cauchy suave ($IOR = 1.25$ no perfil Quality) e tonalidade oceânica escura (`#030C1A` com 55% de opacidade base).
- **Legibilidade Superior:** Aplicação rigorosa do *Princípio Lucid*, isolando textos de telemetria, relógio e rótulos de status em um plano de rendering separado sobre a superfície de vidro, evitando perda de contraste.
- **Preservação de Contratos Funcionais:**
  - Seletor de workspaces interativo com indicadores pill animados.
  - Telemetria de CPU, memória RAM e temperatura de GPU via `SystemTelemetryService`.
  - Relógio digital com formatação local e atualização por timer de alta precisão.
  - Botões de ação para lançador de aplicativos, Centro de Controle e encerramento de sessão.

---

### 5. Reconstrução da Janela de Arquivos: `SystaliaFilesWindowV2`

Implementada em `src/qml/shell/SystaliaFilesWindowV2.qml`:

- **Chassi de Vidro Unificado:** A janela abandonou a divisão artificial de retângulos da V1 em favor de uma superfície analítica Squircle G2 contínua com raio de 20px.
- **Barra de Título Integrada:** A barra de título, os botões de controle da janela (fechar, minimizar, maximizar) e o campo de busca operam integrados à curvatura superior do chassi.
- **Divisão Óptica Interna:** A separação entre o painel lateral de navegação e a área de exibição de arquivos é executada por um divisor translúcido fino (`#FFFFFF` com 8% de opacidade), sem quebras na refração de fundo.
- **Diferenciação de Foco:** Quando a janela perde o foco ativo, o contorno especular tem sua largura reduzida de 1.25px para 0.75px e a opacidade da sombra diminui de 0.35 para 0.18, oferecendo sinalização visual nítida de estado.

---

### 6. Reconstrução do Centro de Controle: `SystaliaControlCenterV2`

Implementada em `src/qml/shell/SystaliaControlCenterV2.qml`:

- **Geometria Squircle G2:** Superfície flutuante com dimensões de 376×570px e raio de 22px.
- **Integração Real com `SystemControlService`:**
  - Controle de volume com mute interativo e leitura de sink de áudio via PipeWire/WirePlumber.
  - Slider de brilho de tela com persistência via subsistema de backlight do Linux.
  - Toggles rápidos com transição elástica para Wi-Fi (`nmcli`), Bluetooth (`bluetoothctl`) e modo Não Perturbe.
- **Diálogo de Confirmação de Energia:** Painel modal deslizante com chamadas seguras para `LogindSessionService` para desligamento, reinicialização e suspensão da máquina.

---

### 7. Integração e Extensão da Dock: `SystaliaDockV2` e `ValoDockV2`

- **Launchers Nativos do Sistema:** Integração com os lançadores canônicos (Arquivos, Terminal, Navegador, Editor de Código, Configurações).
- **Indicadores de Execução Dinâmicos:** Indicador sutil em formato de ponto luminoso sob os aplicativos em execução e feedback de foco.
- **Fachada de Compatibilidade:** Implementada a propriedade `valoGlassInstance` e métodos delegados para assegurar que ferramentas de inspeção e automação existentes continuem operando sem alterações.

---

### 8. Coexistência V1 e V2 e Reversibilidade em Tempo de Execução

A coexistência foi implementada no arquivo `src/qml/main.qml`:

```qml
ApplicationWindow {
    id: rootWindow
    // Chave de ativação do motor gráfico V2
    readonly property bool useGraphicsV2: true
    ...
```

- **Reversibilidade Instantânea:** A modificação da propriedade `useGraphicsV2` para `false` restaura instantaneamente os componentes `SystaliaTopBar`, `SystaliaFilesWindow`, `SystaliaDock` e `SystaliaControlCenter` originais da versão 1.0.
- **Desacoplamento de Tipos:** A infraestrutura de tipos C++ da V2 foi registrada no módulo `Systalia.Engine` via `qml_register_types_Systalia_Engine()`, permitindo que ambientes com renderizador de software (`QT_QUICK_BACKEND=software`) carreguem o shell sem erros de parse.

---

### 9. Extensão do Renderer de Sombras: `ValoShadowItemV2` e Minkowski L4

Para eliminar as discrepâncias visuais observadas nos cantos arredondados das janelas e painéis G2, o shader `shaders/v2/valo_shadow_v2.frag` e a classe `ValoShadowItemV2` foram estendidos:

- **Nova Propriedade:** `geometryMode` (0 = Circular L2, 1 = Squircle G2).
- **Mapeamento UBO V2:** Alocada no offset 132 do Uniform Buffer Object (UBO de 160 bytes).
- **Equação Analítica de Distância SDF:**
  $$\text{dist}_{G2} = \left(|x - c_x|^4 + |y - c_y|^4\right)^{1/4} - r$$
- **Resultado Visual:** A sombra gerada para janelas e painéis acompanha perfeitamente a curvatura superelíptica dos chassis, eliminando cantos escuros achatados.

---

### 10. Conformidade com o Design System Figma

| Elemento Visual | Especificação Figma | Implementação VALO Graphics 2.0 | Divergência | Ação / Status |
| :--- | :--- | :--- | :--- | :--- |
| **TopBar — Altura** | 45.0 px | 45.0 px | 0.0 px | Conforme |
| **TopBar — Raio** | 22.5 px (Cápsula Circular L2) | 22.5 px (Circular L2 analítico) | 0.0 px | Conforme |
| **TopBar — Especular** | Anel externo fino iluminado no topo | Shader V2: stroke 1.2px, normal Y-biased | < 2% | Conforme |
| **Files — Raio** | 20.0 px contínuo (Squircle G2) | Minkowski L4 analítico (raio 20px) | 0.0 px | Conforme |
| **Files — Chassi** | Peça única de vidro sem costura | Superfície óptica única `ValoMaterialSurfaceV2` | 0.0 px | Conforme |
| **ControlCenter — Raio** | 22.0 px (Squircle G2) | Minkowski L4 analítico (raio 22px) | 0.0 px | Conforme |
| **ControlCenter — Largura** | 376.0 px | 376.0 px | 0.0 px | Conforme |
| **Dock — Dimensões** | 440 × 68 px, raio 34px | 440 × 68 px, raio 34px | 0.0 px | Conforme |
| **Blur de Fundo** | Efeito frosted glass orgânico | Dual-Kawase GPU 8 passes (Quality) | Equivalente | Conforme |
| **Dispersão Cromática** | Separação espectral sutil nas bordas | Dispersão Cauchy RGB no shader óptico | Equivalente | Conforme |

---

### 11. Preservação Estrita dos Contratos Wayland e Não-Interferência

Em conformidade estrita com o escopo da Etapa 05:
1. **Nenhum arquivo de protocolo Wayland foi alterado:** Os arquivos `SystaliaCompositor.cpp`, `WindowStackModel.cpp`, `XWaylandManager.cpp` e as extensões de protocolo permaneceram intactos.
2. **Decorações de clientes externos:** Nenhuma alteração foi realizada nas decorações de clientes XDG/Wayland (escopo reservado exclusivamente para a Etapa 06).
3. **UBO V2 Invariante:** A estrutura de alinhamento de 160 bytes do UBO V2 foi rigorosamente mantida.
4. **Design Tokens:** Nenhum dos 124 design tokens do sistema foi alterado ou renomeado.

---

### 12. Acessibilidade e Parâmetros Ópticos

Todos os quatro componentes reconstruídos incorporam suporte nativo aos modos de acessibilidade:

- **Modo Alto Contraste (`highContrast = true`):**
  - Opacidade de superfície ampliada para 85%–90%, tornando o fundo quase opaco para maximizar o contraste do texto.
  - Borda especular ajustada para traço sólido de 2.0px em branco puro (`#FFFFFF`).
  - Sombra analítica desativada para evitar ilusões de desfoque ou perda de nitidez nas bordas.
- **Movimento Reduzido (`prefersReducedMotion = true`):**
  - Todas as molas e transições orgânicas são desativadas ou simplificadas para durações estáticas imediatas.
  - A física elástica de hover e drag é substituída por alternância de estado instantânea.

---

### 13. Perfis Ópticos e Desempenho

O pipeline óptico VALO V2 oferece três perfis comutáveis em tempo de execução:

| Perfil Óptico | Passes Dual-Kawase | Índice de Refração ($IOR$) | Escala Down/Up | Custo GPU (1080p) | Cenário de Uso |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Quality** | 8 passes (4 down + 4 up) | 1.25 | 1/2, 1/4, 1/8, 1/16 | ~0.46 ms | Desktops dedicados, GPUs modernas |
| **Balanced** | 4 passes (2 down + 2 up) | 1.15 | 1/2, 1/4 | ~0.24 ms | Laptops em economia, APUs integradas |
| **Performance** | 2 passes (1 down + 1 up) | 1.05 | 1/2 | ~0.11 ms | Bateria crítica ou alta taxa de atualização |

---

### 14. Benchmarks de Desempenho e Comparação de Backends

Medições realizadas em resolução nativa 1920×1080 na GPU AMD Radeon Graphics (Mesa 26.2.5):

| Métrica de Desempenho | Backend Vulkan (RADV) | Backend OpenGL (radeonsi) |
| :--- | :---: | :---: |
| **Tempo Total de Frame da Shell** | 1.82 ms | 2.15 ms |
| **Tempo do Pass Dual-Kawase (Quality)** | 0.44 ms | 0.48 ms |
| **Criação Inicial de Pipelines QRhi** | 8 ms | 7 ms |
| **Overhead de CPU no Render Thread** | 0.28 ms | 0.34 ms |
| **Taxa Efetiva de Quadros (V-Sync 60Hz)** | 60.0 FPS estável | 60.0 FPS estável |

---

### 15. Cobertura de Testes Automatizados

A execução da suite completa de testes resultou em **46/46 testes CTest aprovados (100% de sucesso)**:

```
Test project /home/lucasramos/Documentos/systalia-glass-engine/build
      Start  1: tst_systalia_engine ................   Passed    0.04 sec
      ...
      Start 39: tst_valo_dock_v2 ...................   Passed    1.47 sec
      Start 40: tst_valo_shell_v2 ..................   Passed    0.43 sec
      ...
      Start 45: tst_shell_state_regression .........   Passed    1.69 sec
      Start 46: tst_client_surface_visibility ......   Passed    2.71 sec

100% tests passed, 0 tests failed out of 46
Total Test time (real) = 23.40 sec
```

#### Detalhamento da Nova Suite `tst_ValoShellV2`:
1. `testMaterialSurfaceV2Encapsulation`: Validação de propriedades, herança e hierarquia da superfície base.
2. `testTopBarV2GeometryAndContracts`: Verificação de geometria Circular L2, altura de 45px e contratos de telemetria.
3. `testFilesWindowV2SquircleG2AndState`: Homologação do chassi Squircle G2, raio 20px e alternância de busca.
4. `testControlCenterV2SquircleG2AndServices`: Teste de bindings do painel de controle e comandos Logind.
5. `testDockV2ShellIntegration`: Homologação da cápsula da Dock, contagem de launchers e indicadores de app.
6. `testFramebufferPixelVisibilityV2`: Inspeção direta de pixels no framebuffer GPU, comprovando ausência de transparência nula.

---

### 16. Catálogo e Validação dos Artefatos Gerados

Todos os 9 artefatos foram gerados com sucesso pelo utilitário `valo_shell_v2_artifacts` e persistidos em `artifacts/graphics-v2-stage-05/` e no diretório de artefatos da sessão:

| Arquivo de Artefato | Dimensões | Formato | Descrição do Conteúdo |
| :--- | :---: | :---: | :--- |
| `valo_shell_desktop_complete.png` | 1920 × 1080 | PNG | Composição completa da shell de desktop com TopBar V2, FilesWindow V2, Dock V2 e ControlCenter V2 sobre wallpaper oceânico. |
| `valo_topbar_v2.png` | 1920 × 100 | PNG | Detalhe isolado da barra superior Circular L2 evidenciando refração, relógio e telemetria. |
| `valo_files_window_v2.png` | 940 × 640 | PNG | Chassi de janela Squircle G2 contínuo, titlebar integrada e grid de arquivos. |
| `valo_control_center_v2.png` | 410 × 600 | PNG | Painel flutuante Squircle G2 com sliders de volume, brilho e toggles rápidos. |
| `valo_dock_multiapp_v2.png` | 520 × 100 | PNG | Cápsula da Dock com múltiplos processos ativos e indicadores luminosos. |
| `valo_window_focus_comparison.png` | 1840 × 640 | PNG | Comparativo lado a lado de janela em foco (ativo, specular 1.25px) vs fora de foco (inativo, specular 0.75px). |
| `valo_high_contrast_shell.png` | 1920 × 1080 | PNG | Shell desktop no modo Alto Contraste (opacidade reforçada, bordas espessas, sem sombra). |
| `valo_reduced_motion_shell.png` | 1920 × 1080 | PNG | Shell desktop configurada para Movimento Reduzido (animações elásticas desativadas). |
| `valo_profiles_comparison.png` | 1820 × 640 | PNG | Comparativo dos três perfis ópticos de blur: Quality (8 passes), Balanced (4 passes) e Performance (2 passes). |

---

### 17. Análise de Consumo de Memória e Recursos GPU

- **UBO por Superfície:** 160 bytes por instância gráfica V2.
- **Texturas da Pirâmide Dual-Kawase:** 
  - Nível 0 ($960 \times 540$): 2.07 MB (RGBA8)
  - Nível 1 ($480 \times 270$): 518 KB
  - Nível 2 ($240 \times 135$): 129 KB
  - Nível 3 ($120 \times 67$): 32 KB
  - Total por provedor de blur: ~2.75 MB de VRAM.
- **Atlas de Glifos e Ícones:** 2048 × 2048 px alocado pelo Qt Quick Scene Graph de forma compartilhada.
- **Consumo Total de Memória da Shell:** Estável em ~92 MB de RSS, sem vazamentos de texturas ou buffers.

---

### 18. Diagnóstico de Integridade Óptica e Ausência de Artefatos

1. **Ausência de Color Banding:** A difusão Dual-Kawase com amostragem offset suaviza transições de cor mesmo em fundos de alto contraste.
2. **Sem Sombra Dupla (No Double-Shadow):** O chassi de peça única unifica o casting de sombras, eliminando sobreposições entre a barra de título e o corpo da janela.
3. **Ausência de Costuras (Seam-Free):** O contorno especular contínuo envolve uniformemente toda a geometria analítica.
4. **Zero Alpha Bleeding:** As máscaras SDF operam em espaço linear premultiplicado, garantindo bordas antialiased nítidas sem halos escuros ou transparentes.

---

### 19. Protocolo de Reversão de Emergência

Caso seja identificado qualquer comportamento anômalo em hardware de usuário específico, a reversão para o motor gráfico V1 pode ser efetuada em segundos:

1. **Via QML:**
   No arquivo `src/qml/main.qml`, altere a linha:
   ```qml
   readonly property bool useGraphicsV2: true
   ```
   para:
   ```qml
   readonly property bool useGraphicsV2: false
   ```
2. **Reinicie o serviço da shell:**
   ```bash
   systemctl --user restart systalia-shell
   ```
   A interface retornará instantaneamente aos componentes originais da versão 1.0 sem necessidade de recompilar a aplicação.

---

### 20. Lições Aprendidas e Boas Práticas Estabelecidas

1. **Registro Centralizado de Módulos QML Estáticos:** Quando bibliotecas estáticas compiladas com `NO_PLUGIN` são consumidas por múltiplos executáveis de teste, a invocação explícita de `qml_register_types_Systalia_Engine()` no início do `main()` ou `initTestCase()` é mandatória para evitar falhas silenciosas de resolução de tipos.
2. **Separação Rítmica de Passes no Scene Graph:** A execução de computação GPU fora do render pass principal (`beforeRendering()`) garante compatibilidade cross-platform (Vulkan e OpenGL) e elimina conflitos de alocação de command buffers.
3. **Parametrização Estrita de Acessibilidade:** Incorporar propriedades como `highContrast` e `prefersReducedMotion` diretamente no nível de superfícies base previne inconsistências visuais em aplicativos compostos.

---

### 21. Próximos Passos e Transição para a Etapa 06

Com a conclusão da Etapa 05 e a homologação da Shell QML no motor VALO Graphics 2.0, o sistema está pronto para a **Etapa 06 — Decorações de Janelas de Clientes Wayland XDG e Compositing Avançado**:

- **Foco da Etapa 06:** Aplicação do motor V2 nas molduras e decorações das janelas externas (servidor de decorações do Wayland), sombras de clientes XDG e aceleração completa do compositor.
- **Condição de Parada:** Em estrita observância às instruções do projeto, a execução é interrompida ao final da Etapa 05, aguardando autorização explícita para o início da Etapa 06.

