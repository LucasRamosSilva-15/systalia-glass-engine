import QtQuick
import QtQuick.Controls
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: topBar
    objectName: "topBar"

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
    readonly property int actionButtonCount: rightSection.children.length - 1
    readonly property bool isWorkspaceTrackVisible: workspaceIndicatorTrack.visible

    // Sombra de oclusão inferior suave
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 3
        anchors.bottomMargin: -4
        radius: topBar.cornerRadii.x
        color: ValoTheme.withAlpha(ValoTheme.primitiveBlack, 0.35)
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

    // Chassi vítreo translúcido com gradiente vertical suave e UMA ÚNICA borda especular
    Rectangle {
        anchors.fill: parent
        radius: topBar.cornerRadii.x
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: ValoTheme.withAlpha(ValoTheme.ocean700, 0.55) }
            GradientStop { position: 0.25; color: ValoTheme.withAlpha(ValoTheme.backgroundTopbar, 0.88) }
            GradientStop { position: 1.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean900, 0.94) }
        }
        border.color: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.28)
        border.width: ValoTheme.strokeFine
        antialiasing: true

        // Highlight especular superior suave (um único reflexo horizontal contínuo)
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: topBar.cornerRadii.x
            anchors.rightMargin: topBar.cornerRadii.x
            height: 1
            color: "transparent"
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.25; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.20) }
                GradientStop { position: 0.50; color: ValoTheme.withAlpha(ValoTheme.accentBright, 0.40) }
                GradientStop { position: 0.75; color: ValoTheme.withAlpha(ValoTheme.textBright, 0.20) }
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
            objectName: "workspaceIndicatorTrack"
            visible: topBar.showWorkspaceIndicators
            anchors.verticalCenter: parent.verticalCenter
            width: workspacePillsRow.implicitWidth + 14
            height: 26
            radius: 13
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean700, 0.45) }
                GradientStop { position: 1.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean900, 0.60) }
            }
            border.color: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.25)
            border.width: ValoTheme.strokeFine

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
                                : ValoTheme.withAlpha(ValoTheme.borderHighlight, pillHover.hovered ? 0.40 : 0.20)
                            border.width: workspaceDelegate.isActive ? 1.0 : ValoTheme.strokeFine
                            antialiasing: true

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
        objectName: "rightSection"
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
                objectName: "statusButton"
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

    // Componente de ícones vetoriais de precisão para a barra de status
    component StatusVectorIcon: Item {
        id: vectorIcon
        objectName: "vectorIcon"
        property string iconType: "control"
        property color iconColor: ValoTheme.iconDefault
        width: 20
        height: 20

        Canvas {
            id: iconCanvas
            anchors.fill: parent
            renderTarget: Canvas.Image
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = vectorIcon.iconColor
                ctx.fillStyle = vectorIcon.iconColor
                ctx.lineWidth = 1.5
                ctx.lineCap = "round"
                ctx.lineJoin = "round"

                var w = width
                var h = height

                if (vectorIcon.iconType === "control") {
                    ctx.strokeRect(3.5, 3.5, 13, 13)
                    ctx.fillRect(7.5, 7.5, 5, 5)
                } else if (vectorIcon.iconType === "terminal") {
                    ctx.beginPath()
                    ctx.moveTo(4, 5)
                    ctx.lineTo(9, 10)
                    ctx.lineTo(4, 15)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(11, 15)
                    ctx.lineTo(16, 15)
                    ctx.stroke()
                } else if (vectorIcon.iconType === "launcher") {
                    var s = 5.0
                    ctx.fillRect(4, 4, s, s)
                    ctx.fillRect(11, 4, s, s)
                    ctx.fillRect(4, 11, s, s)
                    ctx.fillRect(11, 11, s, s)
                } else if (vectorIcon.iconType === "brightness") {
                    ctx.beginPath()
                    ctx.arc(10, 10, 3.5, 0, 2 * Math.PI)
                    ctx.stroke()
                    // 4 raios cardinais
                    ctx.beginPath()
                    ctx.moveTo(10, 2); ctx.lineTo(10, 4.5)
                    ctx.moveTo(10, 18); ctx.lineTo(10, 15.5)
                    ctx.moveTo(2, 10); ctx.lineTo(4.5, 10)
                    ctx.moveTo(18, 10); ctx.lineTo(15.5, 10)
                    // 4 raios diagonais
                    ctx.moveTo(4.5, 4.5); ctx.lineTo(6.2, 6.2)
                    ctx.moveTo(15.5, 4.5); ctx.lineTo(13.8, 6.2)
                    ctx.moveTo(4.5, 15.5); ctx.lineTo(6.2, 13.8)
                    ctx.moveTo(15.5, 15.5); ctx.lineTo(13.8, 13.8)
                    ctx.stroke()
                } else if (vectorIcon.iconType === "volume") {
                    ctx.beginPath()
                    ctx.moveTo(3, 7)
                    ctx.lineTo(7, 7)
                    ctx.lineTo(11, 4)
                    ctx.lineTo(11, 16)
                    ctx.lineTo(7, 13)
                    ctx.lineTo(3, 13)
                    ctx.closePath()
                    ctx.fill()
                    if (typeof SystemControl !== "undefined" && SystemControl && SystemControl.isMuted) {
                        ctx.beginPath()
                        ctx.moveTo(13, 7); ctx.lineTo(17, 13)
                        ctx.moveTo(17, 7); ctx.lineTo(13, 13)
                        ctx.stroke()
                    } else {
                        ctx.beginPath()
                        ctx.arc(10, 10, 4.5, -Math.PI / 3, Math.PI / 3)
                        ctx.stroke()
                        if (typeof SystemControl === "undefined" || !SystemControl || SystemControl.volume > 50) {
                            ctx.beginPath()
                            ctx.arc(10, 10, 7.5, -Math.PI / 3, Math.PI / 3)
                            ctx.stroke()
                        }
                    }
                } else if (vectorIcon.iconType === "power") {
                    ctx.beginPath()
                    ctx.arc(10, 11, 6, -Math.PI * 0.72, -Math.PI * 0.28, true)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(10, 3)
                    ctx.lineTo(10, 9.5)
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
        objectName: "statusButton"
        property string iconType: "control"
        property string accessibleName: "Controle do sistema"
        Accessible.name: accessibleName
        hoverEnabled: true
        padding: 0
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        contentItem: Item {
            implicitWidth: 20
            implicitHeight: 20

            StatusVectorIcon {
                anchors.centerIn: parent
                iconType: statusButton.iconType
                iconColor: statusButton.pressed ? ValoTheme.accentPrimary
                         : (statusButton.hovered ? ValoTheme.textBright : ValoTheme.accentBright)
            }
        }

        background: Rectangle {
            radius: 8
            gradient: statusButton.hovered ? hoverStatusGrad : null
            color: statusButton.hovered ? "transparent" : (statusButton.pressed ? ValoTheme.withAlpha(ValoTheme.primitiveOcean700, 0.30) : "transparent")
            border.color: statusButton.hovered ? ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.40) : "transparent"
            border.width: ValoTheme.strokeFine
            antialiasing: true

            Gradient {
                id: hoverStatusGrad
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean600, 0.40) }
                GradientStop { position: 1.0; color: ValoTheme.withAlpha(ValoTheme.primitiveOcean800, 0.55) }
            }
        }
    }
}
