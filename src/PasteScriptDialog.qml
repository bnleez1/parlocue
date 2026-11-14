import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: root
    anchors.centerIn: Overlay.overlay
    width: Math.min(680, parent ? parent.width - 48 : 680)
    height: Math.min(580, parent ? parent.height - 48 : 580)
    modal: true
    title: qsTr("Paste or Import Script")
    Material.roundedScale: theme.radius

    property bool detectSpeakers: true
    property bool replaceExisting: true
    property var previewData: ({ lineCount: 0, speakerCount: 0, speakers: [] })

    function refreshPreview() {
        const idx = Math.max(0, speakerCombo.currentIndex)
        const spId = backend.speakerModel.rowCount() > idx
            ? backend.speakerModel.data(backend.speakerModel.index(idx, 0), Qt.UserRole + 1)
            : ""
        previewData = backend.parseScriptPreview(scriptInput.text, detectSpeakers, spId)
    }

    onOpened: {
        scriptInput.text = ""
        refreshPreview()
        scriptInput.forceActiveFocus()
    }

    background: Rectangle {
        color: "#18181c"
        border.color: "#2c2c34"
        border.width: 1
        radius: theme.radius
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: qsTr("Paste a lecture or dialogue. Use \"Speaker: text\" for multi-speaker scripts and \"[pause 2s]\" for custom breaks.")
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: "#a8a8b3"
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            CheckBox {
                text: qsTr("Auto-detect speakers (Name: text)")
                checked: root.detectSpeakers
                onToggled: {
                    root.detectSpeakers = checked
                    root.refreshPreview()
                }
            }

            RowLayout {
                spacing: 8
                Label {
                    text: qsTr("Default speaker:")
                    font.pixelSize: 13
                    color: "#c8c8d2"
                }
                ComboBox {
                    id: speakerCombo
                    model: backend.speakerModel
                    textRole: "name"
                    Layout.preferredWidth: 160
                    Material.roundedScale: theme.radius
                    onCurrentIndexChanged: root.refreshPreview()
                }
            }

            Item { Layout.fillWidth: true }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                id: scriptInput
                wrapMode: TextEdit.Wrap
                font.pixelSize: 14
                color: "#f4f4f6"
                topPadding: 10
                bottomPadding: 10
                leftPadding: 12
                rightPadding: 12
                topInset: 0
                bottomInset: 0
                selectionColor: theme.accent
                selectedTextColor: theme.accentForeground
                background: Rectangle {
                    color: "#111115"
                    border.color: scriptInput.activeFocus ? theme.accent : "#2a2a32"
                    border.width: 1
                    radius: Math.min(4, theme.radius)
                }
                onTextChanged: root.refreshPreview()

                Label {
                    x: 10
                    y: 10
                    width: parent.width - 20
                    wrapMode: Text.Wrap
                    visible: scriptInput.text.length === 0
                    text: qsTr("Narrator: Listen to a conversation between a student and an advisor.\nStudent: Hi, do you have a minute?\nAdvisor: Sure, come on in.\n[pause 1.5s]\nStudent: I had a question about the syllabus.")
                    font.pixelSize: 14
                    color: "#686876"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                color: "#22222a"
                radius: Math.min(4, theme.radius)
                Layout.preferredHeight: 30
                Layout.preferredWidth: previewLabel.implicitWidth + 20

                Label {
                    id: previewLabel
                    anchors.centerIn: parent
                    font.pixelSize: 12
                    color: previewData.lineCount > 0 ? theme.accent : "#888894"
                    text: previewData.lineCount > 0
                        ? qsTr("Detected %1 lines · %2 speakers (%3)")
                              .arg(previewData.lineCount)
                              .arg(previewData.speakerCount)
                              .arg((previewData.speakers || []).join(", "))
                        : qsTr("Paste script text above to preview")
                }
            }

            Item { Layout.fillWidth: true }

            CheckBox {
                text: qsTr("Replace current script")
                checked: root.replaceExisting
                onToggled: root.replaceExisting = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Cancel")
                flat: true
                Material.roundedScale: theme.radius
                onClicked: root.close()
            }

            Button {
                text: root.replaceExisting ? qsTr("Import Script") : qsTr("Append Lines")
                highlighted: true
                enabled: previewData.lineCount > 0
                Material.roundedScale: theme.radius
                onClicked: {
                    const idx = Math.max(0, speakerCombo.currentIndex)
                    const spId = backend.speakerModel.rowCount() > idx
                        ? backend.speakerModel.data(backend.speakerModel.index(idx, 0), Qt.UserRole + 1)
                        : ""
                    backend.importScriptText(scriptInput.text, root.detectSpeakers, spId, root.replaceExisting)
                    root.close()
                }
            }
        }
    }
}
