import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Systalia.Engine
import Systalia.System 1.0

Item {
    id: controlCenter

    property bool opened: false
    property var backdropSource: null
    property var screenResolution: Qt.vector2d(width, height)
    property real animationTime: 0.0
    property vector4d cornerRadii: Qt.vector4d(22, 22, 22, 22)
    property string activePowerConfirmAction: ""

    anchors.fill: parent
    z: 115
    visible: opened || opacity > 0.01
    opacity: opened ? 1.0 : 0.0

    function toggle() {
        opened = !opened
        if (!opened) activePowerConfirmAction = ""
    }

    function open() {
        opened = true
    }

    function close() {
        opened = false
        activePowerConfirmAction = ""
    }

    Behavior on opacity {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    // Fundo escuro com clique para fechar
    Rectangle {
        anchors.fill: parent
        visible: controlCenter.opened
        color: "#32020a18"
        MouseArea {
            anchors.fill: parent
            onClicked: controlCenter.close()
        }
    }

    Item {
        id: panel
        width: 376
        height: 570
        x: controlCenter.width - width - 18
        y: controlCenter.opened ? 52 : -height - 16
        clip: false

        Behavior on y {
            NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
        }

        // Sombra perimetral luminosa VALO
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: controlCenter.cornerRadii.x + 4
            color: "#3d09baed"
            z: -2
        }

        // Sombra de oclusão profunda
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 8
            anchors.margins: 4
            radius: controlCenter.cornerRadii.x
            color: "#66000c18"
            z: -1
        }

        // Vidro óptico com cáusticas e dispersão cromática
        SystaliaGlassItem {
            anchors.fill: parent
            backdropSource: controlCenter.backdropSource
            screenResolution: controlCenter.screenResolution
            radii: controlCenter.cornerRadii
            slant: 0.0
            ior: 1.25
            chromaticDispersion: 0.040
            causticIntensity: 0.85
            surfaceOpacity: 0.88
            time: controlCenter.animationTime
        }

        // Moldura G2 contínua
        SystaliaG2Frame {
            anchors.fill: parent
            radii: controlCenter.cornerRadii
            fillColor: "#ea061426"
            strokeColor: "#c9ffffff"
            strokeWidth: 1.25
        }

        // Camada superior de brilho / gloss perimetral VALO
        Rectangle {
            x: 4
            y: 2.5
            width: parent.width - 8
            height: parent.height - 8
            radius: 20
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: "#45c8ffff" }
                GradientStop { position: 0.30; color: "#1a61f0ff" }
                GradientStop { position: 0.70; color: "#0a155279" }
                GradientStop { position: 1.0; color: "#001b1834" }
            }
            border.color: "#28c8ffff"
            border.width: 0.8
            antialiasing: true
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            // Cabeçalho: Título + Botão de Fechar
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item {
                    width: 22
                    height: 22
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "#2414c4e7"
                        border.color: "#6617f5df"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "◈"
                            color: "#17f5df"
                            font.pixelSize: 13
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        text: "ABYSS CONTROL"
                        color: "#f1fbff"
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 14
                        font.bold: true
                        font.letterSpacing: 1.2
                    }
                    Text {
                        text: "Ajustes rápidos do sistema & energia"
                        color: "#8bb2c4"
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 9
                    }
                }

                // Botão fechar
                Item {
                    width: 26
                    height: 26
                    Rectangle {
                        id: closeBtnBg
                        anchors.centerIn: parent
                        width: 24
                        height: 24
                        radius: 12
                        color: closeMouse.containsMouse ? "#55ff4b72" : "#24ff3b60"
                        border.color: closeMouse.containsMouse ? "#ffc2d4" : "#66ff4b72"
                        border.width: 1
                        antialiasing: true

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "#ffffff"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: controlCenter.close()
                    }
                }
            }

            // Separador sutil
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2b4c7388" }

            // 1. Toggles Rápidos (Wi-Fi, Bluetooth, Não incomodar) com ícones vetoriais perfeitos
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                // Wi-Fi
                QuickToggle {
                    Layout.fillWidth: true
                    label: "Wi-Fi"
                    sublabel: SystemControl.wifiEnabled ? "Ligado" : "Desligado"
                    active: SystemControl.wifiEnabled
                    iconType: "wifi"
                    onClicked: SystemControl.toggleWifi()
                }

                // Bluetooth
                QuickToggle {
                    Layout.fillWidth: true
                    label: "Bluetooth"
                    sublabel: !SystemControl.bluetoothAvailable ? "Indisponível"
                              : (SystemControl.bluetoothEnabled ? "Ligado" : "Desligado")
                    active: SystemControl.bluetoothEnabled
                    iconType: "bluetooth"
                    onClicked: SystemControl.toggleBluetooth()
                }

                // Não incomodar
                QuickToggle {
                    Layout.fillWidth: true
                    label: "Silencioso"
                    sublabel: SystemControl.doNotDisturb ? "Ativo" : "Inativo"
                    active: SystemControl.doNotDisturb
                    iconType: "moon"
                    onClicked: SystemControl.setDoNotDisturb(!SystemControl.doNotDisturb)
                }
            }

            // 2. Controle de Volume
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Item {
                        width: 16
                        height: 16
                        SpeakerVectorIcon {
                            anchors.centerIn: parent
                            muted: SystemControl.isMuted
                            iconColor: SystemControl.isMuted ? "#fb7185" : "#17f5df"
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "VOLUME"
                        color: "#90b9cb"
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.8
                    }

                    Text {
                        text: SystemControl.isMuted ? "MUDO" : SystemControl.volume + "%"
                        color: SystemControl.isMuted ? "#fb7185" : "#dffaff"
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                    }

                    // Botão alternar mudo
                    Rectangle {
                        width: 22
                        height: 18
                        radius: 4
                        color: muteMouse.containsMouse ? "#3317f5df" : "#15ffffff"
                        border.color: SystemControl.isMuted ? "#ff4b72" : "#3374d5ee"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: SystemControl.isMuted ? "×" : "◖"
                            color: SystemControl.isMuted ? "#fb7185" : "#17f5df"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        MouseArea {
                            id: muteMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: SystemControl.toggleMute()
                        }
                    }
                }

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: SystemControl.volume
                    onMoved: SystemControl.setVolume(Math.round(value))
                    background: Rectangle {
                        x: volumeSlider.leftPadding
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: volumeSlider.availableWidth
                        height: 12
                        radius: 8
                        color: "#f0031529"
                        border.color: "#4273bada"
                        border.width: 0.7
                        antialiasing: true

                        Rectangle {
                            x: 0
                            y: 0
                            width: Math.max(0, volumeSlider.visualPosition * parent.width)
                            height: parent.height
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: SystemControl.isMuted ? "#64748b" : "#14c4e7" }
                                GradientStop { position: 1.0; color: SystemControl.isMuted ? "#475569" : "#17f5df" }
                            }
                            antialiasing: true
                        }
                    }
                    handle: Item {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition
                           * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: 20
                        height: 20

                        Rectangle {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: 2
                            width: 18
                            height: 18
                            radius: 9
                            color: "#8c000000"
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            radius: 9
                            color: SystemControl.isMuted ? "#94a3b8" : "#ffffff"
                            border.color: "#17f5df"
                            border.width: 1.5
                            antialiasing: true
                        }
                    }
                }
            }

            // 3. Controle de Brilho do Ecrã
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Item {
                        width: 16
                        height: 16
                        SunVectorIcon {
                            anchors.centerIn: parent
                            iconColor: "#facc15"
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "BRILHO DO ECRÃ"
                        color: "#90b9cb"
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.8
                    }

                    Text {
                        text: SystemControl.brightness + "%"
                        color: "#dffaff"
                        font.family: SystaliaTheme.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Slider {
                    id: brightnessSlider
                    Layout.fillWidth: true
                    from: 5
                    to: 100
                    value: SystemControl.brightness
                    onMoved: SystemControl.setBrightness(Math.round(value))
                    background: Rectangle {
                        x: brightnessSlider.leftPadding
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: brightnessSlider.availableWidth
                        height: 12
                        radius: 8
                        color: "#f0031529"
                        border.color: "#4273bada"
                        border.width: 0.7
                        antialiasing: true

                        Rectangle {
                            x: 0
                            y: 0
                            width: Math.max(0, brightnessSlider.visualPosition * parent.width)
                            height: parent.height
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: "#fbbf24" }
                                GradientStop { position: 1.0; color: "#14c4e7" }
                            }
                            antialiasing: true
                        }
                    }
                    handle: Item {
                        x: brightnessSlider.leftPadding + brightnessSlider.visualPosition
                           * (brightnessSlider.availableWidth - width)
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: 20
                        height: 20

                        Rectangle {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: 2
                            width: 18
                            height: 18
                            radius: 9
                            color: "#8c000000"
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            radius: 9
                            color: "#ffffff"
                            border.color: "#fbbf24"
                            border.width: 1.5
                            antialiasing: true
                        }
                    }
                }
            }

            // Separador sutil
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2b4c7388" }

            // 4. Estado da Rede e Conectividade
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5

                Text {
                    text: "CONEXÃO ATIVA"
                    color: "#90b9cb"
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 0.8
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 32
                    radius: 8
                    color: "#180d2235"
                    border.color: "#284d6c82"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: "#17f5df"

                            Rectangle {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                radius: 7
                                color: "#3317f5df"
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: SystemControl.activeNetworkName
                            color: "#ebf9ff"
                            font.family: SystaliaTheme.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: "Conectado"
                            color: "#17f5df"
                            font.family: SystaliaTheme.fontFamily
                            font.pixelSize: 9
                        }
                    }
                }
            }

            // 5. Ações Rápidas de Energia & Sessão (Logind / Systemd)
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: controlCenter.activePowerConfirmAction.length > 0
                          ? "CONFIRMAR AÇÃO" : "ENERGIA & SESSÃO"
                    color: controlCenter.activePowerConfirmAction.length > 0 ? "#f43f5e" : "#90b9cb"
                    font.family: SystaliaTheme.fontFamily
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 0.8
                }

                // Modo normal: 4 botões (Bloquear, Suspender, Reiniciar, Desligar)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: controlCenter.activePowerConfirmAction.length === 0

                    PowerActionButton {
                        Layout.fillWidth: true
                        glyph: "🔒"
                        label: "Bloquear"
                        onClicked: {
                            SystemControl.lockScreen()
                            controlCenter.close()
                        }
                    }

                    PowerActionButton {
                        Layout.fillWidth: true
                        glyph: "💤"
                        label: "Suspender"
                        onClicked: {
                            SystemControl.suspend()
                            controlCenter.close()
                        }
                    }

                    PowerActionButton {
                        Layout.fillWidth: true
                        glyph: "↺"
                        label: "Reiniciar"
                        onClicked: {
                            controlCenter.activePowerConfirmAction = "reboot"
                        }
                    }

                    PowerActionButton {
                        Layout.fillWidth: true
                        glyph: "⏻"
                        label: "Desligar"
                        accentColor: "#f43f5e"
                        onClicked: {
                            controlCenter.activePowerConfirmAction = "poweroff"
                        }
                    }
                }

                // Modo confirmação de energia para evitar desligamento acidental
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: controlCenter.activePowerConfirmAction.length > 0

                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 34
                        background: Rectangle {
                            radius: 8
                            color: "#25000000"
                            border.color: "#4a7e9fc0"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: "Cancelar"
                            color: "#dffaff"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: controlCenter.activePowerConfirmAction = ""
                    }

                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 34
                        background: Rectangle {
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: "#f43f5e" }
                                GradientStop { position: 1.0; color: "#be123c" }
                            }
                            border.color: "#fda4af"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: controlCenter.activePowerConfirmAction === "reboot"
                                  ? "Confirmar Reinício" : "Confirmar Desligar"
                            color: "#ffffff"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (controlCenter.activePowerConfirmAction === "reboot") {
                                SystemControl.reboot()
                            } else {
                                SystemControl.powerOff()
                            }
                            controlCenter.close()
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                text: "VALO OCEANIC GLASS  •  SYSTALIA SDE"
                color: "#658ea4"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // Componente de botão de toggle rápido
    component QuickToggle: Button {
        id: toggleButton
        property string label: ""
        property string sublabel: ""
        property string iconType: "wifi"
        property bool active: false
        implicitHeight: 76
        hoverEnabled: true

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 3

            Item {
                Layout.alignment: Qt.AlignHCenter
                width: 24
                height: 24

                // Ícone Wi-Fi
                WifiVectorIcon {
                    anchors.centerIn: parent
                    visible: toggleButton.iconType === "wifi"
                    iconColor: toggleButton.active ? "#17f5df" : (toggleButton.hovered ? "#cbe6f7" : "#89aabf")
                }

                // Ícone Bluetooth autêntico (desenhado com precisão geométrica)
                BluetoothVectorIcon {
                    anchors.centerIn: parent
                    visible: toggleButton.iconType === "bluetooth"
                    iconColor: toggleButton.active ? "#38bdf8" : (toggleButton.hovered ? "#cbe6f7" : "#89aabf")
                }

                // Ícone Lua / Silencioso
                MoonVectorIcon {
                    anchors.centerIn: parent
                    visible: toggleButton.iconType === "moon"
                    iconColor: toggleButton.active ? "#a78bfa" : (toggleButton.hovered ? "#cbe6f7" : "#89aabf")
                }
            }

            Text {
                Layout.fillWidth: true
                text: toggleButton.label
                color: toggleButton.active ? "#ffffff" : "#b0c9d9"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 10
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: toggleButton.sublabel
                color: toggleButton.active ? "#17f5df" : "#6f8ea1"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }

        background: Item {
            anchors.fill: parent

            // Sombra quando ativo
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 2
                radius: 12
                visible: toggleButton.active
                color: "#5900101d"
            }

            // Fundo ativo
            Rectangle {
                anchors.fill: parent
                radius: 12
                visible: toggleButton.active
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#2d14c4e7" }
                    GradientStop { position: 1.0; color: "#180d2235" }
                }
                border.color: "#8017f5df"
                border.width: 1.2
            }

            // Fundo inativo
            Rectangle {
                anchors.fill: parent
                radius: 12
                visible: !toggleButton.active
                color: toggleButton.hovered ? "#2419354d" : "#140c1e2f"
                border.color: toggleButton.hovered ? "#4a7e9fc0" : "#244d6c82"
                border.width: 1
            }
        }
    }

    // Componente de botão de ação de energia
    component PowerActionButton: Button {
        id: powerBtn
        property string glyph: ""
        property string label: ""
        property color accentColor: "#38bdf8"
        implicitHeight: 46
        hoverEnabled: true

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 1

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: powerBtn.glyph
                color: powerBtn.hovered ? powerBtn.accentColor : "#cadfee"
                font.pixelSize: 13
            }

            Text {
                Layout.fillWidth: true
                text: powerBtn.label
                color: powerBtn.hovered ? "#ffffff" : "#89aabf"
                font.family: SystaliaTheme.fontFamily
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            radius: 8
            color: powerBtn.hovered ? "#251a374f" : "#150b1c2b"
            border.color: powerBtn.hovered ? powerBtn.accentColor : "#244d6c82"
            border.width: 1
            antialiasing: true
        }
    }

    // Ícone Vetorial de Wi-Fi
    component WifiVectorIcon: Canvas {
        id: wifiCanvas
        width: 20
        height: 18
        property color iconColor: "#17f5df"
        onIconColorChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = iconColor
            ctx.lineWidth = 1.8
            ctx.lineCap = "round"

            // Arco exterior
            ctx.beginPath()
            ctx.arc(10, 15, 11, -Math.PI * 0.75, -Math.PI * 0.25, false)
            ctx.stroke()

            // Arco médio
            ctx.beginPath()
            ctx.arc(10, 15, 7.5, -Math.PI * 0.75, -Math.PI * 0.25, false)
            ctx.stroke()

            // Arco inferior
            ctx.beginPath()
            ctx.arc(10, 15, 4, -Math.PI * 0.75, -Math.PI * 0.25, false)
            ctx.stroke()

            // Ponto central
            ctx.fillStyle = iconColor
            ctx.beginPath()
            ctx.arc(10, 15, 1.2, 0, 2 * Math.PI)
            ctx.fill()
        }
    }

    // Ícone Vetorial Autêntico de Bluetooth
    component BluetoothVectorIcon: Canvas {
        id: btCanvas
        width: 16
        height: 20
        property color iconColor: "#38bdf8"
        onIconColorChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = iconColor
            ctx.lineWidth = 1.8
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            var cx = 8
            var topY = 2
            var botY = 18
            var midY = 10
            var w = 5

            // Espinha vertical
            ctx.beginPath()
            ctx.moveTo(cx, topY)
            ctx.lineTo(cx, botY)
            ctx.stroke()

            // Runa contínua
            ctx.beginPath()
            ctx.moveTo(cx - w, topY + 4)
            ctx.lineTo(cx + w, midY + 4)
            ctx.lineTo(cx, botY)
            ctx.lineTo(cx, topY)
            ctx.lineTo(cx + w, midY - 4)
            ctx.lineTo(cx - w, botY - 4)
            ctx.stroke()
        }
    }

    // Ícone Vetorial de Lua / Silencioso
    component MoonVectorIcon: Canvas {
        id: moonCanvas
        width: 18
        height: 18
        property color iconColor: "#a78bfa"
        onIconColorChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = iconColor

            ctx.beginPath()
            ctx.arc(9, 9, 7.5, -Math.PI * 0.4, Math.PI * 0.8, false)
            ctx.bezierCurveTo(4, 13, 4, 5, 13.5, 3.5)
            ctx.closePath()
            ctx.fill()
        }
    }

    // Ícone Vetorial de Alto-Falante (Volume)
    component SpeakerVectorIcon: Canvas {
        id: spkCanvas
        width: 16
        height: 16
        property bool muted: false
        property color iconColor: "#17f5df"
        onMutedChanged: requestPaint()
        onIconColorChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = iconColor
            ctx.strokeStyle = iconColor
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"

            // Cone do alto-falante
            ctx.beginPath()
            ctx.moveTo(2, 6)
            ctx.lineTo(5, 6)
            ctx.lineTo(9, 2.5)
            ctx.lineTo(9, 13.5)
            ctx.lineTo(5, 10)
            ctx.lineTo(2, 10)
            ctx.closePath()
            ctx.fill()

            if (muted) {
                // Barra de mudo
                ctx.strokeStyle = "#fb7185"
                ctx.beginPath()
                ctx.moveTo(11, 5)
                ctx.lineTo(15, 11)
                ctx.moveTo(15, 5)
                ctx.lineTo(11, 11)
                ctx.stroke()
            } else {
                // Ondas sonoras
                ctx.beginPath()
                ctx.arc(9, 8, 4, -Math.PI * 0.3, Math.PI * 0.3, false)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(9, 8, 7, -Math.PI * 0.3, Math.PI * 0.3, false)
                ctx.stroke()
            }
        }
    }

    // Ícone Vetorial de Sol (Brilho)
    component SunVectorIcon: Canvas {
        id: sunCanvas
        width: 16
        height: 16
        property color iconColor: "#facc15"
        onIconColorChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = iconColor
            ctx.fillStyle = iconColor
            ctx.lineWidth = 1.3
            ctx.lineCap = "round"

            var cx = 8
            var cy = 8
            var r = 3

            // Círculo central
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, 2 * Math.PI)
            ctx.fill()

            // 8 Raios
            var rayDistInner = 4.5
            var rayDistOuter = 7
            for (var i = 0; i < 8; ++i) {
                var angle = i * Math.PI / 4
                var cosA = Math.cos(angle)
                var sinA = Math.sin(angle)
                ctx.beginPath()
                ctx.moveTo(cx + cosA * rayDistInner, cy + sinA * rayDistInner)
                ctx.lineTo(cx + cosA * rayDistOuter, cy + sinA * rayDistOuter)
                ctx.stroke()
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: controlCenter.opened
        onActivated: controlCenter.close()
    }
}
