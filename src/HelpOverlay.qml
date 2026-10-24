import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every shortcut, shown with ?.
Rectangle {
    id: overlay
    signal dismissed()
    color: Qt.rgba(0, 0, 0, 0.72)

    readonly property var shortcuts: [
        ["Space", qsTr("Add a dye")],
        ["A", qsTr("Auto-assign every dye to a detector")],
        ["↑ ↓", qsTr("Select a dye")],
        ["← →", qsTr("Move the dye to the previous or next detector")],
        ["Enter", qsTr("Choose the dye's detector")],
        ["Delete", qsTr("Remove the dye")],
        ["1 – 9", qsTr("Show what one laser excites")],
        ["0", qsTr("Show every laser")],
        ["E", qsTr("Show or hide excitation curves")],
        ["I", qsTr("Edit the instrument")],
        ["Ctrl+Z", qsTr("Undo")],
        ["Ctrl+Shift+Z", qsTr("Redo")],
        ["Ctrl+N", qsTr("New panel")],
        ["Ctrl+O", qsTr("Open a panel")],
        ["Ctrl+S", qsTr("Save")],
        ["Ctrl+Shift+S", qsTr("Save as")],
        ["?", qsTr("Show or hide shortcuts")],
        ["Q", qsTr("Quit")]
    ]

    MouseArea { anchors.fill: parent; onClicked: overlay.dismissed() }

    Rectangle {
        anchors.centerIn: parent
        width: grid.implicitWidth + 64
        height: grid.implicitHeight + heading.height + 72
        color: "#16161a"
        radius: theme.radius
        border.color: "#2e2e36"

        Label {
            id: heading
            x: 32
            y: 24
            text: qsTr("Keyboard shortcuts")
            font.bold: true
            font.pixelSize: 17
            color: "#e6e6ea"
        }
        GridLayout {
            id: grid
            x: 32
            anchors.top: heading.bottom
            anchors.topMargin: 16
            columns: 2
            columnSpacing: 28
            rowSpacing: 8
            Repeater {
                model: overlay.shortcuts.length * 2
                Label {
                    readonly property var pair: overlay.shortcuts[Math.floor(index / 2)]
                    text: index % 2 === 0 ? pair[0] : pair[1]
                    color: index % 2 === 0 ? theme.accent : "#c8c8d0"
                    font.bold: index % 2 === 0
                    font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                }
            }
        }
    }
}
