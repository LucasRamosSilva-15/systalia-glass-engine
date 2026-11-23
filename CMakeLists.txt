cmake_minimum_required(VERSION 3.22)
project(systalia_glass_engine LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(cmake/Dependencies.cmake)

add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Werror>")

include(cmake/SgeShaders.cmake)
include(cmake/SgeCore.cmake)
include(cmake/SgeDrmPoc.cmake)
include(cmake/SgeQml.cmake)
include(cmake/SgeInstall.cmake)

option(SGE_ENABLE_WAYLIB_BACKEND "Enable experimental Waylib/wlroots nested compositor backend" OFF)
if(SGE_ENABLE_WAYLIB_BACKEND)
    include(cmake/SgeWaylib.cmake)
endif()

enable_testing()
add_subdirectory(tests)

include(cmake/SgeTools.cmake)
