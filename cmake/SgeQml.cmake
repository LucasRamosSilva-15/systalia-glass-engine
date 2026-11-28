# Módulo QML Systalia.Engine 1.0, singletons, recursos Figma e preservação de aliases QRC
set_source_files_properties(src/theme/qml/ValoTheme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

file(GLOB sge_valo_assets RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "assets/figma/*.svg"
)

foreach(asset IN LISTS sge_valo_assets)
    get_filename_component(asset_name "${asset}" NAME)
    set_source_files_properties(${asset} PROPERTIES QT_RESOURCE_ALIAS "src/qml/assets/figma/${asset_name}")
endforeach()

# Preserva aliases QRC públicos (:/qt/qml/Systalia/Engine/src/qml/...) após reorganização hierárquica
set_source_files_properties(src/shell/core/main.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/main.qml")
set_source_files_properties(src/shell/core/ShellRoot.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/ShellRoot.qml")
set_source_files_properties(src/shell/core/WorkspaceViewport.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WorkspaceViewport.qml")
set_source_files_properties(src/shell/core/WaylandClientView.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WaylandClientView.qml")
set_source_files_properties(src/shell/core/WaylandWindowContainer.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/WaylandWindowContainer.qml")

set_source_files_properties(src/theme/qml/ValoTheme.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/theme/ValoTheme.qml")

set_source_files_properties(src/controls/frames/SystaliaG2Frame.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaG2Frame.qml")
set_source_files_properties(src/controls/windows/SystaliaWindowAction.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaWindowAction.qml")
set_source_files_properties(src/controls/windows/SystaliaWindowHeader.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaWindowHeader.qml")
set_source_files_properties(src/controls/dock/ValoDockItem.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/ValoDockItem.qml")

set_source_files_properties(src/shell/dock/SystaliaDockV2.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaDockV2.qml")
set_source_files_properties(src/shell/topbar/SystaliaTopBarV2.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaTopBarV2.qml")
set_source_files_properties(src/shell/windows/SystaliaFilesWindowV2.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaFilesWindowV2.qml")
set_source_files_properties(src/shell/windows/SystaliaMaterialWindow.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaMaterialWindow.qml")
set_source_files_properties(src/shell/panels/SystaliaControlCenterV2.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/SystaliaControlCenterV2.qml")

foreach(overlay_qml IN ITEMS
    SnapPreviewOverlay.qml
    SystaliaFlowMenu.qml
    SystaliaNotificationHost.qml
    SystaliaTaskSwitcher.qml
    TileSplitter.qml
)
    set_source_files_properties("src/shell/overlays/${overlay_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/shell/${overlay_qml}")
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
    ValoSoftwareCursor.qml
)
    set_source_files_properties("src/graphics/v2/qml/${gfx_qml}" PROPERTIES QT_RESOURCE_ALIAS "src/qml/graphics/v2/${gfx_qml}")
endforeach()

qt_add_qml_module(sge_core
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Systalia/Engine"
    URI Systalia.Engine
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        src/shell/core/main.qml
        src/shell/core/ShellRoot.qml
        src/shell/core/WorkspaceViewport.qml
        src/theme/qml/ValoTheme.qml
        src/shell/core/WaylandClientView.qml
        src/shell/core/WaylandWindowContainer.qml
        src/controls/dock/ValoDockItem.qml
        src/controls/frames/SystaliaG2Frame.qml
        src/controls/windows/SystaliaWindowAction.qml
        src/controls/windows/SystaliaWindowHeader.qml
        src/shell/windows/SystaliaMaterialWindow.qml
        src/shell/overlays/SystaliaFlowMenu.qml
        src/shell/overlays/SystaliaNotificationHost.qml
        src/shell/overlays/SystaliaTaskSwitcher.qml
        src/shell/overlays/SnapPreviewOverlay.qml
        src/shell/overlays/TileSplitter.qml
        src/shell/dock/SystaliaDockV2.qml
        src/shell/topbar/SystaliaTopBarV2.qml
        src/shell/windows/SystaliaFilesWindowV2.qml
        src/shell/panels/SystaliaControlCenterV2.qml
        src/graphics/v2/qml/ValoMaterialSurfaceV2.qml
        src/graphics/v2/qml/ValoInternalWindowBackdropV2.qml
        src/graphics/v2/qml/ValoSingleWindowBackdropV2.qml
        src/graphics/v2/qml/ValoLimitedBackdropV2.qml
        src/graphics/v2/qml/ValoOpticalParticlesV2.qml
        src/graphics/v2/qml/ValoSurfaceV2.qml
        src/graphics/v2/qml/ValoDockItemV2.qml
        src/graphics/v2/qml/ValoDockV2.qml
        src/graphics/v2/qml/ValoDockV2Scene.qml
        src/graphics/v2/qml/ValoSoftwareCursor.qml
    RESOURCES
        ${sge_valo_assets}
    NO_PLUGIN
)
