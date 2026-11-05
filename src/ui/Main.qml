import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: mainWindow
    objectName: "mainWindow"

    title:       "CLEVO · RGB Kontrol"
    width:       1120
    height:      840
    minimumWidth:  920
    minimumHeight: 700
    visible:     true
    color:       Theme.bg

    // Editor state stays separate from the last observed device state.
    property alias selectedColor: colorState.primary
    property alias targetColor: colorState.target
    readonly property string selectedEffect: effectCombo.effectValue
    property alias periodMs: periodSpin.value
    property alias selectedBrightness: brightnessSlider.value

    // Keep color-derived controls legible against the dark surfaces.
    readonly property color _accent: {
        var c = colorState.primary
        var lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        if (lum < 0.08) return Theme.accentFallback
        if (lum < 0.4)
            return Qt.rgba(0.55 + 0.45 * c.r, 0.55 + 0.45 * c.g, 0.55 + 0.45 * c.b, 1)
        return c
    }

    // Pending action for confirmDialog
    property var _pendingConfirm: null

    // ── One-time initialization from controller's already-read device state ──
    Component.onCompleted: {
        brightnessSlider.value = controller.brightness
        colorState.primary     = controller.currentColor
    }

    // ── Confirm dialog ────────────────────────────────────────────────────────
    Dialog {
        id: confirmDialog
        objectName: "confirmDialog"
        parent:  Overlay.overlay
        anchors.centerIn: parent
        modal:   true
        width:   360
        padding: Theme.spLg

        background: Rectangle {
            color:        Theme.card
            border.color: Theme.border
            border.width: 1
            radius:       Theme.radiusLg
        }

        contentItem: ColumnLayout {
            spacing: Theme.spMd

            Text {
                id: confirmTitle
                Layout.fillWidth: true
                font.pixelSize: Theme.fontMd
                font.weight:    Font.SemiBold
                color:  Theme.textPrimary
                wrapMode: Text.Wrap
            }
            Text {
                id: confirmBody
                Layout.fillWidth: true
                font.pixelSize: Theme.fontBase
                color:  Theme.textSecondary
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spSm
                Item { Layout.fillWidth: true }
                StyledButton {
                    text: "Vazgeç"
                    accentColor: mainWindow._accent
                    onClicked: confirmDialog.reject()
                }
                StyledButton {
                    objectName: "confirmButton"
                    text:        "Onayla"
                    destructive: true
                    onClicked: {
                        confirmDialog.accept()
                        if (mainWindow._pendingConfirm) {
                            mainWindow._pendingConfirm()
                            mainWindow._pendingConfirm = null
                        }
                    }
                }
            }
        }
    }

    function _confirm(title, body, action) {
        confirmTitle.text       = title
        confirmBody.text        = body
        mainWindow._pendingConfirm = action
        confirmDialog.open()
    }

    // ── Root layout ───────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing:      0

        // ── HEADER ───────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight:   64
            color: Theme.surface
            z:     2

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1; color: Theme.border
            }

            RowLayout {
                anchors { fill: parent; leftMargin: Theme.spXl; rightMargin: Theme.spMd }
                spacing: Theme.spSm

                // Wordmark
                Row {
                    spacing: 0
                    Text {
                        text: "CLEVO"
                        font.pixelSize:   Theme.fontXl
                        font.weight:      Font.Bold
                        font.letterSpacing: 3
                        color: Theme.textPrimary
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        text: "  ·  "
                        font.pixelSize: Theme.fontXl
                        font.weight:    Font.Light
                        color: Theme.textDisabled
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        text: "RGB KONTROL"
                        font.pixelSize:   Theme.fontXl
                        font.weight:      Font.Light
                        font.letterSpacing: 2
                        color: mainWindow._accent
                        verticalAlignment: Text.AlignVCenter
                        Behavior on color { ColorAnimation { duration: Theme.durMid } }
                    }
                }

                Item { Layout.fillWidth: true }

                // Device availability is static text; errors are reported in the footer.
                Row {
                    spacing: Theme.spSm
                    Rectangle {
                        width: 7; height: 7; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: controller.deviceAvailable ? Theme.success : Theme.error
                    }
                    Text {
                        text: controller.deviceAvailable ? "Klavye bağlı" : "Klavye okunamıyor"
                        font.pixelSize: Theme.fontSm
                        color: Theme.textSecondary
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Refresh
                StyledButton {
                    text:        "↻  Yenile"
                    accentColor: mainWindow._accent
                    enabled:     !controller.busy
                    implicitWidth: 96
                    onClicked: controller.refresh()
                    ToolTip.visible: hovered
                    ToolTip.text:   "Aygıt durumunu yeniden oku"
                    ToolTip.delay:  500
                }
            }
        }

        // ── SCROLLABLE BODY ───────────────────────────────────────────────────
        ScrollView {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            Flickable {
                contentWidth:  width
                contentHeight: bodyCol.implicitHeight + Theme.spXl * 2

                ColumnLayout {
                    id: bodyCol
                    width:   parent.width
                    spacing: Theme.spMd

                    Item { implicitHeight: Theme.spSm }

                    // ── ROW 1: Color card + keyboard/brightness card ──────────
                    RowLayout {
                        Layout.leftMargin:  Theme.spXl
                        Layout.rightMargin: Theme.spXl
                        Layout.fillWidth:   true
                        spacing: Theme.spMd

                        // ── COLOR SELECTION CARD ──────────────────────────────
                        Rectangle {
                            id: colorCard
                            Layout.fillWidth:    true
                            Layout.preferredWidth: 1
                            implicitHeight: colorCardInner.implicitHeight + Theme.spLg * 2
                            color:  Theme.card
                            radius: Theme.radiusLg
                            border.color: Theme.border; border.width: 1

                            ColumnLayout {
                                id: colorCardInner
                                anchors { left: parent.left; right: parent.right; top: parent.top
                                          margins: Theme.spLg }
                                spacing: Theme.spMd

                                Text {
                                    text: effectCombo.effectValue === "transition" ? "Başlangıç Rengi" : "Renk Seçimi"
                                    font.pixelSize: Theme.fontMd
                                    font.weight:    Font.SemiBold
                                    color: Theme.textPrimary
                                }

                                // Swatch + HEX
                                RowLayout {
                                    spacing: Theme.spMd
                                    Layout.fillWidth: true

                                    // Primary swatch — opens color dialog
                                    Rectangle {
                                        id: primarySwatch
                                        width: 64; height: 64
                                        radius: Theme.radius
                                        color:  colorState.primary
                                        border.color: Qt.rgba(1,1,1,0.10); border.width: 1
                                        activeFocusOnTab: true
                                        Behavior on color { ColorAnimation { duration: Theme.durMid } }

                                        Rectangle {
                                            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                                            height: 18; radius: Theme.radiusSm
                                            color: Qt.rgba(0,0,0,0.50)
                                            Text {
                                                anchors.centerIn: parent
                                                text: "Seç"
                                                font.pixelSize: 9
                                                font.weight: Font.Medium
                                                color: "#fff"
                                            }
                                        }
                                        // Focus ring
                                        Rectangle {
                                            anchors.fill: parent; radius: parent.radius
                                            color: "transparent"
                                            border.color: primarySwatch.activeFocus ? mainWindow._accent : "transparent"
                                            border.width: 2
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: primaryColorDlg.open()
                                        }
                                        Keys.onReturnPressed: primaryColorDlg.open()
                                        Keys.onSpacePressed:  primaryColorDlg.open()
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spXs

                                        Text {
                                            text: "HEX"
                                            font.pixelSize: Theme.fontSm
                                            font.weight:    Font.Medium
                                            color: Theme.textSecondary
                                        }

                                        StyledField {
                                            id: hexField
                                            objectName:  "hexField"
                                            Layout.fillWidth: true
                                            accentColor: mainWindow._accent
                                            maximumLength: 7
                                            font.family: "Monospace"
                                            text: colorState.primary.toString().toUpperCase()
                                            validator: RegularExpressionValidator {
                                                regularExpression: /^#[0-9A-Fa-f]{0,6}$/
                                            }

                                            onActiveFocusChanged: { if (activeFocus) selectAll() }

                                            onEditingFinished: {
                                                var h = text.trim()
                                                if (/^#[0-9A-Fa-f]{6}$/.test(h)) {
                                                    colorState.primary = h
                                                } else {
                                                    text = colorState.primary.toString().toUpperCase()
                                                }
                                            }
                                        }
                                    }
                                }

                                // RGB channel sliders + spinboxes
                                ColumnLayout {
                                    spacing: Theme.spSm
                                    Layout.fillWidth: true

                                    Repeater {
                                        model: [
                                            { label: "R", ch: "r", track: "#cc3333" },
                                            { label: "G", ch: "g", track: "#33aa55" },
                                            { label: "B", ch: "b", track: "#3366dd" },
                                        ]

                                        delegate: RowLayout {
                                            required property var  modelData
                                            required property int  index

                                            spacing: Theme.spSm
                                            Layout.fillWidth: true

                                            Text {
                                                text: modelData.label
                                                font.pixelSize: Theme.fontSm
                                                font.weight:    Font.Bold
                                                font.family:    "Monospace"
                                                color: modelData.track
                                                Layout.preferredWidth: 12
                                            }

                                            Slider {
                                                id: rgbSlider
                                                Layout.fillWidth: true
                                                from: 0; to: 255; stepSize: 1
                                                focusPolicy:      Qt.TabFocus
                                                activeFocusOnTab: true

                                                // Read channel value from colorState without two-way binding loop
                                                value: {
                                                    var c = colorState.primary
                                                    if      (modelData.ch === "r") return Math.round(c.r * 255)
                                                    else if (modelData.ch === "g") return Math.round(c.g * 255)
                                                    else                           return Math.round(c.b * 255)
                                                }

                                                onMoved: {
                                                    var c = colorState.primary
                                                    var r = Math.round(c.r * 255)
                                                    var g = Math.round(c.g * 255)
                                                    var b = Math.round(c.b * 255)
                                                    if      (modelData.ch === "r") r = Math.round(rgbSlider.value)
                                                    else if (modelData.ch === "g") g = Math.round(rgbSlider.value)
                                                    else                           b = Math.round(rgbSlider.value)
                                                    colorState.primary = Qt.rgba(r/255, g/255, b/255, 1)
                                                }

                                                background: Rectangle {
                                                    x: rgbSlider.leftPadding
                                                    y: rgbSlider.topPadding + rgbSlider.availableHeight / 2 - height / 2
                                                    width: rgbSlider.availableWidth; height: 4; radius: 2
                                                    color: Theme.bg
                                                    Rectangle {
                                                        width:  rgbSlider.visualPosition * parent.width
                                                        height: parent.height; radius: parent.radius
                                                        color:  modelData.track
                                                    }
                                                }

                                                handle: Rectangle {
                                                    x: rgbSlider.leftPadding + rgbSlider.visualPosition * (rgbSlider.availableWidth - width)
                                                    y: rgbSlider.topPadding  + rgbSlider.availableHeight / 2 - height / 2
                                                    width: 16; height: 16; radius: 8
                                                    color: modelData.track
                                                    border.color: Qt.lighter(modelData.track, 1.4); border.width: 1
                                                    Rectangle {
                                                        anchors.fill: parent; radius: parent.radius
                                                        color: "transparent"
                                                        border.color: rgbSlider.activeFocus ? "#fff" : "transparent"
                                                        border.width: 2
                                                    }
                                                }
                                            }

                                            StyledSpinBox {
                                                from: 0; to: 255
                                                accentColor: mainWindow._accent
                                                Layout.preferredWidth: 82
                                                value: Math.round(rgbSlider.value)
                                                onValueModified: {
                                                    var c = colorState.primary
                                                    var r = Math.round(c.r * 255)
                                                    var g = Math.round(c.g * 255)
                                                    var b = Math.round(c.b * 255)
                                                    if      (modelData.ch === "r") r = value
                                                    else if (modelData.ch === "g") g = value
                                                    else                           b = value
                                                    colorState.primary = Qt.rgba(r/255, g/255, b/255, 1)
                                                }
                                            }
                                        }
                                    }
                                }

                                // Keep hex field in sync when color changes via sliders/swatches
                                Connections {
                                    target: colorState
                                    function onPrimaryChanged() {
                                        if (!hexField.activeFocus)
                                            hexField.text = colorState.primary.toString().toUpperCase()
                                    }
                                }

                                // Curated nominal swatches
                                ColumnLayout {
                                    spacing: Theme.spXs
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Hızlı Renkler"
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color: Theme.textSecondary
                                    }

                                    // Flow needs width set; get it from parent layout
                                    Item {
                                        Layout.fillWidth: true
                                        implicitHeight: swatchFlow.implicitHeight

                                        Flow {
                                            id: swatchFlow
                                            width: parent.width
                                            spacing: 6

                                            Repeater {
                                                model: [
                                                    { name: "Beyaz",     hex: "#ffffff" },
                                                    { name: "Kırmızı",   hex: "#ff2222" },
                                                    { name: "Turuncu",   hex: "#ff7700" },
                                                    { name: "Sarı",      hex: "#ffe600" },
                                                    { name: "Yeşil",     hex: "#22dd44" },
                                                    { name: "Camgöbeği", hex: "#00e5e5" },
                                                    { name: "Mavi",      hex: "#2255ff" },
                                                    { name: "Mor",       hex: "#aa33ff" },
                                                    { name: "Pembe",     hex: "#ff44aa" },
                                                    { name: "Leylak",    hex: "#cc88ff" },
                                                    { name: "Buz Mavi",  hex: "#aad8ff" },
                                                    { name: "Siyah",     hex: "#000000" },
                                                ]

                                                delegate: Rectangle {
                                                    required property var modelData
                                                    width: 28; height: 28
                                                    radius: Theme.radiusSm
                                                    color:  modelData.hex
                                                    activeFocusOnTab: true
                                                    border.color: {
                                                        var sel = colorState.primary.toString().toLowerCase()
                                                        var me  = modelData.hex.toLowerCase()
                                                        return (sel === me || sel === me.replace("#","#ff"))
                                                               ? mainWindow._accent : Qt.rgba(1,1,1,0.12)
                                                    }
                                                    border.width: {
                                                        var sel = colorState.primary.toString().toLowerCase()
                                                        var me  = modelData.hex.toLowerCase()
                                                        return (sel === me || sel === me.replace("#","#ff")) ? 2 : 1
                                                    }
                                                    Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

                                                    ToolTip.visible: swMA.containsMouse
                                                    ToolTip.text:    modelData.name
                                                    ToolTip.delay:   400

                                                    // Focus ring
                                                    Rectangle {
                                                        anchors.fill: parent; radius: parent.radius
                                                        color: "transparent"
                                                        border.color: parent.activeFocus ? mainWindow._accent : "transparent"
                                                        border.width: 2
                                                    }

                                                    MouseArea {
                                                        id: swMA
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape:  Qt.PointingHandCursor
                                                        onClicked: colorState.primary = parent.modelData.hex
                                                    }
                                                    Keys.onReturnPressed: colorState.primary = modelData.hex
                                                    Keys.onSpacePressed:  colorState.primary = modelData.hex
                                                }
                                            }
                                        }
                                    }
                                }
                                // Both transition endpoints are edited here, independently.
                                ColumnLayout {
                                    visible: effectCombo.effectValue === "transition"
                                    spacing: Theme.spSm
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Bitiş Rengi"
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        spacing: Theme.spMd
                                        Layout.fillWidth: true

                                        Rectangle {
                                            id: targetSwatch
                                            width: 36; height: 36
                                            radius: Theme.radiusSm
                                            color:  colorState.target
                                            border.color: Qt.rgba(1,1,1,0.10); border.width: 1
                                            activeFocusOnTab: effectCombo.effectValue === "transition"
                                            Behavior on color { ColorAnimation { duration: Theme.durMid } }

                                            Rectangle {
                                                anchors.fill: parent; radius: parent.radius
                                                color: "transparent"
                                                border.color: targetSwatch.activeFocus ? mainWindow._accent : "transparent"
                                                border.width: 2
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: targetColorDlg.open()
                                            }
                                            Keys.onReturnPressed: targetColorDlg.open()
                                            Keys.onSpacePressed:  targetColorDlg.open()
                                        }

                                        StyledField {
                                            id: targetHexField
                                            objectName: "targetHexField"
                                            Layout.fillWidth: true
                                            accentColor: mainWindow._accent
                                            maximumLength: 7
                                            font.family: "Monospace"
                                            text: colorState.target.toString().toUpperCase()
                                            validator: RegularExpressionValidator {
                                                regularExpression: /^#[0-9A-Fa-f]{0,6}$/
                                            }

                                            onActiveFocusChanged: { if (activeFocus) selectAll() }
                                            onEditingFinished: {
                                                var h = text.trim()
                                                if (/^#[0-9A-Fa-f]{6}$/.test(h)) {
                                                    colorState.target = h
                                                } else {
                                                    text = colorState.target.toString().toUpperCase()
                                                }
                                            }
                                        }

                                        Connections {
                                            target: colorState
                                            function onTargetChanged() {
                                                if (!targetHexField.activeFocus)
                                                    targetHexField.text = colorState.target.toString().toUpperCase()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── KEYBOARD PREVIEW + BRIGHTNESS CARD ───────────────
                        Rectangle {
                            Layout.fillWidth:    true
                            Layout.preferredWidth: 1
                            implicitHeight: colorCard.implicitHeight
                            color:  Theme.card
                            radius: Theme.radiusLg
                            border.color: Theme.border; border.width: 1

                            ColumnLayout {
                                anchors { fill: parent; margins: Theme.spLg }
                                spacing: Theme.spMd

                                Text {
                                    text: "Klavye Önizlemesi"
                                    font.pixelSize: Theme.fontMd
                                    font.weight:    Font.SemiBold
                                    color: Theme.textPrimary
                                }

                                Text {
                                    text: "Seçilen efektin gösterimi — gerçek aygıt durumunu yansıtmaz"
                                    font.pixelSize: Theme.fontSm
                                    color: Theme.textSecondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }

                                Item { Layout.fillHeight: true; Layout.minimumHeight: Theme.spSm }

                                KeyboardPreview {
                                    objectName: "keyboardPreview"
                                    Layout.fillWidth:  true
                                    Layout.alignment:  Qt.AlignVCenter
                                    previewColor: colorState.primary
                                    targetColor: colorState.target
                                    effect: effectCombo.effectValue
                                    periodMs: periodSpin.value
                                    brightnessRatio: brightnessSlider.value / Math.max(controller.maxBrightness, 1)
                                }

                                Item { Layout.fillHeight: true; Layout.minimumHeight: Theme.spSm }

                                // Brightness
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spSm

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "Parlaklık"
                                            font.pixelSize: Theme.fontSm
                                            font.weight:    Font.Medium
                                            color: Theme.textSecondary
                                        }
                                        Item { Layout.fillWidth: true }
                                        Text {
                                            text: brightnessSlider.value
                                                  + " / "
                                                  + (controller.maxBrightness > 0 ? controller.maxBrightness : "—")
                                            font.pixelSize: Theme.fontSm
                                            font.family:    "Monospace"
                                            color: mainWindow._accent
                                            Behavior on color { ColorAnimation { duration: Theme.durMid } }
                                        }
                                    }

                                    Slider {
                                        id: brightnessSlider
                                        objectName: "brightnessSlider"
                                        Layout.fillWidth: true
                                        from:     0
                                        to:       Math.max(controller.maxBrightness, 1)
                                        stepSize: 1
                                        enabled:  controller.maxBrightness > 0
                                        focusPolicy:      Qt.TabFocus
                                        activeFocusOnTab: true
                                        // value initialized once in Component.onCompleted

                                        background: Rectangle {
                                            x: brightnessSlider.leftPadding
                                            y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                                            width: brightnessSlider.availableWidth; height: 6; radius: 3
                                            color: Theme.bg
                                            Rectangle {
                                                width:  brightnessSlider.visualPosition * parent.width
                                                height: parent.height; radius: parent.radius
                                                gradient: Gradient {
                                                    orientation: Gradient.Horizontal
                                                    GradientStop { position: 0.0; color: Qt.rgba(mainWindow._accent.r, mainWindow._accent.g, mainWindow._accent.b, 0.3) }
                                                    GradientStop { position: 1.0; color: mainWindow._accent }
                                                }
                                            }
                                        }

                                        handle: Rectangle {
                                            x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width)
                                            y: brightnessSlider.topPadding  + brightnessSlider.availableHeight / 2 - height / 2
                                            width: 20; height: 20; radius: 10
                                            color: mainWindow._accent
                                            Behavior on color { ColorAnimation { duration: Theme.durMid } }
                                            Rectangle {
                                                anchors.fill: parent; radius: parent.radius
                                                color: "transparent"
                                                border.color: brightnessSlider.activeFocus ? "#fff" : "transparent"
                                                border.width: 2
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ── ROW 2: Effect controls + Profile panel ────────────────
                    RowLayout {
                        Layout.leftMargin:  Theme.spXl
                        Layout.rightMargin: Theme.spXl
                        Layout.fillWidth:   true
                        spacing: Theme.spMd

                        // ── EFFECT CONTROLS ───────────────────────────────────
                        Rectangle {
                            Layout.fillWidth:    true
                            Layout.preferredWidth: 1
                            implicitHeight: effectCardInner.implicitHeight + Theme.spLg * 2
                            Layout.alignment: Qt.AlignTop
                            color:  Theme.card
                            radius: Theme.radiusLg
                            border.color: Theme.border; border.width: 1

                            ColumnLayout {
                                id: effectCardInner
                                anchors { left: parent.left; right: parent.right; top: parent.top
                                          margins: Theme.spLg }
                                spacing: Theme.spMd

                                Text {
                                    text: "Efekt"
                                    font.pixelSize: Theme.fontMd
                                    font.weight:    Font.SemiBold
                                    color: Theme.textPrimary
                                }

                                // Effect mode selector
                                RowLayout {
                                    spacing: Theme.spSm
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Mod"
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color: Theme.textSecondary
                                        Layout.preferredWidth: 80
                                    }

                                    StyledCombo {
                                        id: effectCombo
                                        objectName: "effectCombo"
                                        Layout.fillWidth: true
                                        accentColor: mainWindow._accent

                                        // CLI value for current selection — named to avoid ComboBox.currentValue (valueRole) conflict
                                        readonly property string effectValue: ["static","breathe","cycle","transition"][currentIndex]

                                        model: ["Sabit", "Nefes", "Gökkuşağı", "Geçiş"]
                                    }
                                }

                                // Period — visible for animated effects
                                RowLayout {
                                    visible: effectCombo.effectValue !== "static"
                                    spacing: Theme.spSm
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Periyot (ms)"
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color: Theme.textSecondary
                                        Layout.preferredWidth: 80
                                    }

                                    StyledSpinBox {
                                        id: periodSpin
                                        objectName: "periodSpin"
                                        Layout.fillWidth: true
                                        accentColor: mainWindow._accent
                                        from:     100
                                        to:       3600000
                                        value:    3000
                                        stepSize: 100

                                    }
                                }


                                // Explanatory text for cycle / breathe
                                Text {
                                    visible: effectCombo.effectValue === "cycle"
                                    text: "Gökkuşağı efekti tam renk yelpazesini otomatik olarak döngüye sokar."
                                    font.pixelSize: Theme.fontSm
                                    color: Theme.textSecondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Text {
                                    visible: effectCombo.effectValue === "breathe"
                                    text: "Nefes efekti seçili birincil rengi kullanır."
                                    font.pixelSize: Theme.fontSm
                                    color: Theme.textSecondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }

                                Item { implicitHeight: Theme.spXs }
                            }
                        }

                        // ── PROFILE PANEL ─────────────────────────────────────
                        Rectangle {
                            Layout.fillWidth:    true
                            Layout.preferredWidth: 1
                            implicitHeight: profileCardInner.implicitHeight + Theme.spLg * 2
                            Layout.alignment: Qt.AlignTop
                            color:  Theme.card
                            radius: Theme.radiusLg
                            border.color: Theme.border; border.width: 1

                            ColumnLayout {
                                id: profileCardInner
                                anchors { left: parent.left; right: parent.right; top: parent.top
                                          margins: Theme.spLg }
                                spacing: Theme.spMd

                                Text {
                                    text: "Profiller"
                                    font.pixelSize: Theme.fontMd
                                    font.weight:    Font.SemiBold
                                    color: Theme.textPrimary
                                }

                                // No-device warning
                                Rectangle {
                                    visible: !controller.deviceAvailable
                                    Layout.fillWidth: true
                                    implicitHeight: noDevText.implicitHeight + Theme.spSm * 2
                                    radius: Theme.radiusSm
                                    color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.08)
                                    border.color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.30)
                                    border.width: 1

                                    Text {
                                        id: noDevText
                                        anchors { fill: parent; margins: Theme.spSm }
                                        text: "Aygıt bulunamadı — profiller kaydedilebilir, doğrudan uygulanamaz."
                                        font.pixelSize: Theme.fontSm
                                        color: Theme.warning
                                        wrapMode: Text.Wrap
                                    }
                                }

                                // Profile selector + apply/delete
                                ColumnLayout {
                                    spacing: Theme.spSm
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Mevcut Profiller"
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color: Theme.textSecondary
                                    }

                                    StyledCombo {
                                        id: profileCombo
                                        objectName: "profileCombo"
                                        Layout.fillWidth: true
                                        accentColor: mainWindow._accent
                                        enabled:     controller.profiles.length > 0
                                        model:       controller.profiles.length > 0
                                                     ? controller.profiles
                                                     : ["— Profil yok —"]
                                        onActivated: function(index) {
                                            var name = controller.profiles[index]
                                            if (!name) return
                                            var settings = controller.profileSettings(name)
                                            if (Object.keys(settings).length === 0) return
                                            colorState.primary = settings.color
                                            colorState.target = settings.targetColor
                                            brightnessSlider.value = settings.brightness
                                            effectCombo.currentIndex = ["static", "breathe", "cycle", "transition"].indexOf(settings.effect)
                                            periodSpin.value = settings.periodMs
                                        }
                                    }

                                    RowLayout {
                                        spacing: Theme.spSm
                                        Layout.fillWidth: true

                                        Item { Layout.fillWidth: true }

                                        StyledButton {
                                            id: applyProfileButton
                                            objectName: "applyProfileButton"
                                            text:    "Profili Uygula"
                                            primary: true
                                            accentColor: mainWindow._accent
                                            enabled: !controller.busy
                                                     && controller.deviceAvailable
                                                     && controller.profiles.length > 0
                                            onClicked: {
                                                var name = controller.profiles[profileCombo.currentIndex]
                                                if (name) controller.applyProfile(name)
                                            }
                                        }

                                        StyledButton {
                                            id: deleteProfileButton
                                            objectName: "deleteProfileButton"
                                            text:        "Sil"
                                            destructive: true
                                            enabled: !controller.busy && controller.profiles.length > 0
                                            onClicked: {
                                                var name = controller.profiles[profileCombo.currentIndex]
                                                if (!name) return
                                                mainWindow._confirm(
                                                    "Profili Sil",
                                                    "«" + name + "» profilini kalıcı olarak silmek istediğinizden emin misiniz?",
                                                    function() { controller.deleteProfile(name) }
                                                )
                                            }
                                        }
                                    }
                                }

                                // Divider
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Theme.border
                                }

                                // Save profile
                                ColumnLayout {
                                    spacing: Theme.spSm
                                    Layout.fillWidth: true

                                    Text {
                                        text: "Ayarları Kaydet"
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        spacing: Theme.spSm
                                        Layout.fillWidth: true

                                        StyledField {
                                            id: profileNameField
                                            objectName: "profileNameField"
                                            Layout.fillWidth: true
                                            accentColor:    mainWindow._accent
                                            placeholderText: "Profil adı…"
                                            maximumLength:  64
                                        }

                                        StyledButton {
                                            id: saveProfileButton
                                            objectName: "saveProfileButton"
                                            text:    "Kaydet"
                                            primary: true
                                            accentColor: mainWindow._accent
                                            enabled: !controller.busy && profileNameField.text.trim().length > 0

                                            onClicked: {
                                                var name = profileNameField.text.trim()
                                                if (controller.profiles.indexOf(name) !== -1) {
                                                    mainWindow._confirm(
                                                        "Profili Üzerine Yaz",
                                                        "«" + name + "» zaten mevcut. Üzerine yazmak istiyor musunuz?",
                                                        function() { _doSaveProfile(name) }
                                                    )
                                                } else {
                                                    _doSaveProfile(name)
                                                }
                                            }
                                        }
                                    }
                                }

                                Item { implicitHeight: Theme.spXs }
                            }
                        }
                    }

                    Item { implicitHeight: Theme.spSm }
                }
            }
        }

        // ── FOOTER ───────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight:   footerRow.implicitHeight + Theme.spMd * 2
            color: Theme.surface
            z:     2

            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 1; color: Theme.border
            }

            RowLayout {
                id: footerRow
                anchors { fill: parent; leftMargin: Theme.spXl; rightMargin: Theme.spXl
                          topMargin: Theme.spMd; bottomMargin: Theme.spMd }
                spacing: Theme.spMd

                // Busy spinner
                Item {
                    visible: controller.busy
                    width: 18; height: 18

                    Rectangle {
                        anchors.fill: parent; radius: width / 2
                        color: "transparent"
                        border.color: mainWindow._accent; border.width: 2

                        Rectangle {
                            width: 3; height: 7; radius: 1
                            anchors { top: parent.top; topMargin: 2; horizontalCenter: parent.horizontalCenter }
                            color: mainWindow._accent
                        }

                        RotationAnimator on rotation {
                            running: controller.busy
                            from: 0; to: 360
                            duration: 900
                            loops: Animation.Infinite
                        }
                    }
                }

                // Status message
                Rectangle {
                    implicitHeight: statusLbl.implicitHeight + Theme.spXs * 2
                    implicitWidth:  Math.min(statusLbl.implicitWidth + Theme.spMd * 2, 460)
                    radius: Theme.radiusSm
                    color: controller.error
                           ? Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.10)
                           : "transparent"
                    border.color: controller.error ? Theme.error : Theme.border
                    border.width: 1
                    Behavior on color        { ColorAnimation { duration: Theme.durMid } }
                    Behavior on border.color { ColorAnimation { duration: Theme.durMid } }

                    Text {
                        id: statusLbl
                        anchors { fill: parent; leftMargin: Theme.spMd; rightMargin: Theme.spMd
                                  topMargin: Theme.spXs; bottomMargin: Theme.spXs }
                        text: controller.statusMessage.length > 0 ? controller.statusMessage : "Hazır"
                        font.pixelSize: Theme.fontSm
                        color: controller.error ? Theme.error : Theme.textSecondary
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        Behavior on color { ColorAnimation { duration: Theme.durMid } }
                    }
                    ToolTip.visible: statusHover.containsMouse
                    ToolTip.text: controller.statusMessage
                    MouseArea {
                        id: statusHover
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }

                Item { Layout.fillWidth: true }

                StyledButton {
                    id: stopButton
                    objectName: "stopButton"
                    text: "Efekti Durdur"
                    // stop() is valid regardless of deviceAvailable — recovers running service
                    enabled: !controller.busy
                    accentColor: mainWindow._accent
                    onClicked: controller.stop()
                    ToolTip.visible: hovered
                    ToolTip.text:   "Arka plan efektini durdur ve önceki ayarları geri yükle"
                    ToolTip.delay:  500
                }

                StyledButton {
                    id: applyButton
                    objectName: "applyButton"
                    text:    "Uygula"
                    primary: true
                    accentColor: mainWindow._accent
                    enabled:     !controller.busy && controller.deviceAvailable
                    implicitWidth: 110
                    onClicked: {
                        controller.apply(
                            colorState.primary.toString(),
                            colorState.target.toString(),
                            brightnessSlider.value,
                            effectCombo.effectValue,
                            periodSpin.value
                        )
                    }
                }
            }
        }
    }

    // ── Color state — single source of truth for both color pickers ──────────
    QtObject {
        id: colorState
        property color primary: "#2255ff"
        property color target:  "#ff2222"
    }

    // ── Color dialogs ─────────────────────────────────────────────────────────
    ColorDialog {
        id: primaryColorDlg
        title:         effectCombo.effectValue === "transition" ? "Başlangıç Rengi Seç" : "Renk Seç"
        selectedColor: colorState.primary
        onAccepted:    colorState.primary = selectedColor
    }

    ColorDialog {
        id: targetColorDlg
        title:         "Bitiş Rengi Seç"
        selectedColor: colorState.target
        onAccepted:    colorState.target = selectedColor
    }

    // ── Save profile helper ───────────────────────────────────────────────────
    function _doSaveProfile(name) {
        controller.saveProfile(
            name,
            colorState.primary.toString(),
            colorState.target.toString(),
            brightnessSlider.value,
            effectCombo.effectValue,
            periodSpin.value
        )
        profileNameField.text = ""
    }
}
