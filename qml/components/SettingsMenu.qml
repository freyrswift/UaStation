import QtQuick
import UaApp

Rectangle {
    id: root
    width: 184
    radius: 8
    border.width: 1
    border.color: Palette.stroke
    color: Palette.surface
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

        // Section: theme
        Repeater {
            model: [
                { glyph: "", text: Strings.t(Strings.ThemeSystem), action: () => Settings.themeMode = 0 },
                { glyph: "", text: Strings.t(Strings.ThemeDark),   action: () => Settings.themeMode = 1 },
                { glyph: "", text: Strings.t(Strings.ThemeLight),  action: () => Settings.themeMode = 2 },
            ]
            delegate: MenuRow { label: modelData.text; glyph: modelData.glyph; onTriggered: { modelData.action(); root.closeRequested() } }
        }
        Separator {}
        MenuRow {
            label: "中文";  glyph: ""
            onTriggered: { Strings.lang = 0; root.closeRequested() }
        }
        MenuRow {
            label: "English"; glyph: ""
            onTriggered: { Strings.lang = 1; root.closeRequested() }
        }
        Separator {}
        MenuRow {
            label: Strings.t(Strings.Topmost); glyph: ""
            onTriggered: { Settings.topmost = !Settings.topmost; root.closeRequested() }
        }
        MenuRow {
            label: Strings.t(Strings.Autostart); glyph: ""
            onTriggered: { Settings.autostart = !Settings.autostart; root.closeRequested() }
        }
        Separator {}
        MenuRow {
            label: Strings.t(Strings.CheckUpdate); glyph: ""
            onTriggered: { Updater.check(); root.closeRequested() }
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

    component MenuRow : Rectangle {
        property string label
        property string glyph
        signal triggered()

        width: parent.width
        height: 28
        radius: 4
        color: rowMa.containsMouse ? (Palette.dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.04))
                                   : "transparent"

        IconGlyph {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
            text: glyph; pixelSize: 14; color: Palette.text
        }
        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 32 }
            text: label; color: Palette.text; font.pixelSize: 12
        }
        MouseArea {
            id: rowMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.triggered()
        }
    }

    component Separator : Rectangle {
        width: parent.width - 12
        height: 1
        x: 6
        color: Palette.stroke
    }
}
