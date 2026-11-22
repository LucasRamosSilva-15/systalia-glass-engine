# Dependências externas e políticas Qt6 do SYSTALIA Glass Engine
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
