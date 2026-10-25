import QtQuick

// Icon glyph using Segoe MDL2 Assets.
// Shipped on every supported Windows 10/11; Windows 11's "Segoe Fluent Icons"
// shares the same codepoints (U+E700..E9F8) so this works on both. On
// Linux/macOS we currently fall back to the system font (glyphs will be blank
// until a Fluent-compatible .ttf is bundled; see TODO in main.cpp).
Text {
    property real pixelSize: 14
    font.family: "Segoe MDL2 Assets"
    font.pixelSize: pixelSize
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignHCenter
    renderType: Text.NativeRendering
}
