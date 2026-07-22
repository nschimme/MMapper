import QtQuick
import QtQuick.Controls as QQC2
import MMapper

// The QML map view's chrome, mirroring MapWindow (mapwindow.h/.cpp): a map
// canvas item filling the view, right/bottom scrollbars driven by a
// mapViewModel context property, and a splash-screen overlay.
//
// Two C++-backed canvas types are registered (see ../qml/QmlTypes.cpp):
// MapCanvasUnderlay (MapCanvasUnderlayItem.h) draws the map directly into
// the window's own framebuffer beneath the rest of the Quick scene -- the
// performance end-state, and the default here -- and MapCanvasItem
// (MapCanvasQuickItem.h, a QQuickFramebufferObject) is the older
// item-owns-its-own-FBO fallback, selected instead whenever the
// "useCanvasUnderlay" root context property is false (QmlShellWindow wires
// this to the MMAPPER_CANVAS_FBO=1 environment variable -- see
// QmlShellWindow.cpp). Since the underlay item paints nothing through the
// ordinary Quick scene graph (see MapCanvasUnderlayItem.h's file comment),
// nothing else in this file (or MainShell.qml, which hosts this view) may
// paint an OPAQUE background over the map's rect -- see MainShell.qml's own
// "transparent hole" comment on its ApplicationWindow.background override.
Item {
    id: root

    // Bound by whoever instantiates MapView.qml (the shell); null is always
    // valid here (see MapCanvasUnderlayItem's/MapCanvasQuickItem's `core`
    // property doc comments) and simply means nothing renders yet.
    // Declared as `var` (not a typed `MapCanvasCore` property) because
    // MapCanvasCore is a plain QObject with no QML type registration of its
    // own -- only the two canvas item types are registered (see
    // ../qml/QmlTypes.cpp) -- so a typed QML property declaration would
    // fail to resolve at load time. Assigning a QObject* through `var`
    // works fine.
    property var core: null

    // Defaults to true (the underlay path) when the "useCanvasUnderlay"
    // root context property is absent entirely -- e.g. a standalone test
    // loading this file without QmlShellWindow's context properties set --
    // matching QmlShellWindow's own default.
    readonly property bool useUnderlay: typeof useCanvasUnderlay === "undefined"
                                        ? true : useCanvasUnderlay

    Loader {
        id: canvasLoader
        anchors.fill: parent
        sourceComponent: root.useUnderlay ? underlayComponent : fboComponent
    }

    Component {
        id: underlayComponent
        MapCanvasUnderlay {
            id: canvas
            anchors.fill: parent
            core: root.core
        }
    }

    Component {
        id: fboComponent
        MapCanvasItem {
            id: canvas
            anchors.fill: parent
            core: root.core
        }
    }

    // The map canvas's right-click context menu (../qml/shell/
    // MapContextMenu.qml -- see its own file comment). `parent: root` (the
    // default for a Popup declared as a child Item here) is what makes
    // MapContextMenu.qml's `root.popup(pos.x, pos.y)` position relative to
    // this Item's own coordinate space: `canvas` above fills `root` exactly
    // (anchors.fill: parent, no offset), so MapCanvasCore::
    // sig_customContextMenuRequested's canvas-local QPoint is already in
    // this Item's local coordinate space too, with no extra mapping needed.
    MapContextMenu {
        id: contextMenu
        core: root.core
    }

    // Both scrollbars below mirror MapWindow's QScrollBar pair
    // (mapwindow.cpp:115-129, 153-167, 291-312): `size` is the visible
    // fraction of the scrollable range (unchanged from before this commit),
    // `position` is now a real two-way binding to MapViewModel's current
    // scroll-bar value (see MapViewModel.h's horizontalScrollValue/
    // verticalScrollValue Q_PROPERTYs) instead of the previous always-0
    // default, and `visible` is gated on the same "view.show-scroll-bars"
    // UiCommand MainShell.qml's own menu item drives
    // (../shell/QmlShellWindow.cpp), which mirrors
    // Configuration::general.showScrollBars -- exactly the setting
    // MapWindow::updateScrollBars() reads (mapwindow.cpp:291-312) -- AND a
    // non-empty range, matching updateScrollBars()'s `hMax > 0 &&
    // showScrollBars` / `vMax > 0 && showScrollBars` conditions.
    //
    // QQC2.ScrollBar has no "moved"-while-dragging signal to hook a
    // model-write onto without also fighting the model->view direction (see
    // Qt's own two-way-binding idiom for interactive controls): `position`
    // is driven from the model via a `Binding` that's only active while the
    // user ISN'T dragging (`when: !pressed`), and while dragging (`pressed`
    // true) `onPositionChanged` -- which also fires for the control's own
    // internal, interaction-driven writes to `position` -- pushes the new
    // value back into the model instead.
    QQC2.ScrollBar {
        id: verticalScrollBar
        orientation: Qt.Vertical
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: horizontalScrollBar.top
        size: typeof mapViewModel !== "undefined" && mapViewModel.verticalScrollMax > 0
              ? Math.min(1.0, height / mapViewModel.verticalScrollMax) : 1.0
        Binding on position {
            value: typeof mapViewModel !== "undefined" && mapViewModel.verticalScrollMax > 0
                   ? mapViewModel.verticalScrollValue / mapViewModel.verticalScrollMax : 0.0
            when: !verticalScrollBar.pressed
        }
        onPositionChanged: {
            if (pressed && typeof mapViewModel !== "undefined"
                    && mapViewModel.verticalScrollMax > 0) {
                mapViewModel.verticalScrollValue = Math.round(
                    position * mapViewModel.verticalScrollMax);
            }
        }
        visible: (typeof commands !== "undefined" && commands
                  && commands.command("view.show-scroll-bars")
                  ? commands.command("view.show-scroll-bars").checked : true)
                 && typeof mapViewModel !== "undefined" && mapViewModel.verticalScrollMax > 0
    }

    QQC2.ScrollBar {
        id: horizontalScrollBar
        orientation: Qt.Horizontal
        anchors.left: parent.left
        anchors.right: verticalScrollBar.left
        anchors.bottom: parent.bottom
        size: typeof mapViewModel !== "undefined" && mapViewModel.horizontalScrollMax > 0
              ? Math.min(1.0, width / mapViewModel.horizontalScrollMax) : 1.0
        Binding on position {
            value: typeof mapViewModel !== "undefined" && mapViewModel.horizontalScrollMax > 0
                   ? mapViewModel.horizontalScrollValue / mapViewModel.horizontalScrollMax : 0.0
            when: !horizontalScrollBar.pressed
        }
        onPositionChanged: {
            if (pressed && typeof mapViewModel !== "undefined"
                    && mapViewModel.horizontalScrollMax > 0) {
                mapViewModel.horizontalScrollValue = Math.round(
                    position * mapViewModel.horizontalScrollMax);
            }
        }
        visible: (typeof commands !== "undefined" && commands
                  && commands.command("view.show-scroll-bars")
                  ? commands.command("view.show-scroll-bars").checked : true)
                 && typeof mapViewModel !== "undefined" && mapViewModel.horizontalScrollMax > 0
    }

    // Splash screen overlay, shown until the shell hides it (mirroring
    // MapWindow's SplashWidget / hideSplashImage()). QmlShellWindow sets the
    // "mapLoaded" root context property to true once a load attempt has
    // completed (success or failure) or the map has been modified -- see
    // QmlShellWindow::hideSplash()'s doc comment for the exact two
    // triggers this mirrors. `typeof` guards against this file being loaded
    // standalone (e.g. a future test) with no such context property set, in
    // which case the splash simply stays visible, matching the empty/
    // never-loaded map case.
    Image {
        id: splashImage
        objectName: "splashImage"
        anchors.centerIn: parent
        width: Math.min(parent.width, sourceSize.width)
        height: Math.min(parent.height, sourceSize.height)
        fillMode: Image.PreserveAspectFit
        source: "qrc:/icons/mmapper-hi.svg"
        visible: typeof mapLoaded === "undefined" || !mapLoaded
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
