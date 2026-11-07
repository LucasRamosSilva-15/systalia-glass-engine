# Auditoria Profunda do Sistema Gráfico Legado — VALO Graphics 1.0

**Documento:** `docs/graphics-v2/LEGACY_AUDIT.md`  
**Data:** 07 de Novembro de 2026  
**Status:** Etapa 00 — Auditoria Arquitetural e Diagnóstico  
**Branch:** `valo-graphics-2.0`  
**Alvo:** Subsistema Gráfico, Compositor Wayland, Shaders GLSL/SPIR-V e Componentes QML  

---

## 1. Inventário Geral e Classificação de Módulos

O repositório `systalia-glass-engine` combina um compositor Wayland de alta performance em C++23 com um motor de renderização óptico em Qt Quick / Scene Graph. Para garantir que nenhuma funcionalidade do compositor ou dos serviços de sistema seja corrompida durante a reconstrução gráfica, todos os módulos existentes foram categorizados em quatro papéis arquiteturais estritos:

- **CORE:** Núcleo inegociável. Contém o compositor, protocolos, serviços de hardware, D-Bus, gerenciamento de janelas e ciclo de vida de processos. **Não deve ser modificado.**
- **BRIDGE:** Camada de transição. Expõe buffers Wayland, superfícies XWayland e eventos para o Qt Quick. **Deve manter seus contratos de sinalização e propriedades, admitindo adaptações de acoplamento.**
- **GRAPHICS:** Subsistema visual a ser reconstruído. Inclui materiais QSG, shaders, molduras, gradientes e superfícies ópticas.
- **LEGACY:** Módulos obsoletos ou duplicados que serão substituídos pelo novo motor e descartados após validação.

### Tabela de Classificação dos Módulos

