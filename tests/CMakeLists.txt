find_package(Qt6 REQUIRED COMPONENTS Test)

# Teste 1: Memória std140 e Bytecodes SPIR-V (.qsb)
qt_add_executable(tst_systalia_engine
    graphics/tst_SystaliaEngine.cpp
)
target_link_libraries(tst_systalia_engine PRIVATE
    Qt6::Core
    Qt6::Test
    sge_core
    sge_shaders
)
add_test(NAME tst_systalia_engine COMMAND tst_systalia_engine)

# Teste 2: Geometria de Vértices e Propriedades do ValoGlassItemV2
qt_add_executable(tst_item_geometry
    graphics/tst_ItemGeometry.cpp
)
target_link_libraries(tst_item_geometry PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Test
    sge_core
    sge_shaders
)
add_test(NAME tst_item_geometry COMMAND tst_item_geometry)

# Teste 3: Propriedades Analíticas do SDF Squircle G2
qt_add_executable(tst_sdf_geometry
    graphics/tst_SdfGeometry.cpp
)
target_link_libraries(tst_sdf_geometry PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME tst_sdf_geometry COMMAND tst_sdf_geometry)

# Teste 4: Vinculação de Backdrop e Resolução Screen-Space
qt_add_executable(tst_backdrop_binding
    graphics/tst_BackdropBinding.cpp
)
target_link_libraries(tst_backdrop_binding PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Test
    sge_core
    sge_shaders
)
add_test(NAME tst_backdrop_binding COMMAND tst_backdrop_binding)

# Teste 5: Física Óptica VALO Glass (Refração, Menisco, Cauchy e Normais)
qt_add_executable(tst_optical_physics
    graphics/tst_OpticalPhysics.cpp
)
target_link_libraries(tst_optical_physics PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME tst_optical_physics COMMAND tst_optical_physics)

# Teste 6: Criação, Ciclo de Vida e IPC do Socket Wayland (wayland-systalia-0)
qt_add_executable(tst_wayland_socket
    compositor/tst_WaylandSocket.cpp
)
target_link_libraries(tst_wayland_socket PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::WaylandCompositor
    Qt6::Test
    sge_core
)
add_test(NAME tst_wayland_socket COMMAND tst_wayland_socket)

# Teste 7: Registro do Protocolo XDG Shell (xdg_wm_base) e Capacidades do Assento (Seat)
qt_add_executable(tst_xdg_shell
    compositor/tst_XdgShell.cpp
)
target_link_libraries(tst_xdg_shell PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::WaylandCompositor
    Qt6::Test
    sge_core
)
add_test(NAME tst_xdg_shell COMMAND tst_xdg_shell)

# Teste 8: Protocolo de Decoração Server-Side (zxdg_decoration_manager_v1)
qt_add_executable(tst_xdg_decorations
    compositor/tst_XdgDecorations.cpp
)
target_link_libraries(tst_xdg_decorations PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::WaylandCompositor
    Qt6::Test
    sge_core
)
add_test(NAME tst_xdg_decorations COMMAND tst_xdg_decorations)


# Teste 9: Configure/ack/commit de resize e arraste das oito bordas SSD.
qt_add_executable(tst_xdg_resize compositor/tst_XdgResize.cpp)
target_link_libraries(tst_xdg_resize PRIVATE Qt6::Test Qt6::Quick sge_core)
add_test(NAME tst_xdg_resize COMMAND tst_xdg_resize)
get_filename_component(_qt6_cmake_dir "${Qt6Core_DIR}" DIRECTORY)
get_filename_component(_qt6_arch_lib_dir "${_qt6_cmake_dir}" DIRECTORY)
set(_qt6_plugin_dir "${_qt6_arch_lib_dir}/qt6/plugins")
set(_tst_xdg_resize_environment "QT_QPA_PLATFORM=offscreen")
if(EXISTS "${_qt6_plugin_dir}/platforms/libqwayland-generic.so")
    list(APPEND _tst_xdg_resize_environment "QT_PLUGIN_PATH=${_qt6_plugin_dir}")
