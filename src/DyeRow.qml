import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A dye in the panel: its peaks, and the detector it is read in.
ItemDelegate {
    id: row
    property var dye
    property bool selected

    signal picked()
    signal chooseChannel()
    signal remove()

    height: 58
    focusPolicy: Qt.NoFocus
    onClicked: picked()
    onDoubleClicked: chooseChannel()

    background: Rectangle {
        color: row.selected ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.12)
                            : row.hovered ? "#1c1c22" : "transparent"
        Rectangle {
            width: 3
            height: parent.height
            color: row.selected ? Material.accent : "transparent"
        }
    }

    contentItem: RowLayout {
        spacing: 10
        Rectangle {
            width: 12; height: 12; radius: 6
            color: row.dye.color
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Item {
                Layout.fillWidth: true
                implicitHeight: nameLabel.implicitHeight
                Label {
                    id: nameLabel
                    width: Math.min(implicitWidth, parent.width - stars.implicitWidth - 6)
                    text: row.dye.name
                    font.bold: true
                    elide: Text.ElideRight
                    color: "#e6e6ea"
                }
                Label {
                    id: stars
                    anchors.left: nameLabel.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: nameLabel.verticalCenter
                    text: "★".repeat(Math.round(row.dye.brightness))
                    color: Material.accent
                    font.pixelSize: 10
                    opacity: 0.8
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("ex %1 · em %2").arg(win.num(row.dye.ex)).arg(win.num(row.dye.em))
                font.pixelSize: 12
                color: "#8a8a94"
                elide: Text.ElideRight
            }
        }
        Button {
            id: channelButton
            Layout.preferredWidth: 112
            flat: true
            focusPolicy: Qt.NoFocus
            Material.roundedScale: theme.radius
            onClicked: row.chooseChannel()
            contentItem: Label {
                text: row.dye.channel >= 0 ? row.dye.channelName : qsTr("No detector")
                color: row.dye.channel >= 0 ? row.dye.channelColor : "#ff6b81"
                font.pixelSize: 12
                font.bold: row.dye.channel < 0
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            ToolTip.visible: hovered && row.dye.channel >= 0
            ToolTip.delay: 500
            ToolTip.text: qsTr("Excited %1, filter passes %2 of its emission")
                          .arg(win.pct(row.dye.excitation)).arg(win.pct(row.dye.captured))
        }
        ToolButton {
            Layout.preferredWidth: 32
            text: "×"
            focusPolicy: Qt.NoFocus
            opacity: row.hovered || row.selected ? 1 : 0
            onClicked: row.remove()
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Remove from the panel")
        }
    }
}
