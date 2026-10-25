import QtQuick
import UaApp

Item {
    id: root
    anchors.fill: parent
    signal accepted()

    // Dim backdrop
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
        MouseArea { anchors.fill: parent }  // swallows clicks
    }

    Rectangle {
        anchors.centerIn: parent
        width: 380
        height: 340
        radius: 10
        color: Palette.surface
        border.width: 1
        border.color: Palette.stroke

        Text {
            id: title
            anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 20 }
            text: Strings.t(Strings.DisclaimerTitle)
            color: Palette.text
            font.pixelSize: 16
            font.bold: true
        }
        Flickable {
            anchors { top: title.bottom; left: parent.left; right: parent.right; margins: 20; topMargin: 12 }
            anchors.bottom: acceptBtn.top
            anchors.bottomMargin: 12
            clip: true
            contentWidth: width
            contentHeight: body.implicitHeight
            Text {
                id: body
                width: parent.width
                text: Strings.t(Strings.DisclaimerBody)
                color: Palette.sub
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }
        Rectangle {
            id: acceptBtn
            anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 20 }
            width: 160; height: 32; radius: 6
            color: ma.containsMouse ? Palette.accent : Qt.darker(Palette.accent, 1.1)
            Text {
                anchors.centerIn: parent
                text: Strings.t(Strings.DisclaimerBtn)
                color: "#1A1A1A"
                font.pixelSize: 13
                font.bold: true
            }
            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.accepted()
            }
        }
    }
}
