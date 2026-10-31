# Relatório de Auditoria e Conformidade Visual: style.css vs. QML
**Systalia Glass Engine (SGE) — VALO Desktop**  
**Data:** 31 de Outubro de 2026  
**Fonte de Verdade:** `style.css` (4191 linhas extraídas do Figma `SYSTALIA — VALO | Desktop Concept`)

---

## 1. Resumo Executivo da Auditoria

O arquivo `style.css` reflete o frame principal estático de **1662 × 975 px** (`node 2:2`). A auditoria cruzou as definições determinísticas de CSS (geometria, cores, sombras, gradientes, bordas e tipografia) com a implementação em QML (`src/qml/shell/`).

### Status Geral por Componente
| Componente | Conformidade | Situação |
|---|:---:|---|
| **Botões de Controle de Janela** (`SystaliaWindowAction.qml`) | 95% | Convertido para QML nativo, curvatura corrigida, gradientes radiais e glifos alinhados. |
| **Cabeçalhos de Janela** (`SystaliaWindowHeader.qml`) | 90% | Badges, títulos e ícones calibrados. Falta reflexo sutil no vidro de fundo do SSD. |
| **Janela Systalia Files** (`SystaliaFilesWindow.qml`) | 80% | Sidebar e item "Home" atualizados com gradiente 135° e inset shadow. Falta refinar grid de arquivos e breadcrumb. |
| **Janela Systalia Control Center** (`SystaliaMaterialWindow.qml`) | 85% | Sliders, números contidos e toggle switch 3D calibrados. Falta sidebar esquerda (dropdowns). |
| **Barra Superior (TopBar)** (`SystaliaTopBar.qml`) | 75% | Cápsula e tipografia presentes; pendente alinhar glifos da direita e sombras exatas. |
| **Floating Dock** (`SystaliaDock.qml`) | 80% | Dimensões e slots corretos; reflexos de vidro da cápsula ainda simplificados. |

---

## 2. Detalhamento por Componente e Pendências

### 2.1 Botões de Controle de Janela (`SystaliaWindowAction.qml`)
* **Implementado com sucesso:**
  - Esfera circular perfeita de diâmetro `28px` com raio `14px` (sem cortes no topo ou caixas ao redor).
  - Gradiente radial de 4 stops: Ciano/Azul (`#B0FAFF` → `#00C8FF` → `#007DFC` → `#073E9E`) para minimizar/maximizar e Carmesim (`#FFB1CB` → `#FF3B60` → `#9E1448` → `#540D30`) para fechar.
  - Glifos vetoriais nativos em QML (`strokeWidth: 1.65`, `1.5`, `1.6`) com sombra projetada (`dy: 1px`, `rgba(0,0,0,0.4)`).
* **O que ainda pode ser refinado:**
  - Adicionar o estado de **janela inativa / sem foco**: atenuar ligeiramente a saturação dos botões quando a janela perde o foco ativo do Wayland/Shell.

---

### 2.2 Janela Systalia Files (`SystaliaFilesWindow.qml`)
* **Geometria Base:** `701px × 534px`, `border-radius: 20px`, `top: 193px, left: 113px`.
* **Sidebar (Navegação Esquerda):**
  - **Concluído:** Container `181px × 469px` em `(10, 51)` com `rgba(8, 26, 45, 0.88)` e raio de `14px`.
  - **Concluído:** Item ativo "Home" com gradiente 135°, inset highlight superior (`1px`, `#52ffffff`) e inset shadow inferior (`3px`, `#6606182d`).
  - **Pendente:** Efeito de hover nos itens inativos (`Recent`, `Desktop`, `Documents`, etc.) com highlight sutil (`rgba(159, 239, 255, 0.08)`).
* **Breadcrumb e Barra de Ferramentas:**
  - **Pendente:** Botões de navegação `‹ ›` (`left: 207px, top: 60px`, `font-size: 26px`, cor `rgba(140, 241, 255, 0.92)`).
  - **Pendente:** Botão de pasta atual `⌂ Home` (`width: 90px, height: 30px`, `left: 276px, top: 60px`, fundo `rgba(16, 56, 82, 0.85)`).
  - **Pendente:** Divisor horizontal (`width: 476px, height: 1px`, `left: 208px, top: 97px`, cor `rgba(178, 243, 253, 0.3)`).
* **Grade de Arquivos (6 Ícones de Folha Acrílica Translúcida):**
  - **Pendente:** Os ícones (`All Files`, `Documents`, `Document`, `Download2`, `Divides 1`, `Systion 4`) possuem no CSS um efeito complexo de folha acrílica dobrada (`Folded translucent sheet icon`):
    - Fundo: `linear-gradient(130.54deg, rgba(242, 255, 255, 0.25) 10.64%, rgba(133, 246, 255, 0.18) 26.95%, rgba(61, 151, 206, 0.14) 47.52%, rgba(171, 151, 238, 0.16) 65.96%, rgba(27, 81, 142, 0.3) 81.56%)`.
    - Dobra da folha (`Paper fold`): `16px × 16px`, `rgba(230, 255, 255, 0.39)` com borda fina de `0.8px`.
    - No QML atual, alguns itens usam PNGs rasterizados que podem ser aprimorados para renderização vetorial ou posicionamento exato.

