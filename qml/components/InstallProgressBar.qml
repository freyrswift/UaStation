// InstallProgressBar.qml - Modal progress overlay shown while Injector::install
// is running on a worker thread. Fed by AppController's installProgress signal.

import QtQuick
import QtQuick.Controls as Controls
import UaApp

Item {
    id: root
    objectName: "installProgressOverlay"
    anchors.fill: parent

    property string itemName: ""
    property string currentPath: ""
    property int current: 0
    property int total: 0
    property bool uninstalling: false
    readonly property real fraction: total > 0 ? Math.max(0, Math.min(1, current / total)) : 0

    // Dim + swallow clicks underneath.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.4)
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(380, parent.width - 32)
        height: content.implicitHeight + 40
        radius: 8
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        MouseArea { anchors.fill: parent }

        Column {
            id: content
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
            spacing: 14

            Text {
                objectName: "installProgressTitle"
                width: parent.width
                text: root.itemName + " · " + (root.uninstalling
                        ? (Strings.lang === 0 ? "正在卸载" : "Uninstalling")
                        : (Strings.lang === 0 ? "正在安装" : "Installing"))
                color: Theme.text
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                objectName: "installProgressPath"
                width: parent.width
                text: root.currentPath || (Strings.lang === 0 ? "正在准备…" : "Preparing...")
                color: Theme.sub
                font.pixelSize: 10
                elide: Text.ElideMiddle
            }

            // Progress track + fill
            Controls.ProgressBar {
                id: bar
                objectName: "installProgressBar"
                width: parent.width
                height: 6
                from: 0; to: 1; value: root.fraction
                indeterminate: root.total <= 0
                padding: 0
                background: Rectangle {
                    radius: 3
                    color: Theme.dark ? "#464646" : "#DDDDDD"
                }
                contentItem: Item {
                    clip: true
                    Rectangle {
                        width: bar.indeterminate ? parent.width * 0.3 : parent.width * root.fraction
                        height: parent.height; radius: 3; color: Theme.accent
                        x: bar.indeterminate ? (parent.width - width) * motion.position : 0
                        QtObject { id: motion; property real position: 0 }
                        SequentialAnimation {
                            running: bar.indeterminate && root.visible
                            loops: Animation.Infinite
                            NumberAnimation { target: motion; property: "position"; from: 0; to: 1; duration: 700; easing.type: Easing.InOutSine }
                            NumberAnimation { target: motion; property: "position"; from: 1; to: 0; duration: 700; easing.type: Easing.InOutSine }
                        }
                        Behavior on width { enabled: !bar.indeterminate; NumberAnimation { duration: 100 } }
                    }
                }
            }

            Text {
                objectName: "installProgressLabel"
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.total > 0 ? Math.floor(root.fraction * 100) + "%  (" + Math.min(root.current, root.total) + " / " + root.total + ")"
                    : (Strings.lang === 0 ? "正在处理…" : "Working...")
                color: Theme.sub
                font.pixelSize: 11
            }
        }
    }
}
