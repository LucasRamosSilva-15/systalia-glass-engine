import QtQuick
import QtQuick.Controls

ToolButton {
    id: action

    property string glyph: "•"
    property string accessibleName: "Ação da janela"
    property bool closeAction: false

    Accessible.name: accessibleName
    width: 28
    height: 28
    hoverEnabled: true
    ToolTip.visible: hovered
    ToolTip.text: accessibleName
    ToolTip.delay: 450

    background: Rectangle {
        anchors.centerIn: parent
        width: 28
        height: 28
        color: "transparent"

        Image {
            anchors.centerIn: parent
            width: 38
            height: 38
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-window-action-"
                    + (action.closeAction ? "close" : action.glyph === "−" ? "minimize" : "maximize")
                    + "-bg.png"
            sourceSize: Qt.size(128, 128)
            smooth: true
            mipmap: true
            opacity: action.hovered ? 0.92 : 1
        }
    }

    contentItem: Item {
        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: "qrc:/qt/qml/Systalia/Engine/src/qml/assets/figma/valo-window-"
                    + (action.closeAction ? "close" : action.glyph === "−" ? "minimize" : "maximize")
                    + ".png"
            sourceSize: Qt.size(512, 512)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            opacity: action.hovered ? 0.92 : 1
        }
    }
}
