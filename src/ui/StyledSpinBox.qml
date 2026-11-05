import QtQuick
import QtQuick.Controls.Basic

// Styled spin box for numeric channel / period values
SpinBox {
    id: root

    property color accentColor: Theme.accentFallback

    implicitWidth:  80
    implicitHeight: 36
    font.pixelSize: Theme.fontBase
    font.family:    "Monospace"
    editable:       true
    focusPolicy:    Qt.TabFocus
    activeFocusOnTab: true

    contentItem: TextInput {
        z:                 2
        text:              root.displayText
        font:              root.font
        color:             root.enabled ? Theme.textPrimary : Theme.textDisabled
        selectionColor:    Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.35)
        selectedTextColor: Theme.textPrimary
        horizontalAlignment: TextInput.AlignHCenter
        verticalAlignment:   TextInput.AlignVCenter
        readOnly:          !root.editable
        validator:         root.validator
        inputMethodHints:  Qt.ImhFormattedNumbersOnly
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin:  28
        anchors.rightMargin: 28
    }

    up.indicator: Rectangle {
        x: parent.width - width
        height: parent.height
        width:  28
        color:  root.up.pressed ? Theme.border : (root.up.hovered ? Theme.elevated : "transparent")
        radius: Theme.radiusSm
        Text {
            anchors.centerIn: parent
            text:  "+"
            font.pixelSize: Theme.fontMd
            font.weight: Font.Medium
            color: root.enabled ? Theme.textPrimary : Theme.textDisabled
        }
    }

    down.indicator: Rectangle {
        height: parent.height
        width:  28
        color:  root.down.pressed ? Theme.border : (root.down.hovered ? Theme.elevated : "transparent")
        radius: Theme.radiusSm
        Text {
            anchors.centerIn: parent
            text:  "-"
            font.pixelSize: Theme.fontMd
            font.weight: Font.Medium
            color: root.enabled ? Theme.textPrimary : Theme.textDisabled
        }
    }

    background: Rectangle {
        color:        Theme.bg
        border.color: root.activeFocus ? root.accentColor : Theme.border
        border.width: root.activeFocus ? 2 : 1
        radius:       Theme.radiusSm
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }
}