| Módulo / Arquivo | Caminho | Papel | Descrição / Responsabilidade | Ação na V2 |
| :--- | :--- | :---: | :--- | :--- |
| `SystaliaCompositor` | `src/compositor/` | **CORE** | Servidor Wayland, sockets, emissão de toplevels e popups | Preservar 100% |
| `WindowStackModel` | `src/compositor/` | **CORE** | Modelo de ordenação Z-order, foco e ativação de clientes | Preservar 100% |
| `XWaylandManager` | `src/compositor/` | **CORE** | Inicialização do servidor XWayland e gerência de janelas X11 | Preservar 100% |
| `OutputManagerService` | `src/compositor/` | **CORE** | Gerenciamento de múltiplos monitores, DRM/KMS e resoluções | Preservar 100% |
| `SystaliaSurfaceExtension` | `src/compositor/` | **CORE** | Servidor do protocolo Wayland `z_systalia_surface_v1` | Preservar 100% |
| `SystemTelemetryService` | `src/system/` | **CORE** | Telemetria Linux em tempo real (/proc/stat, /proc/meminfo, FPS) | Preservar 100% |
| `ProcessLauncherService` | `src/system/` | **CORE** | Execução e isolamento de processos (.desktop) e rastreio de PIDs | Preservar 100% |
| `AppRegistryService` | `src/system/` | **CORE** | Indexação assíncrona de arquivos `.desktop` e ícones | Preservar 100% |
| `NotificationServer` | `src/system/` | **CORE** | Servidor D-Bus `org.freedesktop.Notifications` | Preservar 100% |
| `WorkspaceManagerService` | `src/system/` | **CORE** | Lógica de 4 áreas de trabalho virtuais e migração de janelas | Preservar 100% |
| `LiquidTilingService` | `src/system/` | **CORE** | Algoritmo de tiling fluido e divisão binária de janelas | Preservar 100% |
| `InputDeviceManager` | `src/system/` | **CORE** | Configuração libinput, KMS bare-metal e multiplexação de eventos | Preservar 100% |
| `LogindSessionService` | `src/system/` | **CORE** | Sessões logind D-Bus, VT switching, suspensão e poweroff | Preservar 100% |
| `SystemControlService` | `src/system/` | **CORE** | Controle de volume de áudio e brilho de tela | Preservar 100% |
| `SystaliaSurfaceProps.hpp` | `src/gfx/` | **BRIDGE** | Struct UBO std140 de 128 bytes para uniform buffer óptico | Manter contrato V1; criar V2 isolado |
| `SystaliaClientClip.hpp` | `src/gfx/` | **BRIDGE** | Cálculo analítico da geometria e recorte de janelas clientes | Manter interface; refatorar matemática V2 |
| `WaylandWindowContainer.qml`| `src/qml/` | **BRIDGE** | Contêiner de superfícies Wayland, decorações, redimensionamento | Desacoplar efeitos visuais e manter hooks |
| `WorkspaceViewport.qml` | `src/qml/` | **BRIDGE** | Frustum culling, transições e viewport de workspaces | Preservar lógica de frustum |
| `SystaliaGlassItem` | `src/gfx/` | **GRAPHICS** | QQuickItem que submete a geometria óptica ao Scene Graph | Reconstruir em `src/gfx/v2/` |
| `SystaliaGlassMaterial` | `src/gfx/` | **GRAPHICS** | Material QSG com UBO e bindings de textura | Reconstruir com suporte a multi-pass QRhi |
| `valo_optical.frag/.vert`| `shaders/` | **GRAPHICS** | Shaders GLSL 450 (SPIR-V) de refração, IOR e cáusticas | Manter intocados; criar `shaders/v2/` |
| `ValoGlass.qml` | `src/qml/` | **GRAPHICS** | Elemento unificado de vidro VALO (óptica + gradientes + borda) | Reconstruir como casca limpa na V2 |
| `SystaliaG2Frame.qml` | `src/qml/shell/` | **GRAPHICS** | Forma vetorial baseada em ShapePath com Bézier G2 / PathArc | Evoluir para gerador analítico unificado |
| `SystaliaDock.qml` | `src/qml/shell/` | **GRAPHICS** | Cápsula flutuante da Dock e lançadores | Primeiro protótipo da V2 |
| `SystaliaTopBar.qml` | `src/qml/shell/` | **GRAPHICS** | Barra superior com acabamento VALO (layout preservado) | Reconstruir acabamento na V2 |
| `SystaliaFilesWindow.qml` | `src/qml/shell/` | **GRAPHICS** | Gerenciador de arquivos demonstrativo com painéis de vidro | Reconstruir painéis e tokens na V2 |
| `SystaliaControlCenter.qml`| `src/qml/shell/`| **GRAPHICS** | Painel de controle, sliders, toggles 3D e segmented pills | Reconstruir controles na V2 |
| `SystaliaMaterialWindow.qml`| `src/qml/shell/`| **LEGACY** | Janela de depuração de parâmetros ópticos | Substituir por overlay de diagnóstico |
| `WaylandClientView.qml` | `src/qml/` | **LEGACY** | Protótipo primitivo de renderização de clientes Wayland | Descomissionar após migração |

---

## 2. Auditoria Detalhada dos Componentes Gráficos C++ / Shaders

### 2.1 `SystaliaGlassItem.cpp/.hpp`
- **Papel:** Subclasse de `QQuickItem` responsável por integrar o shader óptico à árvore do Scene Graph do Qt Quick implementando `updatePaintNode()`.
- **Pontos Críticos:**
  1. Cria nós do tipo `QSGGeometryNode` contendo malha retangular simples de 4 vértices (`QSGGeometry::defaultAttributes_Point2D()`).
  2. Alimenta duas texturas: `m_backdropTexture` (captura do desktop via `ShaderEffectSource`) e `m_clientTexture` (textura da janela Wayland).
  3. Gerencia oclusão e supressão de atualizações com `shouldSuppressSceneGraphUpdate()`, economizando GPU quando a janela está minimizada.
- **Falhas e Limitações Observadas:**
  - O item é um retângulo plano de 4 vértices; todo o recorte geométrico é realizado no fragment shader por descarte analítico (`if (alphaMask <= 0.001) discard;`).
  - O descarte em fragment shaders impede otimizações de Early-Z em hardware Vulkan/KMS e gera custo de preenchimento desnecessário para superfícies transparentes fora da curvatura.
  - Falta suporte nativo a múltiplos passes para geração de blur de fundo; o blur precisa ser gerado externamente por outro elemento e passado pronto como textura.

### 2.2 `SystaliaGlassMaterial.cpp/.hpp`
- **Papel:** Subclasse de `QSGMaterial` e `QSGMaterialShader` que gerencia a UBO e os descritores de textura SPIR-V.
- **Pontos Críticos:**
  1. Utiliza `QSGMaterialShader::updateUniformData()` para copiar os dados da struct `SystaliaSurfaceProps` diretamente para a memória de vídeo.
  2. Mantém compatibilidade estrita com a especificação std140 de 128 bytes.
