cmake_minimum_required(VERSION 3.22)
project(systalia_glass_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

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
    src/gfx/SystaliaGlassMaterial.hpp
    src/gfx/SystaliaGlassMaterial.cpp
    src/gfx/SystaliaGlassItem.hpp
    src/gfx/SystaliaGlassItem.cpp
    src/compositor/SystaliaCompositor.hpp
    src/compositor/SystaliaCompositor.cpp
)

target_include_directories(sge_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor
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
    URI Systalia.Engine
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        src/qml/main.qml
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

enable_testing()
add_subdirectory(tests)
