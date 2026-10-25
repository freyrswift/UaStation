import QtQuick
import UaApp

// Pill-strip tab bar matching the Direct2D DrawTabs layout. The parent
// Rectangle in Main.qml paints the outer bar; this fills it with equally
// sized tabs and highlights the selected one with an inner pill.
Item {
    id: root
    height: 28

    property int activeTab: 0
    signal tabClicked(int index)

    Row {
        anchors.fill: parent
        Repeater {
            model: [(Strings.lang, Strings.t(Strings.TabEditor)), (Strings.lang, Strings.t(Strings.TabTools))]
            delegate: Rectangle {
                width: root.width / 2 - 4
                height: root.height
                radius: 6
                color: {
                    if (root.activeTab === index) {
                        return Theme.dark ? Qt.rgba(1,1,1,0.11) : Qt.rgba(0,0,0,0.066)
                    }
                    return ma.containsMouse
                        ? (Theme.dark ? Qt.rgba(1,1,1,0.05) : Qt.rgba(0,0,0,0.03))
                        : "transparent"
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: 13
                    font.bold: root.activeTab === index
                    color: (root.activeTab === index || ma.containsMouse) ? Theme.text : Theme.sub
                }
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.tabClicked(index)
                }
            }
        }
    }
}
