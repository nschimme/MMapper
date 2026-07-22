import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: ansiColorPickerController (AnsiColorPickerController, see
// ../mainwindow/AnsiColorPickerController.h) and "dialog" (the enclosing
// QmlDialog, see QmlDialog.h) for the OK/Cancel buttons. Mirrors
// AnsiColorDialog.ui's layout (foreground/background combos with color
// swatches, bold/italic/underline checkboxes, a live preview label, OK/
// Cancel) without a .ui file.
Rectangle {
    id: root
    implicitWidth: 320
    implicitHeight: 220
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    component ColorCombo: ComboBox {
        model: ansiColorPickerController.colorNames
        delegate: Component {
            ItemDelegate {
                width: ListView.view ? ListView.view.width : implicitWidth
                contentItem: Row {
                    spacing: 6
                    Rectangle {
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        border.color: "black"
                        border.width: 1
                        color: ansiColorPickerController.swatchColor(index)
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                    }
                }
            }
        }
    }

    Label {
        id: previewLabel
        objectName: "previewLabel"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        horizontalAlignment: Text.AlignHCenter
        padding: 6
        text: {
            let t = qsTr("Arglebargle, glop-glyf!?!");
            if (ansiColorPickerController.bold) t = "<b>" + t + "</b>";
            if (ansiColorPickerController.italic) t = "<i>" + t + "</i>";
            if (ansiColorPickerController.underline) t = "<u>" + t + "</u>";
            return t;
        }
        textFormat: Text.RichText
        color: ansiColorPickerController.previewFg
        background: Rectangle {
            color: ansiColorPickerController.previewBg
            border.color: "gray"
            border.width: 1
        }

        // Mirrors AnsiColorDialog::slot_updateColors()'s setToolTip() on the
        // example label -- the only place the raw generated ANSI escape
        // string (e.g. "\x1b[1;31m") is surfaced to the user.
        ToolTip.text: ansiColorPickerController.resultAnsiString.length > 0
                      ? ansiColorPickerController.resultAnsiString : "[0m"
        ToolTip.visible: previewHover.hovered
        HoverHandler {
            id: previewHover
        }
    }

    Label {
        id: fgLabel
        anchors.top: previewLabel.bottom
        anchors.left: parent.left
        anchors.margins: 8
        width: 90
        text: qsTr("Foreground:")
    }

    ColorCombo {
        id: fgCombo
        objectName: "fgCombo"
        anchors.verticalCenter: fgLabel.verticalCenter
        anchors.left: fgLabel.right
        anchors.right: parent.right
        anchors.margins: 8
        currentIndex: ansiColorPickerController.fgIndex
        onActivated: ansiColorPickerController.setFgIndex(currentIndex)
    }

    Label {
        id: bgLabel
        anchors.top: fgCombo.bottom
        anchors.left: parent.left
        anchors.margins: 8
        anchors.topMargin: 4
        width: 90
        text: qsTr("Background:")
    }

    ColorCombo {
        id: bgCombo
        objectName: "bgCombo"
        anchors.verticalCenter: bgLabel.verticalCenter
        anchors.left: bgLabel.right
        anchors.right: parent.right
        anchors.margins: 8
        currentIndex: ansiColorPickerController.bgIndex
        onActivated: ansiColorPickerController.setBgIndex(currentIndex)
    }

    Row {
        id: styleRow
        anchors.top: bgCombo.bottom
        anchors.left: parent.left
        anchors.margins: 8
        anchors.topMargin: 8
        spacing: 8

        CheckBox {
            id: boldCheckBox
            objectName: "boldCheckBox"
            text: qsTr("Bold")
            checked: ansiColorPickerController.bold
            onToggled: ansiColorPickerController.setBold(checked)
        }
        CheckBox {
            id: italicCheckBox
            objectName: "italicCheckBox"
            text: qsTr("Italic")
            checked: ansiColorPickerController.italic
            onToggled: ansiColorPickerController.setItalic(checked)
        }
        CheckBox {
            id: underlineCheckBox
            objectName: "underlineCheckBox"
            text: qsTr("Underline")
            checked: ansiColorPickerController.underline
            onToggled: ansiColorPickerController.setUnderline(checked)
        }
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 8

        Button {
            text: qsTr("Cancel")
            onClicked: dialog.reject()
        }
        Button {
            text: qsTr("OK")
            onClicked: dialog.accept()
        }
    }
}
