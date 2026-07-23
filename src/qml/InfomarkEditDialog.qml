import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: infomarkEditController (InfomarkEditController, see
// ../mainwindow/InfomarkEditController.h) and "dialog" (the enclosing
// QmlDialog, see QmlDialog.h) for the Close button. Mirrors
// infomarkseditdlg.ui's layout (marker combo, type/class combos, text
// field, X1/Y1/X2/Y2/layer/angle fields, Modify/Create/Close buttons)
// without a .ui file.
//
// Fields below are local editing state, only sent to the controller when
// Create/Modify is clicked (see InfomarkEditController.h's doc comment);
// they are (re)initialized from the controller's properties whenever
// sig_refreshed() fires (selection change, marker-combo change, or type
// change), not bound to the controller continuously.
//
// The whole layout lives inside a ScrollView so that on a small/phone
// screen -- where QmlDialog may squash this Rectangle below its
// implicitWidth/implicitHeight -- the content scrolls instead of clipping.
// The inner Item keeps the original implicit size so nothing about the
// on-screen layout changes when there's enough room.
Rectangle {
    id: root
    implicitWidth: 400
    implicitHeight: 480
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    ListModel {
        id: typeModel
        ListElement { text: qsTr("Text") }
        ListElement { text: qsTr("Line") }
        ListElement { text: qsTr("Arrow") }
    }

    ListModel {
        id: classModel
        ListElement { text: qsTr("Generic marker") }
        ListElement { text: qsTr("Herb") }
        ListElement { text: qsTr("River") }
        ListElement { text: qsTr("Place") }
        ListElement { text: qsTr("Mobile name") }
        ListElement { text: qsTr("Comment") }
        ListElement { text: qsTr("Road") }
        ListElement { text: qsTr("Object") }
        ListElement { text: qsTr("Action") }
        ListElement { text: qsTr("Locality") }
    }

    // Ported from InfomarksEditDlg::updateMark()'s get_text() lambda.
    function normalizedText(type, rawText) {
        if (type === 0) { // InfomarkTypeEnum::TEXT
            return rawText.length === 0 ? qsTr("New Marker") : rawText;
        }
        return "";
    }

    // Copies the controller's current field values into this dialog's local
    // editing state. Called both at startup and whenever the controller
    // refreshes (see the Connections block below). Combo currentIndex
    // values are assigned explicitly here rather than bound continuously
    // (e.g. "currentIndex: infomarkEditController.type"), because QML
    // permanently breaks a property binding the first time something else
    // assigns to that property -- and ComboBox does that internally on
    // every user selection -- which would stop the Type combo from ever
    // reflecting the controller's "snap back while modifying" quirk again
    // after the user touched it once.
    function syncFromController() {
        markerCombo.currentIndex = infomarkEditController.currentIndex;
        typeCombo.currentIndex = infomarkEditController.type;
        classCombo.currentIndex = infomarkEditController.markerClass;
        textField.text = infomarkEditController.text;
        x1Box.value = infomarkEditController.x1;
        y1Box.value = infomarkEditController.y1;
        x2Box.value = infomarkEditController.x2;
        y2Box.value = infomarkEditController.y2;
        layerBox.value = infomarkEditController.layer;
        angleBox.value = infomarkEditController.angle;
        errorText.visible = false;
    }

    Component.onCompleted: root.syncFromController()

    Connections {
        target: infomarkEditController
        function onSig_refreshed() { root.syncFromController(); }
        function onSig_error(message) {
            errorText.text = message;
            errorText.visible = true;
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: content.height

        Item {
            id: content
            width: scrollView.availableWidth
            height: root.implicitHeight

            Label {
                id: markerLabel
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 8
                text: qsTr("&Marker:")
            }

            ComboBox {
                id: markerCombo
                objectName: "markerCombo"
                anchors.top: markerLabel.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                anchors.topMargin: 2
                model: infomarkEditController.markerNames
                onActivated: infomarkEditController.currentIndex = currentIndex
            }

            Rectangle {
                id: fieldsFrame
                anchors.top: markerCombo.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: closeButton.top
                anchors.margins: 8
                border.color: "gray"
                border.width: 1
                color: "transparent"

                Label {
                    id: fieldsTitle
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 6
                    text: qsTr("Fields:")
                    font.bold: true
                }

                Label {
                    id: typeLabel
                    anchors.top: fieldsTitle.bottom
                    anchors.left: parent.left
                    anchors.margins: 6
                    width: 60
                    text: qsTr("T&ype:")
                }

                ComboBox {
                    id: typeCombo
                    objectName: "typeCombo"
                    anchors.verticalCenter: typeLabel.verticalCenter
                    anchors.left: typeLabel.right
                    anchors.right: parent.right
                    anchors.margins: 6
                    model: typeModel
                    textRole: "text"
                    onActivated: infomarkEditController.setType(currentIndex)
                }

                Label {
                    id: classLabel
                    anchors.top: typeLabel.bottom
                    anchors.left: parent.left
                    anchors.margins: 6
                    anchors.topMargin: 8
                    width: 60
                    text: qsTr("C&lass:")
                }

                ComboBox {
                    id: classCombo
                    anchors.verticalCenter: classLabel.verticalCenter
                    anchors.left: classLabel.right
                    anchors.right: parent.right
                    anchors.margins: 6
                    model: classModel
                    textRole: "text"
                    onActivated: infomarkEditController.setMarkerClass(currentIndex)
                }

                Label {
                    id: textLabel
                    anchors.top: classLabel.bottom
                    anchors.left: parent.left
                    anchors.margins: 6
                    anchors.topMargin: 8
                    width: 60
                    text: qsTr("&Text:")
                }

                TextField {
                    id: textField
                    anchors.verticalCenter: textLabel.verticalCenter
                    anchors.left: textLabel.right
                    anchors.right: parent.right
                    anchors.margins: 6
                    enabled: infomarkEditController.type === 0
                }

                Rectangle {
                    id: positionFrame
                    anchors.top: textLabel.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 6
                    anchors.topMargin: 10
                    height: 130
                    border.color: "gray"
                    border.width: 1
                    color: "transparent"

                    Label {
                        id: positionTitle
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 6
                        text: qsTr("Position:")
                        font.bold: true
                    }

                    Row {
                        id: startEndRow
                        anchors.top: positionTitle.bottom
                        anchors.left: parent.left
                        anchors.margins: 6
                        spacing: 8
                        Label { width: 40; text: qsTr("Start") }
                        Label { width: 90; text: qsTr("End") }
                    }

                    Row {
                        id: row1
                        anchors.top: startEndRow.bottom
                        anchors.left: parent.left
                        anchors.margins: 6
                        anchors.topMargin: 4
                        spacing: 8
                        Label { width: 20; anchors.verticalCenter: parent.verticalCenter; text: qsTr("X1") }
                        SpinBox {
                            id: x1Box
                            from: -2147483647
                            to: 2147483647
                            stepSize: 100
                            editable: true
                        }
                        Label { width: 20; anchors.verticalCenter: parent.verticalCenter; text: qsTr("X2") }
                        SpinBox {
                            id: x2Box
                            objectName: "x2Box"
                            from: -2147483647
                            to: 2147483647
                            stepSize: 100
                            editable: true
                            enabled: infomarkEditController.type !== 0
                        }
                        Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Layer:") }
                        SpinBox {
                            id: layerBox
                            from: -15
                            to: 15
                            editable: true
                        }
                    }

                    Row {
                        id: row2
                        anchors.top: row1.bottom
                        anchors.left: parent.left
                        anchors.margins: 6
                        anchors.topMargin: 4
                        spacing: 8
                        Label { width: 20; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Y1") }
                        SpinBox {
                            id: y1Box
                            from: -2147483647
                            to: 2147483647
                            stepSize: 100
                            editable: true
                        }
                        Label { width: 20; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Y2") }
                        SpinBox {
                            id: y2Box
                            objectName: "y2Box"
                            from: -2147483647
                            to: 2147483647
                            stepSize: 100
                            editable: true
                            enabled: infomarkEditController.type !== 0
                        }
                        Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Angle:") }
                        SpinBox {
                            id: angleBox
                            from: -360
                            to: 360
                            stepSize: 45
                            editable: true
                            enabled: infomarkEditController.type === 0
                        }
                    }
                }

                Label {
                    id: errorText
                    anchors.top: positionFrame.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 6
                    anchors.topMargin: 8
                    color: "red"
                    wrapMode: Text.WordWrap
                    visible: false
                }

                Row {
                    anchors.top: errorText.bottom
                    anchors.left: parent.left
                    anchors.margins: 6
                    anchors.topMargin: 8
                    spacing: 8

                    Button {
                        id: modifyButton
                        objectName: "modifyButton"
                        text: qsTr("&Update")
                        enabled: infomarkEditController.canModify
                        onClicked: {
                            const t = root.normalizedText(infomarkEditController.type, textField.text);
                            textField.text = t;
                            infomarkEditController.modify(infomarkEditController.type,
                                                          infomarkEditController.markerClass,
                                                          t,
                                                          x1Box.value,
                                                          y1Box.value,
                                                          x2Box.value,
                                                          y2Box.value,
                                                          layerBox.value,
                                                          angleBox.value);
                        }
                    }

                    Button {
                        id: createButton
                        objectName: "createButton"
                        text: qsTr("&Create")
                        enabled: !infomarkEditController.canModify
                        onClicked: {
                            const t = root.normalizedText(infomarkEditController.type, textField.text);
                            textField.text = t;
                            infomarkEditController.create(infomarkEditController.type,
                                                          infomarkEditController.markerClass,
                                                          t,
                                                          x1Box.value,
                                                          y1Box.value,
                                                          x2Box.value,
                                                          y2Box.value,
                                                          layerBox.value,
                                                          angleBox.value);
                        }
                    }
                }
            }

            Button {
                id: closeButton
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: 8
                text: qsTr("Close")
                onClicked: dialog.accept()
            }
        }
    }
}
