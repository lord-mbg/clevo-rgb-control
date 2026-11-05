import QtQuick
import QtQuick.Controls.Basic

// Styled combo box
ComboBox {
    id: root

    property color accentColor: Theme.accentFallback

    implicitHeight: 36
    font.pixelSize: Theme.fontBase
    focusPolicy: Qt.TabFocus
    activeFocusOnTab: true
    rightPadding: 28
    indicator: Text {
        x: root.width - width - Theme.spSm
        y: (root.height - height) / 2
        text: "▾"
        font.pixelSize: Theme.fontBase
        color: Theme.textSecondary
    }

    contentItem: Text {
        leftPadding:  Theme.spSm
        text:         root.displayText
        font:         root.font
        color:        root.enabled ? Theme.textPrimary : Theme.textDisabled
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        color:        Theme.bg
        border.color: root.activeFocus ? root.accentColor : Theme.border
        border.width: root.activeFocus ? 2 : 1
        radius:       Theme.radiusSm
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

    }

    delegate: ItemDelegate {
        width: root.width
        implicitHeight: 34
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text:  modelData
            font:  root.font
            color: root.currentIndex === index ? root.accentColor : Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.spSm
        }

        background: Rectangle {
            color: highlighted ? Theme.elevated : (hovered ? Theme.surface : Theme.card)
        }
    }

    popup: Popup {
        y:      root.height + 2
        width:  root.width
        implicitHeight: contentItem.implicitHeight
        padding: 1

        contentItem: ListView {
            clip:            true
            implicitHeight:  Math.min(contentHeight, 220)
            model:           root.delegateModel
            currentIndex:    root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color:        Theme.card
            border.color: Theme.border
            border.width: 1
            radius:       Theme.radiusSm
        }
    }
}
