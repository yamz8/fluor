import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The instrument: its lasers and the bandpass filter of each detector behind
// them. Retuning a laser or filter carries its dyes along.
Popup {
    id: popup
    modal: true
    focus: true
    padding: 0
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

    background: Rectangle {
        color: "#16161a"
        radius: theme.radius
        border.color: "#2e2e36"
    }

    contentItem: ColumnLayout {
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Label {
                text: qsTr("Instrument")
                font.bold: true
                font.pixelSize: 17
                color: "#e6e6ea"
                Layout.fillWidth: true
            }
            ComboBox {
                id: presetBox
                Layout.preferredWidth: 300
                model: backend.presets
                displayText: qsTr("Start from a preset…")
                delegate: ItemDelegate {
                    width: presetBox.width
                    contentItem: ColumnLayout {
                        spacing: 0
                        Label { text: win.presetName(modelData.id); color: "#e6e6ea" }
                        Label { text: modelData.lasers + " nm"; color: "#8a8a94"; font.pixelSize: 12 }
                    }
                    onClicked: backend.applyPreset(modelData.id)
                }
            }
            ToolButton { text: "×"; onClicked: popup.close() }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 12

                // Counted by number, not by the list itself, so stepping a
                // spin box doesn't rebuild the row it lives in.
                Repeater {
                    model: backend.lasers.length
                    Rectangle {
                        id: card
                        readonly property int laser: index
                        readonly property var info: backend.lasers[index] || ({ nm: 0, color: "#ffffff", filters: [] })
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        implicitHeight: cardColumn.implicitHeight + 24
                        color: "#1c1c22"
                        radius: theme.radius
                        border.color: Qt.rgba(card.info.color.r, card.info.color.g, card.info.color.b, 0.35)

                        ColumnLayout {
                            id: cardColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 6

                            RowLayout {
                                spacing: 10
                                Rectangle { width: 14; height: 14; radius: 7; color: card.info.color }
                                Label { text: qsTr("Laser"); color: "#e6e6ea"; font.bold: true }
                                CompactSpinBox {
                                    from: 300; to: 900
                                    editable: true
                                    value: card.info.nm
                                    onValueModified: backend.setLaser(card.laser, value)
                                }
                                Label { text: "nm"; color: "#8a8a94" }
                                Item { Layout.fillWidth: true }
                                Button {
                                    flat: true
                                    text: qsTr("Remove laser")
                                    onClicked: backend.removeLaser(card.laser)
                                }
                            }

                            Repeater {
                                model: card.info.filters.length
                                RowLayout {
                                    readonly property var filter: card.info.filters[index] || ({ center: 0, width: 0, color: "#ffffff" })
                                    readonly property int filterIndex: index
                                    spacing: 8
                                    Item { width: 24 }
                                    Rectangle { width: 10; height: 10; radius: 5; color: filter.color }
                                    Label { text: qsTr("Filter"); color: "#b8b8c0"; Layout.preferredWidth: 56 }
                                    CompactSpinBox {
                                        from: 300; to: 900
                                        editable: true
                                        value: filter.center
                                        onValueModified: backend.setFilter(card.laser, filterIndex, value, filter.width)
                                    }
                                    Label { text: "/"; color: "#8a8a94" }
                                    CompactSpinBox {
                                        from: 1; to: 300
                                        editable: true
                                        value: filter.width
                                        onValueModified: backend.setFilter(card.laser, filterIndex, filter.center, value)
                                    }
                                    Item { Layout.fillWidth: true }
                                    ToolButton {
                                        text: "×"
                                        onClicked: backend.removeFilter(card.laser, filterIndex)
                                        ToolTip.visible: hovered
                                        ToolTip.delay: 500
                                        ToolTip.text: qsTr("Remove filter")
                                    }
                                }
                            }

                            Button {
                                flat: true
                                text: qsTr("Add filter")
                                onClicked: backend.addFilter(card.laser)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.leftMargin: 16
                    Layout.bottomMargin: 16
                    spacing: 10
                    CompactSpinBox {
                        id: newLaser
                        from: 300; to: 900
                        editable: true
                        value: 561
                    }
                    Label { text: "nm"; color: "#8a8a94" }
                    Button {
                        text: qsTr("Add laser")
                        highlighted: true
                        onClicked: backend.addLaser(newLaser.value)
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#6a6a74"
            font.pixelSize: 12
            text: qsTr("Filters are bandpass, written center/width: 530/30 passes 515 to 545 nm.")
        }
    }
}
