import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// One laser in the header; picking it shows what that laser's detectors see.
AbstractButton {
    id: chip
    property string label
    property color chipColor
    property bool active

    focusPolicy: Qt.NoFocus
    implicitWidth: Math.max(44, text.implicitWidth + 20)
    implicitHeight: 28

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: chip.active ? Qt.rgba(chip.chipColor.r, chip.chipColor.g, chip.chipColor.b, 0.22)
                           : chip.hovered ? "#202026" : "transparent"
        border.width: 1
        border.color: chip.active ? chip.chipColor : "#2e2e36"
    }
    contentItem: Text {
        id: text
        text: chip.label
        color: chip.active ? chip.chipColor : "#b8b8c0"
        font.bold: chip.active
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
