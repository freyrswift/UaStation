import QtQuick
import UaApp

// Rounded card: 28×28 logo/glyph tile, title + sub stacked, chevron on
// the right. Matches ui-main.cpp DrawCard().
Rectangle {
    id: root
    radius: 8
    border.width: 1
    border.color: Theme.dark ? Qt.rgba(1,1,1, ma.containsMouse ? 0.24 : 0.12)
                               : Qt.rgba(0,0,0, ma.containsMouse ? 0.16 : 0.08)
    color: Theme.dark ? Qt.rgba(1,1,1, ma.containsMouse ? 0.14 : 0.05)
                        : Qt.rgba(0,0,0, ma.containsMouse ? 0.06 : 0.025)

    property var item: ({})
    signal clicked()

    Behavior on color { ColorAnimation { duration: 90 } }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    // Logo/glyph tile
    Rectangle {
        id: tile
        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
        width: 28; height: 28
        radius: 6
        color: {
            if (root.item && root.item.framed) return "transparent"
            return Theme.dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.048)
        }

        Image {
            anchors.fill: parent
            anchors.margins: root.item && root.item.framed ? 0 : 4
            source: root.item ? (root.item.logo || "") : ""
            visible: root.item && root.item.logo && root.item.logo !== ""
            fillMode: Image.PreserveAspectFit
            smooth: true
            // Rasterise SVG at a comfortable size so scaling stays crisp
            sourceSize: Qt.size(64, 64)
        }
        IconGlyph {
            anchors.centerIn: parent
            visible: root.item && (!root.item.logo || root.item.logo === "") && root.item.glyph !== ""
            text: root.item && root.item.glyph
                  ? String.fromCharCode(parseInt(root.item.glyph, 16))
                  : ""
            pixelSize: 15
            color: Theme.text
        }

        // Status indicator: colored dot at the tile's bottom-right.
        // Green = installed OK, Orange = installed but host upgraded (needs reinstall),
        // Yellow = detected but not installed, hidden if not detected.
        Rectangle {
            visible: root.item && root.item.supported && root.item.detected
            width: 10; height: 10; radius: 5
            anchors { right: parent.right; bottom: parent.bottom; rightMargin: -2; bottomMargin: -2 }
            color: {
                if (!root.item) return "#4CAF50"
                if (root.item.state === 1) {
                    if (root.item.upgraded) return "#F97316"
                    return "#4CAF50"
                }
                return "#FF9800"
            }
            border.width: 2
            border.color: root.color
        }
    }

    // Chevron right (declared BEFORE the text Column so anchoring resolves properly)
    IconGlyph {
        id: chevron
        anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 10 }
        width: 16; height: 16
        text: ""
        pixelSize: 11
        color: ma.containsMouse ? Theme.text : Theme.sub
    }

    // Name + sub
    Column {
        anchors { left: tile.right; leftMargin: 10
                  right: chevron.left; rightMargin: 8
                  verticalCenter: parent.verticalCenter }
        spacing: 2
        Text {
            width: parent.width
            text: root.item ? (root.item.name || "") : ""
            color: Theme.text
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: {
                if (!root.item) return ""
                if (!root.item.supported) return root.item.sub || ""
                if (!root.item.detected)  return (Strings.lang, Strings.t(Strings.ToolNotFound))
                if (root.item.state === 1 && root.item.upgraded)
                    return Strings.lang === 0 ? "宿主已升级，需重装" : "Host upgraded, reinstall"
                switch (root.item.state) {
                case 1:  return (Strings.lang, Strings.t(Strings.Injected))
                case 2:  return (Strings.lang, Strings.t(Strings.FileHasContent))
                default: return (Strings.lang, Strings.t(Strings.NotInjected))
                }
            }
            color: Theme.sub
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
