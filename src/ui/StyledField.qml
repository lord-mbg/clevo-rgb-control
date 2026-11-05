import QtQuick
import QtQuick.Controls.Basic

// Styled text field matching dark graphite card
TextField {
    id: root

    property color accentColor: Theme.accentFallback

    implicitHeight: 36
    color: Theme.textPrimary
    placeholderTextColor: Theme.textDisabled
    selectionColor: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.35)
    selectedTextColor: Theme.textPrimary
    font.pixelSize: Theme.fontBase
    font.family: "Monospace"

    background: Rectangle {
        color: Theme.bg
        border.color: root.activeFocus ? root.accentColor : Theme.border
        border.width: root.activeFocus ? 2 : 1
        radius: Theme.radiusSm
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
        Behavior on border.width { NumberAnimation  { duration: Theme.durFast } }
    }

    leftPadding:  Theme.spSm
    rightPadding: Theme.spSm

    Keys.onReturnPressed:  root.editingFinished()
    Keys.onEnterPressed:   root.editingFinished()
}
