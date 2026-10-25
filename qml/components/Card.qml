import QtQuick
import UaApp

Rectangle {
    id: root
    radius: 8
    border.width: 1
    border.color: Palette.stroke
    color: ma.containsMouse ? Palette.surface : Palette.surfaceAlt

    property var item: ({})
    signal clicked()

    Behavior on color { ColorAnimation { duration: 100 } }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Row {
        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
        spacing: 10

        // Logo tile (SVG) or glyph tile
        Rectangle {
            width: 32; height: 32; radius: 6
            color: root.item && root.item.framed ? "transparent"
                                                  : (Palette.dark ? "#3A3A3A" : "#EAEAEA")
            visible: !!(root.item && (root.item.logo || root.item.glyph))

            Image {
                anchors.fill: parent
                anchors.margins: 1
                visible: root.item && root.item.logo !== ""
                source: root.item ? root.item.logo : ""
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            IconGlyph {
                anchors.centerIn: parent
                visible: root.item && root.item.logo === "" && root.item.glyph !== ""
                text: root.item ? String.fromCharCode(parseInt(root.item.glyph || "0", 16)) : ""
                pixelSize: 18
                color: Palette.text
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: root.item ? root.item.name : ""
                color: Palette.text
                font.pixelSize: 13
                font.bold: true
            }
            Text {
                text: {
                    if (!root.item) return ""
                    if (root.item.sub && root.item.sub !== "") return root.item.sub
                    if (!root.item.supported) return Strings.t(Strings.NotSupported)
                    if (!root.item.detected)  return Strings.t(Strings.ToolNotFound)
                    switch (root.item.state) {
                    case 1:  return Strings.t(Strings.Injected)
                    case 2:  return Strings.t(Strings.FileHasContent)
                    default: return Strings.t(Strings.NotInjected)
                    }
                }
                color: Palette.sub
                font.pixelSize: 11
            }
        }
    }

    // State dot
    Rectangle {
        anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 14 }
        width: 10; height: 10; radius: 5
        visible: root.item && root.item.supported
        color: {
            if (!root.item) return "transparent"
            switch (root.item.state) {
            case 1: return Palette.green
            case 2: return "#F0C000"
            default: return Palette.dark ? "#555" : "#CCC"
            }
        }
    }
}
