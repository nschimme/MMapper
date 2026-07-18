import QtQuick
import QtQuick.Controls as QQC2
import MMapper

// The QML map view's chrome, mirroring MapWindow (mapwindow.h/.cpp): a
// MapCanvasItem filling the view, right/bottom scrollbars driven by a
// mapViewModel context property, and a splash-screen overlay. This commit's
// bar is modest ("loads and would render the core if a GL scene existed") --
// full chrome parity (audio-hint banner logic, config-driven scrollbar
// visibility, etc.) is the future QML shell commit's job. Nothing in the
// running app loads this file yet.
Item {
    id: root

    // Bound by whoever instantiates MapView.qml (the future shell); null is
    // always valid here (see MapCanvasItem's `core` property doc comment in
    // MapCanvasQuickItem.h) and simply means nothing renders yet.
    // Declared as `var` (not a typed `MapCanvasCore` property) because
    // MapCanvasCore is a plain QObject with no QML type registration of its
    // own -- only MapCanvasItem is registered (see ../qml/QmlTypes.cpp) --
    // so a typed QML property declaration would fail to resolve at load
    // time. Assigning a QObject* through `var` works fine.
    property var core: null

    MapCanvasItem {
        id: canvas
        anchors.fill: parent
        core: root.core
    }

    QQC2.ScrollBar {
        id: verticalScrollBar
        orientation: Qt.Vertical
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: horizontalScrollBar.top
        size: typeof mapViewModel !== "undefined" && mapViewModel.verticalScrollMax > 0
              ? Math.min(1.0, height / mapViewModel.verticalScrollMax) : 1.0
        // TODO(shell commit): gate visibility on Configuration's
        // general.showScrollBars, mirroring MapWindow::updateScrollBars().
        // QmlConfig has no facade for that setting yet; always-visible is a
        // safe, honest placeholder until this view is actually driven by
        // the shell.
        visible: true
    }

    QQC2.ScrollBar {
        id: horizontalScrollBar
        orientation: Qt.Horizontal
        anchors.left: parent.left
        anchors.right: verticalScrollBar.left
        anchors.bottom: parent.bottom
        size: typeof mapViewModel !== "undefined" && mapViewModel.horizontalScrollMax > 0
              ? Math.min(1.0, width / mapViewModel.horizontalScrollMax) : 1.0
        visible: true
    }

    // Splash screen overlay, shown until the shell hides it (mirroring
    // MapWindow's SplashWidget / hideSplashImage()).
    Image {
        id: splashImage
        objectName: "splashImage"
        anchors.centerIn: parent
        width: Math.min(parent.width, sourceSize.width)
        height: Math.min(parent.height, sourceSize.height)
        fillMode: Image.PreserveAspectFit
        source: "qrc:/icons/mmapper-hi.svg"
        visible: true
    }

    // TODO(shell commit): AudioHintWidget's banner (offer to enable
    // narration audio) stays widget-side for now; this is just a
    // placeholder reserving the layout slot for when it moves to QML.
    Row {
        id: audioHintBanner
        objectName: "audioHintBanner"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: false
    }
}
