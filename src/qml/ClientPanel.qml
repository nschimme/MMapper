import QtQuick
import QtQuick.Controls

import MMapper

// The full QML MUD client UI, lifting ClientWidget.ui/.cpp's welcome page +
// StackedWidget switch (see ClientWidget.cpp's ctor and isUsingClient())
// into QML, driven entirely by ClientController (see
// ../client/ClientController.h).
//
// Context properties expected to be set by C++ before this component is
// instantiated: clientController (ClientController), clientLineModel
// (ClientLineModel), config (QmlConfig).
FocusScope {
    id: root

    // Mirrors DisplayWidget::sizeHint() (displaywidget.cpp): configured
    // columns/rows in the client font's metrics, plus the same margin/
    // scrollbar/frame fudge terms, so the QML dock can't be squashed
    // smaller than the configured terminal size (see QmlDockWidget::
    // syncMinimumSize()). Kept deliberately simple -- a few px of frame/
    // margin plus a 12px scrollbar extent -- rather than querying the
    // live style, since this only needs to be "close enough" to the
    // widget-era minimum, not pixel-identical.
    readonly property int clientMinMargin: 4
    readonly property int clientMinScrollbar: 12
    implicitWidth: config.clientColumns * clientFm.averageCharWidth + clientMinMargin * 2
                   + clientMinScrollbar
    implicitHeight: config.clientRows * clientFm.lineSpacing + clientMinMargin * 2
                    + clientMinScrollbar

    // Opacity knob for the translucent-over-the-map compact overlay (see
    // MainShell.qml's compactClientOverlay). Defaults to fully opaque so
    // normal/desktop rendering is unchanged.
    property real backgroundOpacity: 1.0

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(config.clientBgColor.r, config.clientBgColor.g, config.clientBgColor.b, root.backgroundOpacity)
    }

    // Font metrics for the client font, shared by the preview tail and the
    // input area's height budget so both size themselves off the same
    // metrics ClientDisplay.qml uses for its own font.
    FontMetrics {
        id: clientFm
        font.family: config.clientFontFamily
        font.pointSize: config.clientFontPointSize > 0 ? config.clientFontPointSize : 10
    }

    // WELCOME page: mirrors ClientWidget.ui's welcomePage (playLabel/
    // playButton/helpLabel/hostname+port row/pixmapLabel) closely enough to
    // read the same to a user, without replicating its QScrollArea chrome.
    Column {
        id: welcomePage
        visible: !clientController.usingClient
        anchors.centerIn: parent
        spacing: 12
        width: Math.min(parent.width - 24, 360)

        Text {
            width: parent.width
            text: qsTr("Play MUME by launching the integrated mud client")
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: config.clientFgColor
        }

        Button {
            objectName: "playButton"
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Play MUME")
            font.bold: true
            icon.source: "qrc:/icons/terminal.png"
            icon.width: 48
            icon.height: 48
            onClicked: clientController.play()
        }

        Text {
            width: parent.width
            text: qsTr("or")
            horizontalAlignment: Text.AlignHCenter
            color: config.clientFgColor
        }

        Text {
            width: parent.width
            text: qsTr("Connect to MMapper using your favorite mud client for a better experience")
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: config.clientFgColor
        }

        Text {
            width: parent.width
            text: qsTr("hostname: localhost   port: %1").arg(clientController.port)
            horizontalAlignment: Text.AlignHCenter
            color: config.clientFgColor
        }

        // Mirrors ClientWidget.ui's pixmapLabel.
        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/pixmaps/mellon.png"
            fillMode: Image.PreserveAspectFit
            width: Math.min(implicitWidth, parent.width)
        }
    }

    // CLIENT page: display + input, mirroring ClientWidget.ui's clientPage
    // QSplitter (display/preview/input, vertical, not collapsible) except
    // that the preview tail is no longer a SplitView child (see below).
    SplitView {
        id: clientPage
        visible: clientController.usingClient
        anchors.fill: parent
        orientation: Qt.Vertical

        ClientDisplay {
            id: display
            SplitView.fillHeight: true
            bgOpacity: root.backgroundOpacity

            // Exposed for future NAWS (telnet window-size negotiation)
            // wiring; see ClientController::reportWindowSize().
            onColsChanged: clientController.reportWindowSize(display.cols, display.rows)
            onRowsChanged: clientController.reportWindowSize(display.cols, display.rows)

            // Peek preview of the trailing scrollback while the user has
            // scrolled the main display up to read backlog (display.atEnd
            // == false), mirroring PreviewWidget's role in ClientWidget.ui.
            // Deliberately NOT a SplitView child (unlike the legacy
            // QSplitter): QQC2's SplitView (pre-6.5) re-allocates its
            // layout whenever a child's `visible` toggles, which made the
            // display and input panes visibly jump every time the preview
            // showed/hid. Anchoring it as an overlay on top of `display`
            // instead means toggling its visibility never touches
            // SplitView's layout at all.
            Rectangle {
                id: previewFrame
                objectName: "clientPreviewFrame"
                visible: !display.atEnd
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: config.clientPreviewLines * clientFm.lineSpacing
                color: config.clientBgColor

                Rectangle {
                    // Subtle 1px separator so the preview reads as an
                    // overlay distinct from the live display behind it.
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: config.clientFgColor
                    opacity: 0.3
                }

                ListView {
                    id: preview
                    objectName: "clientPreviewListView"
                    anchors.fill: parent
                    anchors.topMargin: 1
                    clip: true
                    model: clientLineModel

                    onCountChanged: if (previewFrame.visible) {
                        positionViewAtEnd();
                    }

                    Connections {
                        target: previewFrame
                        function onVisibleChanged() {
                            if (previewFrame.visible) {
                                preview.positionViewAtEnd();
                            }
                        }
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
        }

        // Input area: TextArea (multi-line command input) or TextField
        // (password entry), toggled by clientController.echoVisible, same
        // as StackedInputWidget switching between InputWidget and
        // PasswordDialog (see stackedinputwidget.cpp).
        Item {
            id: inputHolder
            SplitView.preferredHeight: clientFm.lineSpacing * 4

            TextArea {
                id: inputArea
                objectName: "clientInputArea"
                anchors.fill: parent
                visible: clientController.echoVisible
                wrapMode: TextEdit.NoWrap
                // MUD command line: on a mobile/browser soft keyboard, don't
                // auto-capitalize or predict/autocorrect -- commands are
                // case- and spelling-sensitive.
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                font.family: config.clientFontFamily
                font.pointSize: config.clientFontPointSize > 0 ? config.clientFontPointSize : 10
                color: config.clientFgColor

                background: Rectangle {
                    color: config.clientBgColor
                    border.color: inputArea.activeFocus ? "steelblue" : "transparent"
                    border.width: inputArea.activeFocus ? 2 : 0
                }

                // Tab-completion cycling state (see the Key_Tab case below);
                // mirrors InputWidget's m_tabbing/m_tabFragment (inputwidget.h).
                property bool tabbing: false
                property string tabFragment: ""
                property int fragStart: 0

                // Scans backward from the given position to the previous
                // whitespace boundary (or the start of the text), mirroring
                // InputWidget::tabComplete()'s backward-select-to-whitespace
                // loop over the QTextCursor.
                function scanFragment(pos) {
                    const t = inputArea.text;
                    let start = pos;
                    while (start > 0 && !/\s/.test(t.charAt(start - 1))) {
                        start--;
                    }
                    return t.substring(start, pos);
                }

                Keys.onShortcutOverride: function (event) {
                    // Claim any key that resolves to a configured hotkey so
                    // Qt's shortcut system doesn't steal it before onPressed
                    // runs, mirroring InputWidget::event()'s
                    // QEvent::ShortcutOverride handling (inputwidget.cpp).
                    if (clientController.isHotkey(event.key, event.modifiers)) {
                        event.accepted = true;
                    }
                }

                Keys.onPressed: function (event) {
                    // (1) Tab-completion state machine: any non-Tab key ends
                    // the cycle. Backspace/Escape reject the completion
                    // (remove the highlighted delta); anything else accepts
                    // it (collapse the selection) and falls through to be
                    // processed normally, exactly like
                    // InputWidget::keyPressEvent()'s m_tabbing block.
                    if (inputArea.tabbing && event.key !== Qt.Key_Tab) {
                        if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Escape) {
                            inputArea.remove(inputArea.selectionStart, inputArea.selectionEnd);
                            inputArea.tabbing = false;
                            event.accepted = true;
                            return;
                        }
                        inputArea.cursorPosition = inputArea.selectionEnd;
                        inputArea.tabbing = false;
                        // No accept: fall through to the rest of the handler.
                    }

                    // (2) Configured hotkeys (F-keys, numpad movement, ...).
                    if (clientController.sendHotkey(event.key, event.modifiers)) {
                        event.accepted = true;
                        return;
                    }

                    // (3) Terminal-style Ctrl shortcuts, mirroring
                    // InputWidget::handleTerminalShortcut().
                    if (event.modifiers & Qt.ControlModifier) {
                        if (event.key === Qt.Key_H) {
                            inputArea.remove(inputArea.cursorPosition - 1, inputArea.cursorPosition);
                            event.accepted = true;
                            return;
                        } else if (event.key === Qt.Key_U) {
                            inputArea.clear();
                            event.accepted = true;
                            return;
                        } else if (event.key === Qt.Key_W) {
                            const t = inputArea.text;
                            let pos = inputArea.cursorPosition;
                            const end = pos;
                            while (pos > 0 && /\s/.test(t.charAt(pos - 1))) {
                                pos--;
                            }
                            while (pos > 0 && !/\s/.test(t.charAt(pos - 1))) {
                                pos--;
                            }
                            inputArea.remove(pos, end);
                            event.accepted = true;
                            return;
                        }
                    }

                    // (4) Enter/Return always sends, regardless of
                    // modifiers, mirroring InputWidget::gotInput().
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        clientController.sendInput(inputArea.text);
                        if (config.clientClearInputOnEnter) {
                            inputArea.clear();
                        } else {
                            inputArea.selectAll();
                        }
                        event.accepted = true;
                        return;
                    }

                    // (5) Up/Down navigate history, but only when the
                    // cursor is already at the first/last visual line (so
                    // multi-line edits still move the cursor normally),
                    // mirroring InputWidget::tryHistory()'s
                    // QTextCursor::movePosition(Up/Down) boundary check.
                    if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                        const topY = inputArea.positionToRectangle(0).y;
                        const bottomY = inputArea.positionToRectangle(inputArea.length).y;
                        const cursorY = inputArea.cursorRectangle.y;
                        if (event.key === Qt.Key_Up && cursorY <= topY + 1) {
                            const value = clientController.historyUp();
                            // historyUp() returns an invalid QVariant (->
                            // undefined here) at the "reached end of
                            // history" boundary, meaning: leave the text
                            // untouched (see ClientController.h).
                            if (value !== undefined) {
                                inputArea.text = value;
                                inputArea.cursorPosition = inputArea.text.length;
                            }
                            event.accepted = true;
                            return;
                        } else if (event.key === Qt.Key_Down && cursorY >= bottomY - 1) {
                            inputArea.text = clientController.historyDown();
                            inputArea.cursorPosition = inputArea.text.length;
                            event.accepted = true;
                            return;
                        }
                    }

                    // (6) Tab completion. clientController.tabComplete()
                    // wraps TabHistory::nextMatch(), which -- per its
                    // documented wraparound contract (see
                    // ClientController.h and
                    // TestClient::tabHistoryNextMatchCycling()) -- already
                    // resets its own internal iterator whenever it returns
                    // no match (either "exhausted the dictionary" or "no
                    // match at all"). That means the *next* Tab press must
                    // NOT pass restart=true again: doing so would re-derive
                    // tabFragment from the (already-reverted) input text
                    // instead of continuing the cycle from the newest
                    // dictionary entry. So this code only ever passes
                    // restart=true on the very first Tab press of a cycle
                    // (!tabbing); every subsequent press -- including the
                    // one right after a "" (no-match/wraparound) result --
                    // passes restart=false and lets the C++ side's own
                    // reset drive the restart, exactly mirroring
                    // InputWidget::tabComplete()'s apply-then-immediately-
                    // revert quirk on the final match.
                    if (event.key === Qt.Key_Tab) {
                        if (!inputArea.tabbing) {
                            inputArea.tabFragment = inputArea.scanFragment(inputArea.cursorPosition);
                            inputArea.fragStart = inputArea.cursorPosition - inputArea.tabFragment.length;
                        }
                        const match = clientController.tabComplete(inputArea.tabFragment, !inputArea.tabbing);
                        inputArea.tabbing = true;
                        const selEnd = Math.max(inputArea.cursorPosition, inputArea.selectionEnd);
                        if (match !== "") {
                            inputArea.remove(inputArea.fragStart, selEnd);
                            inputArea.insert(inputArea.fragStart, match);
                            inputArea.select(inputArea.fragStart + inputArea.tabFragment.length,
                                              inputArea.fragStart + match.length);
                        } else {
                            // No match, or the dictionary wrapped: revert to
                            // just the fragment, with no selection, ready
                            // for the next Tab press to restart the cycle.
                            inputArea.remove(inputArea.fragStart, selEnd);
                            inputArea.insert(inputArea.fragStart, inputArea.tabFragment);
                            inputArea.cursorPosition = inputArea.fragStart + inputArea.tabFragment.length;
                        }
                        event.accepted = true;
                        return;
                    }

                    // (7) PageUp/PageDown scroll the display, mirroring
                    // InputWidget::keyPressEvent()'s
                    // m_outputs.scrollDisplay() call.
                    if (event.key === Qt.Key_PageUp) {
                        display.pageUp();
                        event.accepted = true;
                        return;
                    } else if (event.key === Qt.Key_PageDown) {
                        display.pageDown();
                        event.accepted = true;
                        return;
                    }
                }
            }

            TextField {
                id: passwordField
                objectName: "clientPasswordField"
                anchors.fill: parent
                visible: !clientController.echoVisible
                echoMode: TextInput.Password
                // In-band MUME password: mark as sensitive so soft keyboards
                // don't cache/predict it, and don't auto-capitalize.
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase
                                  | Qt.ImhNoPredictiveText | Qt.ImhHiddenText
                font.family: config.clientFontFamily
                font.pointSize: config.clientFontPointSize > 0 ? config.clientFontPointSize : 10
                color: config.clientFgColor

                onAccepted: {
                    clientController.sendPassword(text);
                    clear();
                }
            }
        }
    }

    Connections {
        target: clientController

        function onSig_requestInputFocus() {
            inputArea.forceActiveFocus();
        }
        function onSig_passwordPromptRequested() {
            passwordField.forceActiveFocus();
        }
    }

    Component.onCompleted: {
        if (clientController.autoPlay) {
            clientController.play();
        }
    }
}
