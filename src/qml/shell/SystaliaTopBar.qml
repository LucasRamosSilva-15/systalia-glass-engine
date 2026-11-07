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

    // Sombra luminosa externa ciano conforme style.css e tokens VALO
    Rectangle {
        anchors.fill: parent
        radius: topBar.cornerRadii.x
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
        chromaticDispersion: 0.012
        causticIntensity: 0.12
        surfaceOpacity: ValoTheme.opacityTopbar
        time: topBar.animationTime
    }

    // Contorno único de precisão conforme style.css e tokens VALO
    Rectangle {
        anchors.fill: parent
        radius: topBar.cornerRadii.x
        color: ValoTheme.withAlpha(ValoTheme.backgroundTopbar, ValoTheme.opacityTopbar)
        border.color: ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.24)
        border.width: ValoTheme.strokeFine
    }

    // Seção esquerda: logotipo, branding do desktop e telemetria responsiva
    Row {
        id: leftSection
        anchors.left: parent.left
        anchors.leftMargin: topBar.isCompactBar ? 12 : 17
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: centerSection.left
        anchors.rightMargin: 12
        spacing: topBar.isCompactBar ? 10 : 18
        clip: true

        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 23
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-mark.png"
            sourceSize: Qt.size(112, 112)
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
            font.pixelSize: ValoTheme.fontCaption + 2
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
            font.pixelSize: topBar.isCompactBar ? 13 : ValoTheme.fontCaption + 1
            elide: Text.ElideRight
            antialiasing: true
        }
    }

    // Seção central: Relógio e indicador de espaços de trabalho virtuais
    Row {
        id: centerSection
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: topBar.isCompactBar ? 12 : 20

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
            font.pixelSize: topBar.isCompactBar ? 15 : ValoTheme.fontBody
            font.weight: Font.Medium
            antialiasing: true
        }

        // Régua de cápsulas translúcidas para os Espaços de Trabalho Virtuais
        Rectangle {
            id: workspaceIndicatorTrack
            visible: topBar.showWorkspaceIndicators
            anchors.verticalCenter: parent.verticalCenter
            width: workspacePillsRow.implicitWidth + 16
            height: 26
            radius: 13
            color: ValoTheme.withAlpha(ValoTheme.backgroundDesktop, 0.40)
            border.color: ValoTheme.withAlpha(ValoTheme.borderSubtle, 0.35)
            border.width: ValoTheme.strokeFine

            Row {
                id: workspacePillsRow
                anchors.centerIn: parent
                spacing: ValoTheme.spacing8

                Repeater {
                    model: topBar.workspaceManager ? topBar.workspaceManager.totalWorkspaces : 4

                    delegate: Rectangle {
                        id: workspacePill
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

                        width: isActive ? 26 : 18
                        height: 12
                        radius: 6
                        color: isActive ? ValoTheme.accentBright : (pillHover.hovered ? ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.30) : ValoTheme.withAlpha(ValoTheme.borderSubtle, 0.20))
                        border.color: isActive ? ValoTheme.withAlpha(ValoTheme.textBright, 0.70) : ValoTheme.withAlpha(ValoTheme.borderSubtle, 0.40)
                        border.width: ValoTheme.strokeFine
                        scale: isActive ? 1.15 : (pillHover.hovered ? 1.05 : 1.0)
                        antialiasing: true

                        Behavior on width {
                            NumberAnimation {
                                duration: ValoTheme.motionDurationFastMs
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on scale {
                            NumberAnimation {
                                duration: ValoTheme.motionDurationFastMs
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: 150
                            }
                        }

                        // Ponto central indicador para espaços com janelas abertas
                        Rectangle {
                            width: 4
                            height: 4
                            radius: 2
                            anchors.centerIn: parent
                            visible: workspacePill.hasWindows
                            color: workspacePill.isActive ? ValoTheme.backgroundDesktop : ValoTheme.accentBright
                            antialiasing: true
                        }

                        HoverHandler {
                            id: pillHover
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            cursorShape: Qt.PointingHandCursor
                            onTapped: {
                                if (topBar.workspaceManager)
                                    topBar.workspaceManager.switchToWorkspace(workspacePill.index)
                                topBar.workspaceSelected(workspacePill.index)
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
        height: 30
        spacing: topBar.isCompactBar ? 2 : ValoTheme.spacing4

        Repeater {
            model: [
                { glyph: "▰", name: "Estado do sistema", minWidth: 0, action: "control" },
                { glyph: ">_", name: "Terminal", minWidth: 0, action: "terminal" },
                { glyph: "▦", name: "Aplicativos", minWidth: 0, action: "launcher" },
                { glyph: "☼", name: "Brilho do ecrã", minWidth: 740, action: "control" },
                { glyph: "🔊", name: "Rede e som", minWidth: 620, action: "volume" },
                { glyph: "⏻", name: "Energia e sessão", minWidth: 0, action: "power" }
            ]

            delegate: StatusButton {
                required property var modelData
                visible: topBar.width >= modelData.minWidth
                width: topBar.isCompactBar ? 34 : 44
                height: 30
                glyph: {
                    if (modelData.action === "volume") {
                        if (typeof SystemControl !== "undefined" && SystemControl) {
                            if (SystemControl.isMuted) return "🔇"
                            if (SystemControl.volume === 0) return "🔈"
                            if (SystemControl.volume > 50) return "🔊"
                            return "🔉"
                        }
                        return "🔊"
                    }
                    return modelData.glyph
                }
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

    component StatusButton: ToolButton {
        id: statusButton
        property string glyph: "•"
        property string accessibleName: "Controle do sistema"
        Accessible.name: accessibleName
        hoverEnabled: true

        contentItem: Text {
            text: statusButton.glyph
            color: statusButton.hovered ? ValoTheme.textBright : ValoTheme.iconDefault
            font.family: ValoTheme.fontFamily
            font.pixelSize: 15
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            antialiasing: true
        }
        background: Rectangle {
            radius: ValoTheme.radiusIcon
            color: statusButton.hovered ? ValoTheme.withAlpha(ValoTheme.surfaceButton, 0.45) : "transparent"
            border.color: statusButton.hovered ? ValoTheme.withAlpha(ValoTheme.borderHighlight, 0.40) : "transparent"
            border.width: ValoTheme.strokeFine
            antialiasing: true
        }
    }
}
