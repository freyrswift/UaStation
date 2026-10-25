import QtQuick
import UaApp

Rectangle {
    id: root
    width: 196
    radius: 8
    border.width: 1
    border.color: Theme.stroke
    color: Theme.surface
    visible: open
    opacity: open ? 1 : 0
    scale: open ? 1 : 0.95
    transformOrigin: Item.TopRight

    property bool open: false
    signal closeRequested()
    signal aboutRequested()

    Behavior on opacity { NumberAnimation { duration: 120 } }
    Behavior on scale   { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

    // Click-outside to close
    MouseArea {
        id: outside
        parent: root.parent
        anchors.fill: parent
        enabled: root.open
        z: root.z - 1
        onClicked: root.closeRequested()
    }

    height: layout.implicitHeight + 12

    Column {
        id: layout
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 }
        spacing: 2

        // Theme (radio: three mutually-exclusive)
        MenuRow {
            label: Strings.t(Strings.ThemeSystem); glyph: ""
            selected: Settings.themeMode === 0
            onTriggered: { Settings.themeMode = 0 }
        }
        MenuRow {
            label: Strings.t(Strings.ThemeDark); glyph: ""
            selected: Settings.themeMode === 1
            onTriggered: { Settings.themeMode = 1 }
        }
        MenuRow {
            label: Strings.t(Strings.ThemeLight); glyph: ""
            selected: Settings.themeMode === 2
            onTriggered: { Settings.themeMode = 2 }
        }
        Separator {}

        // Language (radio: two)
        MenuRow {
            label: "中文"; glyph: ""
            selected: Strings.lang === 0
            onTriggered: { Strings.lang = 0 }
        }
        MenuRow {
            label: "English"; glyph: ""
            selected: Strings.lang === 1
            onTriggered: { Strings.lang = 1 }
        }
        Separator {}

        // Toggles (show on/off state)
        MenuRow {
            label: Strings.t(Strings.Topmost); glyph: ""
            toggled: Settings.topmost
            onTriggered: { Settings.topmost = !Settings.topmost }
        }
        MenuRow {
            label: Strings.t(Strings.Autostart); glyph: ""
            toggled: Settings.autostart
            onTriggered: { Settings.autostart = !Settings.autostart }
        }
        Separator {}

        // Actions
        MenuRow {
            label: Updater.checking ? Strings.t(Strings.UpdateCheck) : Strings.t(Strings.CheckUpdate)
            glyph: ""
            onTriggered: { Updater.check(); root.closeRequested() }
        }
        MenuRow {
            label: Strings.t(Strings.CleanData); glyph: ""
            onTriggered: { app.showToast("TODO: clean data"); root.closeRequested() }
        }
        MenuRow {
            label: Strings.t(Strings.About); glyph: ""
            onTriggered: root.aboutRequested()
        }
        MenuRow {
            label: Strings.t(Strings.Exit); glyph: ""
            onTriggered: app.quit()
        }
    }

    // ---- inline components ----

    component MenuRow : Rectangle {
        id: row
        property string label
        property string glyph
        property bool   selected: false   // single-of-N indicator (radio-like)
        property bool   toggled:  false   // boolean on/off indicator
        signal triggered()

        width: parent.width
        height: 30
        radius: 5

        color: {
            if (rowMa.pressed) return Theme.dark ? Qt.rgba(1,1,1,0.14) : Qt.rgba(0,0,0,0.08)
            if (rowMa.containsMouse) return Theme.dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.04)
            if (row.selected) return Theme.dark ? Qt.rgba(1,1,1,0.05) : Qt.rgba(0,0,0,0.025)
            return "transparent"
        }

        // Leading glyph
        IconGlyph {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
            text: row.glyph
            pixelSize: 13
            color: Theme.text
        }
        // Label
        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 34
                      right: trailing.left; rightMargin: 8 }
            text: row.label
            color: Theme.text
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        // Trailing: checkmark (selected) OR on/off pill (toggled) OR nothing
        Item {
            id: trailing
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 10 }
            width: 24; height: parent.height

            // Checkmark for radio selection
            IconGlyph {
                anchors.centerIn: parent
                visible: row.selected && !row.toggled
                text: ""      // CheckMark
                pixelSize: 12
                color: Theme.accent
            }
            // On/off indicator for toggles
            Rectangle {
                anchors.centerIn: parent
                visible: row.toggled
                width: 20; height: 10; radius: 5
                color: Theme.accent
                Rectangle {
                    width: 6; height: 6; radius: 3
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 2 }
                    color: "white"
                }
            }
            // Dimmer dot when a toggle is off, so the user still has a visual anchor
            Rectangle {
                anchors.centerIn: parent
                visible: !row.toggled && !row.selected
                       && (row.ListView ? false : false) // placeholder; just hide
                width: 0; height: 0
            }
        }

        MouseArea {
            id: rowMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: row.triggered()
        }
    }

    component Separator : Rectangle {
        width: parent.width - 12
        height: 1
        x: 6
        color: Theme.stroke
    }
}
