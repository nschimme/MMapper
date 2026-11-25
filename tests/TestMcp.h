#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestMcp final : public QObject
{
    Q_OBJECT

public:
    TestMcp() = default;
    ~TestMcp() override = default;

private Q_SLOTS:
    // McpEventLog
    static void eventLogSequenceTest();
    static void eventLogCursorTest();
    static void eventLogFilterAndLimitTest();
    static void eventLogTruncationTest();
    static void eventLogFutureCursorTest();
    static void eventLogOnAppendTest();

    // McpWaiters
    static void waitersMatchTest();
    static void waitersTimeoutTest();
    static void waitersCancelTest();
    static void waitersReentrancyTest();

    // McpHttp
    static void httpSimplePostTest();
    static void httpPartialReadsTest();
    static void httpPipelinedRequestsTest();
    static void httpKeepAliveTest();
    static void httpRejectsBadRequestsTest();
    static void httpLimitsTest();
    static void httpResponseTest();

    // McpToolRegistry
    static void registryValidationTest();
    static void registryOverrideTest();

    // MCP tools that need nothing from the rest of MMapper
    static void readEventsToolTest();
    static void waitForEventLoggedTest();
    static void waitForEventNewEventTest();
    static void waitForEventFutureCursorTest();
    static void waitForEventValidationTest();
    static void waitForEventTruncationTest();
    static void toolResultImageTest();

    // McpAccess
    static void accessTokenCompareTest();
    static void accessCheckTest();
    static void connectGuardTest();

    // McpServer (real loopback sockets)
    static void serverRequestTest();
    static void serverRejectsBadRequestsTest();
    static void serverPipeliningAndDeferredReplyTest();
    static void serverReplyTimeoutTest();
    static void serverLimitsAndListenFailureTest();

    // McpDispatcher
    static void dispatcherInitializeTest();
    static void dispatcherPingAndNotificationTest();
    static void dispatcherToolsListTest();
    static void dispatcherToolsCallTest();
    static void dispatcherToolErrorsTest();
    static void dispatcherDeferredReplyTest();
    static void dispatcherMalformedInputTest();
};
