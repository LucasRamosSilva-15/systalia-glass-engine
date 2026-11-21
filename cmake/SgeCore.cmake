# Biblioteca estática central com o motor óptico, compositor Wayland e serviços de sistema
qt_add_library(sge_core STATIC
    src/graphics/ValoStd140Matrix.hpp
    src/graphics/SystaliaClientClip.hpp
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
    src/compositor/surfaces/SystaliaSurfaceExtension.hpp
    src/compositor/surfaces/SystaliaSurfaceExtension.cpp
    src/compositor/core/SystaliaCompositor.hpp
    src/compositor/core/SystaliaCompositor.cpp
    src/compositor/windows/WindowStackModel.hpp
    src/compositor/windows/WindowStackModel.cpp
    src/compositor/outputs/OutputManagerService.hpp
    src/compositor/outputs/OutputManagerService.cpp
    src/compositor/xwayland/XWaylandManager.hpp
    src/compositor/xwayland/XWaylandManager.cpp
    src/services/system/SystemTelemetryService.hpp
    src/services/system/SystemTelemetryService.cpp
    src/services/applications/ProcessLauncherService.hpp
    src/services/applications/ProcessLauncherService.cpp
    src/services/applications/AppRegistryService.hpp
    src/services/applications/AppRegistryService.cpp
    src/services/notifications/NotificationServer.hpp
    src/services/notifications/NotificationServer.cpp
    src/services/system/SystemControlService.hpp
    src/services/system/SystemControlService.cpp
    src/services/windows/WorkspaceManagerService.hpp
    src/services/windows/WorkspaceManagerService.cpp
    src/services/windows/LiquidTilingService.hpp
    src/services/windows/LiquidTilingService.cpp
    src/services/input/InputDeviceManager.hpp
    src/services/input/InputDeviceManager.cpp
    src/services/session/LogindSessionService.hpp
    src/services/session/LogindSessionService.cpp
    src/theme/ValoTokens.hpp
)

qt6_generate_wayland_protocol_server_sources(sge_core
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml"
)

target_include_directories(sge_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/surfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/outputs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/xwayland
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/applications
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/input
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/session
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/notifications
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/system
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
