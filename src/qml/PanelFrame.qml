import QtQuick

Rectangle {
    id: root

    // Tracks the QApplication palette live (ThemeManager dark/light switches).
    // Named panelPalette (not "palette") because Item already declares a
    // built-in "palette" property since Qt 6.0; aliasing over it would be
    // a duplicate property name error.
    property alias panelPalette: sysPalette

    default property alias content: contentItem.data

    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: 4
    }
}