---

### 2.3 Janela Systalia Control Center (`SystaliaMaterialWindow.qml`)
* **Geometria Base:** `700px × 534px`, `border-radius: 20px`, `top: 193px, left: 864px`.
* **Painel da Direita (Options / Sliders):**
  - **Concluído:** Container `454px × 452px` em `(224, 59)` com `rgba(7, 29, 50, 0.76)` e raio de `13px`.
  - **Concluído:** Trilhos e manípulos dos sliders calibrados com gradiente ciano e thumb esférico 3D.
  - **Concluído:** Alinhamento dos valores numéricos (`85%`, `1.25`, etc.) contidos dentro do painel sem transbordar.
  - **Concluído:** Toggle switch com trilho 135° e knob esférico 3D.
* **Sidebar Esquerda (Navegação do Painel de Ajustes):**
  - **Pendente:** A lista de seções (`General`, `Status`, `Control Center`, `Software`, `Drivers`, `About`) no CSS possui o item `Control Center` ativo com o mesmo estilo da cápsula do `Home`:
    - `width: 175px, height: 32px`, gradiente 135° e inset shadow.
  - **Pendente:** Menu inferior de 3 botões/dropdowns (`Dropdown 0, 1, 2`):
    - `Default`, `Buttons`, `Options` com `width: 165px, height: 32px`, `border-radius: 15px`.
  - **Pendente:** Linha divisória vertical (`Menu divider`): `width: 1px, height: 440px, left: 206px, top: 64px`, cor `rgba(200, 247, 251, 0.28)`.

---

### 2.4 Efeito de Vidro e Prisma Óptico das Janelas (`Upper cyan gloss` & `Caustics`)
* **Upper Cyan Gloss (Camada de Brilho Perimetral):**
  - CSS: `width: 689px, height: 524px, left: 6px, top: 4px, border-radius: 14px`.
  - Gradiente: `linear-gradient(130.23deg, rgba(200, 255, 255, 0.25) 13%, rgba(97, 240, 255, 0.13) 45%, rgba(21, 82, 121, 0.03) 74%, rgba(11, 24, 52, 0) 106%)`.
  - Sombra interna: `inset 0px 1px 1px rgba(255, 255, 255, 0.21)`.
  - **Pendente:** Garantir que o `SystaliaG2Frame.qml` ou a camada superior de reflexo incorpore este inset highlight superior nas molduras.

---

### 2.5 Barra Superior (`SystaliaTopBar.qml`)
* **Geometria:** `1646px × 45px`, `left: 8px, top: 4px`, `border-radius: 22px`.
* **Superfície:** `background: rgba(7, 22, 40, 0.86); border: 1px solid rgba(116, 213, 238, 0.24); box-shadow: 0px 10px 20px rgba(9, 186, 237, 0.2)`.
* **Pendente:**
  - Glifos de status da direita (`回`, `>`, `_`, `☷`, `☼`, `◫`, `⏻`): padronizar os tamanhos (`16px–18px`) e espaçamento de `16px` entre eles.
  - Relógio central: formato `MON 19 OCT > 15:30:22` com separador `>` e tipografia Inter Medium 17px (`#D5E2EE`).

---

### 2.6 Floating Dock (`SystaliaDock.qml`)
* **Geometria:** `555px × 125px`, `left: 555px, top: 817px`, `border-radius: 47px`.
* **Fundo Orgânico de Vidro:**
  - `background: rgba(6, 24, 45, 0.82)`.
  - `border: 1.5px solid rgba(115, 214, 238, 0.4)`.
  - `box-shadow: 0px 25px 50px rgba(0, 229, 255, 0.16), 0px 10px 20px rgba(0, 0, 0, 0.5)`.
* **Slots de Ícones:**
  - 5 slots de `78px × 78px`, espaçamento regular, com cantos arredondados de `17px`.
  - **Pendente:** Ajustar os tamanhos de visualização interna dos ícones (pasta, áudio, terminal, código e estrela magenta) para centralização e escala uniformes.

---

## 3. Plano de Ação Recomendado (Próximos Passos)

1. **Prioridade 1 — Sidebar e Dropdowns da Janela de Configurações:**
   - Adicionar o divisor vertical de `440px` (`left: 206px`).
   - Implementar os 3 botões/dropdowns ovais inferiores (`Default`, `Buttons`, `Options`) com `border-radius: 15px`.
2. **Prioridade 2 — Breadcrumb e Ícones da Janela Files:**
   - Adicionar a linha divisória horizontal (`476px × 1px`) em `top: 97px`.
   - Ajustar os botões de navegação `‹ ›` e o campo de busca.
3. **Prioridade 3 — Polimento da TopBar e Dock:**
   - Aplicar os tokens exatos de sombra ciano na cápsula da TopBar e da Dock.
   - Uniformizar os glifos e atalhos rápidos.

