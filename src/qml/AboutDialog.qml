import QtQuick
import QtQuick.Controls

import MMapper

// The About dialog's QML content, driven by AboutInfo (see
// ../mainwindow/AboutInfo.h) exposed as the "aboutInfo" context property,
// and "dialog" (the enclosing QmlDialog, see QmlDialog.h) for the Close
// button. Mirrors aboutdialog.ui's three-tab layout (About/Authors/
// Licenses) without a .ui file.
Rectangle {
    id: root
    implicitWidth: 640
    implicitHeight: 520
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Image {
        id: logo
        source: "qrc:/icons/mmapper-hi.svg"
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: 96
        height: 96
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(96, 96)
    }

    TabBar {
        id: tabBar
        anchors.top: logo.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right

        TabButton { text: qsTr("About") }
        TabButton { text: qsTr("Authors") }
        TabButton { text: qsTr("Licenses") }
    }

    Item {
        id: contentArea
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: closeButton.top
        anchors.margins: 8

        Flickable {
            anchors.fill: parent
            visible: tabBar.currentIndex === 0
            clip: true
            contentWidth: width
            contentHeight: aboutText.implicitHeight

            Label {
                id: aboutText
                width: parent.width
                text: aboutInfo.aboutHtml
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Flickable {
            anchors.fill: parent
            visible: tabBar.currentIndex === 1
            clip: true
            contentWidth: width
            contentHeight: authorsText.implicitHeight

            Label {
                id: authorsText
                width: parent.width
                text: aboutInfo.authorsHtml
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }

        Flickable {
            anchors.fill: parent
            visible: tabBar.currentIndex === 2
            clip: true
            contentWidth: width
            contentHeight: licensesColumn.implicitHeight

            Column {
                id: licensesColumn
                width: parent.width
                spacing: 8

                Repeater {
                    model: aboutInfo.licenses

                    Column {
                        width: licensesColumn.width
                        spacing: 4

                        Label {
                            width: parent.width
                            text: "<h3>" + model.title + "</h3>"
                            textFormat: Text.RichText
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            width: parent.width
                            visible: model.intro.length > 0
                            text: model.intro
                            textFormat: Text.RichText
                            wrapMode: Text.WordWrap
                            onLinkActivated: link => Qt.openUrlExternally(link)
                        }

                        ScrollView {
                            width: parent.width
                            height: 140

                            TextArea {
                                readOnly: true
                                text: model.text
                                font.family: "monospace"
                                wrapMode: TextArea.NoWrap
                            }
                        }
                    }
                }
            }
        }
    }

    Button {
        id: closeButton
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        text: qsTr("Close")
        onClicked: dialog.reject()
    }
}
