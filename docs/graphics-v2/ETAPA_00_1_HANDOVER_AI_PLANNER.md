# SYSTALIA — VALO GRAPHICS 2.0
# Relatório de Handover Arquitetural e Planejamento: Etapa 00 e 00.1
**Destinatário:** Agente de Inteligência Artificial / AI Planner  
**Autor:** Antigravity AI Graphics Systems Engine Pair  
**Data:** 07 de Novembro de 2026  
**Status do Projeto:** Etapa 00.1 Concluída — Pronto para Execução da Etapa 01  
**Repositório:** `~/Documentos/systalia-glass-engine`  
**Branch de Trabalho:** `valo-graphics-2.0`  
**Base Commit:** `232b4d8` (Merge branch 'main')  
**Suíte de Testes:** 36/36 CTest Aprovados (100% Verde, 0 Regressões)  

---

## 1. Contexto Executivo para o AI Planner

O projeto **Systalia Glass Engine (SGE)** hospeda o compositor Wayland do ambiente desktop **SYSTALIA**, integrado ao motor de renderização translúcido **VALO**.

O backend (servidor Wayland, XWayland, modelo de empilhamento Z-order, gerenciamento de janelas, D-Bus e serviços de sistema) está **maduro, estável e homologado por 36 suítes de teste automatizadas**.

A decisão estratégica foi **preservar integralmente o backend funcional e reconstruir a camada gráfica legada (VALO Graphics 1.0)**, que acumulava problemas estruturais:
- Conflito entre curvas circulares analíticas ($L_2$) e squircles Bézier ($G_2$), gerando desvios perimétricos de até 16.01 px nos cantos da Dock.
- Multiplicidade de itens vetoriais (`ShapePath`) sobrepostos sobre o shader.
- Sombra opaca interna cortando o corpo do vidro translúcido.
- Ausência de adaptação à luminância do fundo e falta de isolamento estrito entre decorações e o conteúdo de clientes Wayland.

---

## 2. O Manifesto VALO (Diretrizes Inegociáveis)

VALO não é um simples efeito estético de "vidro fumê". É um sistema de interfaces fundamentado em quatro pilares interligados:

$$\mathbf{V}\text{ (Vibrant)} \quad\times\quad \mathbf{A}\text{ (Adaptive)} \quad\times\quad \mathbf{L}\text{ (Lucid)} \quad\times\quad \mathbf{O}\text{ (Organic)}$$

### 2.1 Os Quatro Pilares
1. **V — Vibrant (Vibrante):**  
   Superfícies digitais vivas com refração física reativa ($IOR = 1.00 \dots 1.25$), dispersão de Cauchy ($\Delta_{\text{disp}} = 0.040$), arco espectral prismático a $-31^\circ$ (Ciano, Branco Gelo, Âmbar Dourado, Violeta Cósmico), reflexo de luz zenital na borda e cáusticas subaquáticas procedurais com dirty-tracking (congeladas após 5s de inatividade para economia de energia).
2. **A — Adaptive (Adaptável):**  
   Reatividade contínua ao ambiente sem invasão de privacidade. Ajuste dinâmico de opacidade de base ($0.25 \dots 0.55$) dependente da luminância média do papel de parede coberto, supressão atômica de desenho em janelas ocluídas/minimizadas (`shouldSuppressSceneGraphUpdate()`), escala de DPI mista e transição imediata para modo estático sob `prefers-reduced-motion` ou economia de energia. **Zero telemetria de hábitos do usuário ou IA opaca.**
3. **L — Lucid (Lúcido):**  
   A estética jamais pode comprometer a operação. Contraste mínimo WCAG AA ($\ge 4.5:1$ para textos e $\ge 3.0:1$ para glifos/bordas), eliminação de halos ofuscantes, fallback semi-opaco automático sobre fundos ruidosos e **integridade sagrada do buffer de clientes Wayland** (Alacritty, VS Code e navegadores são renderizados com nitidez nativa sem refração ou aberração no seu viewport interno).
4. **O — Organic (Orgânico):**  
   Física com inércia harmônica amortecida ($F = -kx - cv$), microinterações táteis na Dock ($1.08\times$ no hover, $0.94\times$ na compressão de clique), flexão elástica sutil durante o arraste de janelas ($\theta \le 1.5^\circ$) e geometria analítica contínua ($L_2$ e $G_2$).

