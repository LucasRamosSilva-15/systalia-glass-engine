# Systalia Glass Engine (SGE)

> **Em desenvolvimento — experimental.** O SGE ainda tem bastante caminho pela frente. Há componentes em diferentes estágios de implementação e validação; o projeto não está pronto para uso como ambiente de desktop de produção. Recursos presentes no código ainda podem depender do driver gráfico, da sessão Wayland e dos serviços disponíveis no sistema.

O **Systalia Glass Engine** é o compositor e a shell gráfica em desenvolvimento para o **Systalia Desktop Environment (SDE)**. O projeto combina uma interface Qt Quick, um compositor Wayland para aplicações XDG e o material óptico VALO, renderizado com Vulkan e shaders GLSL.

O visual de referência está no arquivo [SYSTALIA — VALO | Desktop Concept](https://www.figma.com/design/kax6aiYKgM2Fk7dllyZRRF/SYSTALIA-%E2%80%94-VALO-%7C-Desktop-Concept). A implementação acompanha esse conceito, mas o arquivo do Figma contém uma composição estática e não especifica todas as janelas, estados ou interações.

## Estado atual

| Área | Situação |
|---|---|
| Shell Qt Quick | Desktop, barra superior, dock, janela Files, controles VALO/SOAR e alguns painéis estão implementados. |
| Material VALO | Shader óptico, refração, cáusticas, raios independentes por canto e controles de material estão no código. |
| Compositor Wayland | XDG Shell, decorações do lado do servidor, foco, empilhamento, minimização e redimensionamento estão implementados. |
| Aplicações externas | O SGE pode iniciar clientes Wayland; a renderização de buffers acelerados e a entrada ainda precisam de validação em ambientes e drivers representativos. |
| Serviços | Telemetria, catálogo e lançamento de aplicações, notificações D-Bus e controles de sistema têm implementação inicial. Alguns dependem de ferramentas e serviços do host. |
| Auditoria de qualidade | A build existe e há testes unitários, de integração e ciclo de vida. A homologação completa, profiling e validação de acessibilidade continuam pendentes. |
| Sessão autônoma | DRM/KMS, logind, entrada direta de dispositivos e inicialização como sessão independente ainda fazem parte do roadmap. |

O SGE atualmente roda como uma aplicação Qt Quick dentro de uma sessão gráfica Linux existente e cria seu próprio socket Wayland. Ele ainda não substitui um ambiente de desktop completo nem deve ser considerado uma sessão independente pronta.

## Tecnologias

- C++23
- Qt 6.7 ou superior: Core, Gui, Quick, Quick Controls 2, Shader Tools, Wayland Compositor e D-Bus
- Qt Quick / Scene Graph
- Vulkan e GLSL 450
- Wayland, XDG Shell e CMake
- Ninja recomendado para compilar

O executável seleciona Vulkan como backend gráfico. É necessário ter um driver Vulkan funcional, além do SDK e das ferramentas de desenvolvimento correspondentes.

## Compilar

### Dependências

Instale um compilador C++ com suporte a C++23, CMake 3.22 ou superior, Ninja, Qt 6.7+, Vulkan SDK/desenvolvimento, Wayland Scanner e as bibliotecas de desenvolvimento Wayland. Os nomes dos pacotes variam por distribuição; confirme que o Qt encontrado pelo CMake tem versão 6.7 ou superior e inclui os módulos listados acima.

Em Debian/Ubuntu, estes pacotes são um ponto de partida quando a versão do repositório fornece Qt compatível:

```bash
sudo apt install build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-shadertools-dev \
  libvulkan-dev libwayland-dev libwayland-bin wayland-protocols libxkbcommon-dev
```

Se o Qt estiver instalado fora dos caminhos padrão, informe sua localização usando `CMAKE_PREFIX_PATH`.

### Configurar e compilar

```bash
git clone https://github.com/LucasRamosSilva-15/systalia-glass-engine.git
cd systalia-glass-engine

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

Para apontar para uma instalação específica do Qt:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="/caminho/para/Qt/6.8/gcc_64"
cmake --build build -j"$(nproc)"
```

## Executar

Inicie o aplicativo dentro de uma sessão gráfica Linux com Vulkan disponível e `XDG_RUNTIME_DIR` configurado:

```bash
./build/systalia_glass_engine
```

O compositor cria o socket `wayland-systalia-0` dentro do `XDG_RUNTIME_DIR` da sessão. Aplicações iniciadas pelo dock recebem o ambiente Wayland do launcher do SGE. Para iniciar um cliente manualmente a partir do mesmo ambiente:

```bash
WAYLAND_DISPLAY=wayland-systalia-0 QT_QPA_PLATFORM=wayland alacritty
```

Substitua `alacritty` pelo cliente instalado. A compatibilidade depende do protocolo e do backend gráfico usado pela aplicação. Em particular, o suporte a buffers EGL acelerados ainda precisa ser confirmado no host de destino.

## Testes

Compile o projeto e execute os testes registrados no CTest:

```bash
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Os testes cobrem partes da geometria e do shader, alinhamento dos dados de GPU, serviços, protocolo de superfície, integração com XDG Shell e decorações do lado do servidor, além do ciclo de vida de clientes. Uma build bem-sucedida não substitui a validação visual e de clientes Wayland em uma sessão gráfica real.

## Estrutura do repositório

```text
src/
  compositor/   Compositor Wayland, protocolo óptico e modelo de janelas
  gfx/          Item de vidro, material e dados enviados aos shaders
  qml/          Shell Qt Quick, janelas, controles e componentes visuais
  system/       Telemetria, processos, catálogo e controles do sistema
shaders/        Shaders GLSL do material VALO
protocols/      Protocolo Wayland z_systalia_surface_v1
tests/          Testes unitários, de integração e ciclo de vida
```

## Próximos passos

O trabalho está organizado em [SGE_IMPLEMENTATION_PLAN.md](SGE_IMPLEMENTATION_PLAN.md), com os contratos técnicos e o estado do projeto descritos em [SGE_MASTER_BLUEPRINT.md](SGE_MASTER_BLUEPRINT.md). Em alto nível, o caminho inclui:

1. Validar buffers, foco, input e redimensionamento com clientes Wayland acelerados em drivers e sessões representativos.
2. Executar a suíte completa, revisar avisos, medir custo de composição e capturas do wallpaper, e fazer auditorias de estabilidade.
3. Comparar o visual em escalas adicionais e especificar estados ausentes, acessibilidade e navegação por teclado.
4. Desenvolver workspaces, tiling e suporte a múltiplos monitores.
5. Avançar para uma sessão independente com DRM/KMS, logind e integração de dispositivos; avaliar XWayland e integração de áudio no roadmap.

O arquivo Figma disponível mostra apenas um estado do desktop. Launcher, notificações, alternador de tarefas, estados de foco e conteúdo de aplicações externas precisam de referências ou decisões de design próprias.

## Contribuir

Relatos de bugs são mais úteis quando incluem os passos para reproduzir, a distribuição, as versões do Qt e do driver Vulkan e os logs do compositor. Antes de enviar uma mudança, compile o projeto e execute os testes relacionados. Mudanças no shader devem preservar o contrato de 128 bytes de `SystaliaSurfaceProps`, salvo quando uma alteração coordenada do material C++, shader e testes for planejada.

## Licença

O repositório ainda não declara uma licença. Consulte o responsável pelo projeto antes de redistribuir ou incorporar o código.
