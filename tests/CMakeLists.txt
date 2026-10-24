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

