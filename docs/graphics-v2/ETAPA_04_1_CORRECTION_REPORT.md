# SYSTALIA — VALO GRAPHICS 2.0
## ETAPA 04.1 — RELATÓRIO DE CORREÇÃO VISUAL, INTEGRIDADE GPU E VALIDAÇÃO DA DOCK V2

**Projeto:** SYSTALIA — VALO Graphics 2.0  
**Repositório:** `~/Documentos/systalia-glass-engine`  
**Branch:** `valo-graphics-2.0`  
**Tecnologias:** C++23, Qt 6.11.2, Qt Quick Scene Graph, QSGMaterial, QRhi/OpenGL/Vulkan, GLSL 450, SPIR-V/QSB  
**Baseline anterior:** 45/45 testes aprovados (auditoria identificou defeitos visuais, sombra em retângulos sobrepostos, passe E offline e estado Focused idêntico a Idle).  
**Resultado da Etapa 04.1:** **PASS — 45/45 testes CTest aprovados**, 20 artefatos oficiais regenerados com execução real na GPU, geometria unificada em peça única, sombra contínua L2, alpha pré-multiplicado estrito e desacoplamento do UBO físico.

---

### 1. Diagnóstico da Duplicação Visual

Na auditoria da Etapa 04, a captura `dock_v2_pass_H_composite.png` apresentava uma aparência de duas cápsulas sobrepostas: uma cápsula azul superior e uma área escura inferior contendo os cinco lançadores.

**Causas raiz identificadas:**
1. **Artefato de fundo espúrio em `ValoDockV2Scene.qml`:**
   Nas linhas 37-40 do arquivo `ValoDockV2Scene.qml`, havia uma forma decorativa residual hardcoded no wallpaper oceânico:
   ```qml
   Rectangle {
       x: 350; y: 400; width: 580; height: 180; radius: 90
       color: Qt.rgba(0.09, 0.45, 0.65, 0.18)
   }
   ```
   Esta forma possuía exatamente as proporções e geometria de uma cápsula de dock e pairava diretamente no backdrop logo acima da Dock real ($y=555$), criando o efeito de uma segunda cápsula.
2. **Sombra em degraus retangulares concêntricos:**
   O componente `ValoDockV2.qml` utilizava dois `Rectangle` com `anchors.margins: -10` e `anchors.margins: -4` deslocados em $Y=12$, cujas bordas opacas recortadas criavam um degrau escuro rígido sob os ícones.

**Correção implementada:**
- Remoção completa da cápsula residual no wallpaper de `ValoDockV2Scene.qml`, substituída por ambiência oceânica difusa e radial não alinhada à dock.
- Substituição dos retângulos por uma sombra analítica contínua baseada em SDF.
- A Dock agora renderiza como **uma única peça contínua de vidro óptico** (555x125 px, raio 47 px), contendo os 5 lançadores em seu interior, respeitando plenamente o princípio **Lucid**.

---

### 2. Causa Raiz da Sombra e Substituição por SDF L2 Contínua

A sombra anterior era implementada através de retângulos QML aninhados com raios aumentados arbitrariamente (`radius: root.pillRadius + 10`), gerando descontinuidades de curvatura nos cantos e bandas escuras sob o chassi.

**Nova Arquitetura de Sombra (`ValoShadowItemV2` + `valo_shadow_v2.frag`):**
Foi criado o item de cena de alta performance `ValoShadowItemV2` e o shader analítico `valo_shadow_v2.frag`. A atenuação espacial é governada pela distância com sinal euclidiana (SDF L2):

\[
\alpha_{\text{shadow}} = A \cdot F(d_{\text{shadow}}, \sigma) \cdot O(d_{\text{glass}})
\]

Onde:
- \(A = 0.35\) (opacidade nominal da sombra Figma).
- \(d_{\text{shadow}}\): distância assinada à cápsula deslocada por \(Y = 12\text{ px}\).
- \(\sigma = 20.0\text{ px}\): escala de suavização contínua (smoothstep cúbico Hermite):
  \[
  F(d_{\text{shadow}}, \sigma) = 1.0 - \text{smoothstep}(-0.5, \sigma, d_{\text{shadow}})
  \]
