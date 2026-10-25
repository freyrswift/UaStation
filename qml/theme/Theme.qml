pragma Singleton
import QtQuick

// Shared colour / geometry tokens. Singleton so any component can read it as
// `Theme.text` without passing it down the tree.
//
// Note: we DO NOT import UaApp here even though we'd like to read Settings.themeMode
// directly, because that creates an implicit circular dependency chain through
// the module and causes the singleton's properties to resolve as undefined at
// runtime. Main.qml binds `mode` and `systemIsDark` explicitly instead.
QtObject {
    // 0 = system, 1 = dark, 2 = light (matches core::Settings::ThemeMode)
    property int  mode: 0
    property bool systemIsDark: false

    readonly property bool dark: mode === 0 ? systemIsDark : mode === 1

    readonly property color bg:         dark ? "#202020" : "#F3F3F3"
    readonly property color surface:    dark ? "#2C2C2C" : "#FBFBFB"
    readonly property color surfaceAlt: dark ? "#262626" : "#F6F6F6"
    readonly property color stroke:     dark ? "#38383B" : "#E4E4E4"
    readonly property color text:       dark ? "#FFFFFF" : "#1A1A1A"
    readonly property color sub:        dark ? "#C5C5C5" : "#5F5F5F"
    readonly property color accent:     "#60CDFF"
    readonly property color green:      "#2EBD59"

    // Shared geometry
    readonly property real windowW: 460
    readonly property real windowH: 524
    readonly property real pad:     14
    readonly property real cardH:   48
    readonly property real gap:     8
    readonly property real radius:  10
}
