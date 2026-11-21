cmake_minimum_required(VERSION 3.22)
project(systalia_glass_engine LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

include(cmake/SgeShaders.cmake)

# Biblioteca estática central com o motor, itens visuais, compositor Wayland e módulo QML
qt_add_library(sge_core STATIC
    src/graphics/ValoStd140Matrix.hpp
    src/graphics/SystaliaClientClip.hpp
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
    src/services/SystemTelemetryService.hpp
    src/services/SystemTelemetryService.cpp
    src/services/ProcessLauncherService.hpp
    src/services/ProcessLauncherService.cpp
    src/services/AppRegistryService.hpp
    src/services/AppRegistryService.cpp
    src/services/NotificationServer.hpp
    src/services/NotificationServer.cpp
    src/services/SystemControlService.hpp
    src/services/SystemControlService.cpp
    src/services/WorkspaceManagerService.hpp
    src/services/WorkspaceManagerService.cpp
    src/services/LiquidTilingService.hpp
    src/services/LiquidTilingService.cpp
    src/services/InputDeviceManager.hpp
    src/services/InputDeviceManager.cpp
    src/services/LogindSessionService.hpp
    src/services/LogindSessionService.cpp
    src/theme/ValoTokens.hpp
    src/graphics/SystaliaSurfacePropsV2.hpp
    src/graphics/ValoGeometryEngineV2.hpp
    src/graphics/ValoGeometryEngineV2.cpp
    src/graphics/ValoOrganicTransform.hpp
    src/graphics/ValoOrganicTransform.cpp
    src/graphics/ValoOpticalContext.hpp
    src/graphics/ValoOpticalContext.cpp
    src/graphics/ValoGlassMaterialV2.hpp
    src/graphics/ValoGlassMaterialV2.cpp
    src/graphics/ValoGlassItemV2.hpp
    src/graphics/ValoGlassItemV2.cpp
    src/graphics/ValoBackdropProviderV2.hpp
    src/graphics/ValoBackdropProviderV2.cpp
    src/graphics/ValoBlurPipelineV2.hpp
    src/graphics/ValoBlurPipelineV2.cpp
    src/graphics/ValoGpuBlurPipelineV2.hpp
    src/graphics/ValoGpuBlurPipelineV2.cpp
    src/graphics/ValoShadowItemV2.hpp
    src/graphics/ValoShadowItemV2.cpp
)

qt6_generate_wayland_protocol_server_sources(sge_core
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml"
)

target_include_directories(sge_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/theme
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

set_source_files_properties(src/theme/qml/SystaliaTheme.qml src/theme/qml/ValoTheme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

file(GLOB sge_valo_assets RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "assets/figma/*.svg"
    "assets/figma/*.png"
)

foreach(asset IN LISTS sge_valo_assets)
    get_filename_component(asset_name "${asset}" NAME)
    set_source_files_properties(${asset} PROPERTIES QT_RESOURCE_ALIAS "src/qml/assets/figma/${asset_name}")
endforeach()

# Preserva aliases QRC públicos (:/qt/qml/Systalia/Engine/src/qml/...) após reorganização modular
set_source_files_properties(
    src/shell/main.qml
    src/shell/WorkspaceViewport.qml
    src/shell/WaylandClientView.qml
    src/shell/WaylandWindowContainer.qml
    PROPERTIES QT_RESOURCE_ALIAS ""
)
set_source_files_properties(src/shell/main.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/main.qml")
set_source_files_properties(src/shell/WorkspaceViewport.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WorkspaceViewport.qml")
set_source_files_properties(src/shell/WaylandClientView.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WaylandClientView.qml")
set_source_files_properties(src/shell/WaylandWindowContainer.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WaylandWindowContainer.qml")

set_source_files_properties(src/theme/qml/SystaliaTheme.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/theme/SystaliaTheme.qml")
set_source_files_properties(src/theme/qml/ValoTheme.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/theme/ValoTheme.qml")

foreach(ctrl_qml IN ITEMS
    SystaliaG2Frame.qml
    SystaliaWindowAction.qml
    SystaliaWindowHeader.qml
    ValoDockItem.qml
)
    set_source_files_properties("src/controls/${ctrl_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/${ctrl_qml}")
endforeach()

foreach(shell_qml IN ITEMS
    SystaliaTopBar.qml
    SystaliaDock.qml
    SystaliaFilesWindow.qml
    SystaliaMaterialWindow.qml
    SystaliaFlowMenu.qml
    SystaliaNotificationHost.qml
    SystaliaControlCenter.qml
    SystaliaTaskSwitcher.qml
    SnapPreviewOverlay.qml
    TileSplitter.qml
    SystaliaDockV2.qml
    SystaliaTopBarV2.qml
    SystaliaFilesWindowV2.qml
    SystaliaControlCenterV2.qml
)
    set_source_files_properties("src/shell/${shell_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/${shell_qml}")
endforeach()

foreach(gfx_qml IN ITEMS
    ValoMaterialSurfaceV2.qml
    ValoInternalWindowBackdropV2.qml
    ValoSingleWindowBackdropV2.qml
    ValoLimitedBackdropV2.qml
    ValoOpticalParticlesV2.qml
    ValoSurfaceV2.qml
    ValoDockItemV2.qml
    ValoDockV2.qml
    ValoDockV2Scene.qml
)
    set_source_files_properties("src/graphics/qml/${gfx_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/graphics/v2/${gfx_qml}")
endforeach()

qt_add_qml_module(sge_core
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Systalia/Engine"
    URI Systalia.Engine
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        src/shell/main.qml
        src/shell/WorkspaceViewport.qml
        src/theme/qml/SystaliaTheme.qml
        src/theme/qml/ValoTheme.qml
        src/shell/WaylandClientView.qml
        src/shell/WaylandWindowContainer.qml
        src/shell/SystaliaTopBar.qml
        src/shell/SystaliaDock.qml
        src/controls/ValoDockItem.qml
        src/controls/SystaliaG2Frame.qml
        src/controls/SystaliaWindowAction.qml
        src/controls/SystaliaWindowHeader.qml
        src/shell/SystaliaFilesWindow.qml
        src/shell/SystaliaMaterialWindow.qml
        src/shell/SystaliaFlowMenu.qml
        src/shell/SystaliaNotificationHost.qml
        src/shell/SystaliaControlCenter.qml
        src/shell/SystaliaTaskSwitcher.qml
        src/shell/SnapPreviewOverlay.qml
        src/shell/TileSplitter.qml
        src/shell/SystaliaDockV2.qml
        src/shell/SystaliaTopBarV2.qml
        src/shell/SystaliaFilesWindowV2.qml
        src/shell/SystaliaControlCenterV2.qml
        src/graphics/qml/ValoMaterialSurfaceV2.qml
        src/graphics/qml/ValoInternalWindowBackdropV2.qml
        src/graphics/qml/ValoSingleWindowBackdropV2.qml
        src/graphics/qml/ValoLimitedBackdropV2.qml
        src/graphics/qml/ValoOpticalParticlesV2.qml
        src/graphics/qml/ValoSurfaceV2.qml
        src/graphics/qml/ValoDockItemV2.qml
        src/graphics/qml/ValoDockV2.qml
        src/graphics/qml/ValoDockV2Scene.qml
    RESOURCES
        ${sge_valo_assets}
    NO_PLUGIN
)

# Executável principal da aplicação
qt_add_executable(systalia_glass_engine
    src/app/main.cpp
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

# Utilitário de geração de artefatos visuais e validação da Shell VALO V2 (Etapa 05)
qt_add_executable(valo_shell_v2_artifacts
    tools/generate_shell_v2_artifacts.cpp
)
target_link_libraries(valo_shell_v2_artifacts PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

# Utilitário de validação óptica e diagnósticos (Etapa 05.1)
qt_add_executable(valo_stage_05_1_artifacts
    tools/generate_stage_05_1_artifacts.cpp
)
target_link_libraries(valo_stage_05_1_artifacts PRIVATE
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
