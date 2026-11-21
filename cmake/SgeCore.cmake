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
