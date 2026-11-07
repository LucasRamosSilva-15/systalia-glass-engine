# Auditoria Cruzada: Figma Concept × style.css × Tokens VALO

**Documento:** `docs/graphics-v2/FIGMA_CSS_TOKENS_AUDIT.md`  
**Data:** 07 de Novembro de 2026  
**Status:** Etapa 00 — Auditoria Arquitetural e Design System  
**Branch:** `valo-graphics-2.0`  
**Fontes de Verdade Auditadas:**
1. **Figma Oficial:** [SYSTALIA — VALO | Desktop Concept (`node-id=2-2`)](https://www.figma.com/design/kax6aiYKgM2Fk7dllyZRRF/SYSTALIA-%E2%80%94-VALO-%7C-Desktop-Concept?node-id=2-2)
2. **CSS Exportado:** `style.css` (6065 linhas de regras de estilo extraídas do frame do Figma)
3. **Design Tokens:** `src/tokens/valo.tokens.json`, `src/tokens/ValoTokens.hpp`, `src/qml/theme/ValoTheme.qml` (124 tokens homologados)

---

## 1. Princípios de Hierarquia e Resolução de Conflitos

Para guiar as decisões da reconstrução gráfica no **VALO Graphics 2.0**, foi estabelecida a seguinte ordem rigorosa de precedência entre as fontes de verdade:

1. **Figma Oficial (Prioridade 1):** Referência soberana para hierarquia visual, dimensões globais, curvaturas, materiais ópticos, vetores SVG e composição espacial.
2. **`style.css` (Prioridade 2):** Especificação matemática detalhada das propriedades CSS compiladas a partir dos nós do Figma (paradas de gradientes, sombras `box-shadow`, opacidades e transformações). *Nota: tratado como especificação técnica de consulta, não executado diretamente como folha de estilos Qt.*
3. **Tokens VALO (Prioridade 3):** Valores numéricos reutilizáveis, contratos semânticos de cores e métricas do sistema.
4. **Código Atual / QML (Prioridade 4):** Referência de comportamentos funcionais, bindings de dados, contratos de sinais e integrações de sistema.

> [!IMPORTANT]
> **Exceção Funcional Mandatória:**  
> A organização espacial da **TopBar** no aplicativo (posicionamento da logomarca à esquerda, telemetria ao lado, relógio centralizado, seletor dos 4 workspaces e indicadores do sistema à direita) deve ser **estritamente preservada**. O Figma define o acabamento estético do vidro, bordas e tipografia da TopBar, mas **não deve forçar uma reorganização do seu layout funcional**.

---

## 2. Matriz Geral de Auditoria Cruzada

A tabela a seguir confronta os parâmetros chave dos componentes do sistema entre o conceito no Figma, as regras extraídas em `style.css`, os 124 tokens homologados e define as ações técnicas para a V2:

| Componente / Propriedade | Valor Figma | Valor `style.css` | Token VALO | Divergência Encontrada | Ação Técnica no VALO Graphics 2.0 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Resolução do Canvas** | 1662 × 975 px | `width: 1662px; height: 975px;` | N/A (Métrica de viewport) | Nenhuma | Canvas base mantido em 1662 × 975 com suporte responsivo HiDPI |
| **Fundo Oceânico (Base)** | `#061426` | `background: #061426;` | `primitive/navy-950` (`#061426`) | Nenhuma | Fundo mestre ancorado no token primitivo |
| **Blur de Fundo (Backdrop)**| 36 px (Gaussiano) | `backdrop-filter: blur(36px);` | `blur/backdrop` (`36`) | Nenhuma | Shader Dual-Kawase V2 calibrado para raio efetivo de 36 px |
| **TopBar — Dimensões** | 1646 × 45 px | `width: 1646px; height: 45px;` | N/A (Métrica de componente) | Nenhuma | TopBar fixada em 1646 × 45 px |
| **TopBar — Raio dos Cantos**| 22 px (Pill) | `border-radius: 22.5px;` | `radius/topbar` (`22`) | Arredondamento subpixel no CSS | Usar raio nominal de 22 px com `circularCorners: true` |
| **TopBar — Opacidade Vidro**| 86% | `background: rgba(7, 22, 40, 0.86);` | `opacity/topbar` (`0.86`) | Nenhuma | Usar `ValoTheme.opacityTopbar` (0.86) |
| **TopBar — Borda Especular**| 1.0 px Especular | `border: 1px solid rgba(138,245,255,0.25)` | `stroke/fine` (`1.0`) | Nenhuma | Traço de 1.0 px com gradiente zenital de iluminação |
| **Dock — Dimensões** | 555 × 125 px | `width: 555px; height: 125px;` | N/A (Métrica de componente) | Nenhuma | Cápsula fixada em 555 × 125 px |
| **Dock — Raio dos Cantos** | 47 px (Pill orgânica) | `border-radius: 47px;` | `radius/dock` (`47`) | Nenhuma | Geometria analítica $L_2$ (`PathArc`) com raio exato de 47 px |
| **Dock — Gradiente de Corpo**| Oceânico 3 stops | `#5D9FB8` 29%, `#194C70` 34%, `#0A2741` 39% | `opacity/dock` (`0.34`) | CSS tem paradas lineares a 180° | Injetar gradiente oceânico triplo como preenchimento volumétrico |
| **Dock — Borda Especular** | 1.25 px contínua | `border: 1.2px solid rgba(184,249,255,0.45)` | `specular/stroke-width` (`1.5`) | Token global é 1.5px; Dock usa 1.25px | Respeitar `strokeWidth: 1.25` específico da Dock no V2 |
| **Dock — Sombra Projetada** | `box-shadow: 0 12px 20px` | `rgba(0, 13, 33, 0.35)` | `shadow/dock-blur` (`20`) | Código legado colocava retângulo opaco a $z=0$ | Sombra externa suave ancorada abaixo da base sem cortar o vidro |
| **Dock — Ícones (Slots)** | 5 lançadores vetoriais | 5 grupos SVG em $x = [33.75..410.75]$ | `size/dock-icon` (`78`) | Nenhuma | 5 instâncias de `ValoDockItem` com vetores SVG oficiais |
| **Files — Dimensões** | 885 × 547 px | `width: 885px; height: 547px;` | N/A (Métrica de componente) | Nenhuma | Dimensão nominal 885 × 547 com suporte a redimensionamento |
| **Files — Raio dos Cantos** | 20 px | `border-radius: 20px;` | `radius/window` (`20`) | Nenhuma | Geometria Squircle $G_2$ de 20 px |
| **Files — Sidebar** | 240 px largura, r=14 | `background: rgba(8, 26, 45, 0.88)` | `opacity/sidebar` (`0.88`), `radius/sidebar` (`14`) | Nenhuma | Sidebar translúcida escura com raio 14 px |
| **Files — Ícones 3D Acrílicos**| Vetores com brilho | Grupos de vetores com gradientes e glints | `size/file-icon-width` (`55`), `size/file-icon-height` (`67`) | Código antigo usava imagens PNG estáticas | Renderizar via SVG otimizado ou glifos vetoriais nativos |
| **Control Center — Dimensões**| 728 × 498 px | `width: 728px; height: 498px;` | N/A (Métrica de componente) | Nenhuma | Janela fixada em 728 × 498 px com painéis adaptativos |
| **Control Center — Raio** | 20 px | `border-radius: 20px;` | `radius/window` (`20`) | Nenhuma | Squircle $G_2$ de 20 px unificada |
| **Controles de Janela (Fechar)**| 28 px diâmetro | Esfera radial 4 paradas carmesim | `size/window-control` (`28`) | Nenhuma | Gradiente radial `#FFB1CB` → `#FF3B60` → `#9E1448` → `#540D30` |
| **Controles de Janela (Min/Max)**| 28 px diâmetro | Esfera radial 4 paradas ciano/azul | `size/window-control` (`28`) | Nenhuma | Gradiente radial `#B0FAFF` → `#00C8FF` → `#007DFC` → `#073E9E` |
| **Sliders de Calibração** | Altura 12 px, thumb 20 px| Trilho com progresso `#14C4E7` → `#17F5DF`| `size/slider-height` (`12`), `size/slider-thumb` (`20`) | Código legado transbordava valor | Trilho de 12 px com cápsula esférica 3D contida |
| **Dispersão Cromática** | Ângulo -31°, arco-íris | Gradientes em -31° com 4 paradas | `glass/prism-angle-deg` (`-31`) | Nenhuma | Efeito prismático espectral com paradas ciano, branco, âmbar, violeta |

---

## 3. Análise Detalhada Componente a Componente

### 3.1 Atmosfera Oceânica e Fundo do Desktop
O Figma define uma cena subaquática atmosférica densa constituída por:
- **Plano de Fundo Principal:** Base azul-marinho profundo (`#061426`).
- **Feixes de Luz Descendentes:** 7 raios volumétricos orientados com desfoque de 50 px (`blur/volumetric-ray: 50`).
- **Halos de Luz Difusa:** 3 halos circulares ciano com desfoque de 35 px (`blur/halo: 35`).
- **Partículas Bioluminescentes:** Múltiplos pontos de luz com opacidade variando de 0.05 a 0.25 e dispersão aleatória.
- **Implementação V2:** O wallpaper deve ser pré-composto ou gerado via shader de baixa frequência para alimentar o `u_BackdropTexture` sem pesar o loop de renderização da shell.

### 3.2 TopBar (`SystaliaTopBar.qml`)
- **Geometria:** 1646 × 45 px, centrada com margem superior de 8 px, cantos arredondados $R = 22\text{ px}$.
- **Contrato Visual:**  
  Vidro translúcido escuro de 86% de opacidade (`#071628` @ 0.86), reflexo superior especular sutil e sombra inferior difusa.
- **Auditoria de Divergência:** O layout no Figma posicionava o relógio mais à direita e agrupava workspaces diferentemente. **Conforme diretriz, o layout do código atual é a fonte de verdade para a organização espacial**; a V2 aplica apenas o acabamento óptico e de bordas sem alterar a ordem dos elementos.

### 3.3 Floating Dock (`SystaliaDock.qml`)
- **Geometria:** 555 × 125 px, raio 47 px.
- **Materiais Ópticos:**
  1. **Passo Refrativo:** Distorção de 45 px com $IOR = 1.25$ e dispersão cromática de Cauchy ($\Delta = 0.040$).
  2. **Gradiente do Corpo:** Paradas oceânicas `#5D9FB8` (0.29), `#194C70` (0.34) e `#0A2741` (0.39).
  3. **Borda Especular:** Traço único de 1.25 px em todo o perímetro.
  4. **Sombra Externa:** Sombra de oclusão inferior suave (`box-shadow: 0px 12px 20px rgba(0, 13, 33, 0.35)`). A sombra é externa e projetada abaixo do chassi, sem introduzir retângulos escuros internos no vidro.
- **Lançadores e Indicadores:**  
  5 ícones vetoriais SVG oficiais. Indicador ativo como pílula ciano de 14 × 3.5 px com halo luminoso de 5 px; indicador em segundo plano como ponto de 5 × 5 px.

### 3.4 Janelas de Aplicativo (`SystaliaFilesWindow` e `SystaliaControlCenter`)
- **Geometria Comum:** Raio dos cantos de 20 px com continuidade $G_2$.
- **Barra de Título:** 36 px de altura, badge do sistema com cantos de 6 px e cor `#0C314F` @ 0.78, títulos na cor `#87BADA`.
- **Botões de Controle de Janela:** Diâmetro exato de 28 px, gradiente radial esférico com reflexo especular na metade superior e glifos brancos com espessura de 1.5 a 1.65 px.

---

## 4. Classificação dos Tokens: Globais vs. Locais

A auditoria identificou que transformar cada número do `style.css` em token global sobrecarrega o sistema de temas. Foi definida a seguinte separação:

### Tokens Globais Reutilizáveis (Mantidos no `ValoTheme`)
- **Paleta de Primitivas e Semântica:** Todas as 33 primitivas e 35 cores semânticas (`background/*`, `surface/*`, `text/*`, `border/*`).
- **Métricas Compartilhadas:** Escala de espaçamento (2, 4, 8, 12, 16, 20, 24, 32), raios canônicos (`radius/window: 20`, `radius/dock: 47`, `radius/topbar: 22`, `radius/button: 15`).
- **Óptica do Vidro:** Opacidades mínimas/máximas (0.25 e 0.41), blurs de backdrop (36 px) e cáusticas (44 px).

### Métricas Específicas de Componentes (Encapsuladas no QML)
- Coordenadas de ancoragem da Dock ($x=33.75, 128.75, \dots$).
- Largura do trilho de opções do Control Center ($180\text{ px}$).
- Posição dos breadcrumbs do Files Window ($x=207, y=60$).

---

## 5. Proposta de Extensão do Design System para a V2 (Sem Quebra de ABI)

Os 124 tokens existentes atendem plenamente às especificações atuais. Para a V2, propõe-se adicionar de forma não destrutiva os seguintes tokens complementares:

```json
{
  "material_v2": {
    "specular/dock-stroke-width": 1.25,
    "dock/fill-stop-0": "#4A5D9FB8",
    "dock/fill-stop-1": "#57194C70",
    "dock/fill-stop-2": "#630A2741",
    "window/titlebar-height": 36,
    "window/client-inset": 8
  }
}
```

> [!NOTE]
> Nenhum token existente foi modificado ou excluído durante esta auditoria, mantendo conformidade de 100% com `tests/unit/tst_ValoTheme.cpp`.

