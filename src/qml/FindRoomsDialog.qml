import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: findRoomsController (FindRoomsController, see
// ../mainwindow/FindRoomsController.h), findRoomsModel (its
// FindRoomsModel, see ../mainwindow/FindRoomsModel.h), and "dialog" (the
// enclosing QmlDialog, see QmlDialog.h) for the Close button. Mirrors
// findroomsdlg.ui's layout (query field, search-kind radio group,
// case/regex checkboxes, result table, select/edit buttons) without a .ui
// file.
Rectangle {
    id: root
    implicitWidth: 500
    implicitHeight: 420
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    // Row indices (into findRoomsModel) currently selected, kept as an
    // object used like a set (row -> true) so toggling is O(1) and QML's
    // property-change binding fires whenever the set is replaced.
    property var selectedRows: ({})

    function isSelected(row) {
        return root.selectedRows[row] === true;
    }

    function toggleSelect(row, extend) {
        var next = {};
        if (extend) {
            for (var key in root.selectedRows) {
                next[key] = true;
            }
            if (next[row]) {
                delete next[row];
            } else {
                next[row] = true;
            }
        } else {
            next[row] = true;
        }
        root.selectedRows = next;
    }

    function selectedRowList() {
        var result = [];
        for (var key in root.selectedRows) {
            result.push(parseInt(key, 10));
        }
        return result;
    }

    function runFind() {
        if (queryField.text.length === 0) {
            return;
        }
        root.selectedRows = ({});
        errorText.visible = false;
        findRoomsController.find(queryField.text,
                                 kindModel.get(kindCombo.currentIndex).kind,
                                 caseCheckBox.checked,
                                 regexCheckBox.checked);
    }

    Connections {
        target: findRoomsController
        function onSig_error(message) {
            errorText.text = message;
            errorText.visible = true;
        }
    }

    ListModel {
        id: kindModel
        // Order mirrors findroomsdlg.ui's radio-button grid; values are
        // PatternKindsEnum (see mapdata/roomfilter.h): NONE=0, DESC=1,
        // CONTENTS=2, NAME=3, NOTE=4, EXITS=5, FLAGS=6, AREA=7, ALL=8.
        ListElement { text: qsTr("Name"); kind: 3 }
        ListElement { text: qsTr("Exits"); kind: 5 }
        ListElement { text: qsTr("Description"); kind: 1 }
        ListElement { text: qsTr("Notes"); kind: 4 }
        ListElement { text: qsTr("Contents"); kind: 2 }
        ListElement { text: qsTr("Flags"); kind: 6 }
        ListElement { text: qsTr("Area"); kind: 7 }
        ListElement { text: qsTr("All"); kind: 8 }
    }

    TextField {
        id: queryField
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: findButton.left
        anchors.margins: 8
        anchors.rightMargin: 8
        placeholderText: qsTr("Query")
        onAccepted: root.runFind()
    }

    Button {
        id: findButton
        anchors.top: parent.top
        anchors.right: closeButton.left
        anchors.margins: 8
        anchors.rightMargin: 8
        text: qsTr("&Find")
        enabled: queryField.text.length > 0
        onClicked: root.runFind()
    }

    Button {
        id: closeButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        text: qsTr("&Close")
        onClicked: dialog.reject()
    }

    ComboBox {
        id: kindCombo
        anchors.top: queryField.bottom
        anchors.left: parent.left
        anchors.margins: 8
        width: 150
        model: kindModel
        textRole: "text"
        currentIndex: 0
    }

    CheckBox {
        id: caseCheckBox
        anchors.top: queryField.bottom
        anchors.left: kindCombo.right
        anchors.topMargin: 8
        text: qsTr("Case sensitive")
    }

    CheckBox {
        id: regexCheckBox
        anchors.top: caseCheckBox.bottom
        anchors.left: kindCombo.right
        text: qsTr("Regular expression")
    }

    Text {
        id: errorText
        anchors.top: kindCombo.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        color: "red"
        wrapMode: Text.WordWrap
        visible: false
    }

    Rectangle {
        id: resultsFrame
        anchors.top: errorText.visible ? errorText.bottom : kindCombo.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomRow.top
        anchors.margins: 8
        border.color: "gray"
        border.width: 1
        color: "transparent"

        ListView {
            id: resultsListView
            objectName: "findRoomsListView"
            anchors.fill: parent
            anchors.margins: 1
            clip: true
            model: findRoomsModel

            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: delegateRoot
                width: ListView.view.width
                height: 22
                color: root.isSelected(index) ? "#3399ff" : "transparent"
                opacity: root.isSelected(index) ? 0.35 : 1.0

                Row {
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 8

                    Label {
                        width: 60
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: model.externalId
                    }
                    Label {
                        width: delegateRoot.width - 60 - 12
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: model.name
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: mouse => root.toggleSelect(index, (mouse.modifiers & Qt.ControlModifier) !== 0)
                    onDoubleClicked: findRoomsController.activateRow(index)
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: resultsListView.count === 0
            text: qsTr("No results")
            opacity: 0.5
        }
    }

    Row {
        id: bottomRow
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 8

        Label {
            id: resultSummaryText
            width: parent.width - selectButton.width - editButton.width - 16
            height: selectButton.height
            verticalAlignment: Text.AlignVCenter
            text: findRoomsController.resultSummary
        }

        Button {
            id: selectButton
            text: qsTr("&Select")
            enabled: root.selectedRowList().length > 0
            onClicked: findRoomsController.selectRows(root.selectedRowList())
        }

        Button {
            id: editButton
            text: qsTr("Select and &Edit")
            enabled: root.selectedRowList().length > 0
            onClicked: findRoomsController.editRows(root.selectedRowList())
        }
    }
}
