# SYSTALIA GLASS ENGINE (SGE) & COMPOSITOR WAYLAND — MASTER ENGINEERING BLUEPRINT

**Agente Executor Alvo:** GPT SOL 6.1  
**Arquitetura:** C++23 | Qt 6.7+ (QRhi Vulkan Nativo, QtQuick SceneGraph, QtWaylandCompositor) | GLSL 450 (SPIR-V) | CMake + Ninja + CTest  
**Diretriz Operacional:** Desenvolvimento estritamente atômico, determinístico, livre de avisos de compilação (`-Wall -Wextra -Werror`) e validado passo a passo por testes automatizados no `ctest`.

---

## 1. VISÃO GERAL E CONTEXTO ARQUITETURAL

O **Systalia Glass Engine (SGE)** não é um tema ou plugin para o KWin, mas um **Compositor Wayland Independente** e Motor Óptico proprietário para o **Systalia Desktop Environment (SDE)**.

Ele substitui a abordagem tradicional de acrílico fosco/borrado (*Gaussian blur* com *blur bleed* nas quinas) pela linguagem visual **VALO Oceanic Glass**:
1. **Superfície como Tensão Superficial de Água Pura:** As janelas comportam-se como corpos de água cristalina contidos por tensão superficial (estética *Frutiger Aero / Oceanic Crystal*), com baixíssima oclusão de pigmento escuro, cáusticas subaquáticas animadas e decomposição espectral prismática.
2. **Geometria Analítica Contínua $G^2$ (Squircle de 4ª Ordem):** Os cantos das janelas não usam arcos de círculo ($C^1$), mas superelipses de ordem 4:
   $$d_{\text{corner}}(\mathbf{q}) = \left(q_x^4 + q_y^4\right)^{\frac{1}{4}} - r$$
3. **Anti-Aliasing Analítico Subpixel & Descarte Estrito:** Calculado via derivada parcial espacial $\text{fw} = \max(\text{fwidth}(d), 0.75)$. Fragmentos com $\alpha_{\text{mask}} \le 0.001$ sofrem `discard` imediato na GPU, garantindo zero vazamento de pixels nas quinas.
4. **Física de Refração de Menisco & Dispersão de Cauchy:** A borda de $28.0\text{ px}$ age como uma lente cilíndrica convexa orientada pela normal externa ($\mathbf{+norm}$), defletindo os raios do backdrop em *Screen-Space* (`gl_FragCoord.xy / u_ScreenResolution`) e separando os canais $R, G, B$.

---

## 2. INVENTÁRIO DO QUE JÁ ESTÁ CONCLUÍDO E VALIDADO NO REPOSITÓRIO

O agente **NÃO DEVE** recriar ou quebrar a fundação existente. Todos os itens abaixo já estão implementados e passando 100% no `ctest`:

* **Fase 1 (Motor Óptico Vulkan & UI Canônica) — `[CONCLUÍDA]`:**
  * `src/gfx/SystaliaSurfaceProps.hpp`: Bloco `std140` alinhado em exatos **128 bytes** (`static_assert(sizeof(SystaliaSurfaceProps) == 128)`), contendo:
    * `matrix` (offset 0, 64B)
    * `radii` (offset 64, 16B — TL, TR, BL, BR)
    * `windowSize` (offset 80, 8B)
    * `screenResolution` (offset 88, 8B)
    * `slant` (offset 96, 4B)
    * `ior` (offset 100, 4B)
    * `chromaticDispersion` (offset 104, 4B)
    * `causticIntensity` (offset 108, 4B)
    * `opacity` (offset 112, 4B)
    * `time` (offset 116, 4B — `u_Time` para cáusticas dinâmicas)
    * `padding[2]` (offset 120, 8B)
  * `src/gfx/SystaliaGlassMaterial.hpp/.cpp` & `SystaliaGlassItem.hpp/.cpp`: Nó customizado `QSGGeometryNode` + `QSGMaterialShader` com compilação offline `.qsb` SPIR-V e captura de fundo via `QSGTextureProvider` (`ShaderEffectSource`).
  * `shaders/valo_optical.vert` & `shaders/valo_optical.frag`: Shaders GLSL 450 operacionais com cáusticas aquáticas e menisco Snelliano.
