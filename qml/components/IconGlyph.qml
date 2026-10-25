import QtQuick

// Icon glyph using the system Fluent / MDL2 font.
// On Windows 11 "Segoe Fluent Icons" ships; Windows 10 falls back to
// "Segoe MDL2 Assets" via the OS font substitution map. Linux/macOS have
// no counterpart yet (see cross-platform-glyphs TODO in main.cpp).
Text {
    property real pixelSize: 14
    font.family: "Segoe Fluent Icons"
    font.pixelSize: pixelSize
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignHCenter
    renderType: Text.NativeRendering
}
