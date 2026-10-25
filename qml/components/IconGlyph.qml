import QtQuick

// Icon glyph using the system Fluent / MDL2 font family.
// On Windows 11 this is "Segoe Fluent Icons"; on Windows 10 it falls back to
// "Segoe MDL2 Assets". Linux/macOS have no counterpart until we ship a bundled
// font, so glyphs are omitted there (SVG logos still render fine).
Text {
    property real pixelSize: 14
    font.family: "Segoe Fluent Icons, Segoe MDL2 Assets"
    font.pixelSize: pixelSize
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignHCenter
    renderType: Text.NativeRendering
}
