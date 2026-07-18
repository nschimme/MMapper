import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: passwordDialogController (PasswordDialogController, see
// ../preferences/PasswordDialogController.h) and "dialog" (the enclosing
// QmlDialog, see QmlDialog.h) for the OK button. Mirrors
// ManagePasswordDialog.ui's layout (account-name field, password field with
// a View echo-mode toggle, Delete button, a single OK button -- the widget's
// QDialogButtonBox only has QDialogButtonBox::Ok, no Cancel) without a .ui
// file.
//
// On WASM (passwordDialogController.showPasswordControls == false) the
// password field, View toggle, and Delete button are hidden, mirroring
// ManagePasswordDialog::ManagePasswordDialog()'s WASM branch (the dialog
// title itself, "Manage Account" vs "Manage Password", is set by the
// launcher in C++, not here).
Rectangle {
    id: root
    implicitWidth: 380
    implicitHeight: passwordDialogController.showPasswordControls ? 220 : 130
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Label {
        id: accountLabel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 8
        text: qsTr("Account:")
    }

    TextField {
        id: accountNameField
        objectName: "accountNameField"
        anchors.verticalCenter: accountLabel.verticalCenter
        anchors.left: accountLabel.right
        anchors.right: parent.right
        anchors.margins: 8
        text: passwordDialogController.accountName
        onTextEdited: passwordDialogController.accountName = text
    }

    Label {
        id: passwordLabel
        visible: passwordDialogController.showPasswordControls
        anchors.top: accountLabel.bottom
        anchors.left: parent.left
        anchors.margins: 8
        anchors.topMargin: 12
        text: qsTr("Password:")
    }

    TextField {
        id: passwordField
        objectName: "passwordField"
        visible: passwordDialogController.showPasswordControls
        anchors.verticalCenter: passwordLabel.verticalCenter
        anchors.left: passwordLabel.right
        anchors.right: viewButton.left
        anchors.margins: 8
        echoMode: viewButton.checked ? TextInput.Normal : TextInput.Password
        text: passwordDialogController.password
        onTextEdited: passwordDialogController.password = text
    }

    Button {
        id: viewButton
        objectName: "viewButton"
        visible: passwordDialogController.showPasswordControls
        checkable: true
        anchors.verticalCenter: passwordLabel.verticalCenter
        anchors.right: parent.right
        anchors.margins: 8
        text: qsTr("View")
    }

    Button {
        id: deleteButton
        objectName: "deleteButton"
        visible: passwordDialogController.showPasswordControls
        enabled: passwordDialogController.hasStoredPassword
        anchors.top: passwordField.bottom
        anchors.left: parent.left
        anchors.margins: 8
        anchors.topMargin: 12
        text: qsTr("Delete")
        onClicked: passwordDialogController.requestDelete()
    }

    Label {
        id: errorLabel
        objectName: "errorLabel"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        anchors.top: deleteButton.bottom
        anchors.topMargin: 8
        color: "red"
        wrapMode: Text.WordWrap
        visible: text.length > 0
        text: passwordDialogController.errorText
    }

    Button {
        id: okButton
        objectName: "okButton"
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 8
        text: qsTr("OK")
        onClicked: dialog.accept()
    }
}
