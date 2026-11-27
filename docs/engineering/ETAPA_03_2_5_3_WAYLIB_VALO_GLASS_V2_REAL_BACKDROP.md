# SYSTALIA — ETAPA 03.2.5.3
## Material VALO Glass V2 Real sobre o Backend Waylib (Backdrop e Pipeline Óptico)

**Repositório:** `LucasRamosSilva-15/systalia-glass-engine`  
**Branch:** `feat/treeland-waylib-backend`  
**Ambiente:** Fedora Linux 44, Qt 6.11.2, C++23, Waylib 0.6.13 (`$HOME/.cache/sge-waylib-prefix`) e wlroots 0.20.2  
**Execução física DRM/KMS:** Não executada (exclusivamente nested/headless)

---

## 1. Backend QRhi ativo e compatibilidade dos shaders SPIR-V

Em execução nested sobre `WOutputRenderWindow` (`QGuiApplication::setAttribute(Qt::AA_UseOpenGLES)`), `QSG_INFO=1` confirmou:
- `qt.scenegraph.general: Creating QRhi with backend OpenGL for window 0x39b47110`
- Os shaders compilados em `.qsb` (`valo_optical_v2.{vert,frag}.qsb`, `valo_kawase_down.frag.qsb`, `valo_kawase_up.frag.qsb`, `valo_shadow_v2.{vert,frag}.qsb`) operaram nativamente sobre `QRhi::OpenGLES2` sem erros de compilação, vinculação de textura ou FBO.

---

## 2. Arquitetura de camadas e prevenção de Feedback Loop

Em `src/qml/waylib/WaylibNestedShell.qml`, os filhos do `WOutputRenderWindow` foram explicitamente separados em irmãs distintas:
1. **`desktopContentLayer` (`z: 10`):**
   - Contém o wallpaper oceânico VALO (`valoWallpaper` com gradientes e pontos de iluminação para validação visual de refração/blur) e o container de janelas `waylibClientLayer` (`Repeater` de `WaylibWindowFrame` + `XdgToplevelSurfaceItem`).
2. **`desktopBackdropSource` (`ShaderEffectSource`, `objectName: "waylibDesktopBackdropSource"`):**
   - Captura exclusivamente `sourceItem: desktopContentLayer`, com `live: true`, `hideSource: false`, `recursive: false`, `format: ShaderEffectSource.RGBA8` e `textureSize` sincronizado com `desktopContentLayer.{width,height} * renderWindow.devicePixelRatio`.
3. **`shellChromeLayer` (`z: 90`):**
   - Irmã fora da subárvore de `desktopContentLayer`, hospedando `SystaliaTopBarV2` (`topBar`, `blurProfile: 1`, `backdropSource: desktopBackdropSource`), `waylibWindowStrip` e `SystaliaDockV2` (`valoDockV2`, `performanceProfile: "balanced"`, `backdropSource: desktopBackdropSource`).
   - Como os consumidores vítreos não pertencem à subárvore capturada por `desktopBackdropSource`, o ciclo de dependência recursiva no Scene Graph é estruturalmente impossível.

---

## 3. Ajustes realizados

- **`src/qml/waylib/WaylibNestedShell.qml`:** Separação explícita entre `desktopContentLayer` e `shellChromeLayer`, criação do `ShaderEffectSource` (`waylibDesktopBackdropSource`) e ativação de `blurProfile: 1` (`SystaliaTopBarV2`) e `performanceProfile: "balanced"` (`SystaliaDockV2`) com `backdropSource` real e `screenResolution` escalonado por DPR.
- **`src/qml/waylib/WaylibWindowFrame.qml`:** Refinamento visual da barra de título SSD (`ssdTitleBar` com gradiente vertical oceânico VALO e borda especular superior de 1 px), mantida dentro de `desktopContentLayer` sem autorreferência ao `desktopBackdropSource` e preservando integralmente foco, arraste, redimensionamento, minimizar/maximizar e fechamento.
- **`ValoBackdropProviderV2` / shaders `.qsb`:** Não foi necessária nenhuma alteração em C++ ou shaders — o provedor e o pipeline Dual-Kawase funcionaram diretamente sobre a textura `ShaderEffectSource` no `WOutputRenderWindow`.
- **`tests/compositor/tst_WaylibBackend.cpp`:** Asserções adicionadas em `headlessAndMultiWindowValoShellLifecycle()` validando `desktopContentLayer`, `waylibDesktopBackdropSource`, `shellChromeLayer`, vinculação de `backdropSource` em `topBar` e `valoDockV2`, `blurProfile >= 1` e ausência de `topBar`/`valoDockV2` dentro de `desktopContentLayer`.

---

## 4. Evidências de textura real e Blur GPU no Smoke Test ao vivo

Execução nested no KDE Plasma (`WAYLAND_DISPLAY=wayland-1`, output `WL-1`) com `./build/waylib/systalia_waylib_nested --socket sge-nested-poc --auto-quit-ms 3500` e 2 instâncias simultâneas de `tst_waylib_test_client` (`Smoke Client A` e `Smoke Client B`):

- **Antes (Etapa 03.2.5.2, `backdropSource: null`):**
  `[SGE-BACKDROP-V2] ATENÇÃO: Recorrendo ao fallback sintético createSyntheticBackdrop (source=0x0)`
- **Depois (Etapa 03.2.5.3, `backdropSource: desktopBackdropSource`):**
  - `[SGE-BACKDROP-V2] REAL TEXTURE ATIVA: size=1280x760, rhiTex=0x0, rhi=0x3a99e0d0`
  - `[SGE-BACKDROP-V2] onBeforeRendering frame=1: srcTex=0x39d78750, rhiSrc=0x3ab20630, srcSize=1280x760`
  - `[SGE-BACKDROP-V2] Blur GPU executado com sucesso! outRhi=0x3ad8f410, format=1, size=1280x760`
  - `[SGE-BACKDROP-V2] Blur GPU executado com sucesso! outRhi=0x3ad71190, format=1, size=1280x760`
  - **Zero** ocorrências de `createSyntheticBackdrop (source=0x0)` durante toda a execução nested.
  - Códigos de saída: **`RC1=0`**, **`RC2=0`**, **`RC_COMP=0`**.

---

## 5. Build Incremental e CTest (`SGE_ENABLE_WAYLIB_BACKEND=ON`)

- **Build incremental Release (`build/waylib`, `-Werror`):** **`6.69 s`** (`real`).
- **Testes direcionados (`tst_waylib|tst_valo_shell_v2|tst_valo_glass|tst_backdrop_binding|tst_unified_composition|tst_resource_leak_audit`):** **8/8 PASS** (`5.20 s` reais).
- **Suíte completa CTest (`ctest --test-dir build/waylib --output-on-failure`):** **61/61 PASS** (`37.56 s` reais).

---

## 6. Riscos Residuais e Preparação para Etapa 03.2.5.4 (XWayland)

- No modo `HEADLESS-1` do CTest (`tst_waylib_backend`), o primeiro frame antes do swapchain pode usar o fallback sintético antes de o `ShaderEffectSource` publicar a textura, transitando normalmente sem erros ou crashes; no modo GPU nested (`WL-1`), `REAL TEXTURE ATIVA` e `Blur GPU executado com sucesso!` entram desde o frame 1.
- A base Waylib + VALO Glass V2 real + gerenciador multi-janela está estável e pronta para a **Etapa 03.2.5.4 (Integração XWayland no Waylib)**.
