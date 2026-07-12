import QtQuick
import QtQuick.Controls

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
    // QScrollBar by one page). pageUp() disengages stick (the user is
    // reading backlog now); pageDown() re-engages it using the same
    // "were we already at the end" rule onMovementEnded uses, so paging all
    // the way back down resumes autoscroll exactly like a manual drag-to-
    // bottom would.
    function pageUp() {
        root.stick = false;
        listView.contentY = Math.max(listView.originY, listView.contentY - listView.height);
        listView.returnToBounds();
    }
    function pageDown() {
        const maxY = listView.originY + Math.max(0, listView.contentHeight - listView.height);
        listView.contentY = Math.min(maxY, listView.contentY + listView.height);
        listView.returnToBounds();
        root.stick = listView.atYEnd;
    }

    ListView {
        id: listView
        objectName: "clientListView"
        anchors.fill: parent
        clip: true
        model: clientLineModel

        ScrollBar.vertical: ScrollBar {}

        onMovementEnded: root.stick = listView.atYEnd
        onCountChanged: if (root.stick) {
            positionViewAtEnd()
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
