# Utilitários de benchmark, geração de artefatos e diagnóstico de renderização
qt_add_executable(valo_gpu_benchmark
    tools/bench_and_artifacts.cpp
)
target_link_libraries(valo_gpu_benchmark PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

qt_add_executable(valo_dock_v2_artifacts
    tools/generate_dock_v2_artifacts.cpp
)
target_link_libraries(valo_dock_v2_artifacts PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

qt_add_executable(valo_shell_v2_artifacts
    tools/generate_shell_v2_artifacts.cpp
)
target_link_libraries(valo_shell_v2_artifacts PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

qt_add_executable(valo_stage_05_1_artifacts
    tools/generate_stage_05_1_artifacts.cpp
)
target_link_libraries(valo_stage_05_1_artifacts PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

qt_add_executable(raw_render_diagnostics
    tools/raw_render_diagnostics/raw_render_diagnostics.cpp
)
target_link_libraries(raw_render_diagnostics PRIVATE
    sge_core
    sge_shaders
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)
