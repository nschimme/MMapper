import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMapper

Item {
    id: root
    width: 800
    height: 600

    // The Map is background, filling the whole area
    WidgetProxyItem {
        id: mapProxy
        anchors.fill: parent
        widget: bridge.mapWidget
        z: 0
    }

    // Darkened overlay for the client area (left side)
    Rectangle {
        id: clientBackground
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * 2/3
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#CC000000" }
            GradientStop { position: 0.8; color: "#99000000" }
            GradientStop { position: 1.0; color: "transparent" }
        }
        z: 1
    }

    // The Client Widget overlay
    WidgetProxyItem {
        id: clientProxy
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: parent.width * 2/3 - 20
        widget: bridge.clientWidget
        z: 2
    }

    // Hamburger Button
    Button {
        id: menuButton
        text: "☰"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        onClicked: drawer.open()
        z: 10
        background: Rectangle {
            color: "#88000000"
            radius: 4
        }
        palette.buttonText: "white"
    }

    // Side Drawer (Hamburger Menu)
    Drawer {
        id: drawer
        width: Math.min(parent.width * 0.8, 300)
        height: parent.height

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Label {
                text: "MMapper Menu"
                font.bold: true
                font.pointSize: 16
                Layout.margins: 20
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: [
                    { text: "Open Map", action: "open" },
                    { text: "Log", action: "log" }
                ]

                delegate: ItemDelegate {
                    width: parent.width
                    text: modelData.text
                    onClicked: {
                        if (modelData.action === "log") {
                            panelTitle.text = modelData.text
                            panelProxy.widget = bridge.getWidgetByName(modelData.action)
                            panelPopup.open()
                        } else {
                            bridge.toggleWidget(modelData.action)
                        }
                        drawer.close()
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: "#eee"
                        visible: modelData.text === "Separator"
                    }
                }
            }
        }
    }

    // Popup for secondary widgets
    Popup {
        id: panelPopup
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: parent.width * 0.9
        height: parent.height * 0.9
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        ColumnLayout {
            anchors.fill: parent
            Label {
                id: panelTitle
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }
            WidgetProxyItem {
                id: panelProxy
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            Button {
                text: "Close"
                onClicked: panelPopup.close()
                Layout.alignment: Qt.AlignRight
            }
        }
    }
}