- **Falhas Observadas:**
  - Está amarrado ao layout legado de uniformes. Qualquer adição de novos parâmetros na V2 (ex.: ruído de dispersão, offsets de sombras, controle de camadas de verniz) quebraria a ABI se aplicada diretamente.
  - Não faz pooling de descritores de textura.

### 2.3 `SystaliaSurfaceProps.hpp`
- **Layout de Memória (std140, 128 bytes):**
  - `matrix` (offset 0, 64 bytes)
  - `radii` (offset 64, 16 bytes)
  - `windowSize` (offset 80, 8 bytes)
  - `screenResolution` (offset 88, 8 bytes)
  - `slant` (offset 96, 4 bytes)
  - `ior` (offset 100, 4 bytes)
  - `chromaticDispersion` (offset 104, 4 bytes)
  - `causticIntensity` (offset 108, 4 bytes)
  - `opacity` (offset 112, 4 bytes)
  - `time` (offset 116, 4 bytes)
  - `hasClientTexture` (offset 120, 4 bytes)
  - `titleBarHeight` (offset 124, 4 bytes)
- **Diagnóstico:** O contrato é sólido e auditado por `static_assert`, mas 128 bytes deixam zero espaço livre para novas propriedades gráficas avançadas. A V2 precisará de um bloco estendido isolado (`SystaliaSurfacePropsV2`).

### 2.4 Shaders (`shaders/valo_optical.frag` e `valo_optical.vert`)
- **Papel:** Computa o traçado de raios superficial, distorção refrativa baseada em gradiente normal do SDF, dispersão cromática e cáusticas subaquáticas procedurais.
- **Falhas e Sintomas Visuais Acumulados:**
  1. **Dualidade de Funções SDF:** O shader possui tanto `sdRoundedBox4G2` (superelipse de quarta ordem) quanto `sdRoundedBox2` (euclidiana circular $L_2$). A chave de seleção era `u_TitleBarHeight < 0.0`, o que misturava a semântica da barra de título com a geometria perimétrica.
  2. **Iluminação de Borda Conflitante:** O shader desenha uma borda zenital de rim light interna (`rimLine * zenith`) e um brilho perimétrico suave (`softEdgeGleam`). No entanto, no QML, o `ValoGlass.qml` desenha por cima uma borda vetorial `specularStrokeFrame` com `SystaliaG2Frame`. Se as geometrias não coincidirem perfeitamente, o usuário enxerga duas bordas paralelas com intensidades distintas!
  3. **Amostragem Refrativa sem Mipmaps:** A refração é amostrada diretamente da textura de backdrop estática. Como não há um passe de filtragem com pirâmide de blur integrada, o fundo refratado fica excessivamente nítido ou requer um `ShaderEffectSource` pesado no QML.

---

## 3. Auditoria Detalhada dos Componentes QML da Shell

### 3.1 `ValoGlass.qml`
- **Diagnóstico Estrutural:** O componente atua como um agregador de camadas em cascata:
  - Camada A ($z=0$): Sombra externa via `BorderImage` com `valo-window-shadow.png`.
  - Camada B ($z=1$): Chassi óptico via `SystaliaGlassItem`.
  - Camada C ($z=2$): Gradiente volumétrico via `SystaliaG2Frame` (`volumetricBodyFrame`).
  - Camada D ($z=3$): Reflexos prismáticos via `SystaliaG2Frame` (`spectralPrismFrame`).
  - Camada E ($z=4$): Borda especular via `SystaliaG2Frame` (`specularStrokeFrame`).
  - Camada F ($z=10$): `contentContainer` para os filhos QML.
- **Origem dos Conflitos Visuais:**
  - **Desconexão de Curvatura:** Como demonstrado matematicamente na Etapa 05.1-B.2, instâncias de `SystaliaG2Frame` usavam Bézier cúbica com $c=0.91$, desviando até 16.01 px dos cantos circulares do `SystaliaGlassItem`.
  - **Múltiplos Renderers de Forma:** O preenchimento volumétrico, o prisma espectral e o contorno especular eram itens `Shape` independentes no Scene Graph, gerando três chamadas de desenho vetorial (tesselação de curvas) por superfície, além do passe de shader.

