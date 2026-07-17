#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestClient final : public QObject
{
    Q_OBJECT

public:
    TestClient();
    ~TestClient() final;

private Q_SLOTS:
    void plainTextLine();
    void leadingWhitespacePreserved();
    void foregroundColor();
    void backgroundColor();
    void boldStyle();
    void color256();
    void reverseVideo();
    void partialLineAcrossCalls();
    void crLfTreatedAsSingleBreak();
    void loneCrTreatedAsBreak();
    void crLfSplitAcrossAppendCalls();
    void scrollbackCap();
    void clearResetsModel();

    // InputHistory
    void inputHistoryDedupVsBackOnly();
    void inputHistoryEmptyLineSkipped();
    void inputHistoryCap();
    void inputHistoryNavigation();

    // TabHistory
    void tabHistoryWordExtraction();
    void tabHistoryCap();
    void tabHistoryNextMatchCycling();

    // ClientController
    void controllerSplitCommands();
    void controllerSendInputDisconnected();
    void controllerSendInputConnected();
    void controllerSendPassword();
    void controllerEchoModeChanged();
    void controllerSendToUserWhileHidden();
    void controllerDisconnectedReconnectHint();
    void controllerHotkeys();

    // RemoteEditDocumentOps (widget-free core of the MPI remote editor)
    void remoteEditJustifyTextRewrapsLongLines();
    void remoteEditExpandTabsExpandsSelection();
    void remoteEditRemoveDuplicateSpacesCollapses();
    void remoteEditJoinLinesCombinesBlocks();
    void remoteEditPrefixPartialSelectionQuotesLines();
    void remoteEditFindWrapsAround();
    void remoteEditReplaceAllReplacesEveryOccurrence();
    void remoteEditStatusReportsTabsLongLinesAndTrailingSpace();
};
