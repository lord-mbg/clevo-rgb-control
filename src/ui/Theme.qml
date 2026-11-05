pragma Singleton
import QtQuick

// Dark graphite palette — single authoritative source
QtObject {
    // Surfaces
    readonly property color bg:        "#1a1c1e"
    readonly property color surface:   "#22252a"
    readonly property color card:      "#292d34"
    readonly property color elevated:  "#31363f"
    readonly property color border:    "#3a3f4b"

    // Text
    readonly property color textPrimary:   "#e8eaed"
    readonly property color textSecondary: "#9aa0ab"
    readonly property color textDisabled:  "#555d6a"

    // States
    readonly property color success:  "#4caf82"
    readonly property color error:    "#e05252"
    readonly property color warning:  "#e0a652"

    // Accent — derived from selected color at call sites
    readonly property color accentFallback: "#5b8def"

    // Geometry
    readonly property int radius:     8
    readonly property int radiusSm:   4
    readonly property int radiusLg:  12

    // Spacing
    readonly property int spXs:  4
    readonly property int spSm:  8
    readonly property int spMd: 16
    readonly property int spLg: 24
    readonly property int spXl: 32

    // Typography
    readonly property int fontSm:   11
    readonly property int fontBase:  13
    readonly property int fontMd:   15
    readonly property int fontLg:   18
    readonly property int fontXl:   24
    readonly property int fontHero:  32

    // Animation
    readonly property int durFast:  120
    readonly property int durMid:   220
}
