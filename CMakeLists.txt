cmake_minimum_required(VERSION 3.22)
project(systalia_glass_engine LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SGE_GRAPHICS_V2 "Build VALO Graphics 2.0 foundation subsystem" ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core
    Gui
    GuiPrivate
    Quick
    QuickControls2
    ShaderTools
    WaylandCompositor
    DBus
)

find_package(Vulkan REQUIRED)
find_package(WaylandScanner REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUDEV QUIET IMPORTED_TARGET libudev)
    pkg_check_modules(LIBINPUT QUIET IMPORTED_TARGET libinput)
    pkg_check_modules(XCB QUIET IMPORTED_TARGET xcb xcb-composite xcb-xfixes)
    pkg_check_modules(WAYLAND_SERVER QUIET IMPORTED_TARGET wayland-server)
endif()

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Werror>")

# Biblioteca estática com os bytecodes SPIR-V compilados via ShaderTools
qt_add_library(sge_shaders STATIC)
qt6_add_shaders(sge_shaders "sge_shaders_resource"
    PREFIX
        "/shaders"
    BASE
        "shaders"
    FILES
        shaders/valo_optical.vert
        shaders/valo_optical.frag
)
qt6_add_shaders(sge_shaders "sge_shaders_v2_resource"
    PREFIX
        "/shaders/v2"
    BASE
        "shaders/v2"
    FILES
        shaders/v2/valo_optical_v2.vert
        shaders/v2/valo_optical_v2.frag
        shaders/v2/valo_kawase_down.vert
        shaders/v2/valo_kawase_down.frag
        shaders/v2/valo_kawase_up.vert
        shaders/v2/valo_kawase_up.frag
        shaders/v2/valo_diag_sdf.frag
        shaders/v2/valo_diag_mask.frag
        shaders/v2/valo_diag_volume.frag
        shaders/v2/valo_diag_refraction.frag
        shaders/v2/valo_diag_blur.frag
        shaders/v2/valo_diag_specular.frag
        shaders/v2/valo_shadow_v2.vert
        shaders/v2/valo_shadow_v2.frag
)
qt6_add_shaders(sge_shaders "sge_raw_shaders_resource"
    PREFIX
        "/raw_shaders"
    BASE
        "tools/raw_render_diagnostics/shaders"
    FILES
        tools/raw_render_diagnostics/shaders/raw_t2_minimal.vert
        tools/raw_render_diagnostics/shaders/raw_t2_minimal.frag
        tools/raw_render_diagnostics/shaders/raw_t3_ubo.vert
        tools/raw_render_diagnostics/shaders/raw_t3_ubo.frag
        tools/raw_render_diagnostics/shaders/raw_t4_sdf.vert
        tools/raw_render_diagnostics/shaders/raw_t4_sdf.frag
)
target_link_libraries(sge_shaders PUBLIC
    Qt6::Core
    Qt6::Gui
)

# Biblioteca estática central com o motor, itens visuais, compositor Wayland e módulo QML
qt_add_library(sge_core STATIC
    src/gfx/SystaliaSurfaceProps.hpp
    src/gfx/SystaliaClientClip.hpp
    src/gfx/SystaliaGlassMaterial.hpp
    src/gfx/SystaliaGlassMaterial.cpp
    src/gfx/SystaliaGlassItem.hpp
    src/gfx/SystaliaGlassItem.cpp
    src/compositor/SystaliaSurfaceExtension.hpp
    src/compositor/SystaliaSurfaceExtension.cpp
    src/compositor/SystaliaCompositor.hpp
    src/compositor/SystaliaCompositor.cpp
    src/compositor/WindowStackModel.hpp
    src/compositor/WindowStackModel.cpp
    src/compositor/OutputManagerService.hpp
    src/compositor/OutputManagerService.cpp
    src/compositor/XWaylandManager.hpp
    src/compositor/XWaylandManager.cpp
    src/system/SystemTelemetryService.hpp
    src/system/SystemTelemetryService.cpp
    src/system/ProcessLauncherService.hpp
    src/system/ProcessLauncherService.cpp
    src/system/AppRegistryService.hpp
    src/system/AppRegistryService.cpp
    src/system/NotificationServer.hpp
    src/system/NotificationServer.cpp
    src/system/SystemControlService.hpp
    src/system/SystemControlService.cpp
    src/system/WorkspaceManagerService.hpp
    src/system/WorkspaceManagerService.cpp
    src/system/LiquidTilingService.hpp
    src/system/LiquidTilingService.cpp
    src/system/InputDeviceManager.hpp
    src/system/InputDeviceManager.cpp
    src/system/LogindSessionService.hpp
    src/system/LogindSessionService.cpp
    src/tokens/ValoTokens.hpp
    src/gfx/v2/SystaliaSurfacePropsV2.hpp
    src/gfx/v2/ValoGeometryEngineV2.hpp
    src/gfx/v2/ValoGeometryEngineV2.cpp
    src/gfx/v2/ValoOrganicTransform.hpp
    src/gfx/v2/ValoOrganicTransform.cpp
    src/gfx/v2/ValoOpticalContext.hpp
    src/gfx/v2/ValoOpticalContext.cpp
    src/gfx/v2/ValoGlassMaterialV2.hpp
    src/gfx/v2/ValoGlassMaterialV2.cpp
    src/gfx/v2/ValoGlassItemV2.hpp
    src/gfx/v2/ValoGlassItemV2.cpp
    src/gfx/v2/ValoBackdropProviderV2.hpp
    src/gfx/v2/ValoBackdropProviderV2.cpp
    src/gfx/v2/ValoBlurPipelineV2.hpp
    src/gfx/v2/ValoBlurPipelineV2.cpp
    src/gfx/v2/ValoGpuBlurPipelineV2.hpp
    src/gfx/v2/ValoGpuBlurPipelineV2.cpp
    src/gfx/v2/ValoShadowItemV2.hpp
    src/gfx/v2/ValoShadowItemV2.cpp
)

