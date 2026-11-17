import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: root
    anchors.centerIn: Overlay.overlay
    width: Math.min(720, parent ? parent.width - 48 : 720)
    height: Math.min(600, parent ? parent.height - 48 : 600)
    modal: true
    title: qsTr("Choose Neural Voice")
    Material.roundedScale: theme.radius

    property int targetSpeakerIndex: 0
    property string currentVoice: ""

    onOpened: {
        searchField.text = ""
        backend.voiceModel.search = ""
        searchField.forceActiveFocus()
    }

    background: Rectangle {
        color: theme.surfaceDialog
        border.color: theme.borderStrong
        border.width: 1
        radius: theme.radius
    }

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Search voice, accent, or style (e.g. Jenny, Guy, British, Friendly)")
                font.pixelSize: 13
                Material.roundedScale: theme.radius
                onTextChanged: backend.voiceModel.search = text
            }

            Button {
                text: backend.voicesLoading ? qsTr("Refreshing…") : qsTr("Refresh")
                enabled: backend.ttsAvailable && !backend.voicesLoading
                Material.background: theme.buttonBg
                Material.roundedScale: theme.radius
                onClicked: backend.refreshVoices()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { label: qsTr("English"), val: "en" },
                    { label: "en-US", val: "en-US" },
                    { label: "en-GB", val: "en-GB" },
                    { label: "en-AU", val: "en-AU" },
                    { label: qsTr("All Languages"), val: "all" }
                ]
                delegate: Button {
                    text: modelData.label
                    flat: backend.voiceModel.localeFilter !== modelData.val
                    highlighted: backend.voiceModel.localeFilter === modelData.val
                    font.pixelSize: 12
                    Material.roundedScale: Math.min(4, theme.radius)
                    onClicked: backend.voiceModel.localeFilter = modelData.val
                }
            }

            Item { Layout.fillWidth: true }

            Repeater {
                model: [
                    { label: qsTr("All"), val: "" },
                    { label: qsTr("Female"), val: "Female" },
                    { label: qsTr("Male"), val: "Male" }
                ]
                delegate: Button {
                    text: modelData.label
                    flat: backend.voiceModel.genderFilter !== modelData.val
                    highlighted: backend.voiceModel.genderFilter === modelData.val
                    font.pixelSize: 12
                    Material.roundedScale: Math.min(4, theme.radius)
                    onClicked: backend.voiceModel.genderFilter = modelData.val
                }
            }

            CheckBox {
                text: qsTr("Favorites")
                checked: backend.voiceModel.favoritesOnly
                onToggled: backend.voiceModel.favoritesOnly = checked
            }
        }

        ListView {
            id: voiceList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: backend.voiceModel

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                width: voiceList.width - 10
                height: 60
                radius: Math.min(4, theme.radius)
                color: model.shortName === root.currentVoice ? theme.surfaceCardSelected : theme.surfaceCard
                border.color: model.shortName === root.currentVoice ? theme.accentControl : theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    Button {
                        text: model.isFavorite ? "★" : "☆"
                        flat: true
                        Layout.preferredWidth: 34
                        font.pixelSize: 16
                        Material.foreground: model.isFavorite ? theme.accentText : theme.textDim
                        onClicked: backend.toggleFavoriteVoice(model.shortName)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 8
                            Label {
                                text: model.displayName
                                font.pixelSize: 14
                                font.bold: true
                                color: theme.textPrimary
                            }
                            Rectangle {
                                color: theme.surfacePill
                                radius: Math.min(4, theme.radius)
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: locText.implicitWidth + 12
                                Label {
                                    id: locText
                                    anchors.centerIn: parent
                                    text: model.locale
                                    font.pixelSize: 11
                                    color: theme.textSecondary
                                }
                            }
                            Label {
                                text: model.gender
                                font.pixelSize: 12
                                color: theme.textMuted
                            }
                        }

                        Label {
                            text: model.personalities ? model.personalities : model.shortName
                            font.pixelSize: 12
                            color: theme.textMuted
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Button {
                        text: qsTr("Sample")
                        flat: true
                        enabled: backend.ttsAvailable && !backend.busy
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: backend.sampleVoice(model.shortName)
                    }

                    Button {
                        text: model.shortName === root.currentVoice ? qsTr("Selected") : qsTr("Use")
                        highlighted: model.shortName !== root.currentVoice
                        flat: model.shortName === root.currentVoice
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: {
                            backend.setSpeakerVoice(root.targetSpeakerIndex, model.shortName)
                            root.currentVoice = model.shortName
                            root.close()
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: voiceList.count === 0
                text: qsTr("No voices match your filter.")
                color: theme.textMuted
                font.pixelSize: 14
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("%n voices available", "", backend.voiceModel.totalCount)
                font.pixelSize: 12
                color: theme.textMuted
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Close")
                flat: true
                Material.roundedScale: theme.radius
                onClicked: root.close()
            }
        }
    }
}
