import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor
import Systalia.Engine
import Systalia.Compositor 1.0

ApplicationWindow {
    id: rootWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("Systalia Glass Engine (SGE) — Etapa 2.2: Suporte XDG Shell & Hospedagem Wayland")
    color: "#020611"

    // 0. Servidor Wayland Nativo (IPC Socket wayland-systalia-0 com suporte XDG Shell)
    SystaliaCompositor {
        id: waylandServer
        property string activeClientTitle: ""
        property string activeClientAppId: ""

        onToplevelCreated: (toplevel) => {
            if (toplevel) {
                activeClientTitle = toplevel.title
                activeClientAppId = toplevel.appId
                toplevel.titleChanged.connect(() => { activeClientTitle = toplevel.title })
                toplevel.appIdChanged.connect(() => { activeClientAppId = toplevel.appId })

                const comp = Qt.createComponent("WaylandWindowContainer.qml")
                if (comp.status === Component.Ready) {
                    const offset = (clientWindowsLayer.children.length * 36) % 180
                    comp.createObject(clientWindowsLayer, {
                        toplevel: toplevel,
                        waylandServer: waylandServer,
                        backdropSource: backdropGrab,
                        screenResolution: Qt.point(rootWindow.width * rootWindow.devicePixelRatio,
                                                   rootWindow.height * rootWindow.devicePixelRatio),
                        animTime: animTimer.tick,
                        x: Math.round(160 + offset),
                        y: Math.round(80 + offset)
                    })
                } else if (comp.status === Component.Error) {
                    console.error("Erro ao instanciar WaylandWindowContainer:", comp.errorString())
                }
            }
        }

        Component.onCompleted: {
            create()
        }
    }

    // Saída Wayland Padrão (notifica geometria e escala aos clientes)
    WaylandOutput {
        compositor: waylandServer
        window: rootWindow
    }

    Timer {
        id: animTimer
        interval: 16
        running: true
        repeat: true
        property real tick: 0.0
        onTriggered: tick += 0.02
    }

    // =========================================================================
    // 1. CENA DE FUNDO OCEÂNICO PROFUNDO (Abyssal Backdrop com Luz e Partículas)
    // =========================================================================
    Item {
        id: backgroundScene
        anchors.fill: parent

        // Gradiente abissal profundo
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.00; color: "#020611" }
                GradientStop { position: 0.45; color: "#041528" }
                GradientStop { position: 1.00; color: "#072644" }
            }
        }

        // Raios de luz volumétrica subaquática (Sunbeams em ângulo)
        Item {
            anchors.fill: parent
            opacity: 0.35

            Rectangle {
                x: 100 + Math.sin(animTimer.tick * 0.4) * 40
                y: -100
                width: 180
                height: 1100
                rotation: -28
                transformOrigin: Item.TopLeft
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#4000f2fe" }
                    GradientStop { position: 0.5; color: "#1500d4ff" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            Rectangle {
                x: 420 + Math.cos(animTimer.tick * 0.3) * 50
                y: -100
                width: 260
                height: 1100
                rotation: -24
                transformOrigin: Item.TopLeft
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#3538bdf8" }
                    GradientStop { position: 0.6; color: "#100284c7" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            Rectangle {
                x: 820 - Math.sin(animTimer.tick * 0.35) * 45
                y: -100
                width: 220
                height: 1100
                rotation: -22
                transformOrigin: Item.TopLeft
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#2500f2fe" }
                    GradientStop { position: 0.7; color: "#080369a1" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }

        // Grade analítica sutil de referência espacial
        Grid {
            anchors.fill: parent
            rows: 20
            columns: 32
            opacity: 0.12

            Repeater {
                model: 20 * 32
                Rectangle {
                    width: rootWindow.width / 32
                    height: rootWindow.height / 20
                    color: "transparent"
                    border.color: "#38bdf8"
                    border.width: 1
                }
            }
        }

        // Padrão de calibração de alto contraste (barras finas que cruzam a borda do vidro)
        Row {
            x: 90
            y: 50
            spacing: 10
            Repeater {
                model: 18
                Rectangle {
                    width: 5
                    height: 280
                    color: (index % 2 === 0) ? "#00f2fe" : "#ff007f"
                    opacity: 0.85
                }
            }
        }

        // Orbe luminoso 1: Magenta/Rosa profundo em órbita suave (cruzando a lateral esquerda da janela)
        Rectangle {
            x: 110 + Math.cos(animTimer.tick * 0.6) * 80
            y: 200 + Math.sin(animTimer.tick * 0.5) * 60
            width: 300
            height: 300
            radius: 150
            opacity: 0.75
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#ff007f" }
                GradientStop { position: 1.0; color: "#7928ca" }
            }
        }

        // Orbe luminoso 2: Ciano/Turquesa oceânico cruzando o canto inferior direito
        Rectangle {
            x: 800 - Math.sin(animTimer.tick * 0.5) * 90
            y: 380 - Math.cos(animTimer.tick * 0.7) * 70
            width: 360
            height: 360
            radius: 180
            opacity: 0.70
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00f2fe" }
                GradientStop { position: 1.0; color: "#0284c7" }
            }
        }

        // Partículas oceânicas em suspensão (Ambient underwater bioluminescent specks)
        Repeater {
            model: 20
            Rectangle {
                property real seedX: ((index * 67) % 1200) + 40
                property real seedY: ((index * 43) % 720) + 40
                property real speed: 0.3 + (index % 5) * 0.15
                property real pSize: 3 + (index % 4) * 2

                x: seedX + Math.sin(animTimer.tick * speed + index) * 35
                y: seedY + Math.cos(animTimer.tick * (speed * 0.8) + index * 2) * 25
                width: pSize
                height: pSize
                radius: pSize * 0.5
                color: (index % 3 === 0) ? "#ffffff" : ((index % 3 === 1) ? "#38bdf8" : "#00f2fe")
                opacity: 0.25 + 0.35 * Math.sin(animTimer.tick * 1.5 + index)
            }
        }
    }

    // =========================================================================
    // 2. CAPTURA ATIVA DO BACKDROP (FBO Screen-Space)
    // =========================================================================
    ShaderEffectSource {
        id: backdropGrab
        anchors.fill: backgroundScene
        sourceItem: backgroundScene
        textureSize: Qt.size(Math.max(1, rootWindow.width * rootWindow.devicePixelRatio),
                             Math.max(1, rootWindow.height * rootWindow.devicePixelRatio))
        live: true
        hideSource: false
        visible: false
        recursive: false
        format: ShaderEffectSource.RGBA8
    }

    // =========================================================================
    // 3. CHASSI DE CRISTAL DE ÁGUA LÍQUIDA (Systalia Glass Item nativo QRhi)
    // =========================================================================
    SystaliaGlassItem {
        id: glassChassis
        width: 920
        height: 600
        x: Math.round((rootWindow.width - width) / 2)
        y: Math.round((rootWindow.height - height) / 2)

        backdropSource: backdropGrab
        screenResolution: Qt.vector2d(rootWindow.width * rootWindow.devicePixelRatio,
                                      rootWindow.height * rootWindow.devicePixelRatio)

        // Propriedades ópticas calibradas da Etapa 1.7
        property real currentIOR: 1.25
        property real currentDispersion: 0.040
        property real currentCaustic: 0.85
        property real currentOpacity: 0.85
        property real currentSlant: 0.0

        radii: Qt.vector4d(28.0, 28.0, 28.0, 28.0)
        slant: currentSlant
        ior: currentIOR
        chromaticDispersion: currentDispersion
        causticIntensity: currentCaustic
        surfaceOpacity: currentOpacity
        time: animTimer.tick

        DragHandler {
            id: windowDrag
            target: glassChassis
        }

        // =====================================================================
        // 4. ANATOMIA CANÔNICA EM 3 CAMADAS (Estrutura Fiel a Janela1.jpeg)
        // =====================================================================
        Item {
            anchors.fill: parent

            // -----------------------------------------------------------------
            // CAMADA 4.1: BARRA DE TÍTULO SUPERIOR (Titlebar Translúcida)
            // -----------------------------------------------------------------
            Item {
                id: titleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 52

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 18
                    spacing: 12

                    // Ícone Frutiger Aero: Pasta Azul Ciano Cristalina
                    Rectangle {
                        width: 26
                        height: 22
                        radius: 5
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#5eead4" }
                            GradientStop { position: 0.4; color: "#00f2fe" }
                            GradientStop { position: 1.0; color: "#0284c7" }
                        }
                        border.color: "#ffffff"
                        border.width: 1

                        // Aba superior da pasta
                        Rectangle {
                            x: 3; y: -3
                            width: 10
                            height: 5
                            radius: 2
                            color: "#5eead4"
                        }

                        // Reflexo especular no ícone
                        Rectangle {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: parent.height * 0.45
                            radius: 4
                            color: "#40ffffff"
                        }
                    }

                    // Título Canônico da Janela
                    Text {
                        text: "Systalia Files"
                        font.pixelSize: 14
                        font.bold: true
                        font.family: "Inter, Noto Sans, sans-serif"
                        color: "#f8fafc"
                        style: Text.Outline
                        styleColor: "#60020611"
                    }

                    // Badge Translúcido "ABYSSUS VFS"
                    Rectangle {
                        height: 20
                        implicitWidth: badgeText.implicitWidth + 16
                        radius: 10
                        color: "#1800f2fe"
                        border.color: "#5000f2fe"
                        border.width: 1

                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: "ABYSSUS VFS"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.2
                            color: "#38bdf8"
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Botões de Controle em Cápsula (Minimizar, Maximizar, Fechar)
                    Row {
                        spacing: 8

                        // Botão Minimizar
                        Rectangle {
                            width: 28
                            height: 22
                            radius: 11
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: minMouse.containsMouse ? "#45ffffff" : "#25ffffff" }
                                GradientStop { position: 1.0; color: minMouse.containsMouse ? "#200284c7" : "#0d1b2a" }
                            }
                            border.color: minMouse.containsMouse ? "#00f2fe" : "#30ffffff"
                            border.width: 1

                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 2
                                radius: 1
                                color: "#f8fafc"
                            }

                            MouseArea {
                                id: minMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        // Botão Maximizar
                        Rectangle {
                            width: 28
                            height: 22
                            radius: 11
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: maxMouse.containsMouse ? "#45ffffff" : "#25ffffff" }
                                GradientStop { position: 1.0; color: maxMouse.containsMouse ? "#200284c7" : "#0d1b2a" }
                            }
                            border.color: maxMouse.containsMouse ? "#00f2fe" : "#30ffffff"
                            border.width: 1

                            Rectangle {
                                anchors.centerIn: parent
                                width: 9
                                height: 9
                                radius: 1.5
                                color: "transparent"
                                border.color: "#f8fafc"
                                border.width: 1.5
                            }

                            MouseArea {
                                id: maxMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        // Botão Fechar (com gradiente aero coral suave ao passar o cursor)
                        Rectangle {
                            width: 28
                            height: 22
                            radius: 11
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop {
                                    position: 0.0
                                    color: closeMouse.containsMouse ? "#ff5c7a" : "#30ffffff"
                                }
                                GradientStop {
                                    position: 1.0
                                    color: closeMouse.containsMouse ? "#b91c1c" : "#0d1b2a"
                                }
                            }
                            border.color: closeMouse.containsMouse ? "#fca5a5" : "#35ffffff"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                font.pixelSize: 10
                                font.bold: true
                                color: "#f8fafc"
                            }

                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }
                    }
                }
            }

            // -----------------------------------------------------------------
            // CORPO DA JANELA: BARRA LATERAL FLUTUANTE + ÁREA DE ARQUIVOS
            // -----------------------------------------------------------------
            Item {
                anchors.top: titleBar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: controlsDrawer.top
                anchors.margins: 14

                // CAMADA 4.2: BARRA LATERAL FLUTUANTE ESCURA (Canonical Sidebar)
                Rectangle {
                    id: floatingSidebar
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    width: 200
                    radius: 16
                    color: Qt.rgba(0.03, 0.07, 0.13, 0.65)
                    border.color: Qt.rgba(1.0, 1.0, 1.0, 0.10)
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        Text {
                            text: "FAVORITES"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.0
                            color: "#64748b"
                            Layout.leftMargin: 8
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                        }

                        // Item 1: Recent (Ativo com indicador e glow ciano)
                        Rectangle {
                            Layout.fillWidth: true
                            height: 32
                            radius: 8
                            color: "#2500f2fe"
                            border.color: "#4000f2fe"
                            border.width: 1

                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 4
                                width: 3
                                height: 16
                                radius: 1.5
                                color: "#00f2fe"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 10
                                spacing: 10

                                Text { text: "⏱"; font.pixelSize: 13; color: "#00f2fe" }
                                Text { text: "Recent"; font.pixelSize: 12; font.bold: true; color: "#f8fafc" }
                            }
                        }

                        // Itens de Menu Canônicos
                        Repeater {
                            model: [
                                { label: "Home", icon: "⌂" },
                                { label: "Desktop", icon: "🖥" },
                                { label: "Documents", icon: "📁" },
                                { label: "Downloads", icon: "⬇" }
                            ]

                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                radius: 8
                                color: itemMouse.containsMouse ? "#15ffffff" : "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 10
                                    spacing: 10

                                    Text { text: modelData.icon; font.pixelSize: 12; color: "#94a3b8" }
                                    Text { text: modelData.label; font.pixelSize: 12; color: "#cbd5e1" }
                                }

                                MouseArea {
                                    id: itemMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#1e293b"
                            Layout.topMargin: 8
                            Layout.bottomMargin: 8
                        }

                        Text {
                            text: "VOLUMES"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.0
                            color: "#64748b"
                            Layout.leftMargin: 8
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 32
                            radius: 8
                            color: volMouse.containsMouse ? "#15ffffff" : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 10
                                spacing: 10

                                Text { text: "💾"; font.pixelSize: 12; color: "#38bdf8" }
                                Text { text: "Abyssus SSD"; font.pixelSize: 12; color: "#cbd5e1" }
                            }

                            MouseArea {
                                id: volMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        Item { Layout.fillHeight: true }

                        // Info de capacidade no rodapé da barra lateral
                        Rectangle {
                            Layout.fillWidth: true
                            height: 38
                            radius: 8
                            color: "#10000000"
                            border.color: "#15ffffff"
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 3

                                RowLayout {
                                    Text { text: "420 GB free"; font.pixelSize: 10; color: "#94a3b8" }
                                    Item { Layout.fillWidth: true }
                                    Text { text: "VALO FS"; font.pixelSize: 9; font.bold: true; color: "#00f2fe" }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 3
                                    radius: 1.5
                                    color: "#1e293b"

                                    Rectangle {
                                        width: parent.width * 0.65
                                        height: parent.height
                                        radius: 1.5
                                        color: "#00f2fe"
                                    }
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // CAMADA 4.3: ÁREA DE ARQUIVOS (Fundo 100% Líquido e Translúcido)
                // -------------------------------------------------------------
                Item {
                    id: fileViewArea
                    anchors.top: parent.top
                    anchors.left: floatingSidebar.right
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 14

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        // Barra de Navegação Superior (Breadcrumbs + Busca)
                        RowLayout {
                            Layout.fillWidth: true
                            height: 34

                            // Caminho em cápsula de vidro
                            Rectangle {
                                height: 32
                                Layout.fillWidth: true
                                radius: 8
                                color: "#180f172a"
                                border.color: "#20ffffff"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8

                                    Text { text: "Abyssus SSD"; font.pixelSize: 11; font.bold: true; color: "#38bdf8" }
                                    Text { text: "›"; font.pixelSize: 12; color: "#64748b" }
                                    Text { text: "Systalia Files"; font.pixelSize: 11; font.bold: true; color: "#f8fafc" }
                                }
                            }

                            // Barra de Busca Translúcida
                            Rectangle {
                                width: 170
                                height: 32
                                radius: 8
                                color: "#180f172a"
                                border.color: "#20ffffff"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 6

                                    Text { text: "🔍"; font.pixelSize: 11; color: "#64748b" }
                                    Text { text: "Filter files..."; font.pixelSize: 11; color: "#64748b" }
                                }
                            }
                        }

                        // Grade de Ícones Canônicos (Visual Frutiger Aero Crystal)
                        Grid {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            columns: 4
                            spacing: 16

                            Repeater {
                                model: [
                                    { name: "All Files", type: "stack", color1: "#38bdf8", color2: "#0284c7" },
                                    { name: "Documents", type: "folder", color1: "#00f2fe", color2: "#0369a1" },
                                    { name: "Document", type: "file", color1: "#e2e8f0", color2: "#94a3b8" },
                                    { name: "Download2", type: "download", color1: "#22d3ee", color2: "#0891b2" },
                                    { name: "Oceanic.raw", type: "code", color1: "#818cf8", color2: "#4338ca" },
                                    { name: "Caustics.exr", type: "image", color1: "#f472b6", color2: "#db2777" },
                                    { name: "Kernel.spv", type: "code", color1: "#34d399", color2: "#059669" },
                                    { name: "Archive.tar", type: "zip", color1: "#fbbf24", color2: "#d97706" }
                                ]

                                Rectangle {
                                    width: (fileViewArea.width - 3 * 16) / 4
                                    height: 105
                                    radius: 12
                                    color: fileMouse.containsMouse ? "#20ffffff" : "transparent"
                                    border.color: fileMouse.containsMouse ? "#4000f2fe" : "transparent"
                                    border.width: 1

                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 8

                                        // Ícone Tridimensional com Brilho Especular Frutiger Aero
                                        Rectangle {
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 48
                                            height: 44
                                            radius: 10
                                            gradient: Gradient {
                                                orientation: Gradient.Vertical
                                                GradientStop { position: 0.0; color: modelData.color1 }
                                                GradientStop { position: 1.0; color: modelData.color2 }
                                            }
                                            border.color: "#60ffffff"
                                            border.width: 1.2

                                            // Reflexo especular no topo do ícone
                                            Rectangle {
                                                anchors.top: parent.top
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: parent.height * 0.45
                                                radius: 8
                                                color: "#50ffffff"
                                            }

                                            // Glifo do ícone
                                            Text {
                                                anchors.centerIn: parent
                                                text: (modelData.type === "folder") ? "📁" :
                                                      ((modelData.type === "download") ? "⬇" :
                                                      ((modelData.type === "stack") ? "🗂" :
                                                      ((modelData.type === "image") ? "🖼" : "📄")))
                                                font.pixelSize: 18
                                            }
                                        }

                                        // Nome do Arquivo com Sombra para Leitura Cristalina
                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: modelData.name
                                            font.pixelSize: 11
                                            font.bold: true
                                            color: "#f8fafc"
                                            style: Text.Outline
                                            styleColor: "#80020611"
                                        }
                                    }

                                    MouseArea {
                                        id: fileMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // -----------------------------------------------------------------
            // CAMADA 4.4: PAINEL DE CALIBRAÇÃO ÓPTICA RETRÁTIL (Rodapé Elegante)
            // -----------------------------------------------------------------
            Rectangle {
                id: controlsDrawer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.bottomMargin: 10
                height: drawerToggle.expanded ? 70 : 28
                radius: 10
                color: Qt.rgba(0.02, 0.05, 0.10, 0.75)
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.12)
                border.width: 1

                Behavior on height {
                    NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    // Linha de Status / Toggle com Telemetria Wayland IPC
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: waylandServer.created ? "#10b981" : "#ef4444"
                        }

                        // Badge do Servidor Wayland
                        Rectangle {
                            height: 18
                            implicitWidth: socketBadgeRow.implicitWidth + 12
                            radius: 4
                            color: "#1500f2fe"
                            border.color: "#4000f2fe"
                            border.width: 1

                            RowLayout {
                                id: socketBadgeRow
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    text: "WAYLAND:"
                                    font.pixelSize: 8
                                    font.bold: true
                                    color: "#64748b"
                                }
                                Text {
                                    text: waylandServer.socketName
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: "#00f2fe"
                                }
                            }
                        }

                        // Badge de Status do Socket
                        Rectangle {
                            height: 18
                            implicitWidth: statusBadgeText.implicitWidth + 12
                            radius: 4
                            color: waylandServer.created ? "#1510b981" : "#15ef4444"
                            border.color: waylandServer.created ? "#4010b981" : "#40ef4444"
                            border.width: 1

                            Text {
                                id: statusBadgeText
                                anchors.centerIn: parent
                                text: waylandServer.created ? "Ativo / Escutando" : "Inativo"
                                font.pixelSize: 9
                                font.bold: true
                                color: waylandServer.created ? "#10b981" : "#ef4444"
                            }
                        }

                        // Badge de Clientes Conectados
                        Rectangle {
                            height: 18
                            implicitWidth: clientBadgeText.implicitWidth + 12
                            radius: 4
                            color: "#10ffffff"
                            border.color: "#20ffffff"
                            border.width: 1

                            Text {
                                id: clientBadgeText
                                anchors.centerIn: parent
                                text: "Clientes: " + waylandServer.clientCount
                                font.pixelSize: 9
                                font.bold: true
                                color: "#cbd5e1"
                            }
                        }

                        // Badge da Janela de Cliente Ativa
                        Rectangle {
                            visible: waylandServer.activeClientTitle.length > 0 || waylandServer.activeClientAppId.length > 0
                            height: 18
                            implicitWidth: activeClientText.implicitWidth + 12
                            radius: 4
                            color: "#18f59e0b"
                            border.color: "#50f59e0b"
                            border.width: 1

                            Text {
                                id: activeClientText
                                anchors.centerIn: parent
                                text: "Cliente: " + (waylandServer.activeClientTitle.length > 0
                                                    ? waylandServer.activeClientTitle
                                                    : waylandServer.activeClientAppId)
                                font.pixelSize: 9
                                font.bold: true
                                color: "#f59e0b"
                            }
                        }

                        Text {
                            text: "• VALO Glass (IOR " + glassChassis.currentIOR.toFixed(2) + " / Disp " + glassChassis.currentDispersion.toFixed(3) + ")"
                            font.pixelSize: 9
                            color: "#64748b"
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            id: drawerToggle
                            property bool expanded: false
                            width: 90; height: 18; radius: 9
                            color: drawerMouse.containsMouse ? "#2500f2fe" : "#10ffffff"
                            border.color: "#3000f2fe"; border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: drawerToggle.expanded ? "Ocultar Controles" : "Ajustar Óptica ⚙"
                                font.pixelSize: 9; font.bold: true; color: "#00f2fe"
                            }

                            MouseArea {
                                id: drawerMouse
                                anchors.fill: parent
                                onClicked: drawerToggle.expanded = !drawerToggle.expanded
                            }
                        }
                    }

                    // Sliders Expansíveis para Calibração Rápida
                    RowLayout {
                        Layout.fillWidth: true
                        visible: drawerToggle.expanded
                        spacing: 16

                        RowLayout {
                            Text { text: "IOR:"; color: "#38bdf8"; font.pixelSize: 10 }
                            Slider {
                                Layout.preferredWidth: 100
                                from: 1.00; to: 1.50
                                value: glassChassis.currentIOR
                                onMoved: glassChassis.currentIOR = value
                            }
                            Text { text: glassChassis.currentIOR.toFixed(2); color: "#f8fafc"; font.pixelSize: 10 }
                        }

                        RowLayout {
                            Text { text: "Dispersão:"; color: "#38bdf8"; font.pixelSize: 10 }
                            Slider {
                                Layout.preferredWidth: 100
                                from: 0.000; to: 0.080
                                value: glassChassis.currentDispersion
                                onMoved: glassChassis.currentDispersion = value
                            }
                            Text { text: glassChassis.currentDispersion.toFixed(3); color: "#f8fafc"; font.pixelSize: 10 }
                        }

                        RowLayout {
                            Text { text: "Cáusticas:"; color: "#38bdf8"; font.pixelSize: 10 }
                            Slider {
                                Layout.preferredWidth: 100
                                from: 0.00; to: 1.50
                                value: glassChassis.currentCaustic
                                onMoved: glassChassis.currentCaustic = value
                            }
                            Text { text: glassChassis.currentCaustic.toFixed(2); color: "#f8fafc"; font.pixelSize: 10 }
                        }

                        RowLayout {
                            Text { text: "Opacidade:"; color: "#38bdf8"; font.pixelSize: 10 }
                            Slider {
                                Layout.preferredWidth: 100
                                from: 0.20; to: 1.00
                                value: glassChassis.currentOpacity
                                onMoved: glassChassis.currentOpacity = value
                            }
                            Text { text: Math.round(glassChassis.currentOpacity * 100) + "%"; color: "#f8fafc"; font.pixelSize: 10 }
                        }
                    }
                }
            }
        }
    }

    // Camada flutuante superior para janelas de clientes Wayland
    Item {
        id: clientWindowsLayer
        anchors.fill: parent
        z: 10
    }
}