### 3.2 `WaylandWindowContainer.qml`
- **Ciclo de Vida de Buffers Wayland:**
  - O conteúdo Wayland chega através de `ShellSurfaceItem` associado ao `xdgSurface`.
  - Para alimentar o shader óptico com a textura do cliente, é utilizado um `ShaderEffectSource` (`clientTextureCapture`) apontando para o `clientSurfaceItem`.
  - **Armadilha Crítica Descoberta:** Na Etapa 04.1, a tentativa de esconder o `clientSurfaceItem` (`hideSource: true`) para que apenas o shader renderizasse a textura do cliente causou invisibilidade total das aplicações (Alacritty e VS Code), pois a cópia para FBO pelo `ShaderEffectSource` sofria dessincronização de frames Wayland. A correção obrigatória foi manter `hideSource: false` e apresentar o `ShellSurfaceItem` diretamente dentro de `clientViewport` com recorte via clipping.
  - **Preservação Obrigatória na V2:** O novo motor não deve tentar redirecionar a textura do cliente para fora do scene graph sem garantir atomicidade de buffer release.

### 3.3 `SystaliaDock.qml` e `ValoDockItem.qml`
- **Geometria:** 555 × 125 px, raio nominal de 47 px, 5 lançadores posicionados em $x = [33.75, 128.75, 222.75, 316.75, 410.75]$.
- **Artefatos Históricos Identificados:**
  1. Foco espúrio: `ValoDockItem` inicializava com `focus: true`, desenhando anel circular permanente na pasta.
  2. Sombra invasiva: Um `Rectangle` preto com `topMargin: 12` e `radius: 47` colocado a $z=0$ criava uma linha de corte escura visível dentro do vidro translúcido.
  3. Mismatch L2/G2: Bordas duplas causadas pela falta de propagação de `circularCorners` para o `ValoGlass`.

### 3.4 `SystaliaTopBar.qml`
- **Geometria:** 1646 × 45 px, raio 22 px, centralizada horizontalmente.
- **Layout Contratual a Preservar:** Logomarca SYSTALIA à esquerda, telemetria (CPU/RAM/FPS), relógio centralizado, indicador de 4 workspaces e controles à direita.
- **Falha Visual Legada:** Uso de retângulos auxiliares desenhando bordas e sombras redundantes simultaneamente com o `SystaliaGlassItem`.

### 3.5 `SystaliaFilesWindow.qml` e `SystaliaControlCenter.qml`
- **Problema de Escala e Reutilização:** Ambos os componentes redefinem internamente gradientes, sombras e materiais de vidro, gerando inconsistência de opacidade e cores com relação aos tokens centrais de `ValoTheme.qml`.

---

## 4. Matriz de Riscos de Regressão na Migração

| Risco de Regressão | Nível | Módulo Afetado | Causa / Gatilho | Mitigação Arquitetural na V2 |
| :--- | :---: | :--- | :--- | :--- |
| **Invisibilidade de Superfícies Wayland** | **CRÍTICO** | `WaylandWindowContainer` | Redirecionamento forçado de buffers para FBOs intermediários | Apresentação direta do `ShellSurfaceItem` com stencil masking |
| **Quebra de ABI std140 (128 bytes)** | **CRÍTICO** | `SystaliaSurfaceProps.hpp` | Adição de novos atributos na struct antiga | Isolamento absoluto de `SystaliaSurfacePropsV2` |
| **Falha em Testes CTest (36 suítes)** | **ALTO** | `tests/` | Renomeação de propriedades QML públicas ou hooks C++ | Manter wrappers compatíveis com a API pública legado |
| **Degradação de FPS por Multi-Sampling** | **ALTO** | `shaders/v2/` | Loops pesados de blur no fragment shader | Algoritmo Dual-Kawase pré-filtrado em FBOs reduzidos |
| **Desalinhamento de Máscaras em HiDPI** | **MÉDIO** | `SystaliaG2Frame` / Shaders | Diferenças de interpretação de `devicePixelRatio` | Cálculo centralizado de geometria em coordenadas físicas de pixel |
| **Vazamento de VRAM em Redimensionamento** | **MÉDIO** | Materiais e FBOs | Recriação descontrolada de texturas no resize de janelas | Cache e reuso de alocações QRhi via pooling |

