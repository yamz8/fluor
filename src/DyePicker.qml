import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Finds a dye by name, or by a laser or emission wavelength typed as a number.
Popup {
    id: picker
    signal chosen(string name)

    width: 520
    height: 500
    modal: true
    focus: true
    padding: 0
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

    property var results: backend.searchDyes(search.text)

    // Set up before the popup animates in, so typing straight away isn't lost.
    onAboutToShow: {
        search.text = ""
        list.currentIndex = 0
        search.forceActiveFocus()
    }

    function choose(index) {
        const hit = results[index]
        if (!hit || hit.inPanel) return
        chosen(hit.name)
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

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 8
            placeholderText: qsTr("Search dyes, or type a wavelength")
            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onUpPressed: list.decrementCurrentIndex()
            Keys.onReturnPressed: picker.choose(list.currentIndex)
            Keys.onEnterPressed: picker.choose(list.currentIndex)
            onTextChanged: list.currentIndex = 0
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: picker.results
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                width: list.width
                height: 44
                focusPolicy: Qt.NoFocus
                enabled: !modelData.inPanel
                highlighted: ListView.isCurrentItem
                onClicked: picker.choose(index)
                contentItem: RowLayout {
                    spacing: 12
                    Rectangle { width: 12; height: 12; radius: 6; color: modelData.color; opacity: modelData.inPanel ? 0.4 : 1 }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        font.bold: true
                        color: modelData.inPanel ? "#6a6a74" : "#e6e6ea"
                        elide: Text.ElideRight
                    }
                    Label {
                        text: modelData.inPanel ? qsTr("In the panel")
                              : qsTr("ex %1 · em %2").arg(win.num(modelData.ex)).arg(win.num(modelData.em))
                        color: "#8a8a94"
                        font.pixelSize: 12
                    }
                    Label {
                        Layout.preferredWidth: 56
                        text: "★".repeat(Math.round(modelData.brightness))
                        color: modelData.inPanel ? "#4a4a54" : Material.accent
                        font.pixelSize: 11
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: picker.results.length === 0
                color: "#8a8a94"
                text: qsTr("No dye matches “%1”").arg(search.text)
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            horizontalAlignment: Text.AlignHCenter
            color: "#6a6a74"
            font.pixelSize: 12
            text: qsTr("Enter adds the dye to the best free detector · Esc closes")
        }
    }
}
