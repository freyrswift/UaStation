import QtQuick
import UaApp

Item {
    id: root
    anchors.fill: parent
    signal closeRequested()

    property string itemId: ""
    property var    item: itemId !== "" ? (app.items.find(it => it.id === itemId) || ({})) : ({})
    property int    selectedPrompt: 0
    property var    promptList: app.prompts

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        MouseArea { anchors.fill: parent; onClicked: root.closeRequested() }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 380
        height: 440
        radius: 10
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        MouseArea { anchors.fill: parent }  // block click-through

        // Close
        TitleButton {
            anchors { top: parent.top; right: parent.right; margins: 6 }
            glyph: ""
            onClicked: root.closeRequested()
        }

        // Logo + title
        Row {
            anchors { top: parent.top; left: parent.left; margins: 16 }
            spacing: 12
            Rectangle {
                width: 36; height: 36; radius: 6
                color: root.item && root.item.framed
                       ? "transparent"
                       : (Theme.dark ? "#3A3A3A" : "#EAEAEA")
                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: root.item ? root.item.logo || "" : ""
                    fillMode: Image.PreserveAspectFit
                    visible: root.item && root.item.logo !== ""
                }
            }
            Column {
                spacing: 2
                Text {
                    text: root.item ? root.item.name : ""
                    color: Theme.text
                    font.pixelSize: 14
                    font.bold: true
                }
                Text {
                    text: {
                        if (!root.item) return ""
                        if (!root.item.supported) return Strings.t(Strings.NotSupported)
                        if (!root.item.detected)  return Strings.t(Strings.ToolNotFound)
                        switch (root.item.state) {
                        case 1:  return Strings.t(Strings.Injected)
                        case 2:  return Strings.t(Strings.FileHasContent)
                        default: return Strings.t(Strings.NotInjected)
                        }
                    }
                    color: Theme.sub
                    font.pixelSize: 11
                }
            }
        }

        // Prompts list
        Text {
            id: promptsLabel
            anchors { top: parent.top; left: parent.left; margins: 16; topMargin: 72 }
            text: Strings.t(Strings.SelectPrompt)
            color: Theme.sub
            font.pixelSize: 11
        }

        ListView {
            id: promptsList
            anchors {
                top: promptsLabel.bottom; left: parent.left; right: parent.right
                margins: 16; topMargin: 6
            }
            height: 240
            clip: true
            model: root.promptList
            delegate: Rectangle {
                width: promptsList.width
                height: 36
                radius: 6
                color: index === root.selectedPrompt
                       ? Theme.accent
                       : (hoverMa.containsMouse ? Theme.surfaceAlt : "transparent")
                border.width: 1
                border.color: index === root.selectedPrompt ? Theme.accent : Theme.stroke

                Text {
                    anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 12 }
                    text: modelData.name
                    color: index === root.selectedPrompt ? "#1A1A1A" : Theme.text
                    font.pixelSize: 12
                }
                MouseArea {
                    id: hoverMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selectedPrompt = index
                }
            }
        }

        Text {
            anchors { top: promptsList.bottom; horizontalCenter: parent.horizontalCenter; topMargin: 4 }
            visible: root.promptList.length === 0
            text: Strings.t(Strings.NoPrompts)
            color: Theme.sub
            font.pixelSize: 11
        }

        // Buttons row
        Row {
            anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
            spacing: 10

            DialogButton {
                label: Strings.t(Strings.Website)
                enabled: root.item && root.item.url !== ""
                onTriggered: app.openUrl(root.item.url)
            }
            DialogButton {
                label: Strings.t(Strings.Uninstall)
                enabled: root.item && root.item.state === 1
                onTriggered: {
                    if (app.uninstall(root.itemId)) {
                        app.showToast(root.item.name + Strings.t(Strings.UninstallOk))
                        root.closeRequested()
                    }
                }
            }
            DialogButton {
                label: Strings.t(Strings.Inject)
                primary: true
                enabled: root.item && root.item.detected && root.promptList.length > 0
                onTriggered: {
                    const promptFile = root.promptList[root.selectedPrompt].fullPath
                    if (app.install(root.itemId, promptFile)) {
                        app.showToast(root.item.name + Strings.t(Strings.InjectRestartHint))
                        root.closeRequested()
                    }
                }
            }
            DialogButton {
                label: Strings.t(Strings.Open)
                onTriggered: app.revealTargetDir(root.itemId)
            }
        }
    }

    component DialogButton : Rectangle {
        property string label
        property bool  primary: false
        property bool  enabled: true
        signal triggered()

        width: 72; height: 30; radius: 6
        color: !enabled
               ? (Theme.dark ? "#333" : "#DDD")
               : primary ? (ma.containsMouse ? Theme.accent : Qt.darker(Theme.accent, 1.1))
                         : (ma.containsMouse ? Theme.surfaceAlt : "transparent")
        border.width: primary ? 0 : 1
        border.color: Theme.stroke

        Text {
            anchors.centerIn: parent
            text: label
            color: primary ? "#1A1A1A" : Theme.text
            font.pixelSize: 11
            font.bold: primary
        }
        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (enabled) parent.triggered()
        }
    }
}
