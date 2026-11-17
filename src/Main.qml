import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1100
    height: 740
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    title: (backend.dirty ? "* " : "") + backend.projectTitle + " — ParloCue"
    color: theme.bgBottom

    Material.theme: theme.isDark ? Material.Dark : Material.Light
    Material.accent: theme.accentControl
    Material.foreground: theme.textPrimary
    Material.background: theme.surfaceDialog
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool allowClose: false
    property int pendingActionAfterDiscard: 0 // 0=none, 1=quit, 2=new, 3=open
    readonly property bool textInputFocused: activeFocusItem !== null
        && (activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit)

    component AccentBtn: Button {
        id: abtn
        implicitHeight: 34
        leftPadding: 14
        rightPadding: 14
        topInset: 0
        bottomInset: 0
        contentItem: Label {
            text: abtn.text
            font.pixelSize: 13
            font.bold: true
            color: abtn.enabled ? theme.accentControlForeground : theme.disabledFg
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: abtn.enabled ? (abtn.down ? Qt.darker(theme.accentControl, 1.12) : theme.accentControl) : theme.disabledBg
            radius: Math.min(4, theme.radius)
            opacity: abtn.hovered && abtn.enabled ? 0.92 : 1.0
        }
    }

    component CompactBtn: Button {
        id: cbtn
        implicitHeight: 26
        implicitWidth: Math.max(26, cbtnLbl.implicitWidth + 12)
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        leftInset: 0
        rightInset: 0
        topInset: 0
        bottomInset: 0
        property color labelColor: theme.textSecondary
        contentItem: Label {
            id: cbtnLbl
            text: cbtn.text
            font.pixelSize: 13
            color: cbtn.enabled ? cbtn.labelColor : theme.disabledFg
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: cbtn.down ? theme.buttonDown : (cbtn.hovered && cbtn.enabled ? theme.buttonHover : "transparent")
            radius: Math.min(4, theme.radius)
        }
    }

    component PauseInput: TextField {
        id: pinput
        property int valueMs: 0
        property int maxMs: 30000
        signal committed(int ms)

        implicitWidth: 46
        implicitHeight: 26
        leftPadding: 4
        rightPadding: 4
        topPadding: 0
        bottomPadding: 0
        leftInset: 0
        rightInset: 0
        topInset: 0
        bottomInset: 0
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 12
        font.family: "monospace"
        color: theme.textPrimary
        selectByMouse: true
        text: win.formatSeconds(valueMs)
        onValueMsChanged: if (!activeFocus) text = win.formatSeconds(valueMs)
        background: Rectangle {
            color: pinput.activeFocus ? theme.surfaceInput : "transparent"
            border.color: pinput.activeFocus ? theme.accentControl : (pinput.hovered ? theme.borderStrong : "transparent")
            border.width: 1
            radius: Math.min(4, theme.radius)
        }
        function commitValue() {
            const ms = win.parsePauseInputMs(text, valueMs, maxMs)
            text = win.formatSeconds(ms)
            if (ms !== valueMs)
                committed(ms)
        }
        onAccepted: {
            commitValue()
            focus = false
        }
        onEditingFinished: commitValue()
    }

    function formatSeconds(ms) {
        void(translator.language)
        return (ms / 1000.0).toLocaleString(Qt.locale(), 'f', 1)
    }

    function parsePauseInputMs(rawText, fallbackMs, maxMs) {
        const s = (rawText || "").trim().toLowerCase()
        if (!s) return fallbackMs
        let ms = NaN
        if (s.endsWith("ms")) {
            ms = parseFloat(s.slice(0, -2).trim())
        } else if (s.endsWith("s")) {
            ms = parseFloat(s.slice(0, -1).trim()) * 1000.0
        } else {
            ms = parseFloat(s.replace(",", ".")) * 1000.0
        }
        if (isNaN(ms)) return fallbackMs
        return Math.max(0, Math.min(maxMs, Math.round(ms)))
    }

    function formatSpeedLabel(ratePct) {
        void(translator.language)
        const mult = (1.0 + ratePct / 100.0).toLocaleString(Qt.locale(), 'f', 2)
        const sign = ratePct >= 0 ? "+" : ""
        return qsTr("%1x (%2%3%)").arg(mult).arg(sign).arg(ratePct)
    }

    function requestQuit() {
        if (backend.dirty) {
            win.pendingActionAfterDiscard = 1
            discardDialog.open()
        } else {
            Qt.quit()
        }
    }

    function requestNew() {
        if (backend.dirty) {
            win.pendingActionAfterDiscard = 2
            discardDialog.open()
        } else {
            backend.newProject()
        }
    }

    function requestOpen() {
        if (backend.dirty) {
            win.pendingActionAfterDiscard = 3
            discardDialog.open()
        } else {
            backend.openProjectDialog()
        }
    }

    onClosing: function(close) {
        if (!win.allowClose && backend.dirty) {
            close.accepted = false
            win.pendingActionAfterDiscard = 1
            discardDialog.open()
        }
    }

    Shortcut {
        sequence: "?"
        enabled: !win.textInputFocused
        context: Qt.ApplicationShortcut
        onActivated: helpOverlay.visible ? helpOverlay.close() : helpOverlay.open()
    }
    Shortcut {
        sequence: "F1"
        context: Qt.ApplicationShortcut
        onActivated: helpOverlay.visible ? helpOverlay.close() : helpOverlay.open()
    }
    Shortcut {
        sequence: "Q"
        enabled: !win.textInputFocused
        context: Qt.ApplicationShortcut
        onActivated: win.requestQuit()
    }
    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: win.requestQuit()
    }
    Shortcut {
        sequence: "Space"
        enabled: !win.textInputFocused
        context: Qt.ApplicationShortcut
        onActivated: backend.primaryAction()
    }
    Shortcut {
        sequence: "Ctrl+Space"
        context: Qt.ApplicationShortcut
        onActivated: backend.primaryAction()
    }
    Shortcut {
        sequence: "Ctrl+Return"
        context: Qt.ApplicationShortcut
        onActivated: backend.playLine(backend.selectedLineIndex)
    }
    Shortcut {
        sequence: "Ctrl+I"
        context: Qt.ApplicationShortcut
        onActivated: pasteDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+L"
        context: Qt.ApplicationShortcut
        onActivated: backend.addLine(backend.selectedLineIndex)
    }
    Shortcut {
        sequence: "Ctrl+N"
        context: Qt.ApplicationShortcut
        onActivated: win.requestNew()
    }
    Shortcut {
        sequence: "Ctrl+O"
        context: Qt.ApplicationShortcut
        onActivated: win.requestOpen()
    }
    Shortcut {
        sequence: "Ctrl+S"
        context: Qt.ApplicationShortcut
        onActivated: backend.saveProject()
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        context: Qt.ApplicationShortcut
        onActivated: backend.saveProjectDialog()
    }
    Shortcut {
        sequence: "Ctrl+E"
        context: Qt.ApplicationShortcut
        onActivated: exportDialog.open()
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: !win.textInputFocused
        context: Qt.ApplicationShortcut
        onActivated: backend.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: !win.textInputFocused
        context: Qt.ApplicationShortcut
        onActivated: backend.redo()
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.busy) backend.cancelWork()
            else if (backend.playing) backend.stopPlayback()
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.bgTop }
                GradientStop { position: 0.45; color: theme.bgMid }
                GradientStop { position: 1.0; color: theme.bgBottom }
            }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(theme.bgTop.r, theme.bgTop.g, theme.bgTop.b, 0.28) }
                GradientStop { position: 0.55; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(theme.bgTop.r, theme.bgTop.g, theme.bgTop.b, 0.22) }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top Toolbar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                color: theme.surfaceBar
                border.color: theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 8

                    Label {
                        text: "ParloCue"
                        font.pixelSize: 16
                        font.bold: true
                        color: theme.accentText
                    }

                    Label {
                        text: "/"
                        color: theme.textDim
                        font.pixelSize: 16
                    }

                    TextField {
                        id: titleField
                        text: backend.projectTitle
                        font.pixelSize: 14
                        color: theme.textPrimary
                        Layout.preferredWidth: 250
                        Layout.preferredHeight: 34
                        topPadding: 6
                        bottomPadding: 6
                        leftPadding: 10
                        rightPadding: 10
                        selectByMouse: true
                        selectionColor: theme.accentControl
                        selectedTextColor: theme.accentControlForeground
                        background: Rectangle {
                            color: theme.surfaceInput
                            border.color: titleField.activeFocus ? theme.accentControl : theme.border
                            border.width: 1
                            radius: Math.min(4, theme.radius)
                        }
                        onEditingFinished: backend.projectTitle = text

                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            x: 10
                            visible: titleField.text.length === 0
                            text: qsTr("Script title")
                            font.pixelSize: 14
                            color: theme.textDim
                        }
                    }

                    Label {
                        visible: backend.dirty
                        text: "●"
                        font.pixelSize: 11
                        color: theme.accentText
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("Paste Script")
                        flat: true
                        Material.roundedScale: theme.radius
                        onClicked: pasteDialog.open()
                    }

                    Button {
                        text: qsTr("Import Word/PDF")
                        flat: true
                        Material.roundedScale: theme.radius
                        onClicked: backend.importDocumentDialog()
                    }

                    Button {
                        text: qsTr("New")
                        flat: true
                        Material.roundedScale: theme.radius
                        onClicked: win.requestNew()
                    }

                    Button {
                        text: qsTr("Open")
                        flat: true
                        Material.roundedScale: theme.radius
                        onClicked: win.requestOpen()
                    }

                    Button {
                        text: qsTr("Save")
                        flat: true
                        Material.roundedScale: theme.radius
                        onClicked: backend.saveProject()
                    }

                    Button {
                        text: qsTr("Close")
                        flat: true
                        Material.roundedScale: theme.radius
                        onClicked: win.requestQuit()
                    }

                    Rectangle {
                        width: 1
                        height: 24
                        color: theme.border
                    }

                    Button {
                        text: backend.playing ? qsTr("Stop") : qsTr("Play Full")
                        enabled: !backend.busy && backend.ttsAvailable
                        Material.background: theme.buttonBg
                        Material.roundedScale: theme.radius
                        onClicked: backend.primaryAction()
                    }

                    AccentBtn {
                        text: qsTr("Export MP3…")
                        enabled: !backend.busy && backend.ttsAvailable
                        onClicked: exportDialog.open()
                    }

                    CompactBtn {
                        text: "?"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        onClicked: helpOverlay.open()
                    }
                }
            }

            // Missing Dependency / Status Banner
            Rectangle {
                visible: !backend.ttsAvailable || !backend.ffmpegAvailable || backend.statusBanner.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                color: (!backend.ttsAvailable || !backend.ffmpegAvailable || backend.statusBannerIsError)
                    ? theme.bannerErrorBg : theme.bannerOkBg
                border.color: (!backend.ttsAvailable || !backend.ffmpegAvailable || backend.statusBannerIsError)
                    ? theme.bannerErrorBorder : theme.bannerOkBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        color: theme.textPrimary
                        elide: Text.ElideRight
                        text: {
                            if (!backend.ttsAvailable || !backend.ffmpegAvailable)
                                return backend.ttsStatusMessage
                            return backend.statusBanner
                        }
                    }

                    Button {
                        visible: !backend.ttsAvailable && !backend.busy
                        text: qsTr("Install Voices Engine")
                        highlighted: true
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: backend.installTts()
                    }

                    Button {
                        visible: backend.ttsAvailable && backend.ffmpegAvailable && backend.statusBanner.length > 0
                        text: qsTr("Dismiss")
                        flat: true
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: backend.dismissBanner()
                    }
                }
            }

            // Main Two-Pane Workspace
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Left Pane: Script Timeline
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    // Script sub-bar
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        color: theme.surfaceBar
                        border.color: theme.border
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 10

                            Button {
                                text: qsTr("+ Add Line")
                                flat: true
                                Material.foreground: theme.accentText
                                Material.roundedScale: Math.min(4, theme.radius)
                                onClicked: backend.addLine(backend.selectedLineIndex)
                            }

                            Rectangle { width: 1; height: 20; color: theme.border }

                            Label {
                                text: qsTr("Default pause:")
                                font.pixelSize: 12
                                color: theme.textMuted
                            }

                            CompactBtn {
                                text: "−"
                                onClicked: backend.defaultPauseMs = Math.max(0, backend.defaultPauseMs - 200)
                            }

                            RowLayout {
                                spacing: 2
                                PauseInput {
                                    valueMs: backend.defaultPauseMs
                                    maxMs: 15000
                                    onCommitted: function(ms) { backend.defaultPauseMs = ms }
                                }
                                Label {
                                    text: qsTr("s")
                                    font.pixelSize: 12
                                    font.family: "monospace"
                                    color: theme.textMuted
                                }
                            }

                            CompactBtn {
                                text: "+"
                                onClicked: backend.defaultPauseMs = Math.min(15000, backend.defaultPauseMs + 200)
                            }

                            Button {
                                text: qsTr("Apply to All")
                                flat: true
                                font.pixelSize: 12
                                Material.roundedScale: Math.min(4, theme.radius)
                                onClicked: backend.applyDefaultPauseToAll()
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: qsTr("%n lines", "", backend.lineCount) + " · " + qsTr("%n words", "", backend.wordCount)
                                font.pixelSize: 12
                                color: theme.textMuted
                            }
                        }
                    }

                    // Lines List
                    ListView {
                        id: lineList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 12
                        spacing: 10
                        clip: true
                        model: backend.lineModel

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        onCurrentIndexChanged: {}

                        Connections {
                            target: backend
                            function onPlaybackChanged() {
                                if (backend.playingFullScript && backend.playingLineIndex >= 0)
                                    lineList.positionViewAtIndex(backend.playingLineIndex, ListView.Contain)
                            }
                        }

                        delegate: Rectangle {
                            id: lineCard
                            width: lineList.width - 12
                            height: cardCol.implicitHeight + 20
                            radius: theme.radius
                            color: (backend.playingLineIndex === index && backend.playing)
                                ? theme.surfaceCardPlaying
                                : (backend.selectedLineIndex === index ? theme.surfaceCardSelected : theme.surfaceCard)
                            border.color: (backend.playingLineIndex === index && backend.playing)
                                ? theme.accentControl
                                : (backend.selectedLineIndex === index ? theme.borderStrong : theme.border)
                            border.width: 1

                            // Left speaker accent strip
                            Rectangle {
                                width: 4
                                height: parent.height - 2
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.topMargin: 1
                                color: (backend.playingLineIndex === index && backend.playing)
                                    ? theme.accentControl : Qt.darker(model.speakerColor, 1.15)
                                radius: Math.min(2, theme.radius)
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                onPressed: function(mouse) {
                                    backend.selectedLineIndex = index
                                    mouse.accepted = false
                                }
                            }

                            ColumnLayout {
                                id: cardCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 10
                                anchors.leftMargin: 14
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Label {
                                        text: (index + 1).toString().padStart(2, '0')
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                        color: theme.textDim
                                    }

                                    // Speaker Picker Pill
                                    Rectangle {
                                        Layout.preferredHeight: 26
                                        Layout.preferredWidth: spPillRow.implicitWidth + 16
                                        radius: Math.min(4, theme.radius)
                                        color: theme.surfacePill
                                        border.color: Qt.darker(model.speakerColor, 1.25)
                                        border.width: 1

                                        RowLayout {
                                            id: spPillRow
                                            anchors.centerIn: parent
                                            spacing: 6

                                            Rectangle {
                                                width: 8
                                                height: 8
                                                radius: 4
                                                color: Qt.darker(model.speakerColor, 1.15)
                                            }
                                            Label {
                                                text: model.speakerName
                                                font.pixelSize: 12
                                                font.bold: true
                                                color: theme.textPrimary
                                            }
                                            Label {
                                                text: "▾"
                                                font.pixelSize: 10
                                                color: theme.textMuted
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                speakerMenu.targetLineIndex = index
                                                backend.selectedLineIndex = index
                                                speakerMenu.open()
                                            }
                                        }

                                        Menu {
                                            id: speakerMenu
                                            property int targetLineIndex: -1
                                            Material.roundedScale: theme.radius
                                            Material.background: theme.surfaceDialog
                                            Material.foreground: theme.textPrimary
                                            Instantiator {
                                                model: backend.speakerModel
                                                delegate: MenuItem {
                                                    text: model.name + " (" + model.voiceDisplay + ")"
                                                    onTriggered: backend.setLineSpeaker(speakerMenu.targetLineIndex, model.speakerId)
                                                }
                                                onObjectAdded: function(idx, obj) { speakerMenu.insertItem(idx, obj) }
                                                onObjectRemoved: function(idx, obj) { speakerMenu.removeItem(obj) }
                                            }
                                        }
                                    }

                                    // Voice Picker Pill (reassign speaker's neural voice directly from left column)
                                    Rectangle {
                                        Layout.preferredHeight: 26
                                        Layout.preferredWidth: voicePillRow.implicitWidth + 14
                                        radius: Math.min(4, theme.radius)
                                        color: voicePillMouse.containsMouse ? theme.buttonHover : theme.surfacePill
                                        border.color: theme.border
                                        border.width: 1

                                        RowLayout {
                                            id: voicePillRow
                                            anchors.centerIn: parent
                                            spacing: 4

                                            Label {
                                                text: model.speakerVoiceDisplay
                                                font.pixelSize: 11
                                                color: theme.textSecondary
                                            }
                                            Label {
                                                text: "▾"
                                                font.pixelSize: 10
                                                color: theme.textMuted
                                            }
                                        }

                                        MouseArea {
                                            id: voicePillMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                const spIdx = backend.speakerIndexForId(model.speakerId)
                                                if (spIdx >= 0) {
                                                    backend.selectedLineIndex = index
                                                    backend.selectedSpeakerIndex = spIdx
                                                    voicePicker.targetSpeakerIndex = spIdx
                                                    voicePicker.currentVoice = model.speakerVoice || ""
                                                    voicePicker.open()
                                                }
                                            }
                                        }
                                    }

                                    // Play Line Button
                                    CompactBtn {
                                        text: (backend.playing && !backend.playingFullScript && backend.playingLineIndex === index)
                                            ? "⏹" : "▶"
                                        Layout.preferredWidth: 30
                                        enabled: !backend.busy && backend.ttsAvailable
                                        labelColor: theme.accentText
                                        onClicked: {
                                            backend.selectedLineIndex = index
                                            if (backend.playing && !backend.playingFullScript && backend.playingLineIndex === index)
                                                backend.stopPlayback()
                                            else
                                                backend.playLine(index)
                                        }
                                    }

                                    // Synthesis cache dot
                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: {
                                            if (model.clipState === "ready") return theme.statusOk
                                            if (model.clipState === "synthesizing") return theme.accentControl
                                            if (model.clipState === "error") return theme.statusError
                                            return theme.borderStrong
                                        }
                                    }

                                    // Override indicator
                                    Rectangle {
                                        visible: model.hasRateOverride || model.hasPitchOverride
                                        color: theme.overrideBg
                                        border.color: theme.accentControl
                                        border.width: 1
                                        radius: Math.min(4, theme.radius)
                                        Layout.preferredHeight: 22
                                        Layout.preferredWidth: ovLbl.implicitWidth + 10
                                        Label {
                                            id: ovLbl
                                            anchors.centerIn: parent
                                            text: qsTr("Custom")
                                            font.pixelSize: 11
                                            color: theme.accentText
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    // Pause control
                                    Label {
                                        text: qsTr("Pause:")
                                        font.pixelSize: 11
                                        color: theme.textMuted
                                    }
                                    CompactBtn {
                                        text: "−"
                                        onClicked: backend.setLinePause(index, Math.max(0, model.pauseMs - 200))
                                    }
                                    RowLayout {
                                        spacing: 1
                                        PauseInput {
                                            valueMs: model.pauseMs
                                            maxMs: 30000
                                            onCommitted: function(ms) { backend.setLinePause(index, ms) }
                                        }
                                        Label {
                                            text: "s"
                                            font.pixelSize: 12
                                            font.family: "monospace"
                                            color: theme.textMuted
                                        }
                                    }
                                    CompactBtn {
                                        text: "+"
                                        onClicked: backend.setLinePause(index, Math.min(30000, model.pauseMs + 200))
                                    }

                                    // Reorder & Actions
                                    CompactBtn {
                                        text: "↑"
                                        enabled: index > 0
                                        onClicked: backend.moveLine(index, index - 1)
                                    }
                                    CompactBtn {
                                        text: "↓"
                                        enabled: index < backend.lineCount - 1
                                        onClicked: backend.moveLine(index, index + 1)
                                    }
                                    CompactBtn {
                                        text: "×"
                                        labelColor: theme.textMuted
                                        onClicked: backend.removeLine(index)
                                    }
                                }

                                TextArea {
                                    id: lineEdit
                                    Layout.fillWidth: true
                                    Layout.minimumHeight: 44
                                    text: model.text
                                    wrapMode: TextEdit.Wrap
                                    font.pixelSize: 14
                                    color: theme.textPrimary
                                    selectByMouse: true
                                    selectionColor: theme.accentControl
                                    selectedTextColor: theme.accentControlForeground
                                    topPadding: 10
                                    bottomPadding: 10
                                    leftPadding: 12
                                    rightPadding: 12
                                    topInset: 0
                                    bottomInset: 0
                                    background: Rectangle {
                                        color: theme.surfaceInput
                                        border.color: lineEdit.activeFocus ? theme.accentControl : theme.border
                                        border.width: 1
                                        radius: Math.min(4, theme.radius)
                                    }
                                    onActiveFocusChanged: {
                                        if (activeFocus)
                                            backend.selectedLineIndex = index
                                        else
                                            backend.commitLineText(index, text)
                                    }
                                    onTextChanged: {
                                        if (text !== model.text)
                                            backend.setLineText(index, text)
                                    }

                                    Label {
                                        x: 12
                                        y: 10
                                        visible: lineEdit.text.length === 0
                                        text: qsTr("Type or paste spoken text for this line…")
                                        font.pixelSize: 14
                                        color: theme.textDim
                                    }
                                }

                                Label {
                                    visible: model.clipState === "error" && model.clipError.length > 0
                                    text: model.clipError
                                    font.pixelSize: 11
                                    color: theme.statusError
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                // Vertical Divider
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    color: theme.border
                }

                // Right Pane: Speakers & Voice Studio
                ScrollView {
                    id: rightScroll
                    Layout.preferredWidth: 350
                    Layout.minimumWidth: 300
                    Layout.fillHeight: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: rightScroll.availableWidth
                        spacing: 14

                        Item { Layout.preferredHeight: 2 }

                        // Speakers Header
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14

                            Label {
                                text: qsTr("Speakers & Voices")
                                font.pixelSize: 15
                                font.bold: true
                                color: theme.textPrimary
                                Layout.fillWidth: true
                            }
                            Button {
                                text: qsTr("Randomize")
                                flat: true
                                Material.roundedScale: Math.min(4, theme.radius)
                                onClicked: backend.randomizeSpeakerVoices()
                            }
                            Button {
                                text: qsTr("+ Add")
                                flat: true
                                Material.foreground: theme.accentText
                                Material.roundedScale: Math.min(4, theme.radius)
                                onClicked: backend.addSpeaker()
                            }
                        }

                        // Speaker Chips / List
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            spacing: 6

                            Repeater {
                                model: backend.speakerModel
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 48
                                    radius: Math.min(4, theme.radius)
                                    color: backend.selectedSpeakerIndex === index ? theme.surfaceCardSelected : theme.surfaceCard
                                    border.color: backend.selectedSpeakerIndex === index ? theme.accentControl : theme.border
                                    border.width: 1

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.selectedSpeakerIndex = index
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 8
                                        spacing: 8

                                        Rectangle {
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: model.color
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Label {
                                                text: model.name
                                                font.pixelSize: 13
                                                font.bold: true
                                                color: theme.textPrimary
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: model.voiceDisplay
                                                font.pixelSize: 11
                                                color: theme.textMuted
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        CompactBtn {
                                            visible: backend.speakerModel.rowCount() > 1
                                            text: "×"
                                            onClicked: backend.removeSpeaker(index)
                                        }
                                    }
                                }
                            }
                        }

                        // Selected Speaker Settings Card
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            Layout.preferredHeight: spSettingsCol.implicitHeight + 24
                            radius: theme.radius
                            color: theme.surfaceCard
                            border.color: theme.border
                            border.width: 1

                            ColumnLayout {
                                id: spSettingsCol
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Speaker name")
                                    text: backend.selectedSpeaker.name || ""
                                    font.pixelSize: 14
                                    selectByMouse: true
                                    Material.roundedScale: Math.min(4, theme.radius)
                                    onEditingFinished: backend.setSpeakerName(backend.selectedSpeakerIndex, text)
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Label {
                                        text: qsTr("Neural Voice")
                                        font.pixelSize: 12
                                        color: theme.textMuted
                                    }
                                    Label {
                                        text: backend.selectedSpeaker.voiceDisplay || ""
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: theme.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        AccentBtn {
                                            text: qsTr("Change Voice…")
                                            Layout.fillWidth: true
                                            onClicked: {
                                                voicePicker.targetSpeakerIndex = backend.selectedSpeakerIndex
                                                voicePicker.currentVoice = backend.selectedSpeaker.voice || ""
                                                voicePicker.open()
                                            }
                                        }
                                        Button {
                                            text: qsTr("Test")
                                            enabled: backend.ttsAvailable && !backend.busy
                                            Material.background: theme.buttonBg
                                            Material.roundedScale: Math.min(4, theme.radius)
                                            onClicked: backend.sampleVoice(
                                                backend.selectedSpeaker.voice,
                                                backend.selectedSpeaker.rate || 0,
                                                backend.selectedSpeaker.pitch || 0
                                            )
                                        }
                                    }
                                }

                                // Speed (Rate)
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: qsTr("Speed")
                                            font.pixelSize: 12
                                            color: theme.textMuted
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: win.formatSpeedLabel(backend.selectedSpeaker.rate || 0)
                                            font.pixelSize: 12
                                            font.family: "monospace"
                                            color: theme.accentText
                                        }
                                    }
                                    Slider {
                                        id: rateSlider
                                        Layout.fillWidth: true
                                        from: -50
                                        to: 100
                                        stepSize: 5
                                        value: backend.selectedSpeaker.rate || 0
                                        onPressedChanged: if (pressed) backend.saveUndoPoint()
                                        onMoved: backend.setSpeakerRate(backend.selectedSpeakerIndex, Math.round(value))
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: [
                                                { lbl: "-15%", val: -15 },
                                                { lbl: "0%", val: 0 },
                                                { lbl: "+15%", val: 15 }
                                            ]
                                            delegate: Button {
                                                text: modelData.lbl
                                                flat: (backend.selectedSpeaker.rate || 0) !== modelData.val
                                                font.pixelSize: 11
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 28
                                                Material.roundedScale: Math.min(4, theme.radius)
                                                onClicked: {
                                                    backend.saveUndoPoint()
                                                    backend.setSpeakerRate(backend.selectedSpeakerIndex, modelData.val)
                                                }
                                            }
                                        }
                                    }
                                }

                                // Pitch
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: qsTr("Pitch")
                                            font.pixelSize: 12
                                            color: theme.textMuted
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            readonly property int pVal: backend.selectedSpeaker.pitch || 0
                                            text: (pVal >= 0 ? "+" : "") + pVal + " Hz"
                                            font.pixelSize: 12
                                            font.family: "monospace"
                                            color: theme.textSecondary
                                        }
                                        Button {
                                            text: qsTr("Reset")
                                            flat: true
                                            font.pixelSize: 11
                                            visible: (backend.selectedSpeaker.pitch || 0) !== 0
                                            Material.roundedScale: Math.min(4, theme.radius)
                                            onClicked: {
                                                backend.saveUndoPoint()
                                                backend.setSpeakerPitch(backend.selectedSpeakerIndex, 0)
                                            }
                                        }
                                    }
                                    Slider {
                                        Layout.fillWidth: true
                                        from: -50
                                        to: 50
                                        stepSize: 2
                                        value: backend.selectedSpeaker.pitch || 0
                                        onPressedChanged: if (pressed) backend.saveUndoPoint()
                                        onMoved: backend.setSpeakerPitch(backend.selectedSpeakerIndex, Math.round(value))
                                    }
                                }

                                // Speaker Default Pause
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: qsTr("Default pause for speaker")
                                            font.pixelSize: 12
                                            color: theme.textMuted
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: qsTr("%1 s").arg(win.formatSeconds(backend.selectedSpeaker.defaultPauseMs || 0))
                                            font.pixelSize: 12
                                            font.family: "monospace"
                                            color: theme.textSecondary
                                        }
                                    }
                                    Slider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 5000
                                        stepSize: 100
                                        value: backend.selectedSpeaker.defaultPauseMs || 600
                                        onPressedChanged: if (pressed) backend.saveUndoPoint()
                                        onMoved: backend.setSpeakerDefaultPause(backend.selectedSpeakerIndex, Math.round(value))
                                    }
                                }
                            }
                        }

                        // Selected Line Override Card
                        Rectangle {
                            visible: backend.lineCount > 0
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            Layout.preferredHeight: lineOvCol.implicitHeight + 24
                            radius: theme.radius
                            color: theme.surfaceCard
                            border.color: theme.border
                            border.width: 1

                            ColumnLayout {
                                id: lineOvCol
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                Label {
                                    text: qsTr("Line %1 Override").arg(backend.selectedLineIndex + 1)
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: theme.textPrimary
                                }

                                CheckBox {
                                    text: qsTr("Override speed for this line")
                                    checked: backend.selectedLine.hasRateOverride || false
                                    onToggled: backend.setLineRateOverride(
                                        backend.selectedLineIndex,
                                        checked,
                                        checked ? (backend.selectedLine.speakerRate || 0) : 0
                                    )
                                }

                                RowLayout {
                                    visible: backend.selectedLine.hasRateOverride || false
                                    Layout.fillWidth: true
                                    Slider {
                                        Layout.fillWidth: true
                                        from: -50
                                        to: 100
                                        stepSize: 5
                                        value: backend.selectedLine.rateOverride || 0
                                        onMoved: backend.setLineRateOverride(backend.selectedLineIndex, true, Math.round(value))
                                    }
                                    Label {
                                        text: win.formatSpeedLabel(backend.selectedLine.rateOverride || 0)
                                        font.pixelSize: 11
                                        font.family: "monospace"
                                        color: theme.accentText
                                    }
                                }

                                CheckBox {
                                    text: qsTr("Override pitch for this line")
                                    checked: backend.selectedLine.hasPitchOverride || false
                                    onToggled: backend.setLinePitchOverride(
                                        backend.selectedLineIndex,
                                        checked,
                                        checked ? (backend.selectedLine.speakerPitch || 0) : 0
                                    )
                                }

                                RowLayout {
                                    visible: backend.selectedLine.hasPitchOverride || false
                                    Layout.fillWidth: true
                                    Slider {
                                        Layout.fillWidth: true
                                        from: -50
                                        to: 50
                                        stepSize: 2
                                        value: backend.selectedLine.pitchOverride || 0
                                        onMoved: backend.setLinePitchOverride(backend.selectedLineIndex, true, Math.round(value))
                                    }
                                    Label {
                                        readonly property int pv: backend.selectedLine.pitchOverride || 0
                                        text: (pv >= 0 ? "+" : "") + pv + " Hz"
                                        font.pixelSize: 11
                                        font.family: "monospace"
                                        color: theme.accentText
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 10 }
                    }
                }
            }

            // Bottom Transport / Progress Bar
            Rectangle {
                visible: backend.busy || backend.playing || backend.playbackDurationMs > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: theme.surfaceBar
                border.color: theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    // Synthesis Progress Mode
                    Label {
                        visible: backend.busy
                        text: backend.workMessage
                        font.pixelSize: 13
                        color: theme.textPrimary
                    }

                    ProgressBar {
                        visible: backend.busy
                        Layout.fillWidth: true
                        indeterminate: backend.workTotal === 0
                        from: 0
                        to: Math.max(1, backend.workTotal)
                        value: backend.workCompleted
                    }

                    Button {
                        visible: backend.busy
                        text: qsTr("Cancel")
                        flat: true
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: backend.cancelWork()
                    }

                    // Playback Transport Mode
                    Button {
                        visible: !backend.busy
                        text: backend.playing ? "⏹" : "▶"
                        flat: true
                        Material.foreground: theme.accentText
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: backend.playing ? backend.stopPlayback() : backend.playAll()
                    }

                    Label {
                        visible: !backend.busy
                        text: backend.playingFullScript && backend.playingLineIndex >= 0
                            ? qsTr("Playing line %1").arg(backend.playingLineIndex + 1)
                            : (backend.playingLineIndex >= 0
                                ? qsTr("Line %1").arg(backend.playingLineIndex + 1)
                                : qsTr("Voice sample"))
                        font.pixelSize: 12
                        color: theme.textMuted
                    }

                    Slider {
                        visible: !backend.busy
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, backend.playbackDurationMs)
                        value: backend.playbackPositionMs
                        onMoved: backend.seekPlayback(Math.round(value))
                    }

                    Label {
                        visible: !backend.busy
                        text: backend.formatDuration(backend.playbackPositionMs) + " / " + backend.formatDuration(backend.playbackDurationMs)
                        font.pixelSize: 12
                        font.family: "monospace"
                        color: theme.textSecondary
                    }
                }
            }
        }
    }

    PasteScriptDialog { id: pasteDialog }
    Connections {
        target: backend
        function onDocumentTextExtracted(text, fileName) {
            pasteDialog.openWithText(text)
        }
    }
    VoicePickerDialog { id: voicePicker }
    ExportDialog { id: exportDialog }
    ShortcutOverlay { id: helpOverlay }

    Dialog {
        id: discardDialog
        anchors.centerIn: Overlay.overlay
        width: 400
        modal: true
        title: qsTr("Unsaved Changes")
        Material.roundedScale: theme.radius

        background: Rectangle {
            color: theme.surfaceDialog
            border.color: theme.borderStrong
            border.width: 1
            radius: theme.radius
        }

        contentItem: ColumnLayout {
            spacing: 14
            Label {
                text: qsTr("Save changes to \"%1\" before continuing?").arg(backend.projectTitle)
                wrapMode: Text.Wrap
                color: theme.textSecondary
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    flat: true
                    Material.roundedScale: theme.radius
                    onClicked: discardDialog.close()
                }
                Button {
                    text: qsTr("Discard")
                    flat: true
                    Material.foreground: theme.statusError
                    Material.roundedScale: theme.radius
                    onClicked: {
                        discardDialog.close()
                        if (win.pendingActionAfterDiscard === 1) {
                            win.allowClose = true
                            Qt.quit()
                        } else if (win.pendingActionAfterDiscard === 2) {
                            backend.newProject()
                        } else if (win.pendingActionAfterDiscard === 3) {
                            backend.openProjectDialog()
                        }
                    }
                }
                Button {
                    text: qsTr("Save")
                    highlighted: true
                    Material.roundedScale: theme.radius
                    onClicked: {
                        discardDialog.close()
                        backend.saveProject()
                    }
                }
            }
        }
    }
}