- \(O(d_{\text{glass}})\): fator de oclusão sob a cápsula principal de vidro:
  \[
  O(d_{\text{glass}}) = \text{smoothstep}(-2.0, 1.0, d_{\text{glass}})
  \]
  Quando o fragmento está no interior do vidro (\(d_{\text{glass}} < -2.0\)), \(O = 0.0\), impedindo que a sombra escureça internamente a refração e o preenchimento oceânico. No exterior, \(O = 1.0\).
- Na visualização isolada do Passe G, \(O\) é configurado para \(1.0\) via flag de diagnóstico, permitindo inspecionar a sombra integral sem corte.
- A geometria do item cobre uma área expandida (\(\text{padding} = 40\text{ px}\)), eliminando qualquer recorte em quinas.
- Não há impacto na área clicável dos lançadores (`ItemHasContents` ativo, sem interceptação de eventos de ponteiro).

---

### 3. Correções de Composição e Conexão de Backdrop

Em `ValoDockV2Scene.qml`, a propriedade `backdropSource` da Dock apontava diretamente para um `Item { id: backdropContainer }`. No Qt Quick, um `Item` padrão possui `isTextureProvider() == false`, forçando o fallback para textura sintética 256x256.

**Correção:**
- Introdução de um `ShaderEffectSource` intermediário:
  ```qml
  ShaderEffectSource {
      id: backdropEffectSource
      anchors.fill: parent
      sourceItem: backdropContainer
      live: true
      hideSource: false
      recursive: false
  }
  ```
- O `ShaderEffectSource` implementa `QSGTextureProvider`, disponibilizando a textura real da cena renderizada ao `ValoBackdropProviderV2`.
- O chassi óptico consome a textura real do fundo, refletindo e refratando os diferentes papéis de parede dinâmicos (Oceânico, Grade, Claro, Escuro e Prisma).

---

### 4. Correções no Shader GLSL (`valo_optical_v2.frag`)

#### Correção do `smoothstep` com bordas invertidas
No código anterior, a máscara interna para cáusticas utilizava:
```glsl
float interiorMask = smoothstep(-2.0, -18.0, dist); // INVÁLIDO: edge0 (-2) > edge1 (-18)
```
Em conformidade com a especificação GLSL, `edge0` deve ser estritamente menor que `edge1`. A expressão foi corrigida para:
```glsl
float interiorMask = 1.0 - smoothstep(-18.0, -2.0, dist);
```
- Para distâncias profundas (\(dist \le -18.0\)): `interiorMask = 1.0` (cáusticas plenas no interior).
- Para a borda e exterior (\(dist \ge -2.0\)): `interiorMask = 0.0` (cáusticas suprimidas na borda para não competir com a linha especular zenital de 1,25 px).
- Transição monótona suave Hermite no intervalo \([-18, -2]\).
- A correção foi validada pelo teste unitário `testSmoothstepBorderOrderRegression`.

---

### 5. Estratégia de Alpha Pré-Multiplicado

Identificou-se que a adição de reflexos especulares e cáusticas (`glassBody += causticLight + spectralReflection`) podia levar os componentes RGB a valores superiores a \(1.0\). Ao multiplicar por `compositeAlpha`, os canais RGB ultrapassavam o canal Alpha, violando o contrato de alpha pré-multiplicado exigido pelos compositores Wayland e pelo Scene Graph do Qt Quick.

**Correção:**
```glsl
vec3 compositeRgb = clamp(mix(glassBody, strokeColor, strokeAlpha), 0.0, 1.0);
float fillAlpha = max(u_SurfaceOpacity, 0.15);
float compositeAlpha = clamp(fillAlpha + strokeAlpha * (1.0 - fillAlpha), 0.0, 1.0) * coverage;

fragColor = vec4(compositeRgb * compositeAlpha, compositeAlpha);
```
Todos os shaders diagnósticos e de produção foram auditados. O teste unitário `testPremultipliedAlphaContractAcrossDiagnosticPasses` agora inspeciona os bytes brutos (`QRgb` via `constScanLine`) e comprova que \(R \le A\), \(G \le A\) e \(B \le A\) em 100% dos pixels renderizados.

---

### 6. Novo Contrato de Diagnóstico e Desacoplamento de `shadowOffsetY`

