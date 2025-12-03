import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    width: 600
    height: 480
    title: "About MMapper"
    modal: true

    contentItem: ColumnLayout {
        TabView {
            id: tabView
            Layout.fillWidth: true
            Layout.fillHeight: true

            Tab {
                title: "About"
                ColumnLayout {
                    anchors.fill: parent
                    Image {
                        source: "qrc:/pixmaps/splash.png"
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Label {
                        text: aboutDialogBackend.aboutText
                        textFormat: Text.RichText
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Tab {
                title: "Authors"
                Flickable {
                    contentWidth: availableWidth
                    contentHeight: authorsLabel.implicitHeight
                    TextArea {
                        id: authorsLabel
                        text: aboutDialogBackend.authorsText
                        textFormat: Text.RichText
                        readOnly: true
                    }
                }
            }

            Tab {
                title: "License"
                Flickable {
                    contentWidth: availableWidth
                    contentHeight: licensesLayout.implicitHeight
                    ColumnLayout {
                        id: licensesLayout
                        Repeater {
                            model: aboutDialogBackend.licenses
                            delegate: ColumnLayout {
                                Label {
                                    text: modelData.title
                                    font.bold: true
                                    font.pointSize: 14
                                }
                                Label {
                                    text: modelData.introText
                                    textFormat: Text.RichText
                                    wrapMode: Text.WordWrap
                                }
                                TextArea {
                                    text: modelData.licenseText
                                    readOnly: true
                                    height: 160
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }

        DialogButtonBox {
            standardButtons: DialogButtonBox.Ok
            onAccepted: root.close()
        }
    }
}
