import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Fluor

ApplicationWindow {
    id: win
    width: 1280
    height: 820
    minimumWidth: 900
    minimumHeight: 560
    visible: true
    title: (backend.dirty ? "• " : "") + (backend.title || backend.fileName || qsTr("Untitled panel")) + " — Fluor"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property color surface: "#16161a"
    readonly property color line: "#26262c"
    readonly property color textColor: "#e6e6ea"
    readonly property color muted: "#8a8a94"
    readonly property color danger: "#ff6b81"
    readonly property color caution: "#f5b041"

    property int selected: backend.dyes.length > 0 ? 0 : -1
    property int laserView: -1
    property bool showExcitation: false
    property bool helpVisible: false
    // What to do once unsaved work is saved or discarded: "quit", "new",
    // "open", or "drop" (load pendingUrl, a panel dropped on the window).
    property string pending: ""
    property url pendingUrl
    property bool closingConfirmed: false

    readonly property bool modal: dyePicker.opened || channelPicker.opened || instrument.opened || confirm.opened
    // Typing into a field must not trigger the single-key shortcuts.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition")
    readonly property bool keys: !modal && !typing

    // Numbers follow the Omarchy language's digits and separators.
    function num(value) {
        translator.language
        return Number(value).toLocaleString(Qt.locale(), 'f', 0)
    }
    function pct(value) { return qsTr("%1%").arg(num(Math.round(value * 100))) }

    function issueText(issue) {
        switch (issue.kind) {
        case "unassigned": return qsTr("%1 has no detector").arg(issue.a)
        case "unseen": return qsTr("No laser and filter on this instrument sees %1").arg(issue.a)
        case "shared": return qsTr("%1 and %2 share the %3 detector").arg(issue.a).arg(issue.b).arg(issue.aChannel)
        case "dim": return qsTr("The %1 nm laser excites %2 at only %3 of its peak").arg(num(issue.laser)).arg(issue.a).arg(pct(issue.value))
        case "spill": return qsTr("%1 spills %2 into %3's detector").arg(issue.a).arg(pct(issue.value)).arg(issue.b)
        case "blind": return qsTr("%1's detector sees almost none of its light").arg(issue.a)
        case "similar": return qsTr("%1 and %2 look alike across every detector").arg(issue.a).arg(issue.b)
        }
        return ""
    }
    function severityColor(severity) {
        return severity === "error" ? danger : severity === "warning" ? caution : muted
    }
    function presetName(id) {
        switch (id) {
        case "vbr": return qsTr("Violet, blue, red analyzer")
        case "vbyr": return qsTr("Violet, blue, yellow-green, red analyzer")
        case "uvbyr": return qsTr("Five-laser analyzer")
        case "confocal": return qsTr("Confocal microscope")
        }
        return id
    }

    function select(index) {
        if (backend.dyes.length === 0) { selected = -1; return }
        selected = Math.max(0, Math.min(backend.dyes.length - 1, index))
        dyeList.positionViewAtIndex(selected, ListView.Contain)
    }
    function openChannelPicker() {
        if (selected < 0) return
        channelPicker.dye = selected
        channelPicker.open()
    }

    // Unsaved work is never dropped without asking.
    function guard(action) {
        if (!backend.dirty) { run(action); return }
        pending = action
        confirm.open()
    }
    function run(action) {
        pending = ""
        if (action === "quit") { closingConfirmed = true; Qt.quit() }
        else if (action === "new") { backend.newPanel(); selected = -1 }
        else if (action === "open") backend.open()
        else if (action === "drop") backend.load(pendingUrl)
    }

    onClosing: function(close) {
        if (backend.dirty && !closingConfirmed) {
            close.accepted = false
            guard("quit")
        }
    }

    Connections {
        target: backend
        function onSaved() { if (win.pending !== "") win.run(win.pending) }
        // A save that didn't happen must not leave its quit or new waiting
        // to fire after some later, unrelated save.
        function onSaveAbandoned() { win.pending = "" }
        function onChanged() {
            if (win.selected >= backend.dyes.length) win.selected = backend.dyes.length - 1
            if (win.selected < 0 && backend.dyes.length > 0) win.selected = 0
            if (win.laserView >= backend.lasers.length) win.laserView = -1
        }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.modal && !win.typing || win.helpVisible; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.guard("quit") }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: dyePicker.open() }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: win.keys && backend.dyes.length > 0; onActivated: backend.autoAssign() }
    Shortcut { sequence: "I"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: instrument.open() }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.showExcitation = !win.showExcitation }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.select(win.selected - 1) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.select(win.selected + 1) }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; enabled: win.keys && win.selected >= 0; onActivated: backend.stepChannel(win.selected, -1) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; enabled: win.keys && win.selected >= 0; onActivated: backend.stepChannel(win.selected, 1) }
    Shortcut { sequences: ["Return", "Enter"]; context: Qt.ApplicationShortcut; enabled: win.keys && win.selected >= 0; onActivated: win.openChannelPicker() }
    Shortcut { sequences: ["Delete", "Backspace"]; context: Qt.ApplicationShortcut; enabled: win.keys && win.selected >= 0; onActivated: backend.removeDye(win.selected) }
    Shortcut { sequence: "0"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.laserView = -1 }
    Repeater {
        model: 9
        Item {
            Shortcut {
                sequence: String(index + 1)
                context: Qt.ApplicationShortcut
                enabled: win.keys && index < backend.lasers.length
                onActivated: win.laserView = win.laserView === index ? -1 : index
            }
        }
    }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !win.modal; onActivated: backend.undo() }
    Shortcut { sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]; context: Qt.ApplicationShortcut; enabled: !win.modal; onActivated: backend.redo() }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; enabled: !win.modal; onActivated: win.guard("new") }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; enabled: !win.modal; onActivated: win.guard("open") }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; enabled: !win.modal; onActivated: backend.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; enabled: !win.modal; onActivated: backend.saveAs() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: win.helpVisible || win.typing && !win.modal
        onActivated: { if (win.helpVisible) win.helpVisible = false; else content.forceActiveFocus() } }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        id: content
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        DropArea {
            anchors.fill: parent
            keys: ["text/uri-list"]
            onDropped: function(drop) {
                if (drop.hasUrls && drop.urls.length > 0 && !backend.dirty) backend.load(drop.urls[0])
                else if (drop.hasUrls && drop.urls.length > 0) {
                    win.pendingUrl = drop.urls[0]
                    win.guard("drop")
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header: the panel's name, which laser the chart shows, and the main actions.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: win.surface

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12
                    spacing: 10

                    // A plain input rather than a Material TextField, whose
                    // floating label would sit on top of the title.
                    TextInput {
                        id: titleField
                        Layout.preferredWidth: 280
                        Layout.alignment: Qt.AlignVCenter
                        text: backend.title
                        color: win.textColor
                        selectionColor: theme.accent
                        selectedTextColor: theme.accentForeground
                        font.pixelSize: 17
                        font.bold: true
                        clip: true
                        selectByMouse: true
                        onEditingFinished: { backend.setTitle(text); content.forceActiveFocus() }
                        Keys.onEscapePressed: { text = backend.title; content.forceActiveFocus() }
                        Text {
                            anchors.fill: parent
                            visible: !parent.text && !parent.activeFocus
                            text: qsTr("Untitled panel")
                            color: win.muted
                            font: parent.font
                        }
                        Connections {
                            target: backend
                            function onChanged() { if (!titleField.activeFocus) titleField.text = backend.title }
                        }
                    }
                    Label {
                        text: (backend.fileName || qsTr("Not saved")) + (backend.dirty && backend.fileName ? " •" : "")
                        color: win.muted
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: 220
                    }
                    Item { Layout.fillWidth: true }

                    Label { text: qsTr("Lasers"); color: win.muted }
                    Row {
                        spacing: 4
                        LaserChip {
                            label: qsTr("All")
                            chipColor: theme.accent
                            active: win.laserView === -1
                            onClicked: win.laserView = -1
                        }
                        Repeater {
                            model: backend.lasers
                            LaserChip {
                                label: win.num(modelData.nm)
                                chipColor: modelData.color
                                active: win.laserView === index
                                onClicked: win.laserView = win.laserView === index ? -1 : index
                            }
                        }
                    }
                    ToolSeparator {}
                    Button {
                        flat: true
                        checkable: true
                        checked: win.showExcitation
                        text: qsTr("Excitation")
                        onToggled: win.showExcitation = checked
                        focusPolicy: Qt.NoFocus
                    }
                    Button { flat: true; text: qsTr("Instrument"); onClicked: instrument.open(); focusPolicy: Qt.NoFocus }
                    Button {
                        flat: true
                        text: qsTr("Auto-assign")
                        enabled: backend.dyes.length > 0
                        onClicked: backend.autoAssign()
                        focusPolicy: Qt.NoFocus
                    }
                    Button { flat: true; text: "?"; onClicked: win.helpVisible = !win.helpVisible; focusPolicy: Qt.NoFocus }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.line }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // The panel: its dyes, each on a detector, and what's wrong with it.
                Rectangle {
                    Layout.preferredWidth: 360
                    Layout.fillHeight: true
                    color: win.surface

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 12
                            Layout.bottomMargin: 4
                            Label {
                                text: qsTr("%n dyes", "", backend.dyes.length)
                                font.bold: true
                                color: win.textColor
                                Layout.fillWidth: true
                            }
                            Button {
                                text: qsTr("Add dye")
                                highlighted: true
                                onClicked: dyePicker.open()
                                focusPolicy: Qt.NoFocus
                            }
                        }

                        ListView {
                            id: dyeList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: backend.dyes
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            delegate: DyeRow {
                                width: dyeList.width
                                dye: modelData
                                selected: index === win.selected
                                onPicked: win.selected = index
                                onChooseChannel: { win.selected = index; win.openChannelPicker() }
                                onRemove: backend.removeDye(index)
                            }
                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 48
                                visible: backend.dyes.length === 0
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                color: win.muted
                                text: qsTr("No dyes yet. Press Space to add one.")
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: win.line }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 12
                            Layout.bottomMargin: 4
                            Label {
                                text: qsTr("Problems")
                                font.bold: true
                                color: win.textColor
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: backend.dyes.length > 1
                                elide: Text.ElideRight
                                Layout.maximumWidth: 160
                                color: backend.worstSpill >= 0.25 ? win.caution : win.muted
                                text: qsTr("Worst spill %1").arg(win.pct(backend.worstSpill))
                            }
                        }
                        ListView {
                            id: issueList
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(contentHeight + 8, parent.height * 0.4)
                            Layout.minimumHeight: 44
                            clip: true
                            model: backend.issues
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            delegate: ItemDelegate {
                                width: issueList.width
                                height: Math.max(34, issueLabel.implicitHeight + 14)
                                focusPolicy: Qt.NoFocus
                                onClicked: win.select(modelData.aIndex)
                                contentItem: RowLayout {
                                    spacing: 10
                                    Rectangle {
                                        Layout.alignment: Qt.AlignTop
                                        Layout.topMargin: 5
                                        width: 8; height: 8; radius: 4
                                        color: win.severityColor(modelData.severity)
                                    }
                                    Label {
                                        id: issueLabel
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        color: modelData.severity === "note" ? win.muted : win.textColor
                                        text: win.issueText(modelData)
                                    }
                                }
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: backend.issues.length === 0 && backend.dyes.length > 0
                                color: win.muted
                                text: qsTr("No overlaps worth worrying about")
                            }
                        }
                    }
                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: win.line }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    SpectraPlot {
                        id: plot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 8
                        source: backend
                        laser: win.laserView
                        selected: win.selected
                        excitation: win.showExcitation
                        emptyText: qsTr("Press Space to add a dye")

                        HoverHandler {
                            id: hover
                            onPointChanged: plot.hoverX = point.position.x
                            onHoveredChanged: if (!hovered) plot.hoverX = -1
                        }
                        TapHandler {
                            onTapped: function(eventPoint) {
                                const dye = plot.dyeAt(eventPoint.position.x, eventPoint.position.y)
                                if (dye >= 0) win.selected = dye
                            }
                        }
                        Label {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: 8
                            visible: win.laserView >= 0
                            color: win.muted
                            font.pixelSize: 12
                            text: win.laserView >= 0 && win.laserView < backend.lasers.length
                                  ? qsTr("Emission as excited by the %1 nm laser").arg(win.num(backend.lasers[win.laserView].nm)) : ""
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: win.line; visible: matrix.visible }

                    SpilloverMatrix {
                        id: matrix
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(implicitHeight, win.height * 0.42)
                        visible: backend.dyes.length > 1
                        selected: win.selected
                        onPicked: function(index) { win.selected = index }
                    }
                }
            }

            // A problem with a file or the file chooser is said, not swallowed.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                visible: backend.error !== ""
                color: "#3a1820"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8
                    Label { text: backend.error; color: win.danger; elide: Text.ElideRight; Layout.fillWidth: true }
                    Button { flat: true; text: qsTr("Dismiss"); onClicked: backend.clearError(); focusPolicy: Qt.NoFocus }
                }
            }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onDismissed: win.helpVisible = false
        }
    }

    DyePicker {
        id: dyePicker
        anchors.centerIn: Overlay.overlay
        onChosen: function(name) {
            if (backend.addDye(name)) win.select(backend.dyes.length - 1)
        }
    }

    ChannelPicker {
        id: channelPicker
        anchors.centerIn: Overlay.overlay
    }

    InstrumentPopup {
        id: instrument
        anchors.centerIn: Overlay.overlay
        width: Math.min(640, win.width - 64)
        height: win.height - 96
    }

    Dialog {
        id: confirm
        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("Save changes to this panel?")
        Material.roundedScale: theme.radius
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }
        background: Rectangle {
            color: win.surface
            radius: theme.radius
            border.color: "#2e2e36"
        }
        header: Label {
            text: confirm.title
            padding: 24
            bottomPadding: 8
            font.pixelSize: 18
            font.bold: true
            color: win.textColor
        }
        Label {
            text: qsTr("Your changes will be lost if you don't save them.")
            color: win.muted
        }
        footer: DialogButtonBox {
            background: Item {}
            Button { text: qsTr("Save"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Don't save"); DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole }
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            onAccepted: backend.save()
            onRejected: win.pending = ""
            onClicked: function(button) {
                if (button.DialogButtonBox.buttonRole === DialogButtonBox.DestructiveRole) {
                    confirm.close()
                    win.run(win.pending)
                }
            }
        }
    }

}
