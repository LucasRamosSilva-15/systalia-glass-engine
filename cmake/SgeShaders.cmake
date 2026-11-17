# Biblioteca estática com os bytecodes SPIR-V compilados via ShaderTools
qt_add_library(sge_shaders STATIC)
qt6_add_shaders(sge_shaders "sge_shaders_resource"
    PREFIX
        "/shaders"
    BASE
        "shaders"
    FILES
        shaders/valo_optical.vert
        shaders/valo_optical.frag
)
qt6_add_shaders(sge_shaders "sge_shaders_v2_resource"
    PREFIX
        "/shaders/v2"
    BASE
        "shaders/v2"
    FILES
        shaders/v2/valo_optical_v2.vert
        shaders/v2/valo_optical_v2.frag
        shaders/v2/valo_backdrop_copy.frag
        shaders/v2/valo_particles.frag
        shaders/v2/valo_kawase_down.vert
        shaders/v2/valo_kawase_down.frag
        shaders/v2/valo_kawase_up.vert
        shaders/v2/valo_kawase_up.frag
        shaders/v2/valo_diag_sdf.frag
        shaders/v2/valo_diag_mask.frag
        shaders/v2/valo_diag_volume.frag
        shaders/v2/valo_diag_refraction.frag
        shaders/v2/valo_diag_blur.frag
        shaders/v2/valo_diag_specular.frag
        shaders/v2/valo_diag_caustics.frag
        shaders/v2/valo_diag_reflection.frag
        shaders/v2/valo_diag_basic.frag
        shaders/v2/valo_shadow_v2.vert
        shaders/v2/valo_shadow_v2.frag
)
qt6_add_shaders(sge_shaders "sge_raw_shaders_resource"
    PREFIX
        "/raw_shaders"
    BASE
        "tools/raw_render_diagnostics/shaders"
    FILES
        tools/raw_render_diagnostics/shaders/raw_t2_minimal.vert
        tools/raw_render_diagnostics/shaders/raw_t2_minimal.frag
        tools/raw_render_diagnostics/shaders/raw_t3_ubo.vert
        tools/raw_render_diagnostics/shaders/raw_t3_ubo.frag
        tools/raw_render_diagnostics/shaders/raw_t4_sdf.vert
        tools/raw_render_diagnostics/shaders/raw_t4_sdf.frag
)
target_link_libraries(sge_shaders PUBLIC
    Qt6::Core
    Qt6::Gui
)

