import QtQuick
import QtQuick.Controls
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: topBar

    property var telemetry: null
    property var workspaceManager: null
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0
    property date dateNow: new Date()
    property vector4d cornerRadii: Qt.vector4d(ValoTheme.radiusTopbar, ValoTheme.radiusTopbar,
                                               ValoTheme.radiusTopbar, ValoTheme.radiusTopbar)

    signal launcherRequested()
    signal controlCenterRequested()
    signal terminalRequested()
    signal powerRequested()
    signal workspaceSelected(int index)

    width: 1646
    height: 45
    z: 100

    // Regras responsivas para evitar sobreposição em larguras compactas
    readonly property bool showDesktopBranding: width >= 1220
    readonly property bool showFullTelemetry: width >= 1040
    readonly property bool showCompactTelemetry: width >= 820 && width < 1040
    readonly property bool showClockDate: width >= 920
    readonly property bool showWorkspaceIndicators: width >= 720
    readonly property bool isCompactBar: width < 860

    // Sombra de oclusão profunda inferior
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.bottomMargin: -6
        radius: topBar.cornerRadii.x
        color: ValoTheme.withAlpha(ValoTheme.primitiveBlack, 0.40)
        z: -2
    }

    // Sombra luminosa externa ciano conforme conceito VALO
    Rectangle {
        anchors.fill: parent
        anchors.margins: -2
        radius: topBar.cornerRadii.x + 2
        color: ValoTheme.withAlpha(ValoTheme.accentPrimary, 0.20)
        z: -1
    }

    SystaliaGlassItem {
        anchors.fill: parent
        backdropSource: topBar.backdropSource
        screenResolution: topBar.screenResolution
        radii: topBar.cornerRadii
        circularCorners: true
        slant: 0
        ior: 1.10
        chromaticDispersion: 0.015
        causticIntensity: 0.16
        surfaceOpacity: ValoTheme.opacityTopbar
        time: topBar.animationTime
    }

    // Chassi vítreo translúcido com gradiente vertical suave VALO
    Rectangle {
        anchors.fill: parent
        radius: topBar.cornerRadii.x
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: ValoTheme.withAlpha(ValoTheme.ocean700, 0.55) }
            GradientStop { position: 0.3; color: ValoTheme.withAlpha(ValoTheme.backgroundTopbar, 0.88) }
            GradientStop { position: 1.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean900, 0.92) }
        }
        border.color: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.32)
        border.width: ValoTheme.strokeFine
        antialiasing: true

        // Filete de brilho especular perimetral superior (Upper cyan gloss)
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 1
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            height: 1
            radius: 0.5
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.2; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.35) }
                GradientStop { position: 0.5; color: ValoTheme.withAlpha(ValoTheme.accentBright, 0.55) }
                GradientStop { position: 0.8; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.35) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    // Seção esquerda: logotipo, branding do desktop e telemetria responsiva
    Row {
        id: leftSection
        anchors.left: parent.left
        anchors.leftMargin: topBar.isCompactBar ? 12 : 17
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: centerSection.left
        anchors.rightMargin: 14
        spacing: topBar.isCompactBar ? 10 : 16
        clip: true

        // Símbolo oficial SYSTALIA (vetor SVG do Figma Node 65:2)
        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 21
            height: 28
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-systalia-symbol.svg"
            sourceSize: Qt.size(42, 56)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true

            TapHandler {
                cursorShape: Qt.PointingHandCursor
                onTapped: topBar.launcherRequested()
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "SYSTALIA"
            color: ValoTheme.textBright
            font.family: ValoTheme.fontFamily
            font.pixelSize: topBar.isCompactBar ? 16 : 19
            font.weight: Font.DemiBold
            antialiasing: true
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: topBar.showDesktopBranding
            text: "SDE-KWIN | VALO"
            color: ValoTheme.textSecondary
            font.family: ValoTheme.fontFamily
            font.pixelSize: 14
            antialiasing: true
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: topBar.showFullTelemetry || topBar.showCompactTelemetry
            text: {
                if (!topBar.telemetry)
                    return "CPU --%"
                if (topBar.showFullTelemetry) {
                    return "CPU %1%  |  RAM %2GB  |  FPS %3"
                        .arg(Math.round(topBar.telemetry.cpuUsage))
                        .arg(topBar.telemetry.ramUsageGb.toFixed(1))
                        .arg(topBar.telemetry.fps)
                }
                return "%1% CPU | %2G | %3"
                    .arg(Math.round(topBar.telemetry.cpuUsage))
                    .arg(topBar.telemetry.ramUsageGb.toFixed(1))
                    .arg(topBar.telemetry.fps)
            }
            color: ValoTheme.textSecondary
            font.family: ValoTheme.fontFamily
            font.pixelSize: topBar.isCompactBar ? 12 : 14
            elide: Text.ElideRight
            antialiasing: true
        }
    }

    // Seção central: Relógio e indicador de espaços de trabalho virtuais
    Row {
        id: centerSection
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: topBar.isCompactBar ? 12 : 18

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: {
                if (topBar.showClockDate) {
                    return topBar.telemetry
                        ? topBar.telemetry.systemTime
                        : topBar.dateNow.toLocaleDateString(Qt.locale("en_US"), "ddd dd MMM").toUpperCase()
                          + "  >  "
                          + topBar.dateNow.toLocaleTimeString(Qt.locale("en_US"), "HH:mm:ss")
                }
                return topBar.dateNow.toLocaleTimeString(Qt.locale("en_US"), "HH:mm:ss")
            }
            color: ValoTheme.textPrimary
            font.family: ValoTheme.fontFamily
            font.pixelSize: topBar.isCompactBar ? 14 : 16
            font.weight: Font.Medium
            antialiasing: true
        }

        // Régua de cápsulas translúcidas para os Espaços de Trabalho Virtuais
        Rectangle {
            id: workspaceIndicatorTrack
            visible: topBar.showWorkspaceIndicators
            anchors.verticalCenter: parent.verticalCenter
            width: workspacePillsRow.implicitWidth + 14
            height: 26
            radius: 13
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean700, 0.50) }
                GradientStop { position: 1.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean900, 0.65) }
            }
            border.color: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.32)
            border.width: ValoTheme.strokeFine

            // Sombra interna e bisel sutil
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 12
                color: "transparent"
                border.color: ValoTheme.withAlpha(ValoTheme.primitiveWhite, 0.12)
                border.width: 0.7
            }

            Gradient {
                id: activeWorkspaceGrad
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#39F5F4" }
                GradientStop { position: 1.0; color: "#127FD0" }
            }
            Gradient {
                id: inactiveWorkspaceGrad
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: "#1a3b5c" }
                GradientStop { position: 1.0; color: "#0d2238" }
            }
            Gradient {
                id: hoverWorkspaceGrad
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: "#3d5f84" }
                GradientStop { position: 1.0; color: "#1a3b5c" }
            }

            Row {
                id: workspacePillsRow
                anchors.centerIn: parent
                spacing: ValoTheme.spacing8

                Repeater {
                    model: topBar.workspaceManager ? topBar.workspaceManager.totalWorkspaces : 4

                    delegate: Item {
                        id: workspaceDelegate
                        required property int index
                        readonly property bool isActive: topBar.workspaceManager
                                                         ? (topBar.workspaceManager.currentWorkspace === index)
                                                         : (index === 0)
                        readonly property bool hasWindows: {
                            if (!topBar.workspaceManager)
                                return index === 0
                            const rev = topBar.workspaceManager.workspaceWindowsRevision
                            return rev >= 0 && topBar.workspaceManager.hasWindowsInWorkspace(index)
                        }

                        width: isActive ? 28 : 18
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter

                        Rectangle {
                            id: workspacePill
                            anchors.fill: parent
                            radius: 7
                            gradient: workspaceDelegate.isActive
                                ? activeWorkspaceGrad
                                : (pillHover.hovered ? hoverWorkspaceGrad : inactiveWorkspaceGrad)
                            border.color: workspaceDelegate.isActive
                                ? ValoTheme.withAlpha(ValoTheme.primitiveWhite, 0.85)
                                : ValoTheme.withAlpha(ValoTheme.borderHighlight, pillHover.hovered ? 0.45 : 0.22)
                            border.width: workspaceDelegate.isActive ? 1.0 : ValoTheme.strokeFine
                            antialiasing: true

                            // Brilho difuso exterior para o workspace ativo
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: -2
                                radius: 9
                                z: -1
                                visible: workspaceDelegate.isActive
                                color: ValoTheme.withAlpha(ValoTheme.accentPrimary, 0.40)
                                antialiasing: true
                            }

                            // Ponto central indicador para espaços com janelas abertas
                            Rectangle {
                                width: 4
                                height: 4
                                radius: 2
                                anchors.centerIn: parent
                                visible: workspaceDelegate.hasWindows
                                color: workspaceDelegate.isActive ? "#071628" : "#8CFFFF"
                                antialiasing: true
                            }
                        }

                        Behavior on width {
                            NumberAnimation {
                                duration: ValoTheme.motionDurationFastMs
                                easing.type: Easing.OutCubic
                            }
                        }

                        HoverHandler {
                            id: pillHover
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            cursorShape: Qt.PointingHandCursor
                            onTapped: {
                                if (topBar.workspaceManager)
                                    topBar.workspaceManager.switchToWorkspace(workspaceDelegate.index)
                                topBar.workspaceSelected(workspaceDelegate.index)
                            }
                        }
                    }
                }
            }
        }
    }

    Row {
        id: rightSection
        anchors.right: parent.right
        anchors.rightMargin: topBar.isCompactBar ? 10 : 18
        anchors.verticalCenter: parent.verticalCenter
        height: 32
        spacing: topBar.isCompactBar ? 4 : 8

        Repeater {
            model: [
                { iconType: "control", name: "Estado do sistema", minWidth: 0, action: "control" },
                { iconType: "terminal", name: "Terminal", minWidth: 0, action: "terminal" },
                { iconType: "launcher", name: "Aplicativos", minWidth: 0, action: "launcher" },
                { iconType: "brightness", name: "Brilho do ecrã", minWidth: 740, action: "control" },
                { iconType: "volume", name: "Rede e som", minWidth: 620, action: "volume" },
                { iconType: "power", name: "Energia e sessão", minWidth: 0, action: "power" }
            ]

            delegate: StatusButton {
                required property var modelData
                visible: topBar.width >= modelData.minWidth
                width: topBar.isCompactBar ? 32 : 38
                height: 30
                iconType: modelData.iconType
                accessibleName: modelData.name
                onClicked: {
                    if (modelData.action === "terminal") {
                        topBar.terminalRequested()
                    } else if (modelData.action === "launcher") {
                        topBar.launcherRequested()
                    } else if (modelData.action === "power") {
                        topBar.powerRequested()
                    } else {
                        topBar.controlCenterRequested()
                    }
                }
            }
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: topBar.dateNow = new Date()
    }

    // Componente de ícones vetoriais puros para a barra de status
    component StatusVectorIcon: Item {
        id: vectorIcon
        property string iconType: "control"
        property color iconColor: ValoTheme.iconDefault
        width: 18
        height: 18

        Canvas {
            id: iconCanvas
            anchors.fill: parent
            renderTarget: Canvas.Image
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = vectorIcon.iconColor
                ctx.fillStyle = vectorIcon.iconColor
                ctx.lineWidth = 1.4
                ctx.lineCap = "round"
                ctx.lineJoin = "round"

                var w = width
                var h = height

                if (vectorIcon.iconType === "control") {
                    ctx.strokeRect(3, 3, w - 6, h - 6)
                    ctx.fillRect(6.5, 6.5, w - 13, h - 13)
                } else if (vectorIcon.iconType === "terminal") {
                    ctx.beginPath()
                    ctx.moveTo(3, 5)
                    ctx.lineTo(8, 9)
                    ctx.lineTo(3, 13)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(10, 13)
                    ctx.lineTo(15, 13)
                    ctx.stroke()
                } else if (vectorIcon.iconType === "launcher") {
                    var s = 4.5
                    ctx.fillRect(3, 3, s, s)
                    ctx.fillRect(10.5, 3, s, s)
                    ctx.fillRect(3, 10.5, s, s)
                    ctx.fillRect(10.5, 10.5, s, s)
                } else if (vectorIcon.iconType === "brightness") {
                    ctx.beginPath()
                    ctx.arc(w / 2, h / 2, 3.2, 0, 2 * Math.PI)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(w / 2, 1.5); ctx.lineTo(w / 2, 3.5)
                    ctx.moveTo(w / 2, h - 1.5); ctx.lineTo(w / 2, h - 3.5)
                    ctx.moveTo(1.5, h / 2); ctx.lineTo(3.5, h / 2)
                    ctx.moveTo(w - 1.5, h / 2); ctx.lineTo(w - 3.5, h / 2)
                    ctx.moveTo(3.5, 3.5); ctx.lineTo(4.8, 4.8)
                    ctx.moveTo(w - 3.5, 3.5); ctx.lineTo(w - 4.8, 4.8)
                    ctx.moveTo(3.5, h - 3.5); ctx.lineTo(4.8, h - 4.8)
                    ctx.moveTo(w - 3.5, h - 3.5); ctx.lineTo(w - 4.8, h - 4.8)
                    ctx.stroke()
                } else if (vectorIcon.iconType === "volume") {
                    ctx.beginPath()
                    ctx.moveTo(3, 6)
                    ctx.lineTo(6, 6)
                    ctx.lineTo(10, 3)
                    ctx.lineTo(10, 15)
                    ctx.lineTo(6, 12)
                    ctx.lineTo(3, 12)
                    ctx.closePath()
                    ctx.fill()
                    if (typeof SystemControl !== "undefined" && SystemControl && SystemControl.isMuted) {
                        ctx.beginPath()
                        ctx.moveTo(12, 6); ctx.lineTo(16, 12)
                        ctx.moveTo(16, 6); ctx.lineTo(12, 12)
                        ctx.stroke()
                    } else {
                        ctx.beginPath()
                        ctx.arc(9, 9, 4.5, -Math.PI / 4, Math.PI / 4)
                        ctx.stroke()
                        if (typeof SystemControl === "undefined" || !SystemControl || SystemControl.volume > 50) {
                            ctx.beginPath()
                            ctx.arc(9, 9, 7.5, -Math.PI / 4, Math.PI / 4)
                            ctx.stroke()
                        }
                    }
                } else if (vectorIcon.iconType === "power") {
                    ctx.beginPath()
                    ctx.arc(w / 2, h / 2 + 1, 6, -Math.PI * 0.7, -Math.PI * 0.3, true)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(w / 2, 2.5)
                    ctx.lineTo(w / 2, 8.5)
                    ctx.stroke()
                }
            }

            Connections {
                target: vectorIcon
                function onIconColorChanged() { iconCanvas.requestPaint() }
                function onIconTypeChanged() { iconCanvas.requestPaint() }
            }

            Connections {
                target: typeof SystemControl !== "undefined" ? SystemControl : null
                function onVolumeChanged() { iconCanvas.requestPaint() }
                function onIsMutedChanged() { iconCanvas.requestPaint() }
            }
        }
    }

    component StatusButton: ToolButton {
        id: statusButton
        property string iconType: "control"
        property string accessibleName: "Controle do sistema"
        Accessible.name: accessibleName
        hoverEnabled: true

        contentItem: Item {
            anchors.centerIn: parent
            width: 18
            height: 18

            StatusVectorIcon {
                anchors.centerIn: parent
                iconType: statusButton.iconType
                iconColor: statusButton.hovered ? ValoTheme.textBright : ValoTheme.accentBright
            }
        }

        background: Rectangle {
            radius: 8
            gradient: statusButton.hovered ? hoverStatusGrad : null
            color: statusButton.hovered ? "transparent" : (statusButton.pressed ? ValoTheme.withAlpha(ValoTheme.primitiveOcean700, 0.30) : "transparent")
            border.color: statusButton.hovered ? ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.45) : "transparent"
            border.width: ValoTheme.strokeFine
            antialiasing: true

            Gradient {
                id: hoverStatusGrad
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean600, 0.40) }
                GradientStop { position: 1.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean800, 0.55) }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 4
                height: 1
                visible: statusButton.hovered
                color: ValoTheme.withAlpha(ValoTheme.primitiveWhite, 0.35)
            }
        }
    }
}
