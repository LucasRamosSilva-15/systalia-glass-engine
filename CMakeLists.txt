cmake_minimum_required(VERSION 3.22)
project(systalia_glass_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    ShaderTools
    WaylandCompositor
)

find_package(Vulkan REQUIRED)
find_package(WaylandScanner REQUIRED)

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
    src/system/SystemTelemetryService.hpp
    src/system/SystemTelemetryService.cpp
    src/system/ProcessLauncherService.hpp
    src/system/ProcessLauncherService.cpp
    src/system/AppRegistryService.hpp
    src/system/AppRegistryService.cpp
)

qt6_generate_wayland_protocol_server_sources(sge_core
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml"
)

target_include_directories(sge_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system
)

target_link_libraries(sge_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::WaylandCompositor
    Vulkan::Vulkan
    sge_shaders
)

qt_add_qml_module(sge_core
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Systalia/Engine"
    URI Systalia.Engine
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        src/qml/main.qml
        src/qml/WaylandClientView.qml
        src/qml/WaylandWindowContainer.qml
        src/qml/shell/SystaliaTopBar.qml
        src/qml/shell/SystaliaDock.qml
        src/qml/shell/SystaliaFlowMenu.qml
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
)

# Shared Qt loads QML plugins at runtime; static builds still need import scanning.
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "SHARED_LIBRARY")
    set_property(TARGET systalia_glass_engine PROPERTY QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
endif()

enable_testing()
add_subdirectory(tests)
