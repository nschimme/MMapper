import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: roomEditController (RoomEditController, see
// ../mainwindow/RoomEditController.h) and "dialog" (the enclosing QmlDialog,
// see QmlDialog.h) for the Close button. Mirrors roomeditattrdlg.ui's layout
// (room combo, Attributes/Terrain/Note/Room Stat/Room Diff tabs, Close
// button) without a .ui file. TestQml.cpp's RoomEditControllerStub mirrors
// RoomEditController's exact Q_PROPERTY/Q_INVOKABLE surface, so keep this
// file's bindings in sync with that header's doc comment whenever either
// changes.
//
// CheckBox tri-state rows (mob/load/exit/door flag lists) never bind
// "checked"/"checkState" bidirectionally: QtQuick's CheckBox toggles its own
// checkState the instant it's clicked, which would permanently sever a
// declarative "checkState: model.checkState" binding the first time the user
// clicked one. Instead every tri-state row is a plain (non-interactive to
// the pointer) CheckBox purely reflecting model state, covered by a
// MouseArea that calls the matching toggleXFlag(index) invokable and lets
// the controller's subsequent refresh()/dataChanged() drive the visual
// state back through the still-intact binding. The same reasoning applies to
// the exit-direction and terrain buttons (bound via the read-only
// "highlighted" property, never "checked").
Rectangle {
    id: root
    implicitWidth: 560
    implicitHeight: 640
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    readonly property int noteTabIndex: 2

    readonly property var terrainNames: [
        qsTr("Undefined"), qsTr("Indoors"), qsTr("City"), qsTr("Field"),
        qsTr("Forest"), qsTr("Hills"), qsTr("Mountains"), qsTr("Shallow"),
        qsTr("Water"), qsTr("Rapids"), qsTr("Underwater"), qsTr("Road"),
        qsTr("Brush"), qsTr("Tunnel"), qsTr("Cavern")
    ]

    ListModel {
        id: alignModel
        ListElement { text: qsTr("Undefined"); value: 0 }
        ListElement { text: qsTr("Good"); value: 1 }
        ListElement { text: qsTr("Neutral"); value: 2 }
        ListElement { text: qsTr("Evil"); value: 3 }
    }
    ListModel {
        id: portableModel
        ListElement { text: qsTr("Undefined"); value: 0 }
        ListElement { text: qsTr("Portable"); value: 1 }
        ListElement { text: qsTr("No-Port"); value: 2 }
    }
    ListModel {
        id: rideableModel
        ListElement { text: qsTr("Undefined"); value: 0 }
        ListElement { text: qsTr("Ridable"); value: 1 }
        ListElement { text: qsTr("No-Ride"); value: 2 }
    }
    ListModel {
        id: lightModel
        ListElement { text: qsTr("Undefined"); value: 0 }
        ListElement { text: qsTr("Dark"); value: 1 }
        ListElement { text: qsTr("Lit"); value: 2 }
    }
    ListModel {
        id: sundeathModel
        ListElement { text: qsTr("Undefined"); value: 0 }
        ListElement { text: qsTr("Sundeath"); value: 1 }
        ListElement { text: qsTr("Safe"); value: 2 }
    }

    // Commits any pending door-name edit before the selected exit changes
    // (see doorNameField below): the widget commits on editingFinished,
    // which fires when focus leaves the field, but switching the selected
    // exit via a direction button doesn't necessarily move focus first.
    function commitPendingDoorName() {
        if (doorNameField.activeFocus && doorNameField.text !== roomEditController.doorName) {
            roomEditController.doorName = doorNameField.text;
        }
    }

    function selectExit(dir) {
        root.commitPendingDoorName();
        roomEditController.selectedExitDir = dir;
    }

    // Guards noteArea.onTextChanged below while this file assigns
    // noteArea.text programmatically, so those assignments don't get
    // reported back to the controller as a (fictitious) user edit -- see
    // forceSyncNoteText()'s use from the Revert/Clear handlers, where a
    // naive "just set the text" would immediately re-dirty the note via
    // setNoteText()'s ASSIGN-always-dirties semantics.
    property bool suppressNoteSync: false

    // Called on every sig_refreshed(): skips the resync while the user has
    // unsaved edits in progress (noteDirty), so an unrelated refresh (e.g.
    // switching exits) never clobbers text the user hasn't applied yet.
    function syncNoteFromController() {
        if (!roomEditController.noteDirty) {
            root.forceSyncNoteText();
        }
    }

    // Unconditionally copies the controller's noteText into the TextArea;
    // used right after revertNote()/clearNote(), whose C++ side has already
    // (synchronously) updated noteText by the time this runs.
    function forceSyncNoteText() {
        root.suppressNoteSync = true;
        noteArea.text = roomEditController.noteText;
        root.suppressNoteSync = false;
    }

    // Copies the controller's current field values into this dialog's local
    // editing state. Called both at startup and whenever the controller
    // refreshes (see the Connections block below) -- ComboBox/TextField
    // currentIndex/text are assigned explicitly here rather than bound
    // continuously, for the same reason InfomarkEditDialog.qml's
    // syncFromController() gives (see that file's comment): a live binding
    // gets permanently severed the first time the control's own internal
    // interaction writes to the bound property.
    function syncFromController() {
        roomCombo.currentIndex = roomEditController.currentRoomIndex;
        doorNameField.text = roomEditController.doorName;
        root.syncNoteFromController();
        errorText.visible = false;
    }

    Component.onCompleted: root.syncFromController()

    Connections {
        target: roomEditController
        function onSig_refreshed() { root.syncFromController(); }
        function onSig_error(message) {
            errorText.text = message;
            errorText.visible = true;
        }
    }

    Shortcut {
        sequence: "Ctrl+H"
        onActivated: roomEditController.toggleHiddenDoor()
    }

    ComboBox {
        id: roomCombo
        objectName: "roomCombo"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        model: roomEditController.roomNames
        onActivated: roomEditController.currentRoomIndex = currentIndex
    }

    TabBar {
        id: tabBar
        objectName: "tabBar"
        anchors.top: roomCombo.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 4

        onCurrentIndexChanged: {
            // Ports the widget's tab-switch guard (see roomeditattrdlg.cpp's
            // tabWidget::currentChanged handler): while the note has unsaved
            // edits, every other tab is off-limits.
            if (roomEditController.noteDirty && currentIndex !== root.noteTabIndex) {
                currentIndex = root.noteTabIndex;
            }
        }

        TabButton { text: qsTr("Attributes") }
        TabButton { text: qsTr("Terrain") }
        TabButton { text: qsTr("Note") }
        TabButton { text: qsTr("Room Stat") }
        TabButton { text: qsTr("Room Diff") }
    }

    Item {
        id: tabContent
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: errorText.top
        anchors.margins: 8

        // ---------------- Attributes tab ----------------
        Item {
            id: attributesTab
            anchors.fill: parent
            visible: tabBar.currentIndex === 0

            Rectangle {
                id: exitsFrame
                objectName: "exitsFrame"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 150
                border.color: "gray"
                border.width: 1
                color: "transparent"
                enabled: roomEditController.hasSelectedRoom

                Grid {
                    id: exitGrid
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 8
                    columns: 3
                    spacing: 2

                    Item { width: 32; height: 32 }
                    Button {
                        width: 32; height: 32
                        text: qsTr("N")
                        highlighted: roomEditController.selectedExitDir === 0
                        onClicked: root.selectExit(0)
                    }
                    Button {
                        width: 32; height: 32
                        text: qsTr("U")
                        highlighted: roomEditController.selectedExitDir === 4
                        onClicked: root.selectExit(4)
                    }
                    Button {
                        width: 32; height: 32
                        text: qsTr("W")
                        highlighted: roomEditController.selectedExitDir === 3
                        onClicked: root.selectExit(3)
                    }
                    Item { width: 32; height: 32 }
                    Button {
                        width: 32; height: 32
                        text: qsTr("E")
                        highlighted: roomEditController.selectedExitDir === 2
                        onClicked: root.selectExit(2)
                    }
                    Button {
                        width: 32; height: 32
                        text: qsTr("D")
                        highlighted: roomEditController.selectedExitDir === 5
                        onClicked: root.selectExit(5)
                    }
                    Button {
                        width: 32; height: 32
                        text: qsTr("S")
                        highlighted: roomEditController.selectedExitDir === 1
                        onClicked: root.selectExit(1)
                    }
                    Item { width: 32; height: 32 }
                }

                Label {
                    id: doorNameLabel
                    anchors.top: parent.top
                    anchors.left: exitGrid.right
                    anchors.margins: 8
                    text: qsTr("Door name:")
                }
                TextField {
                    id: doorNameField
                    objectName: "doorNameField"
                    anchors.top: doorNameLabel.bottom
                    anchors.left: exitGrid.right
                    anchors.right: parent.right
                    anchors.margins: 8
                    anchors.topMargin: 2
                    enabled: roomEditController.doorFieldsEnabled
                    onEditingFinished: roomEditController.doorName = text
                }

                Label {
                    id: exitFlagsLabel
                    anchors.top: doorNameField.bottom
                    anchors.left: exitGrid.right
                    anchors.margins: 8
                    anchors.topMargin: 6
                    text: qsTr("Exit flags:")
                }
                ListView {
                    id: exitFlagsListView
                    objectName: "exitFlagsListView"
                    anchors.top: exitFlagsLabel.bottom
                    anchors.left: exitGrid.right
                    anchors.right: doorFlagsListView.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 8
                    anchors.topMargin: 2
                    clip: true
                    model: roomEditController.exitFlagsModel
                    delegate: Item {
                        width: ListView.view.width
                        height: 20
                        CheckBox {
                            id: exitCb
                            anchors.verticalCenter: parent.verticalCenter
                            text: model.name
                            tristate: true
                            checkState: model.checkState
                            enabled: model.checkable
                        }
                        MouseArea {
                            anchors.fill: exitCb
                            enabled: model.checkable
                            onClicked: roomEditController.toggleExitFlag(index)
                        }
                    }
                }

                ListView {
                    id: doorFlagsListView
                    objectName: "doorFlagsListView"
                    anchors.top: exitFlagsLabel.bottom
                    anchors.right: parent.right
                    width: parent.width / 2 - 16
                    anchors.bottom: parent.bottom
                    anchors.margins: 8
                    anchors.topMargin: 2
                    clip: true
                    enabled: roomEditController.doorFieldsEnabled
                    model: roomEditController.doorFlagsModel
                    delegate: Item {
                        width: ListView.view.width
                        height: 20
                        CheckBox {
                            id: doorCb
                            anchors.verticalCenter: parent.verticalCenter
                            text: model.name
                            tristate: true
                            checkState: model.checkState
                            enabled: model.checkable && roomEditController.doorFieldsEnabled
                        }
                        MouseArea {
                            anchors.fill: doorCb
                            enabled: model.checkable && roomEditController.doorFieldsEnabled
                            onClicked: roomEditController.toggleDoorFlag(index)
                        }
                    }
                }
            }

            Row {
                id: flagsRow
                anchors.top: exitsFrame.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 8
                height: 140
                spacing: 8

                Column {
                    width: (flagsRow.width - 8) / 2
                    height: parent.height
                    Label { text: qsTr("Load flags") }
                    ListView {
                        id: loadFlagsListView
                        objectName: "loadFlagsListView"
                        width: parent.width
                        height: parent.height - 20
                        clip: true
                        model: roomEditController.loadFlagsModel
                        delegate: Item {
                            width: ListView.view.width
                            height: 20
                            Row {
                                anchors.fill: parent
                                spacing: 4
                                Image {
                                    source: model.iconSource
                                    width: 16; height: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                CheckBox {
                                    id: loadCb
                                    text: model.name
                                    tristate: true
                                    checkState: model.checkState
                                    enabled: model.checkable
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: model.checkable
                                onClicked: roomEditController.toggleLoadFlag(index)
                            }
                        }
                    }
                }

                Column {
                    width: (flagsRow.width - 8) / 2
                    height: parent.height
                    Label { text: qsTr("Mob flags") }
                    ListView {
                        id: mobFlagsListView
                        objectName: "mobFlagsListView"
                        width: parent.width
                        height: parent.height - 20
                        clip: true
                        model: roomEditController.mobFlagsModel
                        delegate: Item {
                            width: ListView.view.width
                            height: 20
                            Row {
                                anchors.fill: parent
                                spacing: 4
                                Image {
                                    source: model.iconSource
                                    width: 16; height: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                CheckBox {
                                    id: mobCb
                                    text: model.name
                                    tristate: true
                                    checkState: model.checkState
                                    enabled: model.checkable
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: model.checkable
                                onClicked: roomEditController.toggleMobFlag(index)
                            }
                        }
                    }
                }
            }

            Row {
                id: radioRow
                anchors.top: flagsRow.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.topMargin: 8
                spacing: 4

                Column {
                    width: radioRow.width / 5
                    Label { text: qsTr("Align"); font.bold: true }
                    Repeater {
                        model: alignModel
                        delegate: Item {
                            width: parent.width
                            height: rb.implicitHeight
                            RadioButton {
                                id: rb
                                text: model.text
                                checked: roomEditController.align === model.value
                            }
                            MouseArea {
                                anchors.fill: rb
                                onClicked: roomEditController.align = model.value
                            }
                        }
                    }
                }
                Column {
                    width: radioRow.width / 5
                    Label { text: qsTr("Teleport"); font.bold: true }
                    Repeater {
                        model: portableModel
                        delegate: Item {
                            width: parent.width
                            height: rb.implicitHeight
                            RadioButton {
                                id: rb
                                text: model.text
                                checked: roomEditController.portable === model.value
                            }
                            MouseArea {
                                anchors.fill: rb
                                onClicked: roomEditController.portable = model.value
                            }
                        }
                    }
                }
                Column {
                    width: radioRow.width / 5
                    Label { text: qsTr("Ridable"); font.bold: true }
                    Repeater {
                        model: rideableModel
                        delegate: Item {
                            width: parent.width
                            height: rb.implicitHeight
                            RadioButton {
                                id: rb
                                text: model.text
                                checked: roomEditController.rideable === model.value
                            }
                            MouseArea {
                                anchors.fill: rb
                                onClicked: roomEditController.rideable = model.value
                            }
                        }
                    }
                }
                Column {
                    width: radioRow.width / 5
                    Label { text: qsTr("Light"); font.bold: true }
                    Repeater {
                        model: lightModel
                        delegate: Item {
                            width: parent.width
                            height: rb.implicitHeight
                            RadioButton {
                                id: rb
                                text: model.text
                                checked: roomEditController.light === model.value
                            }
                            MouseArea {
                                anchors.fill: rb
                                onClicked: roomEditController.light = model.value
                            }
                        }
                    }
                }
                Column {
                    width: radioRow.width / 5
                    Label { text: qsTr("Sundeath"); font.bold: true }
                    Repeater {
                        model: sundeathModel
                        delegate: Item {
                            width: parent.width
                            height: rb.implicitHeight
                            RadioButton {
                                id: rb
                                text: model.text
                                checked: roomEditController.sundeath === model.value
                            }
                            MouseArea {
                                anchors.fill: rb
                                onClicked: roomEditController.sundeath = model.value
                            }
                        }
                    }
                }
            }
        }

        // ---------------- Terrain tab ----------------
        Item {
            id: terrainTab
            anchors.fill: parent
            visible: tabBar.currentIndex === 1

            Grid {
                id: terrainGrid
                anchors.top: parent.top
                anchors.left: parent.left
                columns: 4
                spacing: 4

                Repeater {
                    model: root.terrainNames.length
                    delegate: Column {
                        Button {
                            width: 56; height: 56
                            highlighted: roomEditController.terrainType === index
                            icon.source: roomEditController.terrainIcon(index)
                            onClicked: roomEditController.terrainType = index
                        }
                        Label {
                            width: 56
                            horizontalAlignment: Text.AlignHCenter
                            text: root.terrainNames[index]
                            font.pixelSize: 10
                        }
                    }
                }
            }

            Rectangle {
                id: terrainPreviewFrame
                anchors.top: parent.top
                anchors.right: parent.right
                width: 72
                height: 72
                border.color: "gray"
                border.width: 1
                color: "transparent"
                Image {
                    anchors.centerIn: parent
                    source: roomEditController.terrainPreviewIcon
                }
            }

            ScrollView {
                anchors.top: terrainGrid.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.topMargin: 8
                clip: true
                enabled: roomEditController.hasSelectedRoom
                TextArea {
                    id: descriptionArea
                    objectName: "descriptionArea"
                    readOnly: true
                    wrapMode: TextArea.NoWrap
                    textFormat: TextEdit.RichText
                    text: roomEditController.descriptionHtml
                }
            }
        }

        // ---------------- Note tab ----------------
        Item {
            id: noteTab
            anchors.fill: parent
            visible: tabBar.currentIndex === 2

            Label {
                id: noteLabel
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                text: qsTr("Type your note for the current room or area:")
            }

            ScrollView {
                id: noteScroll
                anchors.top: noteLabel.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: noteButtonsRow.top
                anchors.margins: 4
                clip: true
                enabled: roomEditController.hasSelectedRoom
                TextArea {
                    id: noteArea
                    objectName: "noteArea"
                    wrapMode: TextArea.WordWrap
                    onTextChanged: {
                        if (!root.suppressNoteSync && text !== roomEditController.noteText) {
                            roomEditController.noteText = text;
                        }
                    }
                }
            }

            Row {
                id: noteButtonsRow
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 8

                Button {
                    id: noteApplyButton
                    objectName: "noteApplyButton"
                    text: qsTr("Apply")
                    enabled: roomEditController.noteDirty
                    onClicked: roomEditController.applyNote()
                }
                Button {
                    id: noteRevertButton
                    objectName: "noteRevertButton"
                    text: qsTr("Revert Changes")
                    enabled: roomEditController.noteDirty
                    onClicked: {
                        roomEditController.revertNote();
                        root.forceSyncNoteText();
                    }
                }
                Button {
                    id: noteClearButton
                    objectName: "noteClearButton"
                    text: qsTr("Clear")
                    enabled: noteArea.text.length > 0
                    onClicked: {
                        roomEditController.clearNote();
                        root.forceSyncNoteText();
                    }
                }
            }
        }

        // ---------------- Room Stat tab ----------------
        Item {
            id: statTab
            anchors.fill: parent
            visible: tabBar.currentIndex === 3

            Label {
                id: statLabel
                anchors.top: parent.top
                anchors.left: parent.left
                text: qsTr("Room Stat:")
            }
            ScrollView {
                anchors.top: statLabel.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.topMargin: 4
                clip: true
                TextArea {
                    id: statArea
                    objectName: "statArea"
                    readOnly: true
                    wrapMode: TextArea.NoWrap
                    textFormat: TextEdit.RichText
                    text: roomEditController.statsHtml
                }
            }
        }

        // ---------------- Room Diff tab ----------------
        Item {
            id: diffTab
            anchors.fill: parent
            visible: tabBar.currentIndex === 4

            Label {
                id: diffLabel
                anchors.top: parent.top
                anchors.left: parent.left
                text: qsTr("Room Diff:")
            }
            ScrollView {
                id: diffScroll
                anchors.top: diffLabel.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: revertDiffButton.top
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                clip: true
                TextArea {
                    id: diffArea
                    objectName: "diffArea"
                    readOnly: true
                    wrapMode: TextArea.NoWrap
                    textFormat: TextEdit.RichText
                    text: roomEditController.diffHtml
                }
            }
            Label {
                anchors.centerIn: diffScroll
                visible: roomEditController.diffHtml.length === 0
                text: qsTr("No changes since last save.")
                opacity: 0.6
            }
            Button {
                id: revertDiffButton
                objectName: "revertDiffButton"
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                text: qsTr("Revert")
                enabled: roomEditController.revertEnabled
                onClicked: roomEditController.revertDiff()
            }
        }
    }

    Label {
        id: errorText
        objectName: "errorText"
        anchors.bottom: closeButton.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        anchors.bottomMargin: 4
        color: "red"
        wrapMode: Text.WordWrap
        visible: false
    }

    Button {
        id: closeButton
        objectName: "closeButton"
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 8
        text: qsTr("Close")
        enabled: !roomEditController.noteDirty
        onClicked: dialog.accept()
    }
}
