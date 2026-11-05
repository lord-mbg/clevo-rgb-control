import QtQuick
import QtQuick.Window

// Stylized three-zone keyboard preview — pure visual, not device state.
// One-zone whole-keyboard illumination: all zones share the same computed color.
// Timing mirrors src/effects.cpp (breathe cosine brightness, cycle hue, transition blend).
Item {
    id: root

    property color previewColor: "#5b8def"
    property color targetColor: "#ff0000"
    property string effect: "static"   // static | breathe | cycle | transition
    property int periodMs: 3000
    property real brightnessRatio: 1.0  // 0..1 peak brightness

    // Full-chroma effect color (pre-brightness)
    property color displayColor: previewColor
    // 0..1 brightness multiplier applied to displayColor for lighting layers
    property real displayLevel: brightnessRatio

    readonly property color litColor: Qt.rgba(
        Math.max(0, Math.min(1, displayColor.r * displayLevel)),
        Math.max(0, Math.min(1, displayColor.g * displayLevel)),
        Math.max(0, Math.min(1, displayColor.b * displayLevel)),
        1)

    readonly property bool windowShown: Window.window ? Window.window.visible : true
    readonly property bool previewLive: visible && windowShown && opacity > 0
    readonly property bool effectAnimated: effect === "breathe"
                                           || effect === "cycle"
                                           || effect === "transition"

    property real _epochMs: 0

    implicitWidth:  460
    implicitHeight: 146

    function _clamp01(v) {
        return Math.max(0, Math.min(1, v))
    }

    // Match effects.cpp rainbow(phase): phase in [0,1) → 6 hue sectors
    function _rainbow(phase) {
        var hue = phase * 6.0
        var sector = Math.floor(hue)
        if (sector >= 6)
            sector = 5
        var rising = Math.round((hue - sector) * 255.0)
        var falling = 255 - rising
        switch (sector) {
        case 0:  return Qt.rgba(1, rising / 255, 0, 1)
        case 1:  return Qt.rgba(falling / 255, 1, 0, 1)
        case 2:  return Qt.rgba(0, 1, rising / 255, 1)
        case 3:  return Qt.rgba(0, falling / 255, 1, 1)
        case 4:  return Qt.rgba(rising / 255, 0, 1, 1)
        default: return Qt.rgba(1, 0, falling / 255, 1)
        }
    }

    function _restartClock() {
        _epochMs = Date.now()
    }

    function _applyStatic() {
        displayColor = previewColor
        displayLevel = _clamp01(brightnessRatio)
    }

    function _tickFrame() {
        if (!effectAnimated || periodMs <= 0) {
            _applyStatic()
            return
        }

        var elapsed = Date.now() - _epochMs
        var period = periodMs
        var phase = (elapsed / period) % 1.0
        if (phase < 0)
            phase += 1.0
        var peak = _clamp01(brightnessRatio)

        if (effect === "breathe") {
            // level = peak * (1 - cos(2π·phase)) / 2
            var wave = (1.0 - Math.cos(2.0 * Math.PI * phase)) / 2.0
            displayColor = previewColor
            displayLevel = peak * wave
        } else if (effect === "cycle") {
            displayColor = _rainbow(phase)
            displayLevel = peak
        } else if (effect === "transition") {
            // One period: from → to → from via triangle blend
            var blend = 1.0 - Math.abs(2.0 * phase - 1.0)
            displayColor = Qt.rgba(
                previewColor.r + (targetColor.r - previewColor.r) * blend,
                previewColor.g + (targetColor.g - previewColor.g) * blend,
                previewColor.b + (targetColor.b - previewColor.b) * blend,
                1)
            displayLevel = peak
        } else {
            _applyStatic()
        }
    }

    function _syncMode() {
        if (!previewLive || !effectAnimated || periodMs <= 0) {
            tick.stop()
            _applyStatic()
            return
        }
        if (!tick.running)
            _restartClock()
        _tickFrame()
        tick.start()
    }

    // ~25 fps — same cadence as effects.cpp frame sleep
    Timer {
        id: tick
        interval: 40
        repeat: true
        running: false
        onTriggered: root._tickFrame()
    }

    onEffectChanged: {
        _restartClock()
        _syncMode()
    }
    onEffectAnimatedChanged: _syncMode()
    onPeriodMsChanged: {
        _restartClock()
        _syncMode()
    }
    onPreviewColorChanged: {
        if (!effectAnimated || effect === "breathe" || effect === "transition")
            _tickFrame()
        else if (!tick.running)
            _applyStatic()
    }
    onTargetColorChanged: {
        if (effect === "transition")
            _tickFrame()
    }
    onBrightnessRatioChanged: {
        if (tick.running)
            _tickFrame()
        else
            _applyStatic()
    }
    onPreviewLiveChanged: {
        if (previewLive) {
            _restartClock()
            _syncMode()
        } else {
            tick.stop()
        }
    }

    Component.onCompleted: {
        _restartClock()
        _syncMode()
    }

    // Outer chassis
    Rectangle {
        anchors.fill: parent
        color:        "#19191b"
        radius:       10
        border.color: "#2e3036"
        border.width: 1
    }

    // Three illuminated zones — same litColor for whole-keyboard preview
    Row {
        anchors { fill: parent; margins: 14 }
        spacing: 6

        Repeater {
            model: 3
            delegate: Item {
                width:  (root.width - 28 - 12) / 3
                height: root.height - 28

                // Glow layer
                Rectangle {
                    anchors.centerIn: parent
                    width:  parent.width + 16
                    height: parent.height + 16
                    radius: 14
                    color:  Qt.rgba(root.litColor.r,
                                    root.litColor.g,
                                    root.litColor.b, 0.18)
                }

                // Key bed
                Rectangle {
                    anchors.fill: parent
                    color:        "#1e2025"
                    radius:       8
                    border.color: Qt.rgba(root.litColor.r,
                                          root.litColor.g,
                                          root.litColor.b, 0.55)
                    border.width: 1

                    // Keycap grid — simplified rows of rounded caps
                    Column {
                        anchors { fill: parent; margins: 6 }
                        spacing: 4

                        Repeater {
                            model: 4
                            delegate: Row {
                                property int rowIndex: index
                                width:  parent.width
                                height: (parent.height - 12) / 4
                                spacing: 3

                                Repeater {
                                    // Fewer caps on the last row to suggest spacebar area
                                    model: rowIndex === 3 ? 5 : 9
                                    delegate: Rectangle {
                                        width: {
                                            // widen spacebar-area key in zone 1 last row
                                            if (rowIndex === 3 && index === 2) return capW * 3
                                            return capW
                                        }
                                        property real capW: (parent.width - (9 - 1) * 3) / 9
                                        height: parent.height
                                        radius: 2
                                        color:  Qt.rgba(root.litColor.r,
                                                        root.litColor.g,
                                                        root.litColor.b, 0.72)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // "Önizleme" label at bottom-right
    Text {
        anchors { bottom: parent.bottom; right: parent.right; margins: 6 }
        text:  "Önizleme"
        font.pixelSize: Theme.fontSm
        font.weight: Font.Medium
        color: Theme.textDisabled
        opacity: 0.75
    }
}
