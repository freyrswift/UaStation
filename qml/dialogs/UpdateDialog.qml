import QtQuick
import QtQuick.Controls
import UaApp

FocusScope {
    id: root
    anchors.fill: parent
    focus: true

    readonly property bool failed: Updater.error.length > 0

    Keys.onEscapePressed: {
        if (!Updater.applying) Updater.cancel()
    }
    Component.onCompleted: laterButton.forceActiveFocus()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
        MouseArea {
            anchors.fill: parent
            onWheel: (wheel) => wheel.accepted = true
        }
    }

    Rectangle {
        id: panel
        objectName: "updatePanel"
        anchors.centerIn: parent
        width: Math.min(388, root.width - 24)
        height: Math.min(root.failed ? 364 : 286, root.height - 24)
        radius: 8
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke

        Text {
            id: title
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 20 }
            text: (Strings.lang, Strings.t(Strings.UpdateTitle))
            color: Theme.text
            font.pixelSize: 18
            font.bold: true
            wrapMode: Text.Wrap
        }
        Text {
            id: version
            objectName: "updateVersion"
            anchors { top: title.bottom; left: title.left; right: title.right; topMargin: 8 }
            text: (Strings.lang, Strings.t(Strings.UpdateVersionLine))
                .arg(Updater.latestVersion).arg(app.version)
            color: Theme.sub
            font.pixelSize: 12
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
        }
        Flickable {
            id: statusScroll
            objectName: "updateStatusScroll"
            anchors { top: version.bottom; left: title.left; right: title.right; topMargin: 20 }
            anchors.bottom: progress.top
            anchors.bottomMargin: 16
            clip: true
            contentWidth: width
            contentHeight: statusContent.height
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: statusContent
                width: statusScroll.width - 12
                spacing: 10
                Text {
                    objectName: "updateStatus"
                    width: parent.width
                    text: {
                        if (root.failed)
                            return (Strings.lang, Strings.t(Strings.UpdateFailedTitle))
                        if (Updater.applying)
                            return (Strings.lang, Strings.t(Strings.UpdateApplying))
                        if (Updater.ready && Updater.operationBusy)
                            return (Strings.lang, Strings.t(Strings.UpdateWaitingForTask))
                        if (Updater.downloading)
                            return (Strings.lang, Strings.t(Strings.UpdateDownloading))
                                + " " + Updater.downloadProgress + "%"
                        if (Updater.ready)
                            return (Strings.lang, Strings.t(Updater.supportsAutoInstall ? Strings.UpdateReady : Strings.UpdateReadyManual))
                        return (Strings.lang, Strings.t(Strings.UpdateNew)) + Updater.latestVersion
                    }
                    color: Theme.text
                    font.pixelSize: 13
                    font.bold: true
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
                Text {
                    objectName: "updateError"
                    width: parent.width
                    visible: root.failed
                    text: Updater.error
                    color: Theme.sub
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
                Text {
                    width: parent.width
                    visible: !root.failed
                    text: (Strings.lang, Strings.t(Updater.supportsAutoInstall ? Strings.UpdateRestartNotice : Strings.UpdateManualInstallNotice))
                    color: Theme.sub
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }
        ProgressBar {
            id: progress
            objectName: "updateProgress"
            property real indicatorPosition: 0
            anchors { left: title.left; right: title.right; bottom: releaseButton.top; bottomMargin: 12 }
            height: 6
            visible: !root.failed
            from: 0
            to: 100
            value: Updater.downloadProgress
            indeterminate: Updater.applying || (!Updater.downloading && !Updater.ready)
            NumberAnimation on indicatorPosition {
                from: -progress.width * 0.35
                to: progress.width
                duration: 1100
                loops: Animation.Infinite
                running: progress.visible && progress.indeterminate
            }
            background: Rectangle {
                implicitHeight: 6
                radius: 3
                color: Theme.stroke
            }
            contentItem: Item {
                clip: true
                Rectangle {
                    id: progressFill
                    x: progress.indeterminate ? progress.indicatorPosition : 0
                    width: progress.indeterminate ? parent.width * 0.35 : parent.width * progress.visualPosition
                    height: parent.height
                    radius: 3
                    color: Theme.accent
                }
            }
        }
        Button {
            id: releaseButton
            objectName: "updateReleasePage"
            anchors { left: title.left; right: title.right; bottom: actions.top; bottomMargin: 10 }
            height: root.failed ? 30 : 0
            visible: root.failed
            text: (Strings.lang, Strings.t(Strings.UpdateReleasePage))
            onClicked: Updater.openReleasesPage()
            background: Rectangle { color: "transparent" }
            contentItem: Text {
                text: releaseButton.text
                color: Theme.accent
                font.pixelSize: 12
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
        Row {
            id: actions
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 20 }
            height: 36
            spacing: 12
            Button {
                id: laterButton
                objectName: "updateLater"
                width: retryButton.visible ? (actions.width - actions.spacing) / 2 : actions.width
                height: actions.height
                enabled: !Updater.applying
                text: (Strings.lang, Strings.t(Strings.UpdateLater))
                onClicked: Updater.cancel()
                background: Rectangle {
                    radius: 6
                    color: laterButton.hovered ? Theme.stroke : Theme.surfaceAlt
                    border.color: laterButton.activeFocus ? Theme.accent : Theme.stroke
                }
                contentItem: Text {
                    text: laterButton.text
                    color: laterButton.enabled ? Theme.text : Theme.sub
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Button {
                id: retryButton
                objectName: "updateRetry"
                visible: root.failed || (Updater.ready && !Updater.supportsAutoInstall)
                width: (actions.width - actions.spacing) / 2
                height: actions.height
                enabled: !Updater.applying
                text: (Strings.lang, Strings.t(root.failed ? Strings.UpdateRetry : Strings.UpdateReadyManual))
                onClicked: {
                    if (!Updater.supportsAutoInstall && Updater.ready && !root.failed) Updater.applyUpdate()
                    else Updater.startAutomaticUpdate()
                }
                background: Rectangle {
                    radius: 6
                    color: retryButton.hovered ? Qt.darker(Theme.accent, 1.1) : Theme.accent
                    border.width: retryButton.activeFocus ? 2 : 0
                    border.color: Theme.text
                }
                contentItem: Text {
                    text: retryButton.text
                    color: "#1A1A1A"
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }
}
