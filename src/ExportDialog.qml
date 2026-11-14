import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: root
    anchors.centerIn: Overlay.overlay
    width: Math.min(480, parent ? parent.width - 48 : 480)
    modal: true
    title: qsTr("Export Audio")
    Material.roundedScale: theme.radius

    property int exportMode: 0 // 0 = combined MP3, 1 = individual clips, 2 = both
    property int bitrateKbps: 192
    property bool normalizeLoudness: false

    background: Rectangle {
        color: "#18181c"
        border.color: "#2c2c34"
        border.width: 1
        radius: theme.radius
    }

    contentItem: ColumnLayout {
        spacing: 14

        Label {
            text: qsTr("Choose what to export for Listening I / TOEFL or Kdenlive:")
            font.pixelSize: 13
            color: "#b4b4c0"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        ButtonGroup { id: modeGroup }

        RadioButton {
            ButtonGroup.group: modeGroup
            checked: root.exportMode === 0
            text: qsTr("Combined final .mp3 (with all pauses)")
            onClicked: root.exportMode = 0
        }
        RadioButton {
            ButtonGroup.group: modeGroup
            checked: root.exportMode === 1
            text: qsTr("Individual .mp3 clips per line (for Kdenlive)")
            onClicked: root.exportMode = 1
        }
        RadioButton {
            ButtonGroup.group: modeGroup
            checked: root.exportMode === 2
            text: qsTr("Both combined .mp3 and individual clips")
            onClicked: root.exportMode = 2
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: qsTr("MP3 Bitrate:")
                font.pixelSize: 13
                color: "#d4d4dc"
            }

            ComboBox {
                model: ["128 kbps", "192 kbps", "320 kbps"]
                currentIndex: 1
                Material.roundedScale: theme.radius
                onCurrentIndexChanged: {
                    const rates = [128, 192, 320]
                    root.bitrateKbps = rates[currentIndex]
                }
            }
        }

        CheckBox {
            visible: root.exportMode !== 1
            text: qsTr("Normalize loudness (-14 LUFS, optimal for YouTube)")
            checked: root.normalizeLoudness
            onToggled: root.normalizeLoudness = checked
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
                text: qsTr("Export…")
                highlighted: true
                enabled: backend.ttsAvailable && (root.exportMode === 1 || backend.ffmpegAvailable) && !backend.busy
                Material.roundedScale: theme.radius
                onClicked: {
                    root.close()
                    if (root.exportMode === 0)
                        backend.exportCombinedDialog(root.bitrateKbps, root.normalizeLoudness)
                    else if (root.exportMode === 1)
                        backend.exportClipsDialog(root.bitrateKbps)
                    else
                        backend.exportBothDialog(root.bitrateKbps, root.normalizeLoudness)
                }
            }
        }
    }
}