### 2.2 Regras de Precedência em Conflitos de Decisão
- **Regra 1 (Lucid > Vibrant):** Legibilidade precede transparência. Se o contraste cair abaixo de 4.5:1, a opacidade da superfície é elevada automaticamente.
- **Regra 2 (Entrada Soberana > Deformação Física):** A região interativa de entrada (`wl_surface.set_input_region`) e os limites de clique nunca se descolam do ponteiro do mouse; deformações são restritas à casca visual externa.
- **Regra 3 (Integridade de Cliente > Efeitos Ópticos):** O buffer fornecido por clientes Wayland é inviolável e nunca é distorcido.
- **Regra 4 (Acessibilidade > Efeitos Visuais):** Ao detectar `prefers-reduced-motion` ou Alto Contraste, as cáusticas animadas e oscilações de mola são desativadas imediatamente.

---

## 3. Classificação dos Módulos do Sistema

| Categoria | Diretórios / Módulos | Ação Arquitetural do AI Planner |
| :--- | :--- | :--- |
| **CORE** | `src/compositor/*` (`SystaliaCompositor`, `WindowStackModel`, `XWaylandManager`, `OutputManagerService`, `SystaliaSurfaceExtension`)<br/>`src/system/*` (`Telemetry`, `ProcessLauncher`, `AppRegistry`, `NotificationServer`, `Logind`, `LiquidTiling`, `InputDevice`, `WorkspaceManager`, `SystemControl`) | **PRESERVAR 100%.** Não modificar contratos públicos, sinais, slots ou loops de eventos. Não reimplementar o que já funciona. |
| **BRIDGE** | `src/qml/WaylandWindowContainer.qml`<br/>`src/gfx/SystaliaClientClip.hpp`<br/>`src/gfx/SystaliaSurfaceProps.hpp`<br/>`src/qml/WorkspaceViewport.qml` | **MANTER CONTRATOS.** Isolar dependências visuais. Assegurar apresentação direta de `ShellSurfaceItem` com `hideSource: false`. |
| **GRAPHICS** | `src/gfx/SystaliaGlassItem.*`<br/>`src/gfx/SystaliaGlassMaterial.*`<br/>`shaders/valo_optical.frag/.vert`<br/>`src/qml/ValoGlass.qml`<br/>`src/qml/shell/SystaliaG2Frame.qml`<br/>`src/qml/shell/SystaliaDock.qml`<br/>`src/qml/shell/SystaliaTopBar.qml`<br/>`src/qml/shell/SystaliaFilesWindow.qml`<br/>`src/qml/shell/SystaliaControlCenter.qml` | **RECONSTRUIR NA V2.** Desenvolver em paralelo nos diretórios dedicados (`src/gfx/v2/`, `shaders/v2/`, `src/qml/graphics/v2/`) sem interferir no renderer V1 ativo. |
| **LEGACY** | `src/qml/WaylandClientView.qml`<br/>`src/qml/shell/SystaliaMaterialWindow.qml` | **DESCOMISSIONAR APÓS MIGRAÇÃO.** Substituir por overlays de diagnóstico e contêineres modernos. |

---

## 4. Engenharia do Motor Gráfico V2 (Especificações Técnicas)

