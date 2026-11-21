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
include(cmake/SgeCore.cmake)
include(cmake/SgeQml.cmake)
include(cmake/SgeInstall.cmake)

enable_testing()
add_subdirectory(tests)

include(cmake/SgeTools.cmake)
