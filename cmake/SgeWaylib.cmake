# Integração experimental opcional do backend Waylib/wlroots (Etapa 03.2.5.1).
# Ativada apenas com -DSGE_ENABLE_WAYLIB_BACKEND=ON; não altera o build legado quando OFF.

if(NOT TARGET PkgConfig::WAYLAND_SERVER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
endif()

pkg_check_modules(WAYLIB_PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_check_modules(WAYLIB_XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(WAYLIB_LIBDRM REQUIRED IMPORTED_TARGET libdrm)

find_package(Qt6 REQUIRED COMPONENTS QuickPrivate GuiPrivate)

set(_sge_waylib_hints
    "${SGE_WAYLIB_PREFIX}"
    "$ENV{SGE_WAYLIB_PREFIX}"
    "$ENV{HOME}/.cache/sge-waylib-prefix"
    ${CMAKE_PREFIX_PATH}
)

find_path(WAYLIB_SERVER_INCLUDE_DIR
    NAMES wserver.h
    HINTS ${_sge_waylib_hints}
    PATH_SUFFIXES include/waylibserver waylibserver
)

find_path(WAYLIB_WLROOTS_INCLUDE_DIR
    NAMES wlr/version.h
    HINTS ${_sge_waylib_hints}
    PATH_SUFFIXES include
)

find_library(WAYLIB_SERVER_LIBRARY
    NAMES waylibserver
    HINTS ${_sge_waylib_hints}
    PATH_SUFFIXES lib64 lib
)

find_library(WAYLIB_WLROOTS_LIBRARY
    NAMES waylib-wlroots
    HINTS ${_sge_waylib_hints}
    PATH_SUFFIXES lib64 lib
)

if(NOT WAYLIB_SERVER_INCLUDE_DIR OR NOT WAYLIB_WLROOTS_INCLUDE_DIR
   OR NOT WAYLIB_SERVER_LIBRARY OR NOT WAYLIB_WLROOTS_LIBRARY)
    message(FATAL_ERROR
        "SGE_ENABLE_WAYLIB_BACKEND=ON requer WaylibServer e waylib-wlroots.\n"
        "  WAYLIB_SERVER_INCLUDE_DIR=${WAYLIB_SERVER_INCLUDE_DIR}\n"
        "  WAYLIB_WLROOTS_INCLUDE_DIR=${WAYLIB_WLROOTS_INCLUDE_DIR}\n"
        "  WAYLIB_SERVER_LIBRARY=${WAYLIB_SERVER_LIBRARY}\n"
        "  WAYLIB_WLROOTS_LIBRARY=${WAYLIB_WLROOTS_LIBRARY}\n"
        "Defina -DSGE_WAYLIB_PREFIX=<prefixo> ou CMAKE_PREFIX_PATH com a instalação local do Waylib.")
endif()

get_filename_component(_sge_waylib_root "${WAYLIB_SERVER_INCLUDE_DIR}/../.." ABSOLUTE)
set(SGE_WAYLIB_QML_IMPORT_PATH "${_sge_waylib_root}/qml" CACHE PATH "Caminho QML do módulo Waylib.Server")

add_library(Sge::WaylibWlroots UNKNOWN IMPORTED)
set_target_properties(Sge::WaylibWlroots PROPERTIES
    IMPORTED_LOCATION "${WAYLIB_WLROOTS_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${WAYLIB_WLROOTS_INCLUDE_DIR}"
)

add_library(Sge::WaylibServer UNKNOWN IMPORTED)
set_target_properties(Sge::WaylibServer PROPERTIES
    IMPORTED_LOCATION "${WAYLIB_SERVER_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${WAYLIB_SERVER_INCLUDE_DIR};${WAYLIB_WLROOTS_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "Sge::WaylibWlroots;Qt6::Core;Qt6::Gui;Qt6::GuiPrivate;Qt6::Quick;Qt6::QuickPrivate;PkgConfig::WAYLAND_SERVER;PkgConfig::WAYLIB_PIXMAN;PkgConfig::WAYLIB_XKBCOMMON;PkgConfig::WAYLIB_LIBDRM"
)

add_library(sge_waylib_backend STATIC
    src/compositor/waylib/WaylibCompositorAdapter.hpp
    src/compositor/waylib/WaylibCompositorAdapter.cpp
    src/compositor/waylib/WaylibOutputAdapter.hpp
    src/compositor/waylib/WaylibOutputAdapter.cpp
    src/compositor/waylib/WaylibSurfaceAdapter.hpp
    src/compositor/waylib/WaylibSurfaceAdapter.cpp
)

target_include_directories(sge_waylib_backend PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(sge_waylib_backend PUBLIC
    SGE_HAS_WAYLIB_BACKEND=1
    WLR_USE_UNSTABLE=1
)

target_link_libraries(sge_waylib_backend PUBLIC
    Sge::WaylibServer
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

qt_add_executable(systalia_waylib_nested
    src/app/waylib_nested_main.cpp
)

qt_add_resources(systalia_waylib_nested "sge_waylib_qml"
    PREFIX "/systalia/waylib"
    BASE "src/qml/waylib"
    FILES
        src/qml/waylib/WaylibNestedShell.qml
)

target_link_libraries(systalia_waylib_nested PRIVATE
    sge_waylib_backend
    Qt6::QuickControls2
)

target_compile_definitions(systalia_waylib_nested PRIVATE
    SGE_WAYLIB_QML_IMPORT_PATH="${SGE_WAYLIB_QML_IMPORT_PATH}"
)
