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
