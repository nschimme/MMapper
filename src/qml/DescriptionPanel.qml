import QtQuick

import MMapper

PanelFrame {
    id: root

    // Context property expected to be set by C++ before this component is
    // instantiated: adapter (DescriptionAdapter). The image URLs embed the
    // shared store's revision (image://description/<sharp|blur>/<rev>), so
    // every room/image change produces a new URL and the Image elements
    // below reload automatically instead of reusing a cached pixmap.

    // Matches DescriptionWidget's MAX_DESCRIPTION_WIDTH: the text overlay is
    // capped at ~80 average characters of the configured client font.
    readonly property int maxDescriptionChars: 80

    // TOP_PADDING_LINES in DescriptionWidget.cpp.
    readonly property int topPaddingLines: 5

    // Mirrors DescriptionWidget::updateBackground()'s hasRoomRightOfTextEdit:
    // true when the sharp image's natural size fits in the space to the
    // right of the text overlay block, so it can sit beside the text
    // full-bleed instead of needing to be pushed down below it.
    readonly property bool hasRoomRightOfText: sharpImage.implicitWidth > 0
        && sharpImage.implicitWidth <= width - textBlock.width
    readonly property int imageTopPadding: hasRoomRightOfText ? 0 : topPaddingLines * fm.lineSpacing

    FontMetrics {
        id: fm
        font.family: adapter.fontFamily
        font.pointSize: adapter.fontPointSize > 0 ? adapter.fontPointSize : 10
    }

    // Blurred backdrop stretched to fill the panel, mirroring the
    // downscale-blur-stretch pipeline in DescriptionWidget::updateBackground()
    // (the blur itself happens in DescriptionImageProvider).
    Image {
        objectName: "blurImage"
        anchors.fill: parent
        fillMode: Image.Stretch
        source: adapter.blurUrl
        visible: adapter.blurUrl.toString() !== ""
    }

    // Sharp image centered on top of the blur, preserving aspect ratio
    // (the widget's Qt::KeepAspectRatio + centered draw).
    //
    // Widget parity (DescriptionWidget::updateBackground): when the image
    // cannot fit beside the text block, inset it below by TOP_PADDING_LINES
    // text lines so the blurred backdrop stays visible around it.
    Image {
        id: sharpImage
        objectName: "sharpImage"
        anchors.fill: parent
        anchors.topMargin: root.imageTopPadding
        fillMode: Image.PreserveAspectFit
        source: adapter.imageUrl
        visible: adapter.imageUrl.toString() !== ""
    }

    // Text overlay: room name and description over a block of the client
    // background color (the widget applied integratedClient.backgroundColor
    // as the QTextEdit's per-block background, at the color's own alpha).
    Rectangle {
        id: textBlock
        anchors.left: parent.left
        anchors.top: parent.top
        width: Math.min(root.width, fm.averageCharacterWidth * root.maxDescriptionChars)
        height: Math.min(root.height, textColumn.implicitHeight)
        color: adapter.bgColor
        visible: adapter.roomName !== "" || adapter.roomDesc !== ""

        Column {
            id: textColumn
            width: parent.width

            Text {
                width: parent.width
                text: adapter.roomName
                color: adapter.nameColor
                font.family: adapter.fontFamily
                font.pointSize: fm.font.pointSize
                wrapMode: Text.Wrap
                visible: text !== ""
            }
            Text {
                width: parent.width
                text: adapter.roomDesc
                color: adapter.descColor
                font.family: adapter.fontFamily
                font.pointSize: fm.font.pointSize
                wrapMode: Text.Wrap
                visible: text !== ""
            }
        }
    }
}