Na versão anterior, o modo de diagnóstico era codificado no offset físico da sombra:
```cpp
props.shadowOffsetY = -static_cast<float>(m_debugMode); // PRÁTICA INADEQUADA
```
Isso corrompia a física da sombra durante a depuração e sobrecarregava o shader com múltiplos ramos `if-else`.

**Solução Arquitetural:**
1. `props.shadowOffsetY` permanece **estritamente físico** (12.0f fixo), e a estrutura UBO std140 de 160 bytes permanece intacta (zero alteração de ABI).
2. Foram criados shaders GLSL dedicados e compilados via Qt ShaderTools para cada passe de diagnóstico:
   - `valo_diag_sdf.frag.qsb` (Passe A — Campo SDF e Normais Analíticas)
   - `valo_diag_mask.frag.qsb` (Passe B — Máscara de Cobertura Analítica L2)
   - `valo_diag_volume.frag.qsb` (Passe C — Preenchimento Volumétrico Oceânico)
   - `valo_diag_refraction.frag.qsb` (Passe D — Vetores de Refração e Menisco)
   - `valo_diag_blur.frag.qsb` (Passe E — Blur Dual-Kawase GPU)
   - `valo_diag_specular.frag.qsb` (Passe F — Contorno Especular Isolado)
   - `valo_shadow_v2.frag.qsb` (Passe G — Sombra Analítica Contínua)
   - `valo_optical_v2.frag.qsb` (Passe H — Composição Final Completa de Produção)
3. `ValoGlassMaterialV2` seleciona o bytecode apropriado via `QSGMaterialShader` e retorna tipos `QSGMaterialType` distintos por modo, eliminando qualquer branching em tempo de execução no shader de produção.
4. Remoção de samplers não utilizados nos shaders de diagnóstico analíticos, eliminando overhead de binding de texturas e ponteiros nulos no pipeline RHI.

---

### 7. Comprovação de Blur GPU na Dock

Na Etapa 04, o gerador de artefatos invocava `ValoBlurPipelineV2::filterOffline` (algoritmo em CPU) e simulava o Passe E.

**Implementação e comprovação real:**
1. No item `ValoGlassItemV2.cpp`, o backdrop é obtido exclusivamente através de `m_backdropProvider->blurredTexture(window())`.
2. O método `blurredTexture` vincula o `QRhi` da janela gráfica ao `ValoGpuBlurPipelineV2`.
3. Quando a cena é renderizada no Scene Graph, o provider detecta se um frame RHI está em gravação (`rhi->isRecordingFrame()`) e obtém o `QRhiCommandBuffer` ativo via `CommandListResource`, executando os passes de downsampling e upsampling diretamente na GPU Radeon sem readbacks na CPU.
4. A textura resultante (`QRhiTexture`) é vinculada ao `ValoGlassMaterialV2` e amostrada no fragment shader em coordenadas de tela normalizadas.
5. O método de gravação CPU `filterOffline` foi **completamente removido** do gerador de artefatos `tools/generate_dock_v2_artifacts.cpp`. Todas as capturas agora provêm do framebuffer real via `window.grabWindow()`.

---

### 8. Comparação dos Oito Passes Diagnósticos

| Passe | Descrição | Shader Executado | Backend | Contrato Alpha |
| :---: | :--- | :--- | :---: | :---: |
| **A** | Geometria SDF e Normais Analíticas | `valo_diag_sdf.frag.qsb` | QRhi / OpenGL | Pré-multiplicado (\(col \cdot cov\)) |
| **B** | Máscara Analítica (L2 Capsule Coverage) | `valo_diag_mask.frag.qsb` | QRhi / OpenGL | Pré-multiplicado (\(cov \cdot cov\)) |
| **C** | Preenchimento Volumétrico Oceânico | `valo_diag_volume.frag.qsb` | QRhi / OpenGL | Pré-multiplicado (\(vol \cdot cov\)) |
| **D** | Vetores de Refração e Menisco Óptico | `valo_diag_refraction.frag.qsb` | QRhi / OpenGL | Pré-multiplicado (\(col \cdot cov\)) |
| **E** | Blur Dual-Kawase GPU (8 Passes Quality) | `valo_diag_blur.frag.qsb` | QRhi / OpenGL | Amostragem de textura GPU real |
| **F** | Contorno Especular Isolado de 1,25 px | `valo_diag_specular.frag.qsb` | QRhi / OpenGL | Pré-multiplicado (\(stroke \cdot \alpha\)) |
| **G** | Sombra Externa Analítica Contínua | `valo_shadow_v2.frag.qsb` | QRhi / OpenGL | Atenuação \(F(d, \sigma)\) com oclusão |
| **H** | Composição Final Completa de Produção | `valo_optical_v2.frag.qsb` | QRhi / OpenGL | Síntese óptica pré-multiplicada |

