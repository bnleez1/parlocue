import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Popup {
    id: root
    anchors.centerIn: Overlay.overlay
    width: Math.min(540, parent ? parent.width - 48 : 540)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 24

    background: Rectangle {
        color: "#18181c"
        border.color: "#2c2c34"
        border.width: 1
        radius: theme.radius
    }

    readonly property var shortcuts: [
        { keys: "Space / Ctrl+Space", desc: qsTr("Play or stop full script preview") },
        { keys: "Ctrl+Enter", desc: qsTr("Play selected line") },
        { keys: "Ctrl+I", desc: qsTr("Paste or import a script") },
        { keys: "Ctrl+L", desc: qsTr("Add a new line after selection") },
        { keys: "Ctrl+O", desc: qsTr("Open project (.parlocue, .recite, or .txt)") },
        { keys: "Ctrl+S / Ctrl+Shift+S", desc: qsTr("Save / Save As") },
        { keys: "Ctrl+E", desc: qsTr("Export combined MP3 or clips") },
        { keys: "Ctrl+Z / Ctrl+Shift+Z", desc: qsTr("Undo / Redo") },
        { keys: "Esc", desc: qsTr("Cancel active audio generation or close dialog") },
        { keys: "? / F1", desc: qsTr("Show or hide keyboard shortcuts") },
        { keys: "Q / Ctrl+Q", desc: qsTr("Quit ParloCue") }
    ]

    contentItem: ColumnLayout {
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Keyboard Shortcuts")
                font.pixelSize: 18
                font.bold: true
                color: "#f4f4f6"
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Close")
                flat: true
                Material.roundedScale: theme.radius
                onClicked: root.close()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: root.shortcuts
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 168
                        Layout.preferredHeight: 28
                        color: "#222228"
                        border.color: "#32323c"
                        border.width: 1
                        radius: Math.min(4, theme.radius)

                        Label {
                            anchors.centerIn: parent
                            text: modelData.keys
                            font.pixelSize: 12
                            font.family: "monospace"
                            color: theme.accent
                        }
                    }

                    Label {
                        text: modelData.desc
                        font.pixelSize: 13
                        color: "#d4d4dc"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