* **Etapa 2.1 (Servidor Wayland & Socket IPC) — `[CONCLUÍDA]`:**
  * `src/compositor/SystaliaCompositor.hpp/.cpp`: Herda de `QWaylandCompositor`, inicializa o socket Unix `wayland-systalia-0` em `$XDG_RUNTIME_DIR` e monitora conexões de clientes.
* **Etapa 2.2A & 2.2B (XdgShell & Server-Side Decorations) — `[CONCLUÍDA]`:**
  * Integração de `QWaylandXdgShell` e `QWaylandXdgDecorationManagerV1` forçando `QWaylandXdgToplevel::ServerSideDecoration` (SSD) para impedir barras de título CSD nos clientes e envelopá-los no `WaylandWindowContainer.qml`.

---

## 3. REGRAS DE OURO PARA O GPT SOL 6.1

1. **Proibição de Passos Gigantescos Não Testados:** Execute **uma única sub-etapa por vez**. Ao concluir os arquivos da sub-etapa, rode `cmake --build build` e `ctest --test-dir build --output-on-failure`. Só avance se 100% dos testes passarem.
2. **Integridade do Bloco `std140`:** Nunca altere `SystaliaSurfaceProps.hpp` sem manter os `static_assert` de tamanho e offsets exatos em relação aos blocos uniformes dos shaders `.vert` e `.frag`.
3. **Limpeza de Processos:** Antes de testar visualmente o binário, garanta que nenhuma instância fantasma anterior esteja segurando o socket `wayland-systalia-0` (`killall -9 systalia_glass_engine 2>/dev/null || true`).
4. **C++23 Moderno & Zero Warnings:** Use `std::optional`, `constexpr`, ponteiros inteligentes da árvore `QObject` e inicialização uniforme.

---

## 4. ROTEIRO DE EXECUÇÃO PASSO A PASSO (A PARTIR DO ESTADO ATUAL)

---

### ETAPA 2.2C: Redimensionamento Interativo de Bordas e Cantos via `xdg_toplevel`

#### Objetivo
Permitir que o usuário redimensione qualquer janela de cliente Wayland externo (ex.: `alacritty`, `foot`, `kate`) arrastando as 4 bordas ou os 4 cantos do `WaylandWindowContainer.qml`, realizando o handshake assíncrono `sendConfigure` com o cliente e recalculando o chassi óptico SDF G2 em tempo real a 60+ FPS.

#### Especificação Técnica
1. **Em `src/compositor/SystaliaCompositor.hpp` e `.cpp`:**
   - Adicionar método utilitário invocável por QML para despachar configurações de tamanho com segurança:
     ```cpp
     Q_INVOKABLE void requestToplevelResize(QWaylandXdgToplevel *toplevel, const QSize &newSize, bool isResizing);
     ```
   - Na implementação:
     - Validar se `toplevel != nullptr`.
     - Restringir `newSize` a limites mínimos seguros (`width >= 240`, `height >= 160`).
     - Se `isResizing == true`, invocar:
       ```cpp
       toplevel->sendConfigure(clampedSize, QList<QWaylandXdgToplevel::State>{QWaylandXdgToplevel::ResizingState, QWaylandXdgToplevel::ActivatedState});
       ```
     - Se `isResizing == false` (final do arraste), invocar:
       ```cpp
       toplevel->sendConfigure(clampedSize, QList<QWaylandXdgToplevel::State>{QWaylandXdgToplevel::ActivatedState});
       ```
2. **Em `src/qml/WaylandWindowContainer.qml`:**
   - Definir propriedades `minWindowWidth: 320` e `minWindowHeight: 220`.
   - Criar um anel de 8 zonas sensíveis ao redor do perímetro do chassi (`borderMargin: 8`):
     - `Top`, `Bottom` (`Qt.SizeVerCursor`)
     - `Left`, `Right` (`Qt.SizeHorCursor`)
     - `TopLeft`, `BottomRight` (`Qt.SizeFDiagCursor`)
     - `TopRight`, `BottomLeft` (`Qt.SizeBDiagCursor`)
   - Durante o `onPositionChanged` (quando `pressed`):
     - Calcular $\Delta x$ e $\Delta y$ no sistema de coordenadas do pai.
     - Atualizar `x`, `y` (para bordas esquerda/superior) e solicitar ao compositor `waylandServer.requestToplevelResize(toplevel, Qt.size(newClientW, newClientH), true)`.
     - Sincronizar o tamanho do `SystaliaGlassItem` com o tamanho da superfície confirmada pelo cliente (`xdgSurface.windowGeometry` ou tamanho do buffer + margens da moldura) para evitar descolamento entre vidro e aplicação.
