import QtQuick
import QtQuick.Window
import UaApp

Window {
    id: root
    width: 460
    height: 524
    minimumWidth: 460
    minimumHeight: 524
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"
    visible: true
    title: "Ua Toolbox"

    // Qt would otherwise terminate the process on close; the tray keeps us alive.
    onClosing: (close) => { close.accepted = false; root.hide() }

    // Tray asked to come back
    Connections {
        target: app
        function onShowMainWindowRequested() { root.show(); root.raise(); root.requestActivate() }
    }

    // ---------- state ----------
    property int    activeTab: 0
    property string openDialogItemId: ""
    property bool   skillManagerOpen: false
    property bool   aboutOpen: false
    property bool   settingsMenuOpen: false
    property bool   disclaimerOpen: !Settings.disclaimerAccepted
    property string toastText: ""
    property var    editorModel: app.editorItems
    property var    toolsModel:  app.toolsItems
    property var    promptsSnapshot: app.prompts

    Connections {
        target: app
        function onItemsChanged() {
            root.editorModel = app.editorItems
            root.toolsModel  = app.toolsItems
        }
        function onPromptsChanged()  { root.promptsSnapshot = app.prompts }
        function onToast(text)       { root.showToast(text) }
        function onErrorRaised(text) { root.showToast(text) }
    }

    function showToast(text) {
        toastText = text
        toastTimer.restart()
    }
    Timer { id: toastTimer; interval: 2500; onTriggered: toastText = "" }

    function openInjectDialog(itemId) { openDialogItemId = itemId }
    function closeInjectDialog()      { openDialogItemId = "" }

    // ---------- shell ----------
    Rectangle {
        id: shell
        anchors.fill: parent
        radius: Palette.radius
        color: Qt.rgba(Palette.bg.r, Palette.bg.g, Palette.bg.b, 0.86)
        border.width: 1
        border.color: Palette.stroke

        // Drag area (behind the title bar)
        MouseArea {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 44
            onPressed: (ev) => {
                if (ev.button === Qt.LeftButton) root.startSystemMove()
            }
        }

        // Logo + name
        Row {
            anchors { left: parent.left; top: parent.top; margins: 14 }
            spacing: 10
            Image {
                source: "qrc:/icons/Ua.png"
                sourceSize: Qt.size(28, 28)
                width: 28; height: 28
            }
            Column {
                Text { text: "Ua"; color: Palette.text; font.pixelSize: 16; font.bold: true }
                Text {
                    text: Strings.t(Strings.TrayTooltip)
                    color: Palette.sub
                    font.pixelSize: 11
                }
            }
        }

        // Title buttons
        Row {
            anchors { right: parent.right; top: parent.top; margins: 8 }
            spacing: 4
            z: 10
            TitleButton { glyph: ""; onClicked: root.settingsMenuOpen = !root.settingsMenuOpen }
            TitleButton { glyph: ""; onClicked: root.visibility = Window.Minimized }
            TitleButton { glyph: ""; onClicked: root.hide() }
        }

        TabBar {
            id: tabs
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 56 }
            activeTab: root.activeTab
            onTabClicked: (i) => root.activeTab = i
        }

        // Card list (filtered by active tab so delegates don't collapse-overlap)
        ListView {
            id: content
            anchors {
                top: tabs.bottom
                left: parent.left
                right: parent.right
                bottom: statusBar.top
                topMargin: 8; leftMargin: 14; rightMargin: 14; bottomMargin: 6
            }
            spacing: Palette.gap
            clip: true
            interactive: contentHeight > height
            model: root.activeTab === 0 ? root.editorModel : root.toolsModel
            delegate: Card {
                width: content.width
                height: Palette.cardH
                item: modelData
                onClicked: {
                    if (modelData.isSkillsManager) {
                        root.skillManagerOpen = true
                    } else if (modelData.supported) {
                        root.openInjectDialog(modelData.id)
                    } else if (modelData.url !== "") {
                        app.openUrl(modelData.url)
                    } else {
                        root.showToast(Strings.t(Strings.NoLink))
                    }
                }
            }
        }

        // Status bar
        Rectangle {
            id: statusBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 20
            color: "transparent"
            Text {
                anchors { fill: parent; margins: 10 }
                text: "v" + app.version
                color: Palette.sub
                font.pixelSize: 10
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // ---------- overlays ----------
    SettingsMenu {
        anchors { right: parent.right; top: parent.top; topMargin: 44; rightMargin: 8 }
        open: root.settingsMenuOpen
        onCloseRequested: root.settingsMenuOpen = false
        onAboutRequested: { root.settingsMenuOpen = false; root.aboutOpen = true }
    }

    Loader {
        anchors.fill: parent
        active: root.openDialogItemId !== ""
        sourceComponent: Component {
            InjectDialog {
                itemId: root.openDialogItemId
                onCloseRequested: root.closeInjectDialog()
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: root.skillManagerOpen
        sourceComponent: Component {
            SkillsManager { onCloseRequested: root.skillManagerOpen = false }
        }
    }

    Loader {
        anchors.fill: parent
        active: root.aboutOpen
        sourceComponent: Component {
            AboutCard { onCloseRequested: root.aboutOpen = false }
        }
    }

    Loader {
        anchors.fill: parent
        active: root.disclaimerOpen
        sourceComponent: Component {
            Disclaimer {
                onAccepted: {
                    Settings.disclaimerAccepted = true
                    root.disclaimerOpen = false
                    Updater.check()
                }
            }
        }
    }

    Toast {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 30 }
        text: root.toastText
    }
}
