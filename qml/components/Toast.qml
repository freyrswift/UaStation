import QtQuick
import UaApp

Rectangle {
    id: root
    property alias text: label.text

    width: label.implicitWidth + 24
    height: 32
    radius: 16
    color: Palette.dark ? Qt.rgba(0.1, 0.1, 0.1, 0.9) : Qt.rgba(0.1, 0.1, 0.1, 0.9)
    visible: label.text !== ""
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 150 } }

    Text {
        id: label
        anchors.centerIn: parent
        color: "white"
        font.pixelSize: 12
    }
}
