import QtQuick
import UaApp

Item {
    id: root
    anchors.fill: parent
    signal closeRequested()

    property string itemId: ""
    property var    item: itemId !== "" ? (app.items.find(it => it.id === itemId) || ({})) : ({})
    property var    promptList: itemId !== "" ? app.promptsFor(itemId) : []
    // Default selection: prefer <itemId>.md, else first entry.
    property int    selectedPrompt: {
        const preferred = (itemId + ".md").toLowerCase()
        for (let i = 0; i < promptList.length; i++) {
            if (promptList[i].name.toLowerCase() === preferred) return i
        }
        return 0
    }
    property bool   confirmUninstall: false

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        MouseArea { anchors.fill: parent; onClicked: root.closeRequested() }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 60
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
            glyph: ""
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
                        if (!root.item.supported) return (Strings.lang, Strings.t(Strings.NotSupported))
                        if (!root.item.detected)  return (Strings.lang, Strings.t(Strings.ToolNotFound))
                        switch (root.item.state) {
                        case 1:  return (Strings.lang, Strings.t(Strings.Injected))
                        case 2:  return (Strings.lang, Strings.t(Strings.FileHasContent))
                        default: return (Strings.lang, Strings.t(Strings.NotInjected))
                        }
                    }
                    color: Theme.sub
                    font.pixelSize: 11
                }
                // Where this tool actually reads its instructions from, and how
                // the change takes effect — both differ per product.
                Text {
                    width: 292
                    visible: text !== ""
                    text: root.item && root.item.targetRel ? "~/" + root.item.targetRel : ""
                    color: Theme.sub
                    opacity: 0.75
                    font.pixelSize: 9
                    elide: Text.ElideMiddle
                }
                // Phrasing guide (only some products ship one).
                Rectangle {
                    visible: root.item && root.item.hasTips === true
                    height: 20
                    width: tipLabel.implicitWidth + 16
                    radius: 4
                    color: tipMa.containsMouse ? Theme.surfaceAlt : "transparent"
                    border.width: 1
                    border.color: Theme.stroke
                    Text {
                        id: tipLabel
                        anchors.centerIn: parent
                        text: "使用技巧"
                        color: Theme.accent
                        font.pixelSize: 10
                    }
                    MouseArea {
                        id: tipMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: app.openTip(root.itemId)
                    }
                }
            }
        }

        // Prompts list
        Text {
            id: promptsLabel
            anchors { top: parent.top; left: parent.left; margins: 16; topMargin: 104 }
            text: (Strings.lang, Strings.t(Strings.SelectPrompt))
            color: Theme.sub
            font.pixelSize: 11
        }

        ListView {
            id: promptsList
            anchors {
                top: promptsLabel.bottom; left: parent.left; right: parent.right
                margins: 16; topMargin: 6
            }
            height: 206
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
            text: (Strings.lang, Strings.t(Strings.NoPrompts))
            color: Theme.sub
            font.pixelSize: 11
        }

        // Buttons row
        Row {
            anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
            spacing: 10

            DialogButton {
                label: (Strings.lang, Strings.t(Strings.Website))
                actionable: root.item && root.item.url !== ""
                onTriggered: app.openUrl(root.item.url)
            }
            DialogButton {
                label: (Strings.lang, Strings.t(Strings.Uninstall))
                actionable: root.item && root.item.state === 1
                onTriggered: root.confirmUninstall = true
            }
            DialogButton {
                label: (Strings.lang, Strings.t(Strings.Inject))
                primary: true
                actionable: root.item && root.item.detected && root.promptList.length > 0
                onTriggered: {
                    const promptFile = root.promptList[root.selectedPrompt].fullPath
                    if (app.install(root.itemId, promptFile)) {
                        app.showToast(root.item.name + " · " + (root.item.hint || (Strings.lang, Strings.t(Strings.InjectRestartHint))))
                        root.closeRequested()
                    }
                }
            }
            DialogButton {
                label: (Strings.lang, Strings.t(Strings.Open))
                onTriggered: app.revealTargetDir(root.itemId)
            }
        }
    }

    // Uninstall confirmation overlay — covers the dialog area while a decision is pending.
    Rectangle {
        anchors.fill: parent
        visible: root.confirmUninstall
        color: Qt.rgba(0, 0, 0, 0.5)
        MouseArea { anchors.fill: parent }   // swallow clicks

        Rectangle {
            anchors.centerIn: parent
            width: 320
            implicitHeight: col.implicitHeight + 32
            radius: 10
            color: Theme.surface
            border.width: 1
            border.color: Theme.stroke

            Column {
                id: col
                anchors { left: parent.left; right: parent.right; top: parent.top
                          leftMargin: 20; rightMargin: 20; topMargin: 20 }
                spacing: 12
                Text {
                    width: parent.width
                    text: (Strings.lang, Strings.t(Strings.ConfirmUninstall))
                    color: Theme.text
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10
                    DialogButton {
                        label: (Strings.lang, Strings.t(Strings.Uninstall))
                        primary: true
                        onTriggered: {
                            if (app.uninstall(root.itemId)) {
                                app.showToast(root.item.name + (Strings.lang, Strings.t(Strings.UninstallOk)))
                                root.confirmUninstall = false
                                root.closeRequested()
                            } else {
                                root.confirmUninstall = false
                            }
                        }
                    }
                    DialogButton {
                        label: Strings.lang === 0 ? "取消" : "Cancel"
                        onTriggered: root.confirmUninstall = false
                    }
                }
            }
        }
    }

    component DialogButton : Rectangle {
        id: btn
        property string label
        property bool   primary: false
        property bool   actionable: true   // avoid clashing with QQuickItem.enabled
        signal triggered()

        width: 72; height: 30; radius: 6
        color: !btn.actionable
               ? (Theme.dark ? "#333" : "#DDD")
               : btn.primary ? (ma.containsMouse ? Theme.accent : Qt.darker(Theme.accent, 1.1))
                             : (ma.containsMouse ? Theme.surfaceAlt : "transparent")
        border.width: btn.primary ? 0 : 1
        border.color: Theme.stroke
        opacity: btn.actionable ? 1 : 0.6

        Text {
            anchors.centerIn: parent
            text: btn.label
            color: btn.primary ? "#1A1A1A" : Theme.text
            font.pixelSize: 11
            font.bold: btn.primary
        }
        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: btn.actionable ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (btn.actionable) btn.triggered()
        }
    }
}
