import QtQuick
import UaApp

Item {
    id: root
    anchors.fill: parent
    signal closeRequested()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        MouseArea { anchors.fill: parent; onClicked: root.closeRequested() }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 60
        width: 320
        height: 240
        radius: 10
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke

        // Close
        TitleButton {
            anchors { top: parent.top; right: parent.right; margins: 6 }
            glyph: ""
            onClicked: root.closeRequested()
        }

        Column {
            anchors.centerIn: parent
            spacing: 10
            width: parent.width - 40

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "qrc:/icons/Ua.png"
                sourceSize: Qt.size(48, 48)
                width: 48; height: 48
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: (Strings.lang, Strings.t(Strings.AboutText)) + app.version
                color: Theme.text
                font.pixelSize: 13
                font.bold: true
            }
            Text {
                width: parent.width
                text: (Strings.lang, Strings.t(Strings.AboutLegal))
                color: Theme.sub
                font.pixelSize: 11
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                Rectangle {
                    width: 110; height: 28; radius: 6
                    color: ghMa.containsMouse ? Theme.surfaceAlt : "transparent"
                    border.width: 1; border.color: Theme.stroke
                    Text {
                        anchors.centerIn: parent
                        text: (Strings.lang, Strings.t(Strings.GithubLabel))
                        color: Theme.text
                        font.pixelSize: 11
                    }
                    MouseArea {
                        id: ghMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: app.openUrl("https://github.com/freyrswift/UaStation")
                    }
                }
                Rectangle {
                    width: 140; height: 28; radius: 6
                    color: upMa.containsMouse ? Theme.accent : Qt.darker(Theme.accent, 1.15)
                    Text {
                        anchors.centerIn: parent
                        text: {
                            if (Updater.checking)
                                return (Strings.lang, Strings.t(Strings.UpdateCheck))
                            if (Updater.downloading)
                                return (Strings.lang, Strings.t(Strings.UpdateDownloading))
                                       + " " + Updater.downloadProgress + "%"
                            if (Updater.ready)
                                return (Strings.lang, Strings.t(Strings.UpdateReady))
                            if (Updater.available)
                                return (Strings.lang, Strings.t(Strings.UpdateBtn))
                                       + " " + Updater.latestVersion
                            if (Updater.checked)
                                return (Strings.lang, Strings.t(Strings.UpdateLatest))
                            return (Strings.lang, Strings.t(Strings.CheckUpdate))
                        }
                        color: "#1A1A1A"
                        font.pixelSize: 11
                        font.bold: true
                    }
                    MouseArea {
                        id: upMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (Updater.downloading)        return
                            if (Updater.ready)              { Updater.applyUpdate(); return }
                            if (Updater.available)          { Updater.download();    return }
                            Updater.check()
                        }
                    }
                }
            }
        }
    }
}
