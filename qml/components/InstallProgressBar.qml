// InstallProgressBar.qml - Modal progress overlay shown while Injector::install
// is running on a worker thread. Fed by AppController's installProgress signal.

import QtQuick
import UaApp

Item {
    id: root
    anchors.fill: parent

    property string itemName: ""
    property string currentPath: ""
    property int current: 0
    property int total: 0
    property bool uninstalling: false

    // Dim + swallow clicks underneath.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.4)
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 100
        width: 380
        height: 180
        radius: 10
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        MouseArea { anchors.fill: parent }

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                text: root.itemName + " · " + (root.uninstalling
                        ? (Strings.lang === 0 ? "正在卸载" : "Uninstalling")
                        : (Strings.lang === 0 ? "正在安装" : "Installing"))
                color: Theme.text
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                width: parent.width
                text: root.currentPath
                color: Theme.sub
                font.pixelSize: 10
                elide: Text.ElideMiddle
            }

            // Progress track + fill
            Rectangle {
                width: parent.width
                height: 6
                radius: 3
                color: Theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
                Rectangle {
                    width: parent.width * (root.total > 0 ? root.current / root.total : 0)
                    height: parent.height
                    radius: 3
                    color: Theme.accent
                    Behavior on width { NumberAnimation { duration: 150 } }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.current + " / " + root.total
                color: Theme.sub
                font.pixelSize: 11
            }
        }
    }
}
