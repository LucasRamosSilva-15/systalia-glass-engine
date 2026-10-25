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
qt_add_executable(tst_systalia_protocol tst_SystaliaProtocol.cpp)
qt6_generate_wayland_protocol_client_sources(tst_systalia_protocol
    FILES "${PROJECT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml"
)
target_link_libraries(tst_systalia_protocol PRIVATE
    Qt6::Test Qt6::Quick Qt6::WaylandClient Wayland::Client Wayland::Server sge_core
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
)
set_tests_properties(${_sge_headless_tests} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
set_tests_properties(tst_xdg_resize PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "${_tst_xdg_resize_environment}"
)
