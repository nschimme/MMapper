import QtQuick
import QtQuick.Controls

import MMapper

// Context properties expected to be set by C++ before this component is
// instantiated: remoteEditController (RemoteEditController, see
// ../mpi/RemoteEditController.h) and "dialog" (the enclosing QmlDialog /
// RemoteEditDialogHost, see QmlDialog.h) for Submit/Exit to close the
// window. Ports RemoteEditWidget/RemoteTextEdit (see
// ../mpi/remoteeditwidget.h) without a widget: a MenuBar mirroring the
// XFOREACH_REMOTE_EDIT_MENU_ITEM X-macro's items/shortcuts exactly, a
// NoWrap TextArea sharing its QTextDocument with the controller (via
// attachDocument(TextArea.textDocument), see Qt 6.4's
// QQuickTextEdit::textDocument), and inline find/replace + goto bars
// mirroring findreplacewidget.cpp/gotowidget.cpp's UX.
//
// Deviations from the widget (documented, not oversights):
//  - Edit-only actions (Submit, Undo, Redo, Cut, Paste) and the Alignment/
//    Colors/Whitespace submenus are disabled (not removed) in viewer mode:
//    QQC2's Action type has no "visible" property in Qt 6.4 (only "enabled"),
//    so grey-out is what's available; the widget never even constructs them
//    when !editSession.
//  - Tab/Backtab are only intercepted in edit mode; the widget's
//    RemoteTextEdit::keyPressEvent() intercepts them even when read-only
//    (its Tab handler edits the QTextCursor directly, bypassing
//    QPlainTextEdit's own read-only guard), which reads as a pre-existing
//    widget quirk rather than an intended behavior worth reproducing.
//  - The Mac-only "Ctrl+L" goto shortcut isn't ported; Ctrl+G is used on
//    every platform.
Rectangle {
    id: root
    implicitWidth: 640
    implicitHeight: 480
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    readonly property bool editSession: remoteEditController.isEditSession
    property string footerStatus: qsTr("Ready")

    function reportStatus() {
        remoteEditController.updateStatus(textArea.selectionStart,
                                          textArea.selectionEnd,
                                          textArea.cursorPosition);
    }

    function showFindBar() {
        gotoBar.visible = false;
        findBar.visible = true;
        findInput.forceActiveFocus();
        findInput.selectAll();
    }
    function hideFindBar() {
        findBar.visible = false;
        textArea.forceActiveFocus();
    }
    function showGotoBar() {
        findBar.visible = false;
        gotoBar.visible = true;
        gotoInput.forceActiveFocus();
        gotoInput.selectAll();
    }
    function hideGotoBar() {
        gotoBar.visible = false;
        textArea.forceActiveFocus();
    }

    function performFind(backwards) {
        if (findInput.text.length === 0) {
            return;
        }
        const result = remoteEditController.find(findInput.text,
                                                  backwards,
                                                  textArea.selectionStart,
                                                  textArea.selectionEnd);
        if (result.found) {
            textArea.select(result.start, result.end);
        }
        root.footerStatus = result.message;
    }

    function performReplaceCurrent() {
        if (findInput.text.length === 0 || !replaceToggle.checked) {
            return;
        }
        const result = remoteEditController.replaceCurrent(findInput.text,
                                                            replaceInput.text,
                                                            textArea.selectionStart,
                                                            textArea.selectionEnd);
        if (result.found) {
            textArea.select(result.start, result.end);
        }
        root.footerStatus = result.message;
    }

    function performReplaceAll() {
        if (findInput.text.length === 0 || !replaceToggle.checked) {
            return;
        }
        const result = remoteEditController.replaceAll(findInput.text, replaceInput.text);
        root.footerStatus = result.message;
    }

    function performGoto() {
        const lineNum = parseInt(gotoInput.text, 10);
        if (isNaN(lineNum) || lineNum <= 0) {
            gotoInput.selectAll();
            gotoInput.forceActiveFocus();
            return;
        }
        const result = remoteEditController.gotoLine(lineNum);
        root.footerStatus = result.message;
        if (result.found) {
            textArea.cursorPosition = result.start;
            root.hideGotoBar();
        } else {
            gotoInput.selectAll();
            gotoInput.forceActiveFocus();
        }
    }

    Component.onCompleted: {
        remoteEditController.attachDocument(textArea.textDocument);
        textArea.tabStopDistance = fontMetrics.advanceWidth(" ") * 8;
        root.reportStatus();
    }

    Connections {
        target: remoteEditController
        function onSig_statusChanged() {
            root.footerStatus = remoteEditController.statusText;
        }
    }

    FontMetrics {
        id: fontMetrics
        font.family: remoteEditController.fontFamily
        font.pointSize: remoteEditController.fontPointSize > 0 ? remoteEditController.fontPointSize
                                                                : font.pointSize
    }

    MenuBar {
        id: menuBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        Menu {
            title: qsTr("&File")

            Action {
                text: qsTr("&Submit")
                shortcut: "Ctrl+S"
                enabled: root.editSession
                onTriggered: {
                    remoteEditController.submit();
                    dialog.close();
                }
            }
            Action {
                text: qsTr("E&xit")
                shortcut: "Ctrl+Q"
                onTriggered: {
                    remoteEditController.cancel();
                    dialog.close();
                }
            }
        }

        Menu {
            title: qsTr("&Edit")

            Action {
                text: qsTr("&Undo")
                shortcut: "Ctrl+Z"
                enabled: root.editSession
                onTriggered: textArea.undo()
            }
            Action {
                text: qsTr("&Redo")
                shortcut: "Ctrl+Shift+Z"
                enabled: root.editSession
                onTriggered: textArea.redo()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Select All")
                shortcut: "Ctrl+A"
                onTriggered: textArea.selectAll()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Cu&t")
                shortcut: "Ctrl+X"
                enabled: root.editSession
                onTriggered: textArea.cut()
            }
            Action {
                text: qsTr("&Copy")
                shortcut: "Ctrl+C"
                onTriggered: textArea.copy()
            }
            Action {
                text: qsTr("&Paste")
                shortcut: "Ctrl+V"
                enabled: root.editSession
                onTriggered: textArea.paste()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Find/Replace...")
                shortcut: StandardKey.Find
                onTriggered: root.showFindBar()
            }
            Action {
                text: qsTr("&Go to Line...")
                shortcut: "Ctrl+G"
                onTriggered: root.showGotoBar()
            }
            MenuSeparator {}

            Menu {
                title: qsTr("&Alignment")
                enabled: root.editSession

                Action {
                    text: qsTr("&Justify Entire Message")
                    onTriggered: remoteEditController.justifyAll()
                }
                Action {
                    text: qsTr("Justify &Selection")
                    shortcut: "Ctrl+J"
                    onTriggered: remoteEditController.justifySelection(textArea.selectionStart,
                                                                        textArea.selectionEnd)
                }
                Action {
                    text: qsTr("Joi&n Lines")
                    shortcut: "Ctrl+Shift+J"
                    onTriggered: remoteEditController.joinLines(textArea.selectionStart,
                                                                 textArea.selectionEnd)
                }
                Action {
                    text: qsTr("&Quote Lines")
                    shortcut: "Ctrl+>"
                    onTriggered: remoteEditController.quoteLines(textArea.selectionStart,
                                                                  textArea.selectionEnd)
                }
            }
            Menu {
                title: qsTr("&Colors")
                enabled: root.editSession

                Action {
                    text: qsTr("&Normalize Ansi Codes")
                    shortcut: "Ctrl+N"
                    onTriggered: remoteEditController.normalizeAnsi()
                }
                Action {
                    text: qsTr("&Insert Ansi Reset Code")
                    shortcut: "Ctrl+I"
                    onTriggered: {
                        textArea.cursorPosition = remoteEditController.insertAnsiReset(
                                    textArea.cursorPosition);
                    }
                }
            }
            Menu {
                title: qsTr("&Whitespace")
                enabled: root.editSession

                Action {
                    text: qsTr("&Expand Tabs")
                    shortcut: "Ctrl+E"
                    onTriggered: remoteEditController.expandTabs(textArea.selectionStart,
                                                                  textArea.selectionEnd)
                }
                Action {
                    text: qsTr("Remove Trailing &Whitespace")
                    shortcut: "Ctrl+W"
                    onTriggered: remoteEditController.removeTrailingWhitespace(
                                     textArea.selectionStart, textArea.selectionEnd)
                }
                Action {
                    text: qsTr("Remove &Duplicate Spaces")
                    shortcut: "Ctrl+D"
                    onTriggered: remoteEditController.removeDuplicateSpaces(
                                     textArea.selectionStart, textArea.selectionEnd)
                }
            }
        }

        Menu {
            title: qsTr("&View")

            Action {
                text: qsTr("&Preview Ansi Codes")
                shortcut: "Ctrl+P"
                onTriggered: remoteEditController.previewAnsi()
            }
            Action {
                text: qsTr("Toggle &Whitespace")
                shortcut: "Ctrl+Shift+W"
                onTriggered: remoteEditController.toggleWhitespace()
            }
        }
    }

    // ---------------- Go to Line bar ----------------
    Rectangle {
        id: gotoBar
        objectName: "gotoBar"
        anchors.top: menuBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? 32 : 0
        visible: false
        color: sysPalette.window
        border.color: "gray"
        border.width: visible ? 1 : 0

        Keys.onEscapePressed: root.hideGotoBar()

        Row {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4

            TextField {
                id: gotoInput
                objectName: "gotoInput"
                width: 100
                placeholderText: qsTr("Go to line")
                validator: IntValidator { bottom: 1; top: 1000000 }
                Keys.onEscapePressed: root.hideGotoBar()
                onAccepted: root.performGoto()
            }
            Button {
                text: qsTr("Go")
                onClicked: root.performGoto()
            }
            Button {
                text: qsTr("Close")
                onClicked: root.hideGotoBar()
            }
        }
    }

    // ---------------- Find / Replace bar ----------------
    Rectangle {
        id: findBar
        objectName: "findBar"
        anchors.top: menuBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? (replaceToggle.checked ? 64 : 32) : 0
        visible: false
        color: sysPalette.window
        border.color: "gray"
        border.width: visible ? 1 : 0

        Keys.onEscapePressed: root.hideFindBar()

        Column {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4

            Row {
                width: parent.width
                spacing: 4

                TextField {
                    id: findInput
                    objectName: "findInput"
                    width: 200
                    placeholderText: qsTr("Find")
                    Keys.onEscapePressed: root.hideFindBar()
                    onAccepted: root.performFind(false)
                }
                Button {
                    text: qsTr("Previous")
                    enabled: findInput.text.length > 0
                    onClicked: root.performFind(true)
                }
                Button {
                    text: qsTr("Next")
                    enabled: findInput.text.length > 0
                    onClicked: root.performFind(false)
                }
                CheckBox {
                    id: replaceToggle
                    objectName: "replaceToggle"
                    text: qsTr("Replace")
                    enabled: root.editSession
                }
                Button {
                    text: qsTr("Close")
                    onClicked: root.hideFindBar()
                }
            }

            Row {
                width: parent.width
                spacing: 4
                visible: replaceToggle.checked

                TextField {
                    id: replaceInput
                    objectName: "replaceInput"
                    width: 200
                    placeholderText: qsTr("Replace")
                    Keys.onEscapePressed: root.hideFindBar()
                    onAccepted: root.performReplaceCurrent()
                }
                Button {
                    text: qsTr("Replace")
                    enabled: findInput.text.length > 0
                    onClicked: root.performReplaceCurrent()
                }
                Button {
                    text: qsTr("All")
                    enabled: findInput.text.length > 0
                    onClicked: root.performReplaceAll()
                }
            }
        }
    }

    ScrollView {
        id: scrollView
        anchors.top: findBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footerRow.top
        anchors.margins: 4
        clip: true

        TextArea {
            id: textArea
            objectName: "remoteTextArea"
            readOnly: !root.editSession
            wrapMode: TextEdit.NoWrap
            font.family: remoteEditController.fontFamily
            font.pointSize: remoteEditController.fontPointSize > 0
                            ? remoteEditController.fontPointSize : font.pointSize
            text: remoteEditController.body

            onCursorPositionChanged: root.reportStatus()
            onSelectionStartChanged: root.reportStatus()
            onSelectionEndChanged: root.reportStatus()

            Keys.onPressed: function (event) {
                if (!root.editSession) {
                    return;
                }
                if (event.key === Qt.Key_Tab) {
                    if (textArea.selectionStart !== textArea.selectionEnd) {
                        remoteEditController.indentSelection(textArea.selectionStart,
                                                              textArea.selectionEnd);
                    } else {
                        textArea.cursorPosition = remoteEditController.insertTabExpanded(
                                    textArea.cursorPosition);
                    }
                    event.accepted = true;
                } else if (event.key === Qt.Key_Backtab) {
                    remoteEditController.unindentSelection(textArea.selectionStart,
                                                            textArea.selectionEnd);
                    event.accepted = true;
                }
            }
        }
    }

    Row {
        id: footerRow
        objectName: "footerRow"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4

        Label {
            id: footerLabel
            objectName: "footerLabel"
            text: root.footerStatus
            elide: Text.ElideRight
            width: parent.width
        }
    }
}
