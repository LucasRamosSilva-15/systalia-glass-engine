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
    ShaderTools
)

find_package(Vulkan REQUIRED)

# Biblioteca de interface para cabeçalhos e configurações do core
add_library(sge_core INTERFACE)
target_include_directories(sge_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx
)
target_link_libraries(sge_core INTERFACE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Vulkan::Vulkan
)

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

enable_testing()
add_subdirectory(tests)