3. **Em `tests/tst_XdgResize.cpp` & `tests/CMakeLists.txt`:**
   - Criar teste unitário que instancia `SystaliaCompositor`, valida o clamping de dimensões mínimas (`requestToplevelResize` com valores negativos ou menores que o mínimo não deve causar crash) e verifica a integridade dos sinais.

#### Critério de Aceitação da Etapa 2.2C
- `ctest` 100% aprovado.
- Arrastar bordas e cantos de um terminal `alacritty` altera o número de linhas/colunas do terminal suavemente enquanto a lente líquida acompanha a borda sem serrilhamento.

---

### ETAPA 2.3: Extensão de Protocolo Proprietário (`z_systalia_surface_v1`)

#### Objetivo
Criar o protocolo Wayland nativo do Systalia (`z_systalia_surface_v1.xml`), compilá-lo via `qt6_generate_wayland_protocol_server_sources` e permitir que aplicações nativas do ecossistema SDE informem ao compositor seus raios individuais $(r_{\text{tl}}, r_{\text{tr}}, r_{\text{bl}}, r_{\text{br}})$, inclinação (`slant`), índice de refração (`ior`) e intensidade de cáustica diretamente via Wayland IPC.

#### Especificação Técnica
1. **Arquivo de Protocolo `protocols/z_systalia_surface_v1.xml`:**
   ```xml
   <?xml version="1.0" encoding="UTF-8"?>
   <protocol name="z_systalia_surface_v1">
     <copyright>Copyright (c) 2026 Systalia Project.</copyright>

     <interface name="z_systalia_shell_v1" version="1">
       <description summary="Controlador de superfícies ópticas VALO Glass">
         Permite que clientes Wayland configurem parâmetros físicos e geométricos G2 no SGE.
       </description>

       <request name="destroy" type="destructor"/>

       <request name="get_systalia_surface">
         <arg name="id" type="new_id" interface="z_systalia_surface_v1"/>
         <arg name="surface" type="object" interface="wl_surface"/>
       </request>
     </interface>

     <interface name="z_systalia_surface_v1" version="1">
       <request name="destroy" type="destructor"/>

       <request name="set_corner_radii">
         <description summary="Define os 4 raios da superelipse G2 em formato ponto fixo wl_fixed"/>
         <arg name="tl" type="fixed"/>
         <arg name="tr" type="fixed"/>
         <arg name="bl" type="fixed"/>
         <arg name="br" type="fixed"/>
       </request>

       <request name="set_optical_params">
         <description summary="Define parâmetros físicos do menisco líquido"/>
         <arg name="ior" type="fixed"/>
         <arg name="dispersion" type="fixed"/>
         <arg name="caustic_intensity" type="fixed"/>
         <arg name="opacity" type="fixed"/>
       </request>

       <request name="set_slant">
         <arg name="slant" type="fixed"/>
       </request>
     </interface>
   </protocol>
   ```
2. **Integração no `CMakeLists.txt`:**
   - Utilizar `qt6_generate_wayland_protocol_server_sources(sge_core FILES ${CMAKE_CURRENT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml)`.
3. **Implementação C++ (`src/compositor/SystaliaShellExtension.hpp` e `.cpp`):**
   - Herdar de `QWaylandCompositorExtensionTemplate<SystaliaShellExtension>` e `QtWaylandServer::z_systalia_shell_v1`.
   - Criar a classe `SystaliaSurfaceResource` herdando de `QObject` e `QtWaylandServer::z_systalia_surface_v1`.
   - Converter os argumentos `wl_fixed_t` via `wl_fixed_to_double()` e emitir sinais `radiiChanged(QVector4D)`, `opticalParamsChanged(...)`, `slantChanged(float)` vinculados à `QWaylandSurface` alvo.
4. **Teste Automatizado (`tests/tst_SystaliaProtocol.cpp`):**
   - Instanciar a extensão junto ao `SystaliaCompositor`.
   - Simular chamadas de atualização e verificar se os metadados de `radii`, `ior` e `slant` refletem com precisão nos objetos de superfície do servidor.

#### Critério de Aceitação da Etapa 2.3
- Geração automática dos cabeçalhos `qwayland-server-z-systalia-surface-v1.h` sem erros.
- `ctest` passando 100% com o novo teste `tst_systalia_protocol`.

