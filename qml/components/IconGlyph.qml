import QtQuick

// Icon glyph using the bundled Fluent UI System Icons font (MIT).
// Loaded in main.cpp via QFontDatabase::addApplicationFont so every platform
// (Windows / Linux / macOS) renders the same codepoints regardless of system
// font availability. The old Segoe MDL2 Assets codepoints are no longer used
// — see each caller for the Fluent UI codepoint in use.
Text {
    property real pixelSize: 14
    font.family: "FluentSystemIcons-Resizable"
    font.pixelSize: pixelSize
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignHCenter
    renderType: Text.NativeRendering
}