endif()

# Teste 10: IPC do protocolo óptico, precisão fixed-point e ciclo de vida.
find_package(Qt6 REQUIRED COMPONENTS WaylandClient)
add_subdirectory(protocol-client)
qt_add_executable(tst_systalia_protocol compositor/tst_SystaliaProtocol.cpp)
target_link_libraries(tst_systalia_protocol PRIVATE
    Qt6::Test Qt6::Quick Qt6::WaylandClient Wayland::Server sge_core sge_test_protocol_client
)
add_test(NAME tst_systalia_protocol COMMAND tst_systalia_protocol)
set_tests_properties(tst_systalia_protocol PROPERTIES
    TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Teste 11: composição do cliente, offsets std140 e recorte interno G2.
qt_add_executable(tst_unified_composition graphics/tst_UnifiedComposition.cpp)
target_link_libraries(tst_unified_composition PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    sge_core
)
add_test(NAME tst_unified_composition COMMAND tst_unified_composition)

# Teste 12: parser de /proc/stat, memória real e formato do relógio do sistema.
qt_add_executable(tst_telemetry unit/tst_Telemetry.cpp)
target_link_libraries(tst_telemetry PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    sge_core
)
add_test(NAME tst_telemetry COMMAND tst_telemetry)

# Teste 13: ambiente Wayland e ciclo de vida de processos desacoplados.
qt_add_executable(tst_process_launcher unit/tst_ProcessLauncher.cpp)
target_link_libraries(tst_process_launcher PRIVATE
    Qt6::Core
    Qt6::Test
    sge_core
)
add_test(NAME tst_process_launcher COMMAND tst_process_launcher)

# Test 14: parser .desktop, filtragem da lista de aplicações e notificações do modelo.
qt_add_executable(tst_app_registry unit/tst_AppRegistry.cpp)
target_link_libraries(tst_app_registry PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    sge_core
)
add_test(NAME tst_app_registry COMMAND tst_app_registry)

# Test 15: ciclo de vida, substituição, expiração e sinais D-Bus de notificações.
qt_add_executable(tst_notification_server unit/tst_NotificationServer.cpp)
target_link_libraries(tst_notification_server PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Test
    sge_core
)
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
if(DBUS_RUN_SESSION_EXECUTABLE)
    add_test(NAME tst_notification_server
        COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:tst_notification_server>)
else()
    add_test(NAME tst_notification_server COMMAND tst_notification_server)
endif()

set(_sge_headless_tests
    tst_systalia_engine
    tst_item_geometry
    tst_sdf_geometry
    tst_backdrop_binding
    tst_optical_physics
    tst_wayland_socket
    tst_xdg_shell
    tst_xdg_decorations
    tst_systalia_protocol
    tst_unified_composition
    tst_telemetry
    tst_process_launcher
    tst_app_registry
    tst_notification_server
)
set_tests_properties(${_sge_headless_tests} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
set_tests_properties(tst_xdg_resize PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "${_tst_xdg_resize_environment}"
)

# The expanded suite keeps each executable independently runnable while also
# exposing layer-level build targets and CTest labels.
function(sge_add_expanded_test target source layer)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::WaylandCompositor
        Qt6::Test
        -Wl,--whole-archive sge_core -Wl,--no-whole-archive
    )
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        LABELS "${layer}"
        TIMEOUT 45
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

# Unit tests: pure math, ABI layout, kernel text parsing and desktop Exec parsing.
sge_add_expanded_test(tst_sdf_geometry_math unit/tst_SdfGeometryMath.cpp unit)
target_compile_definitions(tst_sdf_geometry_math PRIVATE
    SGE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
sge_add_expanded_test(tst_memory_alignment unit/tst_MemoryAlignment.cpp unit)
sge_add_expanded_test(tst_linux_parsers unit/tst_LinuxParsers.cpp unit)
sge_add_expanded_test(tst_task_switcher unit/tst_TaskSwitcher.cpp unit)
sge_add_expanded_test(tst_resource_leak_audit unit/tst_ResourceLeakAudit.cpp unit)
sge_add_expanded_test(tst_workspace_manager unit/tst_WorkspaceManager.cpp unit)
sge_add_expanded_test(tst_liquid_tiling unit/tst_LiquidTiling.cpp unit)
sge_add_expanded_test(tst_output_manager unit/tst_OutputManager.cpp unit)
sge_add_expanded_test(tst_phase4_audit unit/tst_Phase4Audit.cpp unit)
sge_add_expanded_test(tst_input_hardware_backend unit/tst_InputHardwareBackend.cpp unit)
if(TARGET PkgConfig::LIBINPUT)
    target_link_libraries(tst_input_hardware_backend PRIVATE PkgConfig::LIBINPUT)
endif()
if(TARGET PkgConfig::LIBUDEV)
    target_link_libraries(tst_input_hardware_backend PRIVATE PkgConfig::LIBUDEV)
endif()
sge_add_expanded_test(tst_logind_integration unit/tst_LogindIntegration.cpp unit)
target_compile_definitions(tst_logind_integration PRIVATE
    SGE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    SGE_BINARY_DIR="${PROJECT_BINARY_DIR}"
)
target_link_libraries(tst_logind_integration PRIVATE Qt6::DBus)
sge_add_expanded_test(tst_xwayland_integration unit/tst_XWaylandIntegration.cpp unit)
if(TARGET PkgConfig::XCB)
    target_link_libraries(tst_xwayland_integration PRIVATE PkgConfig::XCB)
endif()
if(TARGET PkgConfig::WAYLAND_SERVER)
    target_link_libraries(tst_xwayland_integration PRIVATE PkgConfig::WAYLAND_SERVER)
endif()

sge_add_expanded_test(tst_valo_theme unit/tst_ValoTheme.cpp unit)
sge_add_expanded_test(tst_valo_glass unit/tst_ValoGlass.cpp unit)
sge_add_expanded_test(tst_valo_decorations unit/tst_ValoDecorations.cpp unit)
sge_add_expanded_test(tst_sdf_geometry_math_v2 unit/tst_SdfGeometryMathV2.cpp unit)
sge_add_expanded_test(tst_surface_props_alignment_v2 unit/tst_SurfacePropsAlignmentV2.cpp unit)
sge_add_expanded_test(tst_valo_organic_transform_v2 unit/tst_ValoOrganicTransformV2.cpp unit)
sge_add_expanded_test(tst_valo_glass_shader_v2 unit/tst_ValoGlassShaderV2.cpp unit)
sge_add_expanded_test(tst_valo_glass_rendering_v2 unit/tst_ValoGlassRenderingV2.cpp unit)
sge_add_expanded_test(tst_valo_backdrop_blur_v2 unit/tst_ValoBackdropBlurV2.cpp unit)
sge_add_expanded_test(tst_valo_refraction_optics_v2 unit/tst_ValoRefractionOpticsV2.cpp unit)
sge_add_expanded_test(tst_valo_gpu_blur_pipeline_v2 unit/tst_ValoGpuBlurPipelineV2.cpp unit)
sge_add_expanded_test(tst_valo_dock_v2 unit/tst_ValoDockV2.cpp unit)
set_tests_properties(tst_valo_dock_v2 PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
sge_add_expanded_test(tst_valo_shell_v2 unit/tst_ValoShellV2.cpp unit)
set_tests_properties(tst_valo_shell_v2 PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Integration tests exercise actual Wayland wire requests and model resets.
sge_add_expanded_test(tst_protocol_roundtrip integration/tst_ProtocolRoundtrip.cpp integration)
target_link_libraries(tst_protocol_roundtrip PRIVATE
    Qt6::WaylandClient Wayland::Server sge_test_protocol_client
)
sge_add_expanded_test(tst_xdg_shell_decoration integration/tst_XdgShellDecoration.cpp integration)
sge_add_expanded_test(tst_app_model_filtering integration/tst_AppModelFiltering.cpp integration)

# A real Qt Wayland client maps a painted buffer, then disconnects cleanly.
sge_add_expanded_test(tst_wayland_client_lifecycle_e2e
    e2e/tst_WaylandClientLifecycleE2E.cpp e2e
)

set_tests_properties(
    tst_protocol_roundtrip
    tst_xdg_shell_decoration
    tst_wayland_client_lifecycle_e2e
    PROPERTIES ENVIRONMENT "${_tst_xdg_resize_environment}"
)

add_custom_target(unit_tests DEPENDS
    tst_systalia_engine
    tst_item_geometry
    tst_backdrop_binding
    tst_sdf_geometry
    tst_optical_physics
    tst_unified_composition
    tst_sdf_geometry_math
    tst_memory_alignment
    tst_linux_parsers
    tst_task_switcher
    tst_resource_leak_audit
    tst_workspace_manager
    tst_liquid_tiling
    tst_output_manager
    tst_phase4_audit
    tst_input_hardware_backend
    tst_logind_integration
    tst_xwayland_integration
    tst_valo_theme
    tst_valo_glass
    tst_valo_decorations
    tst_sdf_geometry_math_v2
    tst_surface_props_alignment_v2
    tst_valo_organic_transform_v2
    tst_valo_glass_shader_v2
    tst_valo_glass_rendering_v2
    tst_valo_backdrop_blur_v2
    tst_valo_refraction_optics_v2
    tst_valo_gpu_blur_pipeline_v2
    tst_valo_dock_v2
    tst_valo_shell_v2
)
add_custom_target(integration_tests DEPENDS
    tst_wayland_socket
    tst_xdg_shell
    tst_xdg_decorations
    tst_xdg_resize
    tst_systalia_protocol
    tst_telemetry
    tst_process_launcher
    tst_app_registry
    tst_notification_server
    tst_protocol_roundtrip
    tst_xdg_shell_decoration
    tst_app_model_filtering
)
add_custom_target(e2e_tests DEPENDS tst_wayland_client_lifecycle_e2e)

set_tests_properties(
    tst_systalia_engine
    tst_item_geometry
    tst_sdf_geometry
    tst_backdrop_binding
    tst_optical_physics
    tst_unified_composition
    tst_sdf_geometry_math
    tst_memory_alignment
    tst_linux_parsers
    tst_task_switcher
    tst_resource_leak_audit
    tst_workspace_manager
    tst_liquid_tiling
    tst_output_manager
    tst_phase4_audit
    tst_input_hardware_backend
    tst_logind_integration
    tst_xwayland_integration
    tst_valo_theme
    tst_valo_glass
    tst_valo_decorations
    tst_sdf_geometry_math_v2
    tst_surface_props_alignment_v2
    tst_valo_organic_transform_v2
    tst_valo_glass_shader_v2
    tst_valo_glass_rendering_v2
    tst_valo_backdrop_blur_v2
    tst_valo_refraction_optics_v2
    tst_valo_gpu_blur_pipeline_v2
    tst_valo_dock_v2
    tst_valo_shell_v2
    PROPERTIES LABELS "unit"
)
set_tests_properties(
    tst_wayland_socket
    tst_xdg_shell
    tst_xdg_decorations
    tst_xdg_resize
    tst_systalia_protocol
    tst_telemetry
    tst_process_launcher
    tst_app_registry
    tst_notification_server
    tst_protocol_roundtrip
    tst_xdg_shell_decoration
    tst_app_model_filtering
    PROPERTIES LABELS "integration"
)

sge_add_expanded_test(tst_shell_state_regression integration/tst_ShellStateRegression.cpp integration)
set_tests_properties(tst_shell_state_regression PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)
add_dependencies(integration_tests tst_shell_state_regression)

add_dependencies(tst_shell_state_regression tst_xdg_resize)

sge_add_expanded_test(tst_client_surface_visibility integration/tst_ClientSurfaceVisibility.cpp integration)
add_dependencies(integration_tests tst_client_surface_visibility)
add_dependencies(tst_client_surface_visibility tst_xdg_resize)

sge_add_expanded_test(tst_valo_optical_contract_v2 unit/tst_ValoOpticalContractV2.cpp unit)
set_tests_properties(tst_valo_optical_contract_v2 PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=rhi;QSG_RHI_BACKEND=opengl"
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME tst_evidence_cleanup
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/unit/tst_EvidenceCleanup.py"
)
set_tests_properties(tst_evidence_cleanup PROPERTIES LABELS "unit")

add_test(NAME tst_tty3_homolog_scripts
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/unit/tst_Tty3HomologScripts.py"
)
set_tests_properties(tst_tty3_homolog_scripts PROPERTIES LABELS "unit")

qt_add_executable(tst_drm_backend_poc unit/tst_DrmBackendPoc.cpp)
target_link_libraries(tst_drm_backend_poc PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_test(NAME tst_drm_backend_poc COMMAND tst_drm_backend_poc)
set_tests_properties(tst_drm_backend_poc PROPERTIES
    LABELS "unit"
    TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(tst_drm_gbm_integration unit/tst_DrmGbmIntegration.cpp)
target_link_libraries(tst_drm_gbm_integration PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_test(NAME tst_drm_gbm_integration COMMAND tst_drm_gbm_integration)
set_tests_properties(tst_drm_gbm_integration PROPERTIES
    LABELS "unit"
    TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(tst_vulkan_quick_renderer unit/tst_VulkanQuickRenderer.cpp)
target_link_libraries(tst_vulkan_quick_renderer PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_test(NAME tst_vulkan_quick_renderer COMMAND tst_vulkan_quick_renderer)
set_tests_properties(tst_vulkan_quick_renderer PROPERTIES
    LABELS "unit"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(tst_independent_vulkan_session unit/tst_IndependentVulkanSession.cpp)
target_link_libraries(tst_independent_vulkan_session PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_dependencies(tst_independent_vulkan_session sge_vulkan_offscreen)
add_test(NAME tst_independent_vulkan_session COMMAND tst_independent_vulkan_session)
set_tests_properties(tst_independent_vulkan_session PROPERTIES
    LABELS "unit"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

qt_add_executable(tst_independent_frame_loop unit/tst_IndependentFrameLoop.cpp)
target_link_libraries(tst_independent_frame_loop PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_dependencies(tst_independent_frame_loop sge_vulkan_offscreen)
add_test(NAME tst_independent_frame_loop COMMAND tst_independent_frame_loop)
set_tests_properties(tst_independent_frame_loop PROPERTIES
    LABELS "unit"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

qt_add_executable(tst_shell_root_integration unit/tst_ShellRootIntegration.cpp)
target_link_libraries(tst_shell_root_integration PRIVATE
    sge_drm_poc
    -Wl,--whole-archive sge_core -Wl,--no-whole-archive
    Qt6::Test
)
add_dependencies(tst_shell_root_integration sge_vulkan_offscreen)
add_test(NAME tst_shell_root_integration COMMAND tst_shell_root_integration)
set_tests_properties(tst_shell_root_integration PROPERTIES
    LABELS "integration"
    TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

qt_add_executable(tst_unified_vulkan_gbm_pool unit/tst_UnifiedVulkanGbmPool.cpp)
target_link_libraries(tst_unified_vulkan_gbm_pool PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_dependencies(tst_unified_vulkan_gbm_pool sge_vulkan_offscreen)
add_test(NAME tst_unified_vulkan_gbm_pool COMMAND tst_unified_vulkan_gbm_pool)
set_tests_properties(tst_unified_vulkan_gbm_pool PROPERTIES
    LABELS "integration"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

qt_add_executable(tst_vulkan_kms_sync unit/tst_VulkanKmsSync.cpp)
target_link_libraries(tst_vulkan_kms_sync PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_dependencies(tst_vulkan_kms_sync sge_vulkan_offscreen)
add_test(NAME tst_vulkan_kms_sync COMMAND tst_vulkan_kms_sync)
set_tests_properties(tst_vulkan_kms_sync PROPERTIES
    LABELS "integration"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

qt_add_executable(tst_page_flip_and_unified_startup unit/tst_PageFlipAndUnifiedStartup.cpp)
target_link_libraries(tst_page_flip_and_unified_startup PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_dependencies(tst_page_flip_and_unified_startup sge_vulkan_offscreen)
add_test(NAME tst_page_flip_and_unified_startup COMMAND tst_page_flip_and_unified_startup)
set_tests_properties(tst_page_flip_and_unified_startup PROPERTIES
    LABELS "integration"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

qt_add_executable(tst_device_lost_and_compositor_lifecycle unit/tst_DeviceLostAndCompositorLifecycle.cpp)
target_link_libraries(tst_device_lost_and_compositor_lifecycle PRIVATE
    sge_drm_poc
    -Wl,--whole-archive sge_core -Wl,--no-whole-archive
    Qt6::Test
)
add_dependencies(tst_device_lost_and_compositor_lifecycle sge_vulkan_offscreen)
add_test(NAME tst_device_lost_and_compositor_lifecycle COMMAND tst_device_lost_and_compositor_lifecycle)
set_tests_properties(tst_device_lost_and_compositor_lifecycle PROPERTIES
    LABELS "integration"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

qt_add_executable(tst_vulkan_dmabuf_audit unit/tst_VulkanDmabufAudit.cpp)
target_link_libraries(tst_vulkan_dmabuf_audit PRIVATE
    sge_drm_poc
    Qt6::Test
)
add_dependencies(tst_vulkan_dmabuf_audit sge_vulkan_offscreen)
add_test(NAME tst_vulkan_dmabuf_audit COMMAND tst_vulkan_dmabuf_audit)
set_tests_properties(tst_vulkan_dmabuf_audit PROPERTIES
    LABELS "integration"
    TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=sge_vulkan_offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)

if(SGE_ENABLE_WAYLIB_BACKEND)
    qt_add_executable(tst_waylib_test_client compositor/tst_WaylibTestClient.cpp)
    target_link_libraries(tst_waylib_test_client PRIVATE Qt6::Gui)

    qt_add_executable(tst_waylib_backend compositor/tst_WaylibBackend.cpp)
    qt_add_resources(tst_waylib_backend "sge_waylib_test_qml"
        PREFIX "/systalia/waylib"
        BASE "${CMAKE_SOURCE_DIR}/src/qml/waylib"
        FILES "${CMAKE_SOURCE_DIR}/src/qml/waylib/WaylibNestedShell.qml"
    )
    target_link_libraries(tst_waylib_backend PRIVATE
        sge_waylib_backend
        Qt6::Test
    )
    target_compile_definitions(tst_waylib_backend PRIVATE
        SGE_WAYLIB_QML_IMPORT_PATH="${SGE_WAYLIB_QML_IMPORT_PATH}"
    )
    add_dependencies(tst_waylib_backend tst_waylib_test_client)
    add_test(NAME tst_waylib_backend COMMAND tst_waylib_backend)
    set_tests_properties(tst_waylib_backend PROPERTIES
        LABELS "integration"
        TIMEOUT 30
    )
endif()
