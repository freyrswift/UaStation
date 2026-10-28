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

    // Drive the Theme singleton from Settings. Theme can't `import UaApp`
    // itself without breaking its own property initialisation; see Theme.qml.
    Binding { target: Theme; property: "mode";         value: Settings.themeMode }
    Binding { target: Theme; property: "systemIsDark"; value: Settings.systemIsDark() }

    // ---------- state ----------
    property int    activeTab: 0
    property string openDialogItemId: ""
    property bool   skillManagerOpen: false
    property bool   aboutOpen: false
    property bool   settingsMenuOpen: false
    property bool   disclaimerOpen: !Settings.disclaimerAccepted
    property string toastText: ""
    property string searchFilter: ""
    property var    editorModel: app.editorItems
    property var    toolsModel:  app.toolsItems
    property var    promptsSnapshot: app.prompts
    // Filter helper -- case-insensitive substring match over name + sub.
    function filterItems(list) {
        if (!root.searchFilter) return list
        const q = root.searchFilter.toLowerCase()
        return list.filter(function(it) {
            return (it.name || "").toLowerCase().indexOf(q) >= 0 ||
                   (it.sub  || "").toLowerCase().indexOf(q) >= 0
        })
    }

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
        radius: Theme.radius
        // Fully opaque so Linux / macOS / Win10 (no Mica) don't bleed through.
        // On Win11 the DWM Mica tint is applied *behind* the window; a solid
        // body still reads as Mica because the DWM paints the window chrome
        // area outside our client rect.
        color: Theme.bg
        border.width: 1
        border.color: Theme.stroke

        // Drag area: whole top band behind the tab/button strip
        MouseArea {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 50
            z: -1  // Keep below the tab strip so buttons/tabs still get clicks
            onPressed: (ev) => {
                if (ev.button === Qt.LeftButton) root.startSystemMove()
            }
        }

        // Top strip: pill-style tab bar + three window buttons on one row.
        // Matches the Direct2D DrawTabs: y 14..50, kPad 14 on both sides.
        Rectangle {
            id: topStrip
            anchors { top: parent.top; left: parent.left; right: parent.right
                      topMargin: 14; leftMargin: 14; rightMargin: 14 }
            height: 36
            radius: 8
            color: Theme.dark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.036)

            Row {
                anchors { right: parent.right; top: parent.top; margins: 4 }
                spacing: 0
                TitleButton { glyph: ""; onClicked: root.settingsMenuOpen = !root.settingsMenuOpen }
                TitleButton { glyph: ""; onClicked: root.visibility = Window.Minimized }
                TitleButton { glyph: ""; danger: true; onClicked: root.hide() }
            }

            TabBar {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter
                          leftMargin: 4 }
                anchors.right: parent.right
                anchors.rightMargin: 3 * 30 + 10 // reserve for the three buttons
                activeTab: root.activeTab
                onTabClicked: (i) => root.activeTab = i
            }
        }

        // Divider under the tab strip (mirrors the 1-pixel line in DrawTabs)
        Rectangle {
            id: divider
            anchors { left: parent.left; right: parent.right; top: topStrip.bottom
                      topMargin: 14; leftMargin: 14; rightMargin: 14 }
            height: 1
            color: Theme.dark ? Qt.rgba(1,1,1,0.10) : Qt.rgba(0,0,0,0.06)
        }

        // Search box: filters cards by name + sub, case-insensitive.
        Rectangle {
            id: searchBox
            anchors { left: parent.left; right: parent.right; top: divider.bottom
                      topMargin: 10; leftMargin: 14; rightMargin: 14 }
            height: 30
            radius: 6
            color: Theme.dark ? Qt.rgba(1,1,1,0.05) : Qt.rgba(0,0,0,0.03)
            border.width: 1
            border.color: searchInput.activeFocus ? Theme.accent : Theme.stroke

            IconGlyph {
                id: searchIcon
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                text: ""  // Search glyph
                pixelSize: 12
                color: Theme.sub
            }
            TextInput {
                id: searchInput
                anchors { left: searchIcon.right; right: clearBtn.left
                          verticalCenter: parent.verticalCenter
                          leftMargin: 8; rightMargin: 4 }
                clip: true
                color: Theme.text
                font.pixelSize: 12
                selectByMouse: true
                onTextChanged: root.searchFilter = text
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: searchInput.text === ""
                    text: Strings.lang === 0 ? "搜索工具..." : "Search tools..."
                    color: Theme.sub
                    font.pixelSize: 12
                }
            }
            Rectangle {
                id: clearBtn
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
                width: 20; height: 20; radius: 10
                visible: searchInput.text !== ""
                color: clearMa.containsMouse ? (Theme.dark ? Qt.rgba(1,1,1,0.08)
                                                           : Qt.rgba(0,0,0,0.06))
                                             : "transparent"
                IconGlyph {
                    anchors.centerIn: parent
                    text: ""  // close glyph
                    pixelSize: 10
                    color: Theme.sub
                }
                MouseArea {
                    id: clearMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: searchInput.text = ""
                }
            }
        }

        // Two-column card grid (filtered by active tab + search)
        GridView {
            id: content
            anchors {
                top: searchBox.bottom; left: parent.left; right: parent.right
                bottom: statusBar.top
                topMargin: 14; leftMargin: 14; rightMargin: 14; bottomMargin: 6
            }
            // GridView fits Math.floor(width / cellWidth) columns, so we must
            // make cellWidth × 2 <= width or the second column wraps to a new row.
            cellWidth:  Math.floor(width / 2)
            cellHeight: Theme.cardH + Theme.gap
            clip: true
            interactive: contentHeight > height
            model: root.filterItems(root.activeTab === 0 ? root.editorModel : root.toolsModel)

            delegate: Card {
                width: content.cellWidth - Theme.gap
                height: Theme.cardH
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

        // Status bar: 18px high, subtle tint, right-aligned version
        Rectangle {
            id: statusBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 18
            color: Theme.dark ? Qt.rgba(1,1,1,0.04) : Qt.rgba(0,0,0,0.024)
            Text {
                anchors { fill: parent; rightMargin: Theme.pad; leftMargin: Theme.pad }
                text: "v" + app.version
                color: Theme.sub
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
