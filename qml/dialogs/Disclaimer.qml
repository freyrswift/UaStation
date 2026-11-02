import QtQuick
import QtQuick.Controls
import UaApp

FocusScope {
    id: root
    anchors.fill: parent
    focus: true
    signal accepted()
    signal exitRequested()

    function confirm() {
        if (readConfirmation.checked) root.accepted()
    }
    Keys.onEscapePressed: root.exitRequested()
    Component.onCompleted: readConfirmation.forceActiveFocus()

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
        objectName: "noticePanel"
        anchors.centerIn: parent
        width: Math.min(404, root.width - 24)
        height: Math.min(476, root.height - 24)
        radius: 8
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke

        Text {
            id: title
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 20 }
            text: (Strings.lang, Strings.t(Strings.DisclaimerTitle))
            color: Theme.text
            font.pixelSize: 18
            font.bold: true
            wrapMode: Text.Wrap
        }
        Text {
            id: updated
            anchors { top: title.bottom; left: title.left; right: title.right; topMargin: 6 }
            text: (Strings.lang, Strings.t(Strings.NoticeUpdated))
            color: Theme.sub
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
        Flickable {
            id: noticeScroll
            objectName: "noticeScroll"
            anchors { top: updated.bottom; left: parent.left; right: parent.right; margins: 20; topMargin: 14 }
            anchors.bottom: readConfirmation.top
            anchors.bottomMargin: 12
            clip: true
            contentWidth: width
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Text {
                id: body
                objectName: "noticeBody"
                width: noticeScroll.width - 14
                text: (Strings.lang, Strings.t(Strings.DisclaimerBody))
                color: Theme.text
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                lineHeight: 1.3
            }
        }
        CheckBox {
            id: readConfirmation
            objectName: "noticeReadConfirmation"
            anchors { left: parent.left; right: parent.right; bottom: actions.top; margins: 20; bottomMargin: 12 }
            padding: 0
            spacing: 8
            checked: false
            text: (Strings.lang, Strings.t(Strings.NoticeReadConfirm))
            KeyNavigation.tab: exitButton
            KeyNavigation.backtab: acceptButton.enabled ? acceptButton : exitButton
            indicator: Rectangle {
                x: 0
                y: (readConfirmation.height - height) / 2
                width: 18; height: 18; radius: 3
                color: readConfirmation.checked ? Theme.accent : Theme.surfaceAlt
                border.color: readConfirmation.activeFocus ? Theme.accent : Theme.sub
                border.width: readConfirmation.activeFocus ? 2 : 1
                Rectangle {
                    anchors.centerIn: parent
                    width: 8; height: 8; radius: 1
                    color: "#1A1A1A"
                    visible: readConfirmation.checked
                }
            }
            contentItem: Text {
                leftPadding: readConfirmation.indicator.width + readConfirmation.spacing
                text: readConfirmation.text
                color: Theme.text
                font.pixelSize: 12
                wrapMode: Text.Wrap
                verticalAlignment: Text.AlignVCenter
            }
        }
        Row {
            id: actions
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 20 }
            height: 36
            spacing: 12
            Button {
                id: exitButton
                objectName: "noticeExit"
                width: (actions.width - actions.spacing) / 2
                height: actions.height
                text: (Strings.lang, Strings.t(Strings.Exit))
                KeyNavigation.tab: acceptButton.enabled ? acceptButton : readConfirmation
                KeyNavigation.backtab: readConfirmation
                onClicked: root.exitRequested()
                background: Rectangle {
                    radius: 6
                    color: exitButton.hovered ? Theme.stroke : Theme.surfaceAlt
                    border.color: exitButton.activeFocus ? Theme.accent : Theme.stroke
                }
                contentItem: Text {
                    text: exitButton.text
                    color: Theme.text
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Button {
                id: acceptButton
                objectName: "noticeContinue"
                width: (actions.width - actions.spacing) / 2
                height: actions.height
                enabled: readConfirmation.checked
                text: (Strings.lang, Strings.t(Strings.DisclaimerBtn))
                KeyNavigation.tab: readConfirmation
                KeyNavigation.backtab: exitButton
                onClicked: root.confirm()
                background: Rectangle {
                    radius: 6
                    color: !acceptButton.enabled ? Theme.stroke :
                           acceptButton.hovered ? Qt.darker(Theme.accent, 1.1) : Theme.accent
                    border.width: acceptButton.activeFocus ? 2 : 0
                    border.color: Theme.text
                }
                contentItem: Text {
                    text: acceptButton.text
                    color: acceptButton.enabled ? "#1A1A1A" : Theme.sub
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
