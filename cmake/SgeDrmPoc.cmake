# Infraestrutura DRM/KMS, GBM, ponte Vulkan/Qt Quick e QPA neutro (Etapas 03.2.4.2–03.2.4.5).

pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
pkg_check_modules(GBM REQUIRED IMPORTED_TARGET gbm)

add_library(sge_drm_poc STATIC
    src/platform/drm/DrmSessionTypes.hpp
    src/platform/drm/DrmBackendController.hpp
    src/platform/drm/DrmBackendController.cpp
    src/platform/drm/LogindDrmProvider.hpp
    src/platform/drm/LogindDrmProvider.cpp
    src/platform/drm/KmsOutputDiscovery.hpp
    src/platform/drm/KmsOutputDiscovery.cpp
    src/platform/drm/GbmBufferManager.hpp
    src/platform/drm/GbmBufferManager.cpp
    src/platform/drm/AtomicKmsPresenter.hpp
    src/platform/drm/AtomicKmsPresenter.cpp
    src/platform/drm/VulkanQuickContracts.hpp
    src/platform/drm/VulkanQuickRenderer.hpp
    src/platform/drm/VulkanQuickRenderer.cpp
    src/platform/drm/WaylandFrameCoordinator.hpp
    src/platform/drm/WaylandFrameCoordinator.cpp
    src/platform/drm/DrmVulkanMatcher.hpp
    src/platform/drm/DrmVulkanMatcher.cpp
    src/platform/drm/DrmEventDispatcher.hpp
    src/platform/drm/DrmEventDispatcher.cpp
    src/platform/drm/IndependentFrameLoop.hpp
    src/platform/drm/IndependentFrameLoop.cpp
)

target_include_directories(sge_drm_poc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/drm
)

target_link_libraries(sge_drm_poc PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    Qt6::WaylandCompositor
    Vulkan::Vulkan
    sge_core
    PkgConfig::LIBDRM
    PkgConfig::GBM
)

# Plugin QPA neutro com suporte a QVulkanInstance sem abrir /dev/dri/* (Etapa 03.2.4.5).
add_library(sge_vulkan_offscreen MODULE
    src/platform/qpa/SgeVulkanOffscreenQpa.hpp
    src/platform/qpa/SgeVulkanOffscreenQpa.cpp
)

set_target_properties(sge_vulkan_offscreen PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/platforms"
    PREFIX "lib"
)

target_link_libraries(sge_vulkan_offscreen PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Vulkan::Vulkan
)

install(TARGETS sge_vulkan_offscreen
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/platforms"
)

qt_add_executable(sge_vulkan_quick_poc
    src/platform/drm/sge_vulkan_quick_poc.cpp
)

target_link_libraries(sge_vulkan_quick_poc PRIVATE
    sge_drm_poc
)

qt_add_executable(sge_vulkan_dmabuf_harness
    tools/diagnostics/sge_vulkan_dmabuf_harness.cpp
)

target_link_libraries(sge_vulkan_dmabuf_harness PRIVATE
    sge_drm_poc
    -Wl,--whole-archive sge_core -Wl,--no-whole-archive
)

add_dependencies(sge_vulkan_dmabuf_harness sge_vulkan_offscreen)
