# Protótipo isolado do backend DRM/KMS com proprietário único (Etapa 03.2.4.2).
# Compilado separadamente de sge_core e systalia_glass_engine para não alterar
# o caminho de inicialização atualmente utilizado em produção.

add_library(sge_drm_poc STATIC
    src/platform/drm/DrmSessionTypes.hpp
    src/platform/drm/DrmBackendController.hpp
    src/platform/drm/DrmBackendController.cpp
)

target_include_directories(sge_drm_poc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/drm
)

target_link_libraries(sge_drm_poc PUBLIC
    Qt6::Core
)