### 4.1 Separação em Quatro Domínios Estritos (Organic Boundaries)
```text
┌────────────────────────────────────────────────────────────────────────┐
│ 1. DOMÍNIO LÓGICO (Compositor): X, Y, Width, Height, Tiling, xdg-shell │
└──────────────────┬─────────────────────────────────┬───────────────────┘
                   │                                 │
                   ▼                                 ▼
┌───────────────────────────────────┐ ┌──────────────────────────────────┐
│ 2. DOMÍNIO VISUAL ORGÂNICO (Casca)│ │ 4. DOMÍNIO INTERATIVO (Entrada)  │
│   - Mola inercial (dx, dy)        │ │   - wl_surface.set_input_region  │
│   - Flexão angular (theta <= 1.5°)│ │   - Mapeamento direto de cursor  │
│   - Conteúdo cliente NÃO é afetado│ │   - Zero descolamento de clique  │
└──────────────────┬────────────────┘ └──────────────────────────────────┘
                   │
                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 3. DOMÍNIO DE MATERIAL E SHADER (SDF L2/G2, Refração, Blur, Especular) │
└────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Novo Contrato de Memória GPU: `SystaliaSurfacePropsV2`
- **Tamanho:** Exatamente **160 bytes** (alinhados a 16 bytes sob a especificação std140).
- **Isolamento de ABI:** O contrato legado de 128 bytes (`SystaliaSurfaceProps`) em `src/gfx/SystaliaSurfaceProps.hpp` permanece intocado. O V2 reside exclusivamente em `src/gfx/v2/SystaliaSurfacePropsV2.hpp`:

```cpp
struct alignas(16) SystaliaSurfacePropsV2 {
    Matrix4x4_std140 matrix;               // offset 0,   tamanho 64 (MVP Matrix)
    QVector4D radii;                       // offset 64,  tamanho 16 (TL, TR, BL, BR)
    QVector2D windowSize;                  // offset 80,  tamanho 8  (Largura, Altura)
    QVector2D screenResolution;            // offset 88,  tamanho 8  (Resolução do Viewport)
    float slant = 0.0f;                    // offset 96,  tamanho 4  (Inclinação 3D)
    float ior = 1.25f;                     // offset 100, tamanho 4  (Índice de Refração)
    float chromaticDispersion = 0.040f;    // offset 104, tamanho 4  (Dispersão de Cauchy)
    float causticIntensity = 0.85f;        // offset 108, tamanho 4  (Intensidade das Cáusticas)
    float surfaceOpacity = 0.34f;          // offset 112, tamanho 4  (Opacidade do Vidro)
    float time = 0.0f;                     // offset 116, tamanho 4  (Tempo em segundos)
    float hasClientTexture = 0.0f;         // offset 120, tamanho 4  (Flag 0.0 ou 1.0)
    float titleBarHeight = 36.0f;          // offset 124, tamanho 4  (Altura da barra de título)
    // --- Novos parâmetros V2 ---
    float specularStrokeWidth = 1.25f;     // offset 128, tamanho 4  (Espessura da borda especular)
    float geometryMode = 0.0f;             // offset 132, tamanho 4  (0.0 = L2 Circular, 1.0 = G2 Squircle)
    float shadowIntensity = 0.35f;         // offset 136, tamanho 4  (Opacidade da sombra externa)
    float shadowOffsetY = 12.0f;           // offset 140, tamanho 4  (Deslocamento vertical da sombra)
    QVector4D volumetricTint = {0.03f, 0.18f, 0.32f, 0.39f}; // offset 144, tamanho 16
};
static_assert(sizeof(SystaliaSurfacePropsV2) == 160);
```

### 4.3 Métricas de Tolerância Geométrica
- **Tolerância Analítica Contínua:** $|\Delta_{\text{analítico}}| < 0.01\text{ px}$ entre a malha vetorial e o SDF do shader.
- **Tolerância de Rasterização Discreta:** $|\Delta_{\text{raster}}| \le 0.5\text{ px}$ físico (resolvendo o desvio histórico de 16.01 px da V1).

### 4.4 Coordenadas Multi-Monitor e Alpha Pré-Multiplicado
- As coordenadas UV do backdrop são normalizadas sobre o canvas virtual unificado gerenciado por `OutputManagerService::virtualCanvasBounds()`.
- O pipeline opera estritamente em alpha pré-multiplicado (`fragColor = vec4(rgb * a, a)`), eliminando halos escuros na integração com clientes Wayland.

---

## 5. Cronograma Mestre de Implementação (Fases 00 a 08)

O AI Planner deve orquestrar as fases incrementalmente, garantindo que os fundamentos de **Adaptive** e **Organic** existam desde as etapas iniciais:

- **Etapa 00 — Auditoria Arquitetural e Design System:**  
  *Status:* **CONCLUÍDA**. Documentação completa e 36/36 testes passando.
- **Etapa 00.1 — Consolidação da Filosofia VALO:**  
  *Status:* **CONCLUÍDA**. Formalização de V+A+L+O, regras de prioridade e alinhamento de tolerâncias.
- **Etapa 01 — Fundação do VALO Graphics V2 e Sistema Geométrico Analítico:**  
  *Tarefas:* Criação de `SystaliaSurfacePropsV2.hpp`, implementação do gerador de SDF $L_2$ e $G_2$, contratos de isolamento orgânico (`ValoOrganicTransform`) e hooks de acessibilidade (`prefersReducedMotion`).  
  *Critério de Saída:* Testes `tst_SdfGeometryMathV2` aprovados com erro analítico $< 0.01\text{ px}$.
- **Etapa 02 — Renderização de Materiais e Máscaras V2:**  
  *Tarefas:* Implementação de `ValoGlassMaterialV2` e `ValoGlassItemV2` com `valo_optical_v2.frag` básico em passe único, alpha pré-multiplicado e adaptação de luminância para contraste WCAG AA.
- **Etapa 03 — Dual-Kawase Backdrop Blur, Refração e Efeitos Ópticos V2:**  
  *Tarefas:* Pipeline Dual-Kawase em FBOs com dirty-tracking, refração multi-monitor e modulação de cáusticas.
- **Etapa 04 — Protótipo Isolado da Dock VALO V2 e Validação Visual:**  
  *Tarefas:* Construção de `ValoDockV2.qml` com os 5 ícones vetoriais do Figma, física de mola amortecida ($k=300, \zeta=0.82$) e validação dos 8 passes diagnósticos e 7 estados da Dock.
- **Etapa 05 — Reconstrução dos Componentes QML da Shell:**  
  *Tarefas:* Migração da TopBar (preservando rigorosamente o layout e telemetria atuais), Control Center e Files Window para a infraestrutura V2.
- **Etapa 06 — Integração com Decorações de Janelas Wayland e XWayland:**  
  *Tarefas:* Conexão de `WaylandWindowContainer.qml` ao motor V2, preservando apresentação direta do `ShellSurfaceItem` e suporte a `z_systalia_surface_v1`.
- **Etapa 07 — Otimização de Performance, Perfis Gráficos e Interações:**  
  *Tarefas:* Calibração dos perfis Quality, Balanced e Performance, pooling de texturas QRhi e microinterações táteis.
- **Etapa 08 — Validação Final, Decomissionamento Seguro e Limpeza:**  
  *Tarefas:* Remoção do código legado em `src/gfx/` antigo e validação final da suíte completa de testes.

---

## 6. Índice da Documentação Arquitetural de Referência

O AI Planner deve consultar os seguintes documentos na pasta `docs/graphics-v2/`:

1. [`VALO_DESIGN_PHILOSOPHY.md`](file:///home/lucasramos/Documentos/systalia-glass-engine/docs/graphics-v2/VALO_DESIGN_PHILOSOPHY.md) — Filosofia formal V+A+L+O, princípios de UX e contratos de acessibilidade.
2. [`ARCHITECTURE.md`](file:///home/lucasramos/Documentos/systalia-glass-engine/docs/graphics-v2/ARCHITECTURE.md) — Arquitetura técnica em 3 camadas, Pipeline de Superfície Única e UBO V2 de 160 bytes.
3. [`LEGACY_AUDIT.md`](file:///home/lucasramos/Documentos/systalia-glass-engine/docs/graphics-v2/LEGACY_AUDIT.md) — Diagnóstico dos defeitos gráficos legados e mapeamento dos módulos CORE.
4. [`FIGMA_CSS_TOKENS_AUDIT.md`](file:///home/lucasramos/Documentos/systalia-glass-engine/docs/graphics-v2/FIGMA_CSS_TOKENS_AUDIT.md) — Matriz Figma × CSS × 124 Tokens VALO e preservação da TopBar.
5. [`MIGRATION_PLAN.md`](file:///home/lucasramos/Documentos/systalia-glass-engine/docs/graphics-v2/MIGRATION_PLAN.md) — Estratégia de coexistência V1/V2, protótipo da Dock e cronograma de 9 etapas.
6. [`TEST_STRATEGY.md`](file:///home/lucasramos/Documentos/systalia-glass-engine/docs/graphics-v2/TEST_STRATEGY.md) — Pirâmide de testes em 7 camadas, tolerâncias subpixel e matriz de perfis gráficos.

---

## 7. Critérios de Aceite para Acionamento da Etapa 01

A execução da **Etapa 01** está formalmente autorizada assim que o AI Planner aprovar este plano, respeitando as seguintes salvaguardas:
- [x] O compositor Wayland e todos os serviços de sistema permanecem operando sem qualquer alteração.
- [x] O motor V2 será construído em diretórios isolados (`src/gfx/v2/`, `shaders/v2/`).
- [x] Todos os 36 testes CTest continuam passando (100% verde).
- [x] A transição gráfica preserva os contratos de janela de clientes sem regressões de visibilidade.

