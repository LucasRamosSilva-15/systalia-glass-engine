# Executável principal da aplicação e regras de instalação para sessão autônoma Wayland / SDDM
qt_add_executable(systalia_glass_engine
    src/app/main.cpp
)

target_link_libraries(systalia_glass_engine PRIVATE
    sge_core
    Qt6::QuickControls2
    Qt6::WaylandCompositor
    Qt6::DBus
)

# Shared Qt loads QML plugins at runtime; static builds still need import scanning.
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "SHARED_LIBRARY")
    set_property(TARGET systalia_glass_engine PROPERTY QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/systalia-session.in"
    "${CMAKE_CURRENT_BINARY_DIR}/data/systalia-session"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/systalia.desktop.in"
    "${CMAKE_CURRENT_BINARY_DIR}/data/systalia.desktop"
    @ONLY
)

install(TARGETS systalia_glass_engine
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/data/systalia-session"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/data/systalia.desktop"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/eglfs_kms.json"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/systalia
)
