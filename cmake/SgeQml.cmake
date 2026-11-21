# Módulo QML Systalia.Engine 1.0, singletons, recursos Figma e preservação de aliases QRC
set_source_files_properties(src/theme/compatibility/v1/SystaliaTheme.qml src/theme/qml/ValoTheme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

file(GLOB sge_valo_assets RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "assets/figma/*.svg"
    "assets/figma/*.png"
)

foreach(asset IN LISTS sge_valo_assets)
    get_filename_component(asset_name "${asset}" NAME)
    set_source_files_properties(${asset} PROPERTIES QT_RESOURCE_ALIAS "src/qml/assets/figma/${asset_name}")
endforeach()

# Preserva aliases QRC públicos (:/qt/qml/Systalia/Engine/src/qml/...) após reorganização modular
set_source_files_properties(src/shell/main.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/main.qml")
set_source_files_properties(src/shell/WorkspaceViewport.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WorkspaceViewport.qml")
set_source_files_properties(src/shell/WaylandClientView.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WaylandClientView.qml")
set_source_files_properties(src/shell/WaylandWindowContainer.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WaylandWindowContainer.qml")

set_source_files_properties(src/theme/compatibility/v1/SystaliaTheme.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/theme/SystaliaTheme.qml")
set_source_files_properties(src/theme/qml/ValoTheme.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/theme/ValoTheme.qml")

foreach(ctrl_qml IN ITEMS
    SystaliaG2Frame.qml
    SystaliaWindowAction.qml
    SystaliaWindowHeader.qml
    ValoDockItem.qml
)
    set_source_files_properties("src/controls/${ctrl_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/${ctrl_qml}")
endforeach()

foreach(shell_qml IN ITEMS
    SystaliaTopBar.qml
    SystaliaDock.qml
    SystaliaFilesWindow.qml
    SystaliaMaterialWindow.qml
    SystaliaFlowMenu.qml
    SystaliaNotificationHost.qml
    SystaliaControlCenter.qml
    SystaliaTaskSwitcher.qml
    SnapPreviewOverlay.qml
    TileSplitter.qml
    SystaliaDockV2.qml
    SystaliaTopBarV2.qml
    SystaliaFilesWindowV2.qml
    SystaliaControlCenterV2.qml
)
    set_source_files_properties("src/shell/${shell_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/${shell_qml}")
endforeach()

foreach(gfx_qml IN ITEMS
    ValoMaterialSurfaceV2.qml
    ValoInternalWindowBackdropV2.qml
    ValoSingleWindowBackdropV2.qml
    ValoLimitedBackdropV2.qml
    ValoOpticalParticlesV2.qml
    ValoSurfaceV2.qml
    ValoDockItemV2.qml
    ValoDockV2.qml
    ValoDockV2Scene.qml
)
    set_source_files_properties("src/graphics/v2/qml/${gfx_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/graphics/v2/${gfx_qml}")
endforeach()

qt_add_qml_module(sge_core
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Systalia/Engine"
    URI Systalia.Engine
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        src/shell/main.qml
        src/shell/WorkspaceViewport.qml
        src/theme/compatibility/v1/SystaliaTheme.qml
        src/theme/qml/ValoTheme.qml
        src/shell/WaylandClientView.qml
        src/shell/WaylandWindowContainer.qml
        src/shell/SystaliaTopBar.qml
        src/shell/SystaliaDock.qml
        src/controls/ValoDockItem.qml
        src/controls/SystaliaG2Frame.qml
        src/controls/SystaliaWindowAction.qml
        src/controls/SystaliaWindowHeader.qml
        src/shell/SystaliaFilesWindow.qml
        src/shell/SystaliaMaterialWindow.qml
        src/shell/SystaliaFlowMenu.qml
        src/shell/SystaliaNotificationHost.qml
        src/shell/SystaliaControlCenter.qml
        src/shell/SystaliaTaskSwitcher.qml
        src/shell/SnapPreviewOverlay.qml
        src/shell/TileSplitter.qml
        src/shell/SystaliaDockV2.qml
        src/shell/SystaliaTopBarV2.qml
        src/shell/SystaliaFilesWindowV2.qml
        src/shell/SystaliaControlCenterV2.qml
        src/graphics/v2/qml/ValoMaterialSurfaceV2.qml
        src/graphics/v2/qml/ValoInternalWindowBackdropV2.qml
        src/graphics/v2/qml/ValoSingleWindowBackdropV2.qml
        src/graphics/v2/qml/ValoLimitedBackdropV2.qml
        src/graphics/v2/qml/ValoOpticalParticlesV2.qml
        src/graphics/v2/qml/ValoSurfaceV2.qml
        src/graphics/v2/qml/ValoDockItemV2.qml
        src/graphics/v2/qml/ValoDockV2.qml
        src/graphics/v2/qml/ValoDockV2Scene.qml
    RESOURCES
        ${sge_valo_assets}
    NO_PLUGIN
)
