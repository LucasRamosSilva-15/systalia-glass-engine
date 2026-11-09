import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Systalia.Engine
import Systalia.System 1.0
import "../graphics/v2" as GfxV2

/**
 * SystaliaFilesWindowV2.qml — Janela de Arquivos Oficial VALO Graphics 2.0 (Etapa 05)
 *
 * Reconstrução da Files Window com geometria Squircle G2 e motor óptico analítico:
 * - Chassi de peça única em Squircle G2 (superelipse L4 analítica com raio de 20px).
 * - Sombra externa contínua com decaimento suave (sigma 20px, offset Y 16px).
 * - Vidro oceânico volumétrico com refração cromática de Cauchy e Dual-Kawase GPU.
 * - Barra de título integrada com controles funcionais (minimizar, maximizar, fechar, arrastar).
 * - Barra lateral de navegação com estados selecionados calibrados conforme Figma.
 * - Grade de arquivos com os ícones oficiais em acrílico 3D do conceito VALO.
 */
Item {
    id: fileWindow

    // =========================================================================
    // 1. CONTRATOS FUNCIONAIS E INTEGRAÇÃO DE JANELA
    // =========================================================================
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0
    property var controller: null
    property var waylandServer: null
    property real refractiveIndex: 1.25
    property real dispersion: 0.040
    property real caustics: 0.18
    property real surfaceOpacity: 0.48
    property real surfaceSlant: 0
    property real cornerRadius: 20
    property int snapState: 0
    property rect restoreGeometry: Qt.rect(113, 193, 701, 534)
    property vector4d cornerRadii: snapState !== 0
                                   ? LiquidTiling.calculateFusedRadii(snapState, cornerRadius)
                                   : Qt.vector4d(cornerRadius, cornerRadius, cornerRadius, cornerRadius)
    property bool materialActive: true
    property bool windowOpen: true
    property string selectedLocation: "Home"
    property string selectedFile: ""

    // Perfis gráficos: 0 = Performance, 1 = Balanced, 2 = Quality
    property int blurProfile: 2

    // Acessibilidade
    property bool highContrast: false
    property bool prefersReducedMotion: false
    property alias debugPass: glassChassis.debugPass

    width: 701
    height: 534
    visible: true
    clip: false

    // =========================================================================
    // 2. CHASSI ÓPTICO UNIFICADO VALO V2 (Squircle G2, Borda e Sombra analíticas)
    // =========================================================================
    GfxV2.ValoMaterialSurfaceV2 {
        id: glassChassis
        anchors.fill: parent
        geometryMode: GfxV2.ValoGlassItemV2.SquircleG2
        cornerRadii: fileWindow.cornerRadii
        cornerRadius: fileWindow.cornerRadius
        specularStrokeWidth: 1.25
        surfaceOpacity: fileWindow.materialActive
                        ? (fileWindow.highContrast ? 0.88 : fileWindow.surfaceOpacity)
                        : 0.0
        volumetricTint: fileWindow.highContrast
                        ? Qt.rgba(0.01, 0.06, 0.12, 0.94)
                        : Qt.rgba(0.025, 0.12, 0.22, 0.52)
        shadowOffsetY: 16.0
        shadowIntensity: fileWindow.highContrast ? 0.0 : 0.38
        blurProfile: fileWindow.blurProfile
        backdropSource: fileWindow.backdropSource
        highContrast: fileWindow.highContrast
        prefersReducedMotion: fileWindow.prefersReducedMotion
        active: fileWindow.visible && fileWindow.materialActive
        z: 0

        // =====================================================================
        // 3. BARRA DE TÍTULO INTEGRADA COM CONTROLES DA JANELA
        // =====================================================================
        SystaliaWindowHeader {
            id: windowHeader
            objectName: "filesWindowHeader"
            width: parent.width
            height: implicitHeight
            iconGlyph: "▰"
            title: "Systalia Files"
            maximized: fileWindow.controller ? fileWindow.controller.filesWindowMaximized : false
            glassSurface: false
            actionsRightMargin: 18.5
            z: 20

            onMinimizeRequested: {
                if (fileWindow.controller)
                    fileWindow.controller.minimizeFileWindow()
                else
                    fileWindow.visible = false
            }
            onMaximizeRequested: {
                if (fileWindow.controller)
                    fileWindow.controller.toggleFileWindowMaximized()
            }
            onCloseRequested: {
                if (fileWindow.controller)
                    fileWindow.controller.closeFileWindow()
                else
                    fileWindow.visible = false
            }
        }

        // Área invisível de arrasto da janela na barra de título
        Item {
            x: 0
            y: 0
            width: Math.max(200, fileWindow.width - 130)
            height: 49
            z: 25

            DragHandler {
                id: fileWindowDrag
                target: fileWindow
                enabled: fileWindow.controller ? !fileWindow.controller.filesWindowMaximized : true
                onActiveChanged: {
                    if (active) {
                        if (fileWindow.controller) {
                            fileWindow.controller.raiseFileWindow()
                            if (fileWindow.snapState !== 0
                                    && typeof fileWindow.controller.unsnapShellWindow === "function") {
                                fileWindow.controller.unsnapShellWindow(fileWindow)
                            }
                        }
                    } else if (fileWindow.controller
                               && typeof fileWindow.controller.finishShellWindowDrag === "function") {
                        fileWindow.controller.finishShellWindowDrag(
                            fileWindow, fileWindowDrag.centroid.scenePosition)
                    }
                }
                onCentroidChanged: {
                    if (active && fileWindow.controller
                            && typeof fileWindow.controller.updateShellWindowDrag === "function") {
                        fileWindow.controller.updateShellWindowDrag(
                            fileWindow, fileWindowDrag.centroid.scenePosition)
                    }
                }
            }
            TapHandler {
                onTapped: if (fileWindow.controller) fileWindow.controller.raiseFileWindow()
            }
        }

        // =====================================================================
        // 4. BARRA LATERAL DE NAVEGAÇÃO (Sidebar com Acrílico Translúcido)
        // =====================================================================
        Rectangle {
            id: sidebarBackground
            x: 10
            y: 51
            width: 181
            height: Math.max(469, parent.height - 65)
            radius: 14
            color: fileWindow.highContrast ? "#f0081a2d" : "#c0081a2d"
            border.color: fileWindow.highContrast ? "#40ffffff" : "#187ce9f4"
            border.width: 0.8
            antialiasing: true
            z: 10
        }

        Repeater {
            model: [
                { name: "Recent", icon: "valo-sidebar-recent.svg", top: 16 },
                { name: "Home", icon: "valo-sidebar-home.svg", top: 50 },
                { name: "Desktop", icon: "valo-sidebar-desktop.svg", top: 84 },
                { name: "Documents", icon: "valo-sidebar-documents.svg", top: 118 },
                { name: "Downloads", icon: "valo-sidebar-downloads.svg", top: 152 },
                { name: "Music", icon: "valo-sidebar-music.svg", top: 186 },
                { name: "Pictures", icon: "valo-sidebar-pictures.svg", top: 220 },
                { name: "Trash", icon: "valo-sidebar-trash.svg", top: 254 },
                { name: "Other Items", icon: "valo-sidebar-other.svg", top: 306 }
            ]

            delegate: Item {
                id: navigationEntry
                required property var modelData
                x: 19
                y: 51 + modelData.top - 6
                width: 163
                height: 33
                z: 15

                // Fundo do item selecionado (estilo Figma: gradiente 135deg, inset highlights)
                Item {
                    anchors.fill: parent
                    visible: fileWindow.selectedLocation === navigationEntry.modelData.name

                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 2
                        radius: 7
                        color: "#5900101d"
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 7
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#3d9fefff" }
                            GradientStop { position: 0.3; color: "#6e5bbce5" }
                            GradientStop { position: 0.6; color: "#b819587d" }
                            GradientStop { position: 1.0; color: "#e010334e" }
                        }
                        border.color: "#40ffffff"
                        border.width: 0.8
                        antialiasing: true
                    }
                }

                Image {
                    id: navIcon
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + navigationEntry.modelData.icon
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    antialiasing: true
                }

                Text {
                    x: 38
                    anchors.verticalCenter: parent.verticalCenter
                    width: 116
                    text: navigationEntry.modelData.name
                    color: fileWindow.selectedLocation === navigationEntry.modelData.name ? "#eef9ff" : "#d5e2ee"
                    font.family: ValoTheme.fontFamily
                    font.pixelSize: 15
                    font.weight: fileWindow.selectedLocation === navigationEntry.modelData.name ? Font.Medium : Font.Normal
                    elide: Text.ElideRight
                    antialiasing: true
                }

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: fileWindow.selectedLocation = navigationEntry.modelData.name
                }
            }
        }

        // =====================================================================
        // 5. BREADCRUMB, BUSCA E FERRAMENTAS DE NAVEGAÇÃO
        // =====================================================================
        Row {
            x: 207
            y: 60
            spacing: 12
            z: 15

            Text {
                text: "‹"
                color: "#eb8cf1ff"
                font.family: ValoTheme.fontFamily
                font.pixelSize: 26
                font.weight: Font.Bold
                antialiasing: true
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }

            Text {
                text: "›"
                color: "#668cf1ff"
                font.family: ValoTheme.fontFamily
                font.pixelSize: 26
                font.weight: Font.Bold
                antialiasing: true
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }

        // Indicador de pasta atual no breadcrumb
        Rectangle {
            objectName: "filesBreadcrumb"
            x: 275
            y: 59
            width: Math.max(90, breadcrumbRow.implicitWidth + 24)
            height: 30
            radius: 7
            color: "#d9103852"
            border.color: "#28ffffff"
            border.width: 0.8
            antialiasing: true
            z: 15

            Row {
                id: breadcrumbRow
                anchors.centerIn: parent
                spacing: 8

                Image {
                    source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-breadcrumb-home.svg"
                    width: 12
                    height: 13
                    anchors.verticalCenter: parent.verticalCenter
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                Text {
                    id: folderNameText
                    text: fileWindow.selectedLocation
                    color: "#e6fbff"
                    font.family: ValoTheme.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    anchors.verticalCenter: parent.verticalCenter
                    antialiasing: true
                }
            }
        }

        // Campo de busca
        TextField {
            id: searchField
            objectName: "filesSearch"
            anchors.right: parent.right
            anchors.rightMargin: 18
            y: 61
            width: 157
            height: 28
            placeholderText: "⌕ Search"
            color: ValoTheme.textPrimary
            placeholderTextColor: "#bdcfdc"
            font.family: ValoTheme.fontFamily
            font.pixelSize: 15
            leftPadding: 14
            topPadding: 0
            bottomPadding: 0
            verticalAlignment: TextInput.AlignVCenter
            z: 15
            background: Rectangle {
                radius: 14
                color: "#ba0a2535"
                border.color: "#26ffffff"
                border.width: 0.8
                antialiasing: true
            }
            selectionColor: "#7d42e9ee"
        }

        // Alternador de visualização grade/lista
        Image {
            id: filesMenuButton
            objectName: "filesMenuButton"
            anchors.right: searchField.left
            anchors.rightMargin: 10
            y: 61
            width: 24
            height: 24
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-files-menu.svg"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            z: 15
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }

        // Linha divisória horizontal
        Rectangle {
            x: 208
            y: 97
            width: Math.max(0, parent.width - 226)
            height: 1
            color: "#4db2f3fd"
            antialiasing: true
            z: 12
        }

        // =====================================================================
        // 6. GRADE PRINCIPAL DE ARQUIVOS (Ícones Oficiais do Conceito Figma)
        // =====================================================================
        GridView {
            id: filesGrid
            objectName: "filesGrid"
            x: 216
            y: 115
            width: Math.max(0, parent.width - 235)
            height: Math.max(400, parent.height - 130)
            cellWidth: 118
            cellHeight: 120
            clip: true
            interactive: true
            boundsBehavior: Flickable.StopAtBounds
            z: 15

            model: [
                { name: "All Files", icon: "valo-file-folder.svg" },
                { name: "Documents", icon: "valo-file-documents-art.svg" },
                { name: "Document", icon: "valo-file-document.svg" },
                { name: "Download2", icon: "valo-file-download.svg" },
                { name: "Divides 1", icon: "valo-file-list.svg" },
                { name: "Systion 4", icon: "valo-file-check.svg" }
            ]

            delegate: Item {
                id: fileEntry
                required property var modelData
                width: filesGrid.cellWidth
                height: filesGrid.cellHeight

                Rectangle {
                    anchors.centerIn: parent
                    width: 96
                    height: 106
                    visible: fileWindow.selectedFile === fileEntry.modelData.name
                    radius: 8
                    color: "#182ae0ff"
                    border.color: "#507ce9f4"
                    border.width: 1
                    antialiasing: true
                }

                Item {
                    anchors.centerIn: parent
                    width: 96
                    height: 106

                    Image {
                        id: fileIconImg
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: fileEntry.modelData.name === "All Files" ? 2 : 0
                        width: fileEntry.modelData.name === "All Files" ? 76 : 58
                        height: 70
                        source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/" + fileEntry.modelData.icon
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        antialiasing: true
                    }

                    Text {
                        x: 0
                        y: 76
                        width: 96
                        height: 26
                        text: fileEntry.modelData.name
                        color: "#eefcff"
                        font.family: ValoTheme.fontFamily
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        antialiasing: true
                    }
                }

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: fileWindow.selectedFile = fileEntry.modelData.name }
            }
        }
    }
}