---

### ETAPA 2.4: Composição Unificada Zero-Copy e Recorte Analítico SDF G2 no Buffer do Cliente

#### Objetivo
Atualmente, um cliente Wayland externo renderizado dentro de um `WaylandQuickItem` padrão possui um buffer retangular com quinas de $90^\circ$. Se o cliente pintar um fundo sólido até as extremidades, as quinas retas do buffer do cliente vazarão para fora dos cantos arredondados G2 da janela se não forem mascaradas pelo próprio shader óptico.  
Nesta etapa, o buffer de textura da superfície Wayland do cliente (`QSGTexture` proveniente do `QWaylandBufferRef` / DMA-BUF / SHM) será injetado diretamente como **Binding 2 (`u_ClientTexture`)** no `valo_optical.frag`, sofrendo o recorte analítico subpixel SDF G2 na mesma passada de GPU que o chassi de vidro!

#### Especificação Técnica
1. **Expansão do `SystaliaSurfaceProps.hpp` (Mantendo Alinhamento std140):**
   - Utilizar os dois floats finais de `padding[2]` (offsets 120 e 124) para:
     - `float hasClientTexture = 0.0f;` (offset 120, 4B — `1.0f` quando uma superfície cliente está vinculada)
     - `float titleBarHeight = 36.0f;` (offset 124, 4B — altura reservada para a barra de título SSD antes do início do buffer do cliente)
   - O tamanho total do bloco **permanece cravado em 128 bytes** (`sizeof == 128`).
2. **Expansão do `SystaliaGlassMaterial` e `SystaliaGlassItem`:**
   - Adicionar propriedade `Q_PROPERTY(QWaylandSurface* clientSurface READ clientSurface WRITE setClientSurface NOTIFY clientSurfaceChanged)`.
   - Ou permitir vincular um `QQuickItem* clientTextureProvider` (como o `WaylandQuickItem` configurado como `layer.enabled: true` ou provedor direto de textura).
   - No `SystaliaGlassShader::updateSampledImage`:
     - `binding == 1`: continua sendo `u_BackdropTexture` (fundo em Screen-Space refratado).
     - `binding == 2`: recebe `u_ClientTexture` (textura da superfície Wayland do cliente externo; se nula, usa a textura fallback 4×4 transparente).
3. **Fusão Óptica no `shaders/valo_optical.frag`:**
   - Declarar `layout(binding = 2) uniform sampler2D u_ClientTexture;`.
   - Após calcular a cor do vidro líquido (`finalColor` com refração Snelliana, dispersão de Cauchy e cáusticas):
     - Se `u_HasClientTexture > 0.5`:
       - Mapear as coordenadas locais da janela descontando a margem do menisco (`8.0px`) e a altura da barra de título (`u_TitleBarHeight`).
       - Calcular um SDF interno G2 para a área útil do cliente, garantindo que os cantos inferiores do aplicativo externo acompanhem concentricamente os raios $r_{\text{bl}}$ e $r_{\text{br}}$ do chassi.
       - Amostrar `vec4 clientPixel = texture(u_ClientTexture, clientUV);`.
       - Fazer o blend do buffer do cliente sobre o substrato aquático mantendo o reflexo de bisel e a lente de menisco nas bordas externas.
4. **Teste Automatizado (`tests/tst_ClientClipping.cpp`):**
   - Validar os offsets `120` (`hasClientTexture`) e `124` (`titleBarHeight`).
   - Testar a vinculação dupla de samplers (`binding = 1` e `binding = 2`) sem vazamento de recursos RHI.

#### Critério de Aceitação da Etapa 2.4
- O buffer de qualquer cliente Wayland (`alacritty`, `weston-terminal`, `kate`) fica perfeitamente embutido dentro da lâmina de água líquida, com cantos arredondados G2 analíticos, zero serrilhamento e zero vazamento de quinas retas.

---

### ETAPA 3.1: Gerenciamento de Janelas Completo (Z-Order, Foco, Atalhos Globais e Animações de Mola)

#### Objetivo
Transformar o compositor em um gerenciador de janelas de produção completo com empilhamento dinâmico (*Z-order*), foco automático de teclado/ponteiro, minimização para o Dock, maximização suave e física de mola (*Spring Physics*).

