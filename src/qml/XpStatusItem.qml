import QtQuick

// QML-facing replacement for XPStatusWidget. Root is a bare Item (not
// PanelFrame: this sits directly in the status bar, which already paints its
// own background, so there is nothing to frame) sized from a single Text
// child. When the adapter has nothing to show, implicitWidth collapses to 0
// so QQuickWidget::SizeViewToRootObject shrinks the widget to nothing and the
// status bar reclaims the space, matching XPStatusWidget's hide()/show().
Item {
    id: root

    // Context property expected to be set by C++ before this component is
    // instantiated: adapter (XpStatusAdapter).

    visible: adapter.shown
    implicitWidth: adapter.shown ? label.implicitWidth + 8 : 0
    implicitHeight: adapter.shown ? label.implicitHeight : 0

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Text {
        id: label
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        leftPadding: 4
        rightPadding: 4
        text: adapter.text
        color: sysPalette.windowText
        // Subtle hover feedback, mirroring a flat QPushButton's affordance.
        font.underline: hoverHandler.hovered
        opacity: hoverHandler.hovered ? 0.75 : 1.0
    }

    HoverHandler {
        id: hoverHandler
        onHoveredChanged: {
            if (hovered) {
                adapter.hoverEntered();
            } else {
                adapter.hoverExited();
            }
        }
    }

    TapHandler {
        onTapped: adapter.clicked()
    }
}