qt6_generate_wayland_protocol_server_sources(sge_core
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml"
)

target_include_directories(sge_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/v2
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tokens
)

target_link_libraries(sge_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Quick
    Qt6::WaylandCompositor
    Qt6::DBus
    Vulkan::Vulkan
    sge_shaders
)

if(TARGET PkgConfig::LIBUDEV)
    target_compile_definitions(sge_core PUBLIC SGE_HAVE_LIBUDEV=1)
    target_link_libraries(sge_core PUBLIC PkgConfig::LIBUDEV)
endif()
if(TARGET PkgConfig::LIBINPUT)
    target_compile_definitions(sge_core PUBLIC SGE_HAVE_LIBINPUT=1)
    target_link_libraries(sge_core PUBLIC PkgConfig::LIBINPUT)
endif()
if(TARGET PkgConfig::XCB)
    target_compile_definitions(sge_core PUBLIC SGE_HAVE_XCB=1)
    target_link_libraries(sge_core PUBLIC PkgConfig::XCB)
endif()
if(TARGET PkgConfig::WAYLAND_SERVER)
    target_link_libraries(sge_core PUBLIC PkgConfig::WAYLAND_SERVER)
endif()

set_source_files_properties(src/qml/theme/SystaliaTheme.qml src/qml/theme/ValoTheme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set(sge_valo_assets
    src/qml/assets/figma/valo-systalia-symbol.svg
    src/qml/assets/figma/valo-dock-folder.svg
    src/qml/assets/figma/valo-dock-speaker.svg
    src/qml/assets/figma/valo-dock-terminal.svg
    src/qml/assets/figma/valo-dock-code.svg
    src/qml/assets/figma/valo-dock-spark.svg
    src/qml/assets/figma/valo-window-close.svg
    src/qml/assets/figma/valo-window-maximize.svg
    src/qml/assets/figma/valo-window-minimize.svg
)

foreach(asset IN LISTS sge_valo_assets)
    set_source_files_properties(${asset} PROPERTIES QT_RESOURCE_ALIAS "${asset}")
endforeach()

qt_add_qml_module(sge_core
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Systalia/Engine"
    URI Systalia.Engine
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        src/qml/main.qml
        src/qml/WorkspaceViewport.qml
        src/qml/theme/SystaliaTheme.qml
        src/qml/theme/ValoTheme.qml
        src/qml/WaylandClientView.qml
        src/qml/WaylandWindowContainer.qml
        src/qml/ValoGlass.qml
        src/qml/shell/SystaliaTopBar.qml
        src/qml/shell/SystaliaDock.qml
        src/qml/shell/ValoDockItem.qml
        src/qml/shell/SystaliaG2Frame.qml
        src/qml/shell/SystaliaWindowAction.qml
        src/qml/shell/SystaliaWindowHeader.qml
        src/qml/shell/SystaliaFilesWindow.qml
        src/qml/shell/SystaliaMaterialWindow.qml
        src/qml/shell/SystaliaFlowMenu.qml
        src/qml/shell/SystaliaNotificationHost.qml
        src/qml/shell/SystaliaControlCenter.qml
        src/qml/shell/SystaliaTaskSwitcher.qml
        src/qml/shell/SnapPreviewOverlay.qml
        src/qml/shell/TileSplitter.qml
        src/qml/graphics/v2/ValoSurfaceV2.qml
        src/qml/graphics/v2/ValoDockItemV2.qml
        src/qml/graphics/v2/ValoDockV2.qml
        src/qml/graphics/v2/ValoDockV2Scene.qml
    RESOURCES
        ${sge_valo_assets}
    NO_PLUGIN
)

# Executável principal da aplicação
qt_add_executable(systalia_glass_engine
    src/main.cpp
)

target_link_libraries(systalia_glass_engine PRIVATE
    sge_core
    Qt6::QuickControls2
    Qt6::WaylandCompositor
    Qt6::DBus
)

# Shared Qt loads QML plugins at runtime; static builds still need import scanning.
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "SHARED_LIBRARY")
    set_property(TARGET systalia_glass_engine PROPERTY QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
endif()

# Regras de instalação para sessão autónoma Wayland / SDDM
install(TARGETS systalia_glass_engine
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(PROGRAMS data/systalia-session
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

install(FILES data/systalia.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions
)

enable_testing()
add_subdirectory(tests)

# Utilitário de benchmark e extração de artefatos GPU VALO Graphics 2.0
qt_add_executable(valo_gpu_benchmark
    tools/bench_and_artifacts.cpp
)
target_link_libraries(valo_gpu_benchmark PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

# Utilitário de geração de artefatos visuais e validação da Dock VALO V2 (Etapa 04)
qt_add_executable(valo_dock_v2_artifacts
    tools/generate_dock_v2_artifacts.cpp
)
target_link_libraries(valo_dock_v2_artifacts PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

# Ferramenta de Diagnóstico de Renderização Crua (Etapa 04.1.1)
qt_add_executable(raw_render_diagnostics
    tools/raw_render_diagnostics/raw_render_diagnostics.cpp
)
target_link_libraries(raw_render_diagnostics PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)
