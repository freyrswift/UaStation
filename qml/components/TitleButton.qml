import QtQuick
import UaApp

Rectangle {
    id: root
    width: 28
    height: 28
    radius: 6
    color: ma.containsMouse
           ? (Palette.dark ? Qt.rgba(1,1,1,0.1) : Qt.rgba(0,0,0,0.06))
           : "transparent"

    property string glyph: ""
    signal clicked()

    IconGlyph {
        anchors.centerIn: parent
        text: root.glyph
        pixelSize: 14
        color: Palette.text
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
