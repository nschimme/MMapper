import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: ansiViewContent (AnsiViewContent, see
// ../viewers/AnsiViewContent.h) and "dialog" (the enclosing QmlDialog, see
// QmlDialog.h) for the Close button. Ports AnsiViewWindow's read-only
// QTextBrowser (see ../viewers/AnsiViewWindow.cpp) without a widget: a
// ScrollView + read-only TextArea rendering ansiViewContent.html, matching
// QTextBrowser's word-wrap-by-default and external-link-on-click behavior
// (QDesktopServices::openUrl(), here Qt.openUrlExternally()).
Rectangle {
    id: root
    implicitWidth: 640
    implicitHeight: 480
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    ScrollView {
        id: scrollView
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: closeButton.top
        anchors.margins: 8
        anchors.bottomMargin: 4
        clip: true

        TextArea {
            id: textArea
            objectName: "ansiViewTextArea"
            readOnly: true
            wrapMode: TextArea.Wrap
            textFormat: TextEdit.RichText
            text: ansiViewContent.html
            onLinkActivated: function (link) { Qt.openUrlExternally(link) }
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
