import QtQuick
import UaApp

Row {
    id: root
    height: 36
    spacing: 20
    anchors.leftMargin: 14

    property int activeTab: 0
    signal tabClicked(int index)

    Repeater {
        model: [Strings.t(Strings.TabEditor), Strings.t(Strings.TabTools)]
        delegate: Item {
            width: labelText.width + 8
            height: root.height
            Text {
                id: labelText
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData
                color: root.activeTab === index ? Palette.text : Palette.sub
                font.pixelSize: 14
                font.bold: root.activeTab === index
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: 24
                height: 2
                radius: 1
                visible: root.activeTab === index
                color: Palette.accent
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.tabClicked(index)
            }
        }
    }
}
