// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// The Preferences dialog's QML shell, driven by PreferencesController (see
// ../preferences/PreferencesController.h) exposed as the
// "preferencesController" context property, and "dialog" (the enclosing
// QmlDialog, see QmlDialog.h) for the OK/Cancel buttons. Mirrors
// configdialog.ui/configdialog.cpp's layout and behavior without a .ui file:
// a left icon+text nav list, a single scrolling column of ALL nine pages
// (each under a bold section header with a horizontal rule, like
// makeSectionHeader), a scrollspy that keeps the nav selection in sync with
// the scroll position (slot_onScroll), a debounced search across every
// page's controls with a results list replacing the page column
// (slot_search), and a "Back to top" footer button inside the scroll
// content (configdialog.ui's footerWidget).
Rectangle {
    id: root
    implicitWidth: 800
    implicitHeight: 600
    color: sysPalette.window

    // This dialog is its own top-level window (see QmlDialog), separate from
    // the main shell, so it computes its own compact breakpoint from its own
    // width rather than the shell's. Below this width the 180px side nav and
    // its search column would steal too much space from an already-narrow
    // screen, so the chrome (nav/search) reflows into a top header while the
    // page column/scrollspy/search machinery stay exactly as they are.
    readonly property bool compact: width < 600

    // The nine sections, in configdialog.cpp's addPage() order. Icons name
    // files under qrc:/icons/ (see resources/mmapper2.qrc).
    ListModel {
        id: navModel
        ListElement { label: qsTr("General"); icon: "generalcfg.png" }
        ListElement { label: qsTr("Graphics"); icon: "graphicscfg.png" }
        ListElement { label: qsTr("Parser"); icon: "parsercfg.png" }
        ListElement { label: qsTr("Integrated Client"); icon: "terminal.png" }
        ListElement { label: qsTr("Group Panel"); icon: "group-recolor.png" }
        ListElement { label: qsTr("Auto Logger"); icon: "autologgercfg.png" }
        ListElement { label: qsTr("Audio"); icon: "audiocfg.png" }
        ListElement { label: qsTr("Mume Protocol"); icon: "mumeprotocolcfg.png" }
        ListElement { label: qsTr("Path Machine"); icon: "pathmachinecfg.png" }
    }

    // Section items in the same order as navModel, so nav index i maps to
    // sections[i].
    readonly property var sections: [
        generalSection, graphicsSection, parserSection, clientSection,
        groupSection, autoLogSection, audioSection, mumeProtocolSection,
        pathMachineSection
    ]

    // True while a scroll was initiated from code (nav click, search result,
    // back-to-top) so the resulting contentYChanged doesn't fight the nav
    // selection; the widget uses QSignalBlocker for the same purpose.
    property bool programmaticScroll: false

    // True once a debounced non-empty search has run; switches the right
    // pane from the page column to the results list (or the "no matches"
    // placeholder), mirroring configdialog.ui's rightStack.
    property bool searchActive: false

    // Flat result rows: { header, text, icon, target, sectionIndex }.
    // Header rows carry the page's name/icon and scroll to the section;
    // child rows carry a matching control's text and scroll to (and focus)
    // that control, like slot_search()'s Qt::UserRole payload.
    property var searchResults: []

    // Per-page nav enablement while a search is active; empty means "all
    // enabled" (no search). Mirrors slot_search() clearing Qt item flags on
    // non-matching pages.
    property var navEnabled: []

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    SystemPalette {
        id: sysPaletteDisabled
        colorGroup: SystemPalette.Disabled
    }

    // A bold header (pointSize+1) with a 1px rule filling the remaining
    // width, followed by the page body; mirrors makeSectionHeader() plus
    // addPage()'s header+page container.
    component Section: Column {
        id: sectionRoot
        objectName: "preferencesSection"
        property string title
        readonly property Item body: sectionBody
        default property alias content: sectionBody.data
        width: parent ? parent.width : 0
        spacing: 8

        Row {
            width: parent.width
            spacing: 8

            Label {
                id: sectionLabel
                text: sectionRoot.title
                font.bold: true
                font.pointSize: Qt.application.font.pointSize + 1
            }

            Rectangle {
                width: Math.max(0, parent.width - sectionLabel.width - parent.spacing)
                height: 1
                anchors.verticalCenter: parent.verticalCenter
                color: sysPalette.mid
            }
        }

        Column {
            id: sectionBody
            width: parent.width
        }
    }

    // Returns the searchable text of a control, mirroring the widget's
    // getSearchableText() (QCheckBox/QRadioButton/QLabel/QPushButton/
    // QGroupBox). Duck-typed: anything with a non-empty string "text"
    // property that is not an editable text input (TextField/TextInput
    // expose cursorPosition; their user-typed contents are not settings
    // text).
    function searchableText(item) {
        if (typeof item.text !== "string" || item.text === "")
            return "";
        if (item.cursorPosition !== undefined)
            return "";
        return item.text;
    }

    // Depth-first walk over a section body collecting controls whose text
    // contains the (lowercased) needle. Matched-type controls are not
    // descended into, so a CheckBox's internal contentItem Text doesn't
    // match a second time.
    function collectMatches(item, needle, out) {
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i];
            var t = root.searchableText(child);
            if (t !== "") {
                if (t.toLowerCase().indexOf(needle) !== -1) {
                    // Strip accelerator ampersands like the widget does.
                    out.push({ text: t.replace(/&/g, ""), target: child });
                }
            } else {
                root.collectMatches(child, needle, out);
            }
        }
    }

    // Debounced search body, mirroring slot_search(): match page names and
    // control texts case-insensitively, build header+child result rows,
    // disable nav entries for non-matching pages, and swap the right pane.
    function performSearch(text) {
        if (text === "") {
            root.searchActive = false;
            root.searchResults = [];
            root.navEnabled = [];
            return;
        }

        var needle = text.toLowerCase();
        var results = [];
        var enabledArr = [];
        for (var i = 0; i < root.sections.length; ++i) {
            var matches = [];
            root.collectMatches(root.sections[i].body, needle, matches);
            var pageName = navModel.get(i).label;
            if (pageName.toLowerCase().indexOf(needle) !== -1 || matches.length > 0) {
                enabledArr.push(true);
                results.push({
                    header: true,
                    text: pageName,
                    icon: navModel.get(i).icon,
                    target: root.sections[i],
                    sectionIndex: i
                });
                for (var j = 0; j < matches.length; ++j) {
                    results.push({
                        header: false,
                        text: matches[j].text,
                        icon: "",
                        target: matches[j].target,
                        sectionIndex: i
                    });
                }
            } else {
                enabledArr.push(false);
                // If the current nav selection is being disabled, clear it.
                if (navList.currentIndex === i)
                    navList.currentIndex = -1;
            }
        }

        root.navEnabled = enabledArr;
        root.searchResults = results;
        root.searchActive = true;
        searchField.forceActiveFocus();
    }

    function clearSearch() {
        searchField.text = "";
        searchTimer.stop();
        root.performSearch("");
    }

    function clampContentY(y) {
        var maxY = Math.max(0, pagesFlickable.contentHeight - pagesFlickable.height);
        return Math.max(0, Math.min(y, maxY));
    }

    // Nav click / back-to-top: clear any pending search, select the nav
    // item, and jump the flickable to the section, mirroring
    // slot_changePage() + scrollToWidget().
    function scrollToSection(index) {
        if (searchField.text !== "")
            root.clearSearch();
        navList.currentIndex = index;
        var y = root.sections[index].mapToItem(pagesFlickable.contentItem, 0, 0).y;
        root.programmaticScroll = true;
        pagesFlickable.contentY = root.clampContentY(y);
        root.programmaticScroll = false;
    }

    // Search result click: restore the page column, scroll to the matched
    // control, and focus it, mirroring slot_onResultSelected().
    function activateResult(result) {
        root.clearSearch();
        navList.currentIndex = result.sectionIndex;
        var y = result.target.mapToItem(pagesFlickable.contentItem, 0, 0).y;
        root.programmaticScroll = true;
        pagesFlickable.contentY = root.clampContentY(y);
        root.programmaticScroll = false;
        result.target.forceActiveFocus();
    }

    // Invokable-by-index variant for tests (JS object rows are awkward to
    // pass from C++).
    function activateResultAt(index) {
        root.activateResult(root.searchResults[index]);
    }

    // Scrollspy, mirroring slot_onScroll(): pick the last section whose top
    // is at or above the viewport top (+8px slack); when scrolled to the
    // bottom, force the last section.
    function updateScrollspy() {
        if (root.programmaticScroll || searchField.text !== "")
            return;
        var value = pagesFlickable.contentY;
        var active = -1;
        for (var i = 0; i < root.sections.length; ++i) {
            var y = root.sections[i].mapToItem(pagesFlickable.contentItem, 0, 0).y;
            if (y <= value + 8)
                active = i;
        }
        if (value >= Math.max(0, pagesFlickable.contentHeight - pagesFlickable.height))
            active = root.sections.length - 1;
        if (active >= 0 && navList.currentIndex !== active)
            navList.currentIndex = active;
    }

    Timer {
        id: searchTimer
        interval: 50
        repeat: false
        onTriggered: root.performSearch(searchField.text)
    }

    // The single search field, shared by desktop and compact layouts: only
    // its anchoring/width reacts to root.compact, so there is one field
    // feeding searchTimer/performSearch either way (rather than a second
    // duplicate field for the compact header).
    TextField {
        id: searchField
        objectName: "preferencesSearchField"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 8
        // Desktop: sized to match navFrame, as before. Compact: full width
        // minus the section-jumper combo box that takes navFrame's place.
        width: root.compact ? (root.width - 24 - sectionCombo.width) : navFrame.width
        placeholderText: qsTr("Search")
        onTextChanged: searchTimer.restart()
    }

    // Compact-only section jumper, replacing the hidden side nav as the way
    // to jump to a section. Kept in sync with the scrollspy via currentIndex;
    // the binding is restored after each user pick (ComboBox breaks bindings
    // on interactive selection) so scrolling keeps updating it afterwards.
    ComboBox {
        id: sectionCombo
        objectName: "preferencesSectionCombo"
        visible: root.compact
        model: navModel
        textRole: "label"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        currentIndex: navList.currentIndex
        onActivated: {
            root.scrollToSection(currentIndex);
            currentIndex = Qt.binding(function () { return navList.currentIndex; });
        }
    }

    Rectangle {
        id: navFrame
        objectName: "preferencesNavFrame"
        visible: !root.compact
        anchors.top: searchField.bottom
        anchors.left: parent.left
        anchors.bottom: footerRow.top
        anchors.margins: 8
        width: 180
        color: sysPalette.base
        border.color: sysPalette.mid
        border.width: 1

        ListView {
            id: navList
            objectName: "preferencesNavList"
            anchors.fill: parent
            anchors.margins: 1
            clip: true
            model: navModel
            currentIndex: 0

            delegate: Rectangle {
                width: navList.width
                height: 32
                color: navList.currentIndex === index ? sysPalette.highlight : "transparent"

                // False while a search is active and this page has no match;
                // the entry grays out and stops responding, like the widget
                // clearing the item's Qt::ItemIsEnabled flag.
                readonly property bool navItemEnabled: root.navEnabled.length === 0
                                                       || root.navEnabled[index] === true

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    spacing: 6

                    Image {
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/icons/" + model.icon
                        opacity: navItemEnabled ? 1.0 : 0.4
                    }

                    Text {
                        width: parent.width - 16 - 6 - 8
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: model.label
                        color: !navItemEnabled ? sysPaletteDisabled.text
                                               : navList.currentIndex === index ? sysPalette.highlightedText
                                                                                : sysPalette.text
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: navItemEnabled
                    onClicked: root.scrollToSection(index)
                }
            }
        }
    }

    // Right pane, page 0 of configdialog.ui's rightStack: one flickable
    // column of all nine pages plus the footer.
    Flickable {
        id: pagesFlickable
        objectName: "preferencesFlickable"
        anchors.top: parent.top
        // Compact: full width below the compact header (search field +
        // section combo, whichever is taller). Desktop: unchanged, right of
        // navFrame with no extra top offset.
        anchors.topMargin: root.compact ? (16 + Math.max(searchField.height, sectionCombo.height)) : 8
        anchors.left: root.compact ? parent.left : navFrame.right
        anchors.right: parent.right
        anchors.bottom: footerRow.top
        anchors.margins: 8
        clip: true
        visible: !root.searchActive
        contentWidth: width
        contentHeight: contentColumn.height + 16
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        onContentYChanged: root.updateScrollspy()

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: contentColumn
            objectName: "preferencesContentColumn"
            x: 8
            y: 8
            width: pagesFlickable.width - 16
            spacing: 20

            Section { id: generalSection; title: qsTr("General"); PrefsGeneralPage { width: parent.width } }
            Section { id: graphicsSection; title: qsTr("Graphics"); PrefsGraphicsPage { width: parent.width } }
            Section { id: parserSection; title: qsTr("Parser"); PrefsParserPage { width: parent.width } }
            Section { id: clientSection; title: qsTr("Integrated Client"); PrefsClientPage { width: parent.width } }
            Section { id: groupSection; title: qsTr("Group Panel"); PrefsGroupPage { width: parent.width } }
            Section { id: autoLogSection; title: qsTr("Auto Logger"); PrefsAutoLogPage { width: parent.width } }
            Section { id: audioSection; title: qsTr("Audio"); PrefsAudioPage { width: parent.width } }
            Section { id: mumeProtocolSection; title: qsTr("Mume Protocol"); PrefsMumeProtocolPage { width: parent.width } }
            Section { id: pathMachineSection; title: qsTr("Path Machine"); PrefsPathMachinePage { width: parent.width } }

            // configdialog.ui's footerWidget: end-of-content label plus the
            // "Back to top" button.
            Column {
                width: parent.width
                spacing: 10
                topPadding: 20
                bottomPadding: 20

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    font.italic: true
                    color: sysPaletteDisabled.text
                    text: qsTr("The road goes no further.")
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Back to top")
                    onClicked: root.scrollToSection(0)
                }
            }
        }
    }

    // Right pane, page 1 of the rightStack: search results.
    ListView {
        id: resultsList
        objectName: "preferencesSearchResults"
        anchors.fill: pagesFlickable
        clip: true
        visible: root.searchActive && root.searchResults.length > 0
        model: root.searchResults

        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            width: resultsList.width
            height: Theme.rowHeight
            color: "transparent"

            Row {
                anchors.fill: parent
                anchors.leftMargin: modelData.header ? 4 : 20
                spacing: 6

                Image {
                    width: 16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                    visible: modelData.header
                    source: modelData.icon !== "" ? "qrc:/icons/" + modelData.icon : ""
                }

                Text {
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    font.bold: modelData.header
                    text: modelData.header ? modelData.text : "• " + modelData.text
                    color: sysPalette.text
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.activateResult(modelData)
            }
        }
    }

    // Right pane, page 2 of the rightStack: no matches.
    Label {
        objectName: "preferencesNoResultsLabel"
        anchors.fill: pagesFlickable
        visible: root.searchActive && root.searchResults.length === 0
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        font.bold: true
        font.pointSize: Qt.application.font.pointSize + 4
        color: sysPaletteDisabled.text
        text: qsTr("No matches found!\nMaybe try searching for something else?")
    }

    Row {
        id: footerRow
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 8

        Button {
            id: cancelButton
            text: qsTr("Cancel")
            onClicked: {
                preferencesController.cancel();
                dialog.reject();
            }
        }

        Button {
            id: okButton
            text: qsTr("OK")
            onClicked: {
                preferencesController.ok();
                dialog.accept();
            }
        }
    }
}