#### Especificação Técnica
1. **Gerenciador de Pilha de Janelas (`src/compositor/WindowManagerModel.hpp/.cpp`):**
   - Modelo `QAbstractListModel` gerenciando a lista ordenada de janelas abertas.
   - Método `raiseWindow(int index)` que move a janela clicada para o topo da pilha Z e aciona `defaultSeat()->setKeyboardFocus(surface)`.
2. **Captura de Backdrop Multi-Janela Real (Hierarquia de Camadas):**
   - Garantir que uma janela no topo da pilha Z refrate não apenas o papel de parede oceânico, mas também as janelas que estão **atrás dela** no eixo Z.
3. **Física de Transição (`SpringAnimation`):**
   - Ao abrir uma janela: transição de escala ($0.92 \to 1.0$), opacidade ($0.0 \to 0.90$) e índice de refração ($1.05 \to 1.28$).
   - Ao maximizar (`toplevel->sendMaximized(screenSize)`): interpolação contínua dos 4 raios `u_Radii` de `(24, 24, 24, 24)` para `(0, 0, 0, 0)` a 60+ FPS.

---

### ETAPA 3.2: Shell do Systalia Desktop Environment (Top Panel & Crystal Dock em Domo)

#### Objetivo
Implementar os componentes permanentes da área de trabalho conforme o conceito canônico (`upscalemedia-transformed.jpeg`):
1. **Barra Superior (`SystaliaTopBar.qml`):**
   - Barra inteiriça no topo com chassi `SystaliaGlassItem`, exibindo logotipo `SYSTALIA | SDE-VALO`, telemetria real de CPU/RAM/FPS lida de `/proc/stat` e `/proc/meminfo` via helper C++ (`SystemMonitor`), relógio e bandeja do sistema.
2. **Dock Inferior em Domo Trapezoidal (`SystaliaCrystalDock.qml`):**
   - Uso ativo do parâmetro `u_Slant: 0.35` e `u_Radii: Qt.vector4d(36, 36, 14, 14)` do nosso motor óptico para criar a perspectiva de plataforma inclinada em domo 3D.
   - Lançadores integrados capazes de disparar processos reais via `QProcess` já com `WAYLAND_DISPLAY=wayland-systalia-0` injetado no ambiente do processo filho.

---

### ETAPA 4.1: Sessão Autônoma DRM/KMS & Sessão SDDM (`systalia-session`)

#### Objetivo
Permitir que o Systalia rode tanto em modo aninhado (para desenvolvimento dentro do KDE/GNOME) quanto diretamente sobre o hardware gráfico (Bare-Metal DRM/KMS / `eglfs` / Vulkan Display) a partir da tela de login do Linux (SDDM / GDM).

#### Especificação Técnica
1. **Script de Inicialização `scripts/start-systalia-session.sh`:**
   - Configura variáveis de ambiente para aceleração Vulkan nativa (`QSG_RHI_BACKEND=vulkan`, `QT_QPA_PLATFORM=wayland;eglfs`).
   - Inicializa `dbus-run-session` se necessário e lança o binário `systalia_glass_engine`.
2. **Arquivo Desktop de Sessão Wayland (`data/systalia-wayland.desktop`):**
   - Registra a sessão em `/usr/share/wayland-sessions/systalia.desktop` para seleção direta no SDDM.

---

## 5. PROTOCOLO DE TRABALHO PARA O AGENTE (COMO RESPONDER A CADA TURNO)

Sempre que o usuário solicitar o início de uma etapa deste documento, o **GPT SOL 6.1** deve seguir rigorosamente este formato:

1. **Auditoria Pré-Execução:** Ler os arquivos atuais do repositório tocados pela etapa para evitar regressões.
2. **Implementação Atômica:** Escrever o código completo (sem placeholders ou trechos omitidos `// ...`) dos arquivos C++, QML, GLSL e CMake daquela etapa específica.
3. **Criação do Teste Unitário:** Implementar o arquivo `tests/tst_*.cpp` correspondente e registrá-lo em `tests/CMakeLists.txt`.
4. **Verificação Automática:** Compilar com Ninja e executar `ctest --test-dir build --output-on-failure`.
5. **Relatório de Conclusão da Etapa:** Apresentar a saída real do `ctest`, listar os arquivos alterados e sugerir a mensagem de commit semântica e a tag Git correspondente antes de aguardar autorização para a próxima etapa.