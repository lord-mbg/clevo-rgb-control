import QtQuick
import QtQuick.Controls.Basic

// Reusable flat dark button
AbstractButton {
    id: root

    property color accentColor: Theme.accentFallback
    property bool  primary:     false
    property bool  destructive: false

    function contrastText(c) {
        function linear(channel) {
            return channel <= 0.04045 ? channel / 12.92
                                     : Math.pow((channel + 0.055) / 1.055, 2.4)
        }
        var luminance = 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b)
        return luminance > 0.179 ? "#000000" : "#ffffff"
    }

    implicitWidth:  Math.max(contentItem.implicitWidth + Theme.spLg * 2, 96)
    implicitHeight: 36

    focusPolicy: Qt.TabFocus
    activeFocusOnTab: true

    contentItem: Text {
        text:             root.text
        font.pixelSize:   Theme.fontBase
        font.weight:      Font.Medium
        color: {
            if (!root.enabled)            return Theme.textDisabled
            if (root.destructive)         return Theme.error
            if (root.primary)             return root.contrastText(root.background.color)
            return Theme.textPrimary
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment:   Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: {
            if (!root.enabled) return Theme.elevated
            if (root.primary) {
                if (root.pressed)  return Qt.darker(root.accentColor, 1.25)
                if (root.hovered)  return Qt.darker(root.accentColor, 1.1)
                return root.accentColor
            }
            if (root.destructive) {
                if (root.pressed) return Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.25)
                if (root.hovered) return Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.15)
                return Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.08)
            }
            if (root.pressed)  return Theme.border
            if (root.hovered)  return Theme.elevated
            return Theme.surface
        }
        border.color: root.activeFocus ? root.accentColor : "transparent"
        border.width: root.activeFocus ? 2 : 0
        Behavior on color        { ColorAnimation { duration: Theme.durFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }

    Behavior on implicitWidth { NumberAnimation { duration: Theme.durFast } }
}
