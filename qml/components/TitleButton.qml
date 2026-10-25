import QtQuick
import UaApp

// Window-chrome glyph button in the top strip.
// `danger: true` turns hover red (used by the close button).
Rectangle {
    id: root
    width: 30
    height: 28
    radius: 6

    property string glyph: ""
    property bool   danger: false
    signal clicked()

    color: {
        if (!ma.containsMouse) return "transparent"
        if (danger) return "#C42B1C"
        return Palette.dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.05)
    }

    IconGlyph {
        anchors.centerIn: parent
        text: root.glyph
        pixelSize: 12
        color: (root.danger && ma.containsMouse) ? "#FFFFFF"
                                                 : (ma.containsMouse ? Palette.text : Palette.sub)
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
