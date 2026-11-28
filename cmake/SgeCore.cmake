# Biblioteca estática central com o motor óptico, compositor Wayland e serviços de sistema
qt_add_library(sge_core STATIC
    src/graphics/shared/ValoStd140Matrix.hpp
    src/graphics/shared/SystaliaClientClip.hpp
    src/graphics/v2/core/SystaliaSurfacePropsV2.hpp
    src/graphics/v2/geometry/ValoGeometryEngineV2.hpp
    src/graphics/v2/geometry/ValoGeometryEngineV2.cpp
    src/graphics/v2/geometry/ValoOrganicTransform.hpp
    src/graphics/v2/geometry/ValoOrganicTransform.cpp
    src/graphics/v2/core/ValoOpticalContext.hpp
    src/graphics/v2/core/ValoOpticalContext.cpp
    src/graphics/v2/materials/ValoGlassMaterialV2.hpp
    src/graphics/v2/materials/ValoGlassMaterialV2.cpp
    src/graphics/v2/materials/ValoGlassItemV2.hpp
    src/graphics/v2/materials/ValoGlassItemV2.cpp
    src/graphics/v2/backdrop/ValoBackdropProviderV2.hpp
    src/graphics/v2/backdrop/ValoBackdropProviderV2.cpp
    src/graphics/v2/blur/ValoBlurPipelineV2.hpp
    src/graphics/v2/blur/ValoBlurPipelineV2.cpp
    src/graphics/v2/blur/ValoGpuBlurPipelineV2.hpp
    src/graphics/v2/blur/ValoGpuBlurPipelineV2.cpp
    src/graphics/v2/shadows/ValoShadowItemV2.hpp
    src/graphics/v2/shadows/ValoShadowItemV2.cpp
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
    src/theme/tokens/ValoTokens.hpp
)

qt6_generate_wayland_protocol_server_sources(sge_core
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml"
)

target_include_directories(sge_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/v2/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/v2/geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/v2/materials
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/v2/backdrop
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/v2/blur
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/v2/shadows
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/theme/tokens
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

target_compile_definitions(sge_core PRIVATE
    SGE_DATA_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/systalia"
)
add_dependencies(sge_core sge_tokens)
