import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    width: 400
    height: 150
    title: "MMapper Updater"
    modal: true

    property alias statusText: statusLabel.text
    property bool upgradeEnabled: false

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        Label {
            id: statusLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Checking for new version..."
        }

        DialogButtonBox {
            Layout.alignment: Qt.AlignRight
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
            button(DialogButtonBox.Ok).text: "Upgrade"
            button(DialogButtonBox.Ok).enabled: root.upgradeEnabled
            onAccepted: {
                if (Qt.openUrlExternally(updateDialogBackend.downloadUrl)) {
                    root.close()
                }
            }
            onRejected: root.close()
        }
    }

}