---

### 9. Validação dos Sete Estados Interativos e Animação Organic

Os sete estados canônicos foram validados no protótipo isolado e testados no Scene Graph:
1. **State 1 (Idle):** Repouso com escala 1.0, opacidade óptica 0.34, iluminação especular zenital.
2. **State 2 (Hover):** Expansão elástica do lançador Terminal para escala 1.08 através de física SpringAnimation, sem alterar o retângulo de hit-testing (invariante).
3. **State 3 (Press):** Compressão tátil do lançador Terminal para escala 0.94.
4. **State 4 (Active):** Lançador de Arquivos com indicador luminoso ciano ativo (`#2EE8F0`).
5. **State 5 (Background Running):** Lançadores de Áudio e IDE com indicadores em segundo plano.
6. **State 6 (Focused):** Anel de foco ciano elétrico (`#37DDFA`) visível e contrastante ao redor do lançador de Terminal.
7. **State 7 (Reduced Motion / High Contrast):** Desativação de oscilações elásticas, supressão de cáusticas procedurais animadas e elevação da opacidade base para 0.82 (WCAG AA).

---

### 10. Correção do Anel de Foco (Focused State)

Na auditoria anterior, `dock_v2_state_1_idle.png` e `dock_v2_state_6_focused.png` eram arquivos binariamente idênticos devido à condição de visibilidade:
```qml
visible: root.activeFocus && root.showKeyboardFocus
```
Em execuções de teste ou navegação onde o foco do teclado era simulado via estado sem evento do SO, `activeFocus` permanecia falso, ocultando o anel.

**Correção:**
- Em `ValoDockItemV2.qml`:
  ```qml
  visible: root.activeFocus || root.showKeyboardFocus
  opacity: (root.activeFocus || root.showKeyboardFocus) ? 1.0 : 0.0
  ```
- Em `ValoDockV2.qml`:
  ```qml
  isFocused: (root.testState === 6 && index === 2) || activeFocus
  showKeyboardFocus: (root.testState === 6 && index === 2) || activeFocus
  ```
- **Comprovação Numérica:**
  A comparação direta entre as capturas geradas `dock_v2_state_1_idle.png` e `dock_v2_state_6_focused.png` confirma:
  - **Pixels distintos:** 2.492 pixels.
  - **Delta máximo de canal:** 142 (no canal ciano do anel `#37DDFA`).
  - O teste unitário `testVisualFocusedStateMeasurablyDistinctFromIdle` garante a integridade desse comportamento em regressões futuras.

---

### 11. Comparação Visual com o Conceito Figma

**Fonte:** `https://www.figma.com/design/kax6aiYKgM2Fk7dllyZRRF/SYSTALIA-%E2%80%94-VALO-%7C-Desktop-Concept?node-id=2-2`

- **Silhueta e Proporções:** A cápsula possui exatamente 555 × 125 px com raio L2 de 47 px, correspondendo milimetricamente ao layout do Figma.
- **Peça Única de Vidro:** A eliminação da forma espúria no wallpaper e a remoção dos retângulos de sombra restauraram a percepção de uma peça contínua de vidro translúcido, eliminando a falsa sensação de duas cápsulas.
- **Materialidade e Reflexos:** Reflexos espectrais a -31° e borda zenital de 1,25 px proporcionam a tridimensionalidade especificada, enquanto o preenchimento volumétrico oceânico (`#062741` a 34% de opacidade) confere peso e materialidade.
- **Lançadores Vetoriais:** Os cinco ícones SVG oficiais (Arquivos, Áudio, Terminal, IDE e Systalia Core) estão perfeitamente centralizados a \(Y=23.5\text{ px}\) com espaçamento horizontal rigoroso de 94/95 px entre centros.

---

### 12. Arquivos Alterados e Criados

