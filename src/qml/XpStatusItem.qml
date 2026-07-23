import QtQuick
import QtQuick.Controls

// QML-facing replacement for XPStatusWidget. Root is a bare Item (not
// PanelFrame: this sits directly in the status bar, which already paints its
// own background, so there is nothing to frame) sized from a single Text
// child. When xpStatusAdapter has nothing to show, implicitWidth collapses
// to 0 so QQuickWidget::SizeViewToRootObject shrinks the widget to nothing
// and the status bar reclaims the space, matching XPStatusWidget's
// hide()/show().
Item {
    id: root

    // Context property expected to be set by C++ before this component is
    // instantiated: xpStatusAdapter (XpStatusAdapter).

    visible: xpStatusAdapter.shown
    implicitWidth: xpStatusAdapter.shown ? label.implicitWidth + 8 : 0
    implicitHeight: xpStatusAdapter.shown ? label.implicitHeight : 0

    // Surfaces the hourly rate in a transient tooltip. Defined on the root
    // (an Item) so ToolTip.show() resolves against a real attached-property
    // target; calling it from within a TapHandler's scope would not.
    function showHourlyRate() {
        const msg = xpStatusAdapter.hourlyRateText();
        if (msg.length > 0) {
            ToolTip.show(msg, 3000);
        }
    }

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
        text: xpStatusAdapter.text
        color: sysPalette.windowText
        // Subtle hover feedback, mirroring a flat QPushButton's affordance.
        font.underline: hoverHandler.hovered
        opacity: hoverHandler.hovered ? 0.75 : 1.0
    }

    HoverHandler {
        id: hoverHandler
        onHoveredChanged: {
            if (hovered) {
                xpStatusAdapter.hoverEntered();
            } else {
                xpStatusAdapter.hoverExited();
            }
        }
    }

    TapHandler {
        // A quick tap toggles the Adventure dock (matching the flat
        // QPushButton's clicked()); a long press instead surfaces the hourly
        // rate in a transient tooltip. Touch never produces the HoverHandler
        // events above, so long-press is the only way a touch user can see
        // the rate the desktop shows on hover. onLongPressed and onTapped are
        // mutually exclusive on the same handler (a press held past the
        // long-press threshold never emits tapped), so tap keeps toggling.
        onTapped: xpStatusAdapter.clicked()
        onLongPressed: root.showHourlyRate()
    }
}
