# Infraestrutura DRM/KMS, GBM e preparação Vulkan com proprietário único (Etapas 03.2.4.2–03.2.4.3).
# Compilada separadamente do executável principal para preservar o caminho de produção atual.

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
    src/platform/drm/VulkanQuickContracts.hpp
)

target_include_directories(sge_drm_poc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/drm
)

target_link_libraries(sge_drm_poc PUBLIC
    Qt6::Core
    Qt6::DBus
    sge_core
    PkgConfig::LIBDRM
    PkgConfig::GBM
)
