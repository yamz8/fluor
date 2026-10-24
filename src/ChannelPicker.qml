import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Puts a dye on a detector, showing how well each one sees it and who's there.
Popup {
    id: picker
    property int dye: -1
    // Re-read whenever the panel changes, so the bars and occupants stay true.
    readonly property var options: (backend.dyes, backend.channelOptions(dye))
    readonly property var entry: dye >= 0 && dye < backend.dyes.length ? backend.dyes[dye] : null

    width: 460
    height: Math.min(560, list.contentHeight + 110)
    modal: true
    focus: true
    padding: 0
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

    // Set up before the popup animates in, so typing straight away isn't lost.
    onAboutToShow: {
        let current = 0
        for (let i = 0; i < options.length; ++i)
            if (options[i].current) current = i + 1
        list.currentIndex = current
        list.forceActiveFocus()
    }

    function choose(index) {
        backend.setChannel(dye, index - 1)
        close()
    }

    background: Rectangle {
        color: "#16161a"
        radius: theme.radius
        border.color: "#2e2e36"
    }

    contentItem: ColumnLayout {
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 8
            text: picker.entry ? qsTr("Detector for %1").arg(picker.entry.name) : ""
            font.bold: true
            font.pixelSize: 15
            color: "#e6e6ea"
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: picker.options.length + 1
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            Keys.onReturnPressed: picker.choose(currentIndex)
            Keys.onEnterPressed: picker.choose(currentIndex)
            delegate: ItemDelegate {
                readonly property var option: index > 0 ? picker.options[index - 1] : null
                width: list.width
                height: 38
                focusPolicy: Qt.NoFocus
                highlighted: ListView.isCurrentItem
                onClicked: picker.choose(index)
                contentItem: RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        color: option ? option.color : "transparent"
                        border.color: option ? "transparent" : "#6a6a74"
                    }
                    Label {
                        Layout.preferredWidth: 110
                        text: option ? option.name : qsTr("No detector")
                        color: option ? option.color : "#b8b8c0"
                        font.bold: option ? option.current : picker.entry && picker.entry.channel < 0
                    }
                    Rectangle {
                        Layout.preferredWidth: 110
                        Layout.preferredHeight: 6
                        radius: 3
                        color: "#26262c"
                        visible: option !== null
                        Rectangle {
                            width: parent.width * (option ? option.relative : 0)
                            height: parent.height
                            radius: 3
                            color: Material.accent
                        }
                    }
                    Label {
                        Layout.preferredWidth: 40
                        visible: option !== null
                        text: option ? win.pct(option.relative) : ""
                        color: "#8a8a94"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: option && option.occupant ? option.occupant : ""
                        color: "#f5b041"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            horizontalAlignment: Text.AlignHCenter
            color: "#6a6a74"
            font.pixelSize: 12
            text: qsTr("Bars show signal against the dye's best detector")
        }
    }
}
