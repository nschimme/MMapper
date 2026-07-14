import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: updateChecker (UpdateChecker, see
// ../mainwindow/UpdateChecker.h) and dialog (the enclosing QmlDialog, see
// QmlDialog.h) for the Close button.
Item {
    id: root
    implicitWidth: 420
    implicitHeight: 140

    Text {
        id: statusText
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12
        text: updateChecker.statusText
        wrapMode: Text.WordWrap
    }

    Button {
        id: closeButton
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 12
        text: qsTr("Close")
        onClicked: dialog.reject()
    }

    Button {
        id: upgradeButton
        anchors.bottom: parent.bottom
        anchors.right: closeButton.left
        anchors.rightMargin: 8
        anchors.bottomMargin: 12
        visible: updateChecker.upgradeAvailable
        text: qsTr("&Upgrade")
        onClicked: {
            if (updateChecker.upgrade()) {
                dialog.accept();
            }
        }
    }
}
