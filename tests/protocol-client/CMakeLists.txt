qt_add_library(sge_test_protocol_client STATIC)
qt6_generate_wayland_protocol_client_sources(sge_test_protocol_client
    FILES "${PROJECT_SOURCE_DIR}/protocols/z_systalia_surface_v1.xml"
)
target_include_directories(sge_test_protocol_client PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)
target_link_libraries(sge_test_protocol_client PUBLIC
    Qt6::Core
    Qt6::WaylandClient
    Wayland::Client
)
