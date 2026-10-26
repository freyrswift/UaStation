import QtQuick
import UaApp

Rectangle {
    id: root
    width: 232
    radius: 10
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
        spacing: 4

        // Theme: segmented control
        SegmentedRow {
            label: (Strings.lang, Strings.t(Strings.Theme))
            glyph: ""
            options: [
                { text: (Strings.lang, Strings.t(Strings.ThemeSystem)), glyph: "" },
                { text: (Strings.lang, Strings.t(Strings.ThemeDark)),   glyph: "" },
                { text: (Strings.lang, Strings.t(Strings.ThemeLight)),  glyph: "" },
            ]
            selectedIndex: Settings.themeMode
            onSelect: (i) => { Settings.themeMode = i }
        }

        // Language: segmented control
        SegmentedRow {
            label: (Strings.lang, Strings.t(Strings.Lang))
            glyph: ""
            options: [
                { text: "中文",    glyph: "\uE99B" },
                { text: "English", glyph: "\uE99B" },
            ]
            selectedIndex: Strings.lang
            onSelect: (i) => { Strings.lang = i }
        }

        Separator {}

        // Actions        }        }
        ActionRow {
            label: (Strings.lang, Strings.t(Strings.About)); glyph: ""
            onTriggered: root.aboutRequested()
        }
        ActionRow {
            label: (Strings.lang, Strings.t(Strings.Exit)); glyph: ""
            danger: true
            onTriggered: app.quit()
        }
    }

    // =====================================================================
    // Row components
    // =====================================================================

    // Row with a group label + inline segmented picker on the right
    component SegmentedRow : Item {
        id: segRow
        property string label
        property string glyph
        property var    options: []
        property int    selectedIndex: 0
        signal select(int index)

        width: parent.width
        implicitHeight: 54

        IconGlyph {
            anchors { left: parent.left; top: parent.top; margins: 10; topMargin: 6 }
            text: segRow.glyph; pixelSize: 13; color: Theme.sub
        }
        Text {
            anchors { left: parent.left; top: parent.top; leftMargin: 34; topMargin: 4 }
            text: segRow.label
            color: Theme.sub
            font.pixelSize: 11
        }

        Row {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom
                      leftMargin: 10; rightMargin: 10; bottomMargin: 6 }
            spacing: 0
            Repeater {
                model: segRow.options
                delegate: Rectangle {
                    width: (parent.width - 0) / segRow.options.length
                    height: 26
                    radius: 5
                    color: index === segRow.selectedIndex
                           ? Theme.accent
                           : (segMa.containsMouse
                              ? (Theme.dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.04))
                              : "transparent")
                    border.width: 1
                    border.color: index === segRow.selectedIndex ? Theme.accent : Theme.stroke

                    Row {
                        anchors.centerIn: parent
                        spacing: 4
                        IconGlyph {
                            visible: modelData.glyph && modelData.glyph !== ""
                            text: modelData.glyph; pixelSize: 11
                            color: index === segRow.selectedIndex ? "#1A1A1A" : Theme.text
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: modelData.text
                            color: index === segRow.selectedIndex ? "#1A1A1A" : Theme.text
                            font.pixelSize: 11
                            font.bold: index === segRow.selectedIndex
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        id: segMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: segRow.select(index)
                    }
                }
            }
        }
    }

// Plain action row (triggers once on click)
    component ActionRow : Rectangle {
        id: aRow
        property string label
        property string glyph
        property bool   danger: false
        signal triggered()

        width: parent.width
        height: 30
        radius: 5
        color: aMa.pressed ? (Theme.dark ? Qt.rgba(1,1,1,0.14) : Qt.rgba(0,0,0,0.08))
             : aMa.containsMouse ? (Theme.dark ? Qt.rgba(1,1,1,0.06)
                                              : (aRow.danger ? "#FDE7E9" : Qt.rgba(0,0,0,0.03)))
             : "transparent"

        IconGlyph {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
            text: aRow.glyph; pixelSize: 13
            color: aRow.danger && aMa.containsMouse ? "#C42B1C" : Theme.text
        }
        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 34 }
            text: aRow.label
            color: aRow.danger && aMa.containsMouse ? "#C42B1C" : Theme.text
            font.pixelSize: 12
        }

        MouseArea {
            id: aMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: aRow.triggered()
        }
    }

    component Separator : Rectangle {
        width: parent.width - 12
        height: 1
        x: 6
        color: Theme.stroke
    }
}
