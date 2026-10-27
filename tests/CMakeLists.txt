find_package(Qt6 REQUIRED COMPONENTS Test)

# Teste 1: Memória std140 e Bytecodes SPIR-V (.qsb)
qt_add_executable(tst_systalia_engine
    tst_SystaliaEngine.cpp
)
target_link_libraries(tst_systalia_engine PRIVATE
    Qt6::Core
    Qt6::Test
    sge_core
    sge_shaders
)
add_test(NAME tst_systalia_engine COMMAND tst_systalia_engine)

# Teste 2: Geometria de Vértices e Propriedades do SystaliaGlassItem
qt_add_executable(tst_item_geometry
    tst_ItemGeometry.cpp
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
    tst_SdfGeometry.cpp
)
target_link_libraries(tst_sdf_geometry PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME tst_sdf_geometry COMMAND tst_sdf_geometry)

# Teste 4: Vinculação de Backdrop e Resolução Screen-Space
qt_add_executable(tst_backdrop_binding
    tst_BackdropBinding.cpp
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
    tst_OpticalPhysics.cpp
)
target_link_libraries(tst_optical_physics PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME tst_optical_physics COMMAND tst_optical_physics)

# Teste 6: Criação, Ciclo de Vida e IPC do Socket Wayland (wayland-systalia-0)
qt_add_executable(tst_wayland_socket
    tst_WaylandSocket.cpp
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
    tst_XdgShell.cpp
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
    tst_XdgDecorations.cpp
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
qt_add_executable(tst_xdg_resize tst_XdgResize.cpp)
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
qt_add_executable(tst_systalia_protocol tst_SystaliaProtocol.cpp)
target_link_libraries(tst_systalia_protocol PRIVATE
    Qt6::Test Qt6::Quick Qt6::WaylandClient Wayland::Server sge_core sge_test_protocol_client
)
add_test(NAME tst_systalia_protocol COMMAND tst_systalia_protocol)
set_tests_properties(tst_systalia_protocol PROPERTIES
    TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Teste 11: composição do cliente, offsets std140 e recorte interno G2.
qt_add_executable(tst_unified_composition tst_UnifiedComposition.cpp)
target_link_libraries(tst_unified_composition PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    sge_core
)
add_test(NAME tst_unified_composition COMMAND tst_unified_composition)

# Teste 12: parser de /proc/stat, memória real e formato do relógio do sistema.
qt_add_executable(tst_telemetry tst_Telemetry.cpp)
target_link_libraries(tst_telemetry PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    sge_core
)
add_test(NAME tst_telemetry COMMAND tst_telemetry)

# Teste 13: ambiente Wayland e ciclo de vida de processos desacoplados.
qt_add_executable(tst_process_launcher tst_ProcessLauncher.cpp)
target_link_libraries(tst_process_launcher PRIVATE
    Qt6::Core
    Qt6::Test
    sge_core
)
add_test(NAME tst_process_launcher COMMAND tst_process_launcher)

# Test 14: parser .desktop, filtragem da lista de aplicações e notificações do modelo.
qt_add_executable(tst_app_registry tst_AppRegistry.cpp)
target_link_libraries(tst_app_registry PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    sge_core
)
add_test(NAME tst_app_registry COMMAND tst_app_registry)

# Test 15: ciclo de vida, substituição, expiração e sinais D-Bus de notificações.
qt_add_executable(tst_notification_server tst_NotificationServer.cpp)
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
        sge_core
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
