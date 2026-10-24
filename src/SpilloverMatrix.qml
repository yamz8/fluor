import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// How much of each row's dye lands in each column's detector, as a share of
// what its own detector sees. The hot cells are the panel's problems.
Item {
    id: matrix
    property int selected: -1
    signal picked(int index)

    readonly property int cell: 52
    readonly property int rowHeight: 26
    readonly property int nameWidth: 150
    readonly property int headerHeight: 84
    readonly property var rows: backend.spill

    implicitHeight: titleRow.height + headerHeight + rows.length * rowHeight + 20

    // The grid is placed by hand, so mirror it by hand for right-to-left languages.
    readonly property bool rtl: Qt.application.layoutDirection === Qt.RightToLeft
    // Slanted column names reach past the last column.
    readonly property real gridWidth: nameWidth + rows.length * cell + 80
    function place(x, width) { return rtl ? gridWidth - x - width : x }

    function cellColor(value, self) {
        if (self) return "#1c1c22"
        if (value < 0) return "transparent"
        if (value >= 0.25) return Qt.rgba(1, 0.42, 0.5, Math.min(0.85, 0.35 + value * 0.6))
        if (value >= 0.10) return Qt.rgba(0.96, 0.69, 0.25, 0.25 + value)
        return Qt.rgba(1, 1, 1, Math.min(0.12, value * 1.2))
    }

    Row {
        id: titleRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        y: 10
        spacing: 10
        Label {
            id: titleLabel
            text: qsTr("Spillover")
            font.bold: true
            color: "#e6e6ea"
        }
        Label {
            width: Math.min(implicitWidth, titleRow.width - titleLabel.width - 10)
            anchors.baseline: titleLabel.baseline
            text: qsTr("each row's dye in each column's detector, as a share of its own")
            color: "#8a8a94"
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    // Anchored to the leading edge, which layout mirroring flips for right-to-left.
    Flickable {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: titleRow.y + titleRow.height + 4
        anchors.leftMargin: 16
        width: Math.min(matrix.width - 32, matrix.gridWidth)
        clip: true
        contentWidth: matrix.gridWidth
        contentHeight: matrix.headerHeight + matrix.rows.length * matrix.rowHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}

        // Column headers, slanted so long dye names fit over narrow columns.
        Repeater {
            model: matrix.rows
            Item {
                x: matrix.place(matrix.nameWidth + index * matrix.cell, matrix.cell)
                width: matrix.cell
                height: matrix.headerHeight
                Text {
                    x: parent.width / 2 - 4
                    y: parent.height - 6
                    transformOrigin: Item.TopLeft
                    rotation: -45
                    text: modelData.name
                    color: index === matrix.selected ? modelData.color : "#b8b8c0"
                    font.pixelSize: 12
                    font.bold: index === matrix.selected
                }
            }
        }

        Repeater {
            model: matrix.rows
            Item {
                id: rowItem
                readonly property int row: index
                readonly property var entry: modelData
                y: matrix.headerHeight + index * matrix.rowHeight
                width: matrix.nameWidth + matrix.rows.length * matrix.cell
                height: matrix.rowHeight

                Rectangle {
                    x: matrix.place(0, width)
                    width: matrix.gridWidth - 80
                    height: parent.height
                    color: rowItem.row === matrix.selected ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.10) : "transparent"
                }
                Text {
                    x: matrix.place(0, matrix.nameWidth) + (matrix.rtl ? 12 : 0)
                    width: matrix.nameWidth - 12
                    // Dye names are Latin, so pin their alignment to the layout, not the text.
                    LayoutMirroring.enabled: false
                    horizontalAlignment: matrix.rtl ? Text.AlignRight : Text.AlignLeft
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: rowItem.entry.name
                    color: rowItem.entry.color
                    font.pixelSize: 12
                    font.bold: rowItem.row === matrix.selected
                    elide: Text.ElideRight
                }
                Repeater {
                    model: rowItem.entry.cells
                    Rectangle {
                        readonly property bool self: index === rowItem.row
                        x: matrix.place(matrix.nameWidth + index * matrix.cell, matrix.cell) + 1
                        y: 1
                        width: matrix.cell - 2
                        height: matrix.rowHeight - 2
                        radius: Math.min(4, theme.radius)
                        color: matrix.cellColor(modelData, self)
                        Text {
                            anchors.centerIn: parent
                            font.pixelSize: 12
                            color: modelData >= 0.25 && !parent.self ? "#ffffff" : modelData >= 0.10 ? "#f0e0c0" : "#8a8a94"
                            font.bold: modelData >= 0.25 && !parent.self
                            text: parent.self ? "—" : modelData < 0 ? "?" : modelData < 0.005 ? "" : win.pct(modelData)
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: matrix.picked(rowItem.row)
                            ToolTip.visible: containsMouse && !parent.self && modelData >= 0
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("%1 spills %2 into %3's detector")
                                          .arg(rowItem.entry.name).arg(win.pct(modelData)).arg(matrix.rows[index].name)
                        }
                    }
                }
            }
        }
    }
}
