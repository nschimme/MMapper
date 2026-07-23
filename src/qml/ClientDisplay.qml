import QtQuick
import QtQuick.Controls

import MMapper

// Not a PanelFrame: the client display is a terminal surface, so it paints
// full-bleed using the configured client background/foreground colors
// instead of the ambient SystemPalette PanelFrame uses for docked panels.
//
// Context properties expected to be set by C++ before this component is
// instantiated: clientLineModel (ClientLineModel), config (QmlConfig).
Rectangle {
    id: root

    color: config.clientBgColor

    // Stick-to-bottom autoscroll: only force the view back to the end when
    // it was already at (or essentially at) the end before new rows
    // arrived. If the user has scrolled up to read backlog, new output
    // must not yank the view back down.
    property bool stick: true

    // Guards listView's onContentYChanged while autoscroll() itself is
    // repositioning the view, so that programmatic repositioning doesn't
    // get misread as the user scrolling away from the end. Without this,
    // positionViewAtEnd()'s own contentY change would immediately flip
    // stick back off (or on) based on a transient mid-scroll position.
    property bool programmaticScroll: false

    // True when the view is close enough to the bottom to count as pinned.
    // The 4px tolerance mirrors the old widget's ALMOST_ALL_THE_WAY slack
    // (see displaywidget.cpp), which treated "within 4px of the scrollbar
    // maximum" as at-bottom so tiny rounding offsets never broke autoscroll.
    function atBottom() {
        return listView.contentY >= listView.contentHeight - listView.height - 4;
    }

    // Re-pins the view to the end. Unlike the old onMovementEnded-only
    // approach (which only ever ran after a flick gesture finished), this
    // is invoked any time content grows while stick is true, so wheel
    // scrolling and scrollbar dragging -- which never fire
    // onMovementEnded -- are covered too (see onContentYChanged below,
    // which is what actually keeps `stick` accurate for those interactions).
    function autoscroll() {
        // Re-check `stick` at execution time, not just at schedule time:
        // this can run one or more event-loop turns after Qt.callLater()
        // queued it (see the ListView handlers below), and the user may
        // have scrolled away in the meantime -- in which case jumping to
        // the end now would silently undo that scroll.
        if (!root.stick) {
            return;
        }
        root.programmaticScroll = true;
        listView.positionViewAtEnd();
        root.programmaticScroll = false;
    }

    FontMetrics {
        id: fm
        font.family: config.clientFontFamily
        font.pointSize: config.clientFontPointSize > 0 ? config.clientFontPointSize : 10
    }

    // Exposed for future NAWS (telnet window-size negotiation) wiring.
    readonly property int cols: fm.averageCharWidth > 0
        ? Math.max(1, Math.floor(listView.width / fm.averageCharWidth))
        : 0
    readonly property int rows: fm.lineSpacing > 0
        ? Math.max(1, Math.floor(listView.height / fm.lineSpacing))
        : 0

    // True when the view is pinned to the bottom (the normal "live output"
    // state); false once the user has scrolled up to read backlog. Callers
    // (ClientPanel.qml) use this to decide whether to show the tail preview.
    readonly property bool atEnd: stick

    // PageUp/PageDown support for ClientPanel.qml's input area, mirroring
    // ClientWidget.cpp's virt_scrollDisplay() (which nudges the widget's
    // QScrollBar by one page). Paging sets contentY directly, which never
    // sets listView.moving or vbar.pressed, so onContentYChanged's
    // user-gesture gate ignores it; each function therefore updates `stick`
    // explicitly from the post-move position, so paging away disengages
    // autoscroll and paging all the way back down re-engages it exactly
    // like a manual drag-to-bottom would.
    function pageUp() {
        listView.contentY = Math.max(listView.originY, listView.contentY - listView.height);
        listView.returnToBounds();
        root.stick = root.atBottom();
    }
    function pageDown() {
        const maxY = listView.originY + Math.max(0, listView.contentHeight - listView.height);
        listView.contentY = Math.min(maxY, listView.contentY + listView.height);
        listView.returnToBounds();
        root.stick = root.atBottom();
    }

    // Right-click / long-press "Copy All" for the scrollback, mirroring
    // LogPanel.qml's context menu. Per-line drag-select is deliberately not
    // implemented here: a per-delegate selectable text control would fight
    // the ListView's own drag-to-scroll and the stick-to-bottom gesture
    // gating above, so this is Copy-All only (see ClientLineModel::copyAll(),
    // which mirrors LogModel::copyAll()).
    Menu {
        id: contextMenu

        MenuItem {
            implicitHeight: Theme.controlHeight
            text: qsTr("Copy All")
            onTriggered: clientLineModel.copyAll()
        }
    }

    ListView {
        id: listView
        objectName: "clientListView"
        anchors.fill: parent
        clip: true
        model: clientLineModel

        ScrollBar.vertical: ScrollBar {
            id: vbar
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: contextMenu.popup()
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onLongPressed: contextMenu.popup()
        }

        // Only re-derive `stick` from a *user-driven* movement:
        // listView.moving covers wheel/drag/flick, vbar.pressed covers an
        // active scrollbar drag. ListView also emits contentY changes for
        // purely layout-driven reasons while rows are appended
        // (contentHeight estimate refinement, dataChanged on the trailing
        // partial row), during which atYEnd is momentarily false -- deriving
        // stick from every contentY change let those transients silently
        // unpin the view, so new output stopped autoscrolling. The
        // programmaticScroll guard still keeps autoscroll()'s own
        // repositioning from being misread as user input.
        onContentYChanged: if (!root.programmaticScroll && (listView.moving || vbar.pressed)) {
            root.stick = root.atBottom();
        }
        // New rows land as a contentHeight change (and, for the very first
        // rows, possibly a height change too); re-pin whenever the view is
        // supposed to be stuck. Qt.callLater coalesces bursts of appends
        // into a single reposition after layout settles.
        onCountChanged: if (root.stick) {
            Qt.callLater(root.autoscroll);
        }
        onContentHeightChanged: if (root.stick) {
            Qt.callLater(root.autoscroll);
        }
        onHeightChanged: if (root.stick) {
            Qt.callLater(root.autoscroll);
        }

        delegate: Text {
            width: ListView.view.width
            textFormat: Text.RichText
            text: model.html
            wrapMode: Text.Wrap
            font.family: config.clientFontFamily
            font.pointSize: config.clientFontPointSize > 0 ? config.clientFontPointSize : 10
            color: config.clientFgColor
        }
    }
}