#### Arquivos Criados:
- `shaders/v2/valo_shadow_v2.vert`
- `shaders/v2/valo_shadow_v2.frag`
- `shaders/v2/valo_diag_sdf.frag`
- `shaders/v2/valo_diag_mask.frag`
- `shaders/v2/valo_diag_volume.frag`
- `shaders/v2/valo_diag_refraction.frag`
- `shaders/v2/valo_diag_blur.frag`
- `shaders/v2/valo_diag_specular.frag`
- `src/gfx/v2/ValoShadowItemV2.hpp`
- `src/gfx/v2/ValoShadowItemV2.cpp`
- `docs/graphics-v2/ETAPA_04_1_CORRECTION_REPORT.md`

#### Arquivos Modificados:
- `CMakeLists.txt` (registro dos novos shaders no `sge_shaders_v2_resource` e inclusão do `ValoShadowItemV2` na biblioteca `sge_core`)
- `shaders/v2/valo_optical_v2.frag` (correção do `smoothstep` invertido, clamp de alpha pré-multiplicado e purificação sem branches de diagnóstico)
- `src/gfx/v2/ValoGlassMaterialV2.hpp` e `.cpp` (suporte a variantes de shaders diagnósticos, proteção contra ponteiro de textura nulo)
- `src/gfx/v2/ValoGlassItemV2.cpp` (desacoplamento de `shadowOffsetY`, conexão com `blurredTexture`)
- `src/gfx/v2/ValoBackdropProviderV2.cpp` (suporte a execução GPU Dual-Kawase dentro de frame de renderização ativo via `CommandListResource`)
- `src/qml/graphics/v2/ValoDockV2.qml` (integração do `ValoShadowItemV2`, mapeamento diagnóstico refinado)
- `src/qml/graphics/v2/ValoDockItemV2.qml` (correção da visibilidade do anel de foco teclado)
- `src/qml/graphics/v2/ValoDockV2Scene.qml` (remoção da cápsula espúria no wallpaper, vinculação de `ShaderEffectSource`)
- `tests/unit/tst_ValoDockV2.cpp` (adição de testes visuais com frame grab, regressão de smoothstep, sombra contínua e contrato de alpha)
- `tools/generate_dock_v2_artifacts.cpp` (remoção total de `filterOffline` e renderização analítica em CPU; captura puramente GPU)
- Todos os 20 arquivos PNG em `artifacts/` e na pasta de artefatos do assistente.

---

### 13. Testes CTest e Regressão Global

Executado: `ctest --test-dir build --output-on-failure`

- **Total de testes:** 45
- **Aprovados:** 45 (100%)
- **Falhos:** 0
- **Ignorados:** 0
- **Duração total:** 22,35 s
- **Dispositivo GPU ativo:** AMD Radeon Graphics (radeonsi, renoir, ACO, DRM 3.64, Mesa 26.2.5)
- **Backend QRhi:** OpenGL (Compatibility Profile 4.6)
- **Zero regressões:** Suítes de compositor Wayland, sockets, protocolos, XWayland, decorações e modelos de janelas preservados integralmente.

---

### 14. Limitações Restantes

1. O compositor de produção ainda utiliza o chassi legado da Dock V1 na sessão principal do desktop; a Dock V2 permanece em modo de protótipo isolado de alta fidelidade e validação óptica.
2. Em ambientes headless sem servidor de janelas físico, a captura do framebuffer depende do offscreen EGL/OpenGL provido pelo driver Mesa DRM/RenderNode.

---

### 15. Critérios para Avançar à Etapa 05

Todos os critérios de encerramento da Etapa 04.1 foram cumpridos:
- [x] Dock visualmente representada como uma peça contínua de vidro óptico.
- [x] Sombra externa suave contínua sem retângulos sobrepostos.
- [x] Inversão do `smoothstep` eliminada.
- [x] Contrato de alpha pré-multiplicado estritamente respeitado.
- [x] UBO de 160 bytes intacto e desacoplado de propriedades de depuração.
- [x] Blur GPU Dual-Kawase real conectado e consumido na Dock.
- [x] Estado Focused visual e quantitativamente distinto de Idle.
- [x] 20 artefatos oficiais regenerados via captura direta da GPU.
- [x] 45/45 testes CTest aprovados.

**Recomendação:** A base gráfica da Dock VALO V2 está homologada e pronta para a **Etapa 05 — Integração Completa da Dock e Decorações V2 no Compositor Wayland**.

