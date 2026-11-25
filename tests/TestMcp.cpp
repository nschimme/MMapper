// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestMcp.h"

#include "../src/mcp/McpDispatcher.h"
#include "../src/mcp/McpEventLog.h"
#include "../src/mcp/McpHttp.h"
#include "../src/mcp/McpToolRegistry.h"

#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

namespace {

// ---- event log helpers ----

void fill(McpEventLog &log, const int count, const QString &kind = QStringLiteral("text"))
{
    for (int i = 0; i < count; ++i) {
        std::ignore = log.append(kind, QString::number(i + 1));
    }
}

// ---- http helpers ----

McpHttpRequest expectRequest(const std::optional<McpHttpParser::Result> &r)
{
    if (!r.has_value()) {
        QTest::qFail("expected a request, but the parser wanted more bytes", __FILE__, __LINE__);
        return {};
    }
    if (const auto *err = std::get_if<McpHttpError>(&*r)) {
        QTest::qFail(qPrintable(QString("expected a request, got error %1: %2")
                                    .arg(err->status)
                                    .arg(QString::fromUtf8(err->reason))),
                     __FILE__,
                     __LINE__);
        return {};
    }
    return std::get<McpHttpRequest>(*r);
}

std::optional<McpHttpError> expectError(const std::optional<McpHttpParser::Result> &r)
{
    if (r.has_value()) {
        if (const auto *err = std::get_if<McpHttpError>(&*r)) {
            return *err;
        }
    }
    return std::nullopt;
}

QByteArray post(const QByteArray &body, const QByteArray &extra = {})
{
    return "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n" + extra
           + "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

// ---- dispatcher helpers ----

struct Reply final
{
    bool called = false;
    int callCount = 0;
    std::optional<QByteArray> body;
};

QJsonObject toObject(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object();
}

McpDispatcher::Respond recorder(const std::shared_ptr<Reply> &reply)
{
    return [reply](std::optional<QByteArray> body) {
        reply->called = true;
        ++reply->callCount;
        reply->body = std::move(body);
    };
}

std::shared_ptr<Reply> call(const McpDispatcher &d, const QByteArray &request)
{
    auto reply = std::make_shared<Reply>();
    d.handle(request, recorder(reply));
    return reply;
}

QByteArray rpc(const QString &method, const QJsonObject &params, const int id = 1)
{
    QJsonObject obj;
    obj["jsonrpc"] = "2.0";
    obj["id"] = id;
    obj["method"] = method;
    obj["params"] = params;
    return QJsonDocument{obj}.toJson(QJsonDocument::Compact);
}

template<typename Fn>
bool throwsInvalidArgument(Fn &&fn)
{
    try {
        fn();
    } catch (const std::invalid_argument &) {
        return true;
    }
    return false;
}

McpToolRegistry makeRegistry()
{
    McpToolRegistry reg;
    reg.addSync("echo",
                "Echoes the text argument.",
                mcpObjectSchema({{"text", mcpStringProperty("text to echo")}}, {"text"}),
                [](const QJsonObject &args) {
                    return McpToolResult::ok(args.value("text").toString(),
                                             QJsonObject{{"echoed", args.value("text")}});
                });
    reg.addSync("fail", "Always fails.", mcpObjectSchema(), [](const QJsonObject &) {
        return McpToolResult::error("it failed");
    });
    reg.addSync("throws", "Throws.", mcpObjectSchema(), [](const QJsonObject &) -> McpToolResult {
        throw std::runtime_error("boom");
    });
    return reg;
}

} // namespace

// ============================== McpEventLog ==============================

void TestMcp::eventLogSequenceTest()
{
    McpEventLog log;
    QCOMPARE(log.lastSeq(), uint64_t{0});
    QCOMPARE(log.append("a", "one"), uint64_t{1});
    QCOMPARE(log.append("b", "two"), uint64_t{2});
    QCOMPARE(log.lastSeq(), uint64_t{2});
    QCOMPARE(log.size(), size_t{2});
}

void TestMcp::eventLogCursorTest()
{
    McpEventLog log;
    fill(log, 5);

    auto all = log.readSince(0);
    QCOMPARE(all.events.size(), size_t{5});
    QCOMPARE(all.events.front().seq, uint64_t{1});
    QCOMPARE(all.events.back().seq, uint64_t{5});
    QCOMPARE(all.nextSeq, uint64_t{5});
    QVERIFY(!all.truncated);

    // Reading from the cursor returns only what is new.
    auto none = log.readSince(all.nextSeq);
    QVERIFY(none.events.empty());
    QCOMPARE(none.nextSeq, uint64_t{5});

    fill(log, 2);
    auto more = log.readSince(all.nextSeq);
    QCOMPARE(more.events.size(), size_t{2});
    QCOMPARE(more.events.front().seq, uint64_t{6});
    QCOMPARE(more.nextSeq, uint64_t{7});
}

void TestMcp::eventLogFilterAndLimitTest()
{
    McpEventLog log;
    std::ignore = log.append("mud", "1");
    std::ignore = log.append("gmcp", "2");
    std::ignore = log.append("mud", "3");
    std::ignore = log.append("gmcp", "4");
    std::ignore = log.append("mud", "5");

    const auto onlyGmcp = log.readSince(0, {"gmcp"});
    QCOMPARE(onlyGmcp.events.size(), size_t{2});
    QCOMPARE(onlyGmcp.events[0].text, QString("2"));
    QCOMPARE(onlyGmcp.events[1].text, QString("4"));
    // Events the filter skipped must not be scanned again next time.
    QCOMPARE(onlyGmcp.nextSeq, uint64_t{5});

    // With a limit, the cursor stops at the last event actually returned.
    const auto firstTwoMud = log.readSince(0, {"mud"}, 2);
    QCOMPARE(firstTwoMud.events.size(), size_t{2});
    QCOMPARE(firstTwoMud.nextSeq, uint64_t{3});
    const auto rest = log.readSince(firstTwoMud.nextSeq, {"mud"}, 2);
    QCOMPARE(rest.events.size(), size_t{1});
    QCOMPARE(rest.events[0].text, QString("5"));
    QCOMPARE(rest.nextSeq, uint64_t{5});
}

void TestMcp::eventLogTruncationTest()
{
    McpEventLog log(/*maxEvents*/ 3);
    fill(log, 5); // seq 1..5, only 3..5 retained
    QCOMPARE(log.size(), size_t{3});

    const auto fromStart = log.readSince(0);
    QVERIFY(fromStart.truncated); // events 1 and 2 are gone
    QCOMPARE(fromStart.events.size(), size_t{3});
    QCOMPARE(fromStart.events.front().seq, uint64_t{3});

    const auto fromTwo = log.readSince(2); // 3 is the next event, nothing was missed
    QVERIFY(!fromTwo.truncated);
    QCOMPARE(fromTwo.events.size(), size_t{3});

    const auto fromOne = log.readSince(1); // 2 was dropped
    QVERIFY(fromOne.truncated);

    // A byte limit also trims, but always keeps the newest event.
    McpEventLog small(100, /*maxBytes*/ 1);
    fill(small, 3);
    QCOMPARE(small.size(), size_t{1});
    QCOMPARE(small.readSince(0).events.front().seq, uint64_t{3});
}

void TestMcp::eventLogFutureCursorTest()
{
    McpEventLog log;
    fill(log, 2);
    const auto r = log.readSince(100); // e.g. a cursor left over from before a restart
    QVERIFY(r.events.empty());
    QVERIFY(!r.truncated);

    McpEventLog empty;
    QVERIFY(empty.readSince(0).events.empty());
    QVERIFY(!empty.readSince(0).truncated);
}

// ================================ McpHttp ================================

void TestMcp::httpSimplePostTest()
{
    McpHttpParser p;
    p.feed(post(R"({"a":1})"));
    const auto req = expectRequest(p.next());
    QCOMPARE(req.method, QByteArray("POST"));
    QCOMPARE(req.target, QByteArray("/mcp"));
    QCOMPARE(req.body, QByteArray(R"({"a":1})"));
    QCOMPARE(req.header("content-type"), QByteArray("application/json"));
    QVERIFY(req.keepAlive);
    QVERIFY(!p.next().has_value());
}

void TestMcp::httpPartialReadsTest()
{
    const QByteArray whole = post(R"({"hello":"world"})");
    // Feed one byte at a time; the request only completes with the last byte.
    McpHttpParser p;
    for (qsizetype i = 0; i < whole.size() - 1; ++i) {
        p.feed(whole.mid(i, 1));
        QVERIFY2(!p.next().has_value(), "request completed too early");
    }
    p.feed(whole.right(1));
    const auto req = expectRequest(p.next());
    QCOMPARE(req.body, QByteArray(R"({"hello":"world"})"));
}

void TestMcp::httpPipelinedRequestsTest()
{
    McpHttpParser p;
    p.feed(post("1") + post("22") + post("333").left(post("333").size() - 1));
    QCOMPARE(expectRequest(p.next()).body, QByteArray("1"));
    QCOMPARE(expectRequest(p.next()).body, QByteArray("22"));
    QVERIFY(!p.next().has_value()); // third request is one byte short
    p.feed("3");
    QCOMPARE(expectRequest(p.next()).body, QByteArray("333"));
}

void TestMcp::httpKeepAliveTest()
{
    {
        McpHttpParser p;
        p.feed(post("{}", "Connection: close\r\n"));
        QVERIFY(!expectRequest(p.next()).keepAlive);
    }
    {
        McpHttpParser p;
        p.feed("GET /mcp HTTP/1.0\r\n\r\n");
        QVERIFY(!expectRequest(p.next()).keepAlive); // HTTP/1.0 closes by default
    }
    {
        McpHttpParser p;
        p.feed("GET /mcp HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
        QVERIFY(expectRequest(p.next()).keepAlive);
    }
    {
        McpHttpParser p;
        p.feed("GET /mcp HTTP/1.1\r\n\r\n"); // no body and no Content-Length is fine for GET
        const auto req = expectRequest(p.next());
        QCOMPARE(req.method, QByteArray("GET"));
        QVERIFY(req.body.isEmpty());
    }
}

void TestMcp::httpRejectsBadRequestsTest()
{
    auto statusOf = [](const QByteArray &bytes) -> int {
        McpHttpParser p;
        p.feed(bytes);
        const auto err = expectError(p.next());
        return err ? err->status : 0;
    };

    QCOMPARE(statusOf("garbage\r\n\r\n"), 400);
    QCOMPARE(statusOf("POST /mcp HTTP/2.0\r\n\r\n"), 505);
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\n\r\n{}"), 411); // POST needs Content-Length
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\nContent-Length: abc\r\n\r\n"), 400);
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\nContent-Length: -1\r\n\r\n"), 400);
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx"), 400);
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"), 501);
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\nExpect: 100-continue\r\nContent-Length: 0\r\n\r\n"),
             417);
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\nBad Header: x\r\nContent-Length: 0\r\n\r\n"), 400);
    QCOMPARE(statusOf("POST /mcp HTTP/1.1\r\n folded: x\r\nContent-Length: 0\r\n\r\n"), 400);

    // After an error the parser is finished.
    McpHttpParser p;
    p.feed("garbage\r\n\r\n");
    QVERIFY(expectError(p.next()).has_value());
    p.feed(post("{}"));
    QVERIFY(!p.next().has_value());
}

void TestMcp::httpLimitsTest()
{
    McpHttpParser::Limits limits;
    limits.maxHeaderBytes = 256;
    limits.maxBodyBytes = 10;

    {
        McpHttpParser p{limits};
        p.feed(post(QByteArray(11, 'x')));
        const auto err = expectError(p.next());
        QVERIFY(err.has_value());
        QCOMPARE(err->status, 413);
    }
    {
        McpHttpParser p{limits};
        p.feed(post(QByteArray(10, 'x')));
        QCOMPARE(expectRequest(p.next()).body.size(), qsizetype{10});
    }
    {
        // Headers that never end must be cut off instead of buffered forever.
        McpHttpParser p{limits};
        p.feed("POST /mcp HTTP/1.1\r\n" + QByteArray(300, 'a'));
        const auto err = expectError(p.next());
        QVERIFY(err.has_value());
        QCOMPARE(err->status, 431);
    }
}

void TestMcp::httpResponseTest()
{
    const QByteArray ok = mcpHttpResponse(200, "application/json", R"({"x":1})", true);
    QVERIFY(ok.startsWith("HTTP/1.1 200 OK\r\n"));
    QVERIFY(ok.contains("Content-Type: application/json\r\n"));
    QVERIFY(ok.contains("Content-Length: 7\r\n"));
    QVERIFY(ok.contains("Connection: keep-alive\r\n"));
    QVERIFY(ok.endsWith("\r\n\r\n"
                        R"({"x":1})"));

    // 202 must not carry a body, even if one is passed.
    const QByteArray accepted = mcpHttpResponse(202, "application/json", "ignored", false);
    QVERIFY(accepted.startsWith("HTTP/1.1 202 Accepted\r\n"));
    QVERIFY(accepted.contains("Content-Length: 0\r\n"));
    QVERIFY(accepted.contains("Connection: close\r\n"));
    QVERIFY(accepted.endsWith("\r\n\r\n"));
    QVERIFY(!accepted.contains("ignored"));

    const QByteArray withHeader = mcpHttpResponse(405, "text/plain", "no", true, {{"Allow", "POST"}});
    QVERIFY(withHeader.contains("Allow: POST\r\n"));
}

// ============================= McpToolRegistry =============================

void TestMcp::registryValidationTest()
{
    McpToolRegistry reg;
    const McpToolHandler handler = [](const QJsonObject &, McpReply reply) {
        reply(McpToolResult::ok(""));
    };
    auto tryAdd =
        [&reg, &handler](const QString &name, const QJsonObject &schema, const McpToolHandler &h) {
            reg.add(McpTool{name, "d", schema, h});
        };
    const QJsonObject objectSchema = mcpObjectSchema();
    const QJsonObject stringSchema{{"type", "string"}};

    QVERIFY(throwsInvalidArgument([&]() { tryAdd("bad name", objectSchema, handler); }));
    QVERIFY(throwsInvalidArgument([&]() { tryAdd("", objectSchema, handler); }));
    QVERIFY(throwsInvalidArgument([&]() { tryAdd(QString(129, 'a'), objectSchema, handler); }));
    QVERIFY(throwsInvalidArgument([&]() { tryAdd("ok", stringSchema, handler); }));
    QVERIFY(throwsInvalidArgument([&]() { tryAdd("ok", objectSchema, nullptr); }));
    QVERIFY(reg.tools().empty());

    tryAdd("valid_name-1.x", objectSchema, handler);
    QCOMPARE(reg.tools().size(), size_t{1});
    QVERIFY(reg.find("valid_name-1.x") != nullptr);
    QVERIFY(reg.find("missing") == nullptr);
}

void TestMcp::registryOverrideTest()
{
    McpToolRegistry reg;
    reg.addSync("t", "generated", mcpObjectSchema(), [](const QJsonObject &) {
        return McpToolResult::ok("generated");
    });
    reg.addSync("other", "other", mcpObjectSchema(), [](const QJsonObject &) {
        return McpToolResult::ok("other");
    });
    reg.addSync("t", "hand-written", mcpObjectSchema(), [](const QJsonObject &) {
        return McpToolResult::ok("hand-written");
    });

    // Replaced in place, so the listing order is stable.
    QCOMPARE(reg.tools().size(), size_t{2});
    QCOMPARE(reg.tools()[0].name, QString("t"));
    QCOMPARE(reg.tools()[0].description, QString("hand-written"));
}

// ============================== McpDispatcher ==============================

void TestMcp::dispatcherInitializeTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher d{reg, "MMapper", "1.2.3", "be nice"};

    // A version we speak is echoed back.
    {
        const auto r = call(d, rpc("initialize", {{"protocolVersion", "2025-06-18"}}));
        QVERIFY(r->called && r->body);
        const auto result = toObject(*r->body).value("result").toObject();
        QCOMPARE(result.value("protocolVersion").toString(), QString("2025-06-18"));
        QCOMPARE(result.value("serverInfo").toObject().value("name").toString(), QString("MMapper"));
        QCOMPARE(result.value("serverInfo").toObject().value("version").toString(),
                 QString("1.2.3"));
        QCOMPARE(result.value("instructions").toString(), QString("be nice"));
        QVERIFY(result.value("capabilities").toObject().contains("tools"));
        QCOMPARE(toObject(*r->body).value("id").toInt(), 1);
    }
    // One we don't know gets our latest instead.
    {
        const auto r = call(d, rpc("initialize", {{"protocolVersion", "1999-01-01"}}));
        const auto result = toObject(*r->body).value("result").toObject();
        QCOMPARE(result.value("protocolVersion").toString(),
                 QString(McpDispatcher::LATEST_PROTOCOL_VERSION));
    }
    QVERIFY(McpDispatcher::isSupportedProtocolVersion("2024-11-05"));
    QVERIFY(!McpDispatcher::isSupportedProtocolVersion("nope"));
}

void TestMcp::dispatcherPingAndNotificationTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher d{reg, "MMapper", "1"};

    const auto ping = call(d, rpc("ping", {}, 7));
    QVERIFY(ping->body);
    QCOMPARE(toObject(*ping->body).value("id").toInt(), 7);
    QVERIFY(toObject(*ping->body).value("result").isObject());

    // A notification has no id and gets no reply body (the HTTP layer sends 202).
    const auto note = call(d, R"({"jsonrpc":"2.0","method":"notifications/initialized"})");
    QVERIFY(note->called);
    QCOMPARE(note->callCount, 1);
    QVERIFY(!note->body.has_value());

    // An unknown notification is ignored the same way.
    const auto unknown = call(d, R"({"jsonrpc":"2.0","method":"notifications/whatever"})");
    QVERIFY(unknown->called && !unknown->body.has_value());

    // Unknown method with an id is a JSON-RPC error.
    const auto bad = call(d, rpc("does/not/exist", {}));
    QCOMPARE(toObject(*bad->body).value("error").toObject().value("code").toInt(), -32601);
}

void TestMcp::dispatcherToolsListTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher d{reg, "MMapper", "1"};

    // No initialize needed first.
    const auto r = call(d, rpc("tools/list", {}));
    const auto tools = toObject(*r->body).value("result").toObject().value("tools").toArray();
    QCOMPARE(tools.size(), 3);
    const auto first = tools[0].toObject();
    QCOMPARE(first.value("name").toString(), QString("echo"));
    QCOMPARE(first.value("description").toString(), QString("Echoes the text argument."));
    const auto schema = first.value("inputSchema").toObject();
    QCOMPARE(schema.value("type").toString(), QString("object"));
    QVERIFY(schema.value("properties").toObject().contains("text"));
    QCOMPARE(schema.value("required").toArray().at(0).toString(), QString("text"));
}

void TestMcp::dispatcherToolsCallTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher d{reg, "MMapper", "1"};

    const auto r = call(d,
                        rpc("tools/call",
                            {{"name", "echo"}, {"arguments", QJsonObject{{"text", "hi"}}}}));
    QVERIFY(r->body);
    const auto result = toObject(*r->body).value("result").toObject();
    QCOMPARE(result.value("isError").toBool(true), false);
    const auto content = result.value("content").toArray();
    QCOMPARE(content.size(), 1);
    QCOMPARE(content[0].toObject().value("type").toString(), QString("text"));
    QCOMPARE(content[0].toObject().value("text").toString(), QString("hi"));
    QCOMPARE(result.value("structuredContent").toObject().value("echoed").toString(), QString("hi"));

    // "arguments" may be left out entirely.
    const auto noArgs = call(d, rpc("tools/call", {{"name", "fail"}}));
    QVERIFY(noArgs->body);
}

void TestMcp::dispatcherToolErrorsTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher d{reg, "MMapper", "1"};

    // A tool that ran and failed is a normal result with isError.
    const auto failed = call(d, rpc("tools/call", {{"name", "fail"}}));
    const auto failedResult = toObject(*failed->body).value("result").toObject();
    QVERIFY(failedResult.value("isError").toBool());
    QCOMPARE(failedResult.value("content").toArray().at(0).toObject().value("text").toString(),
             QString("it failed"));
    QVERIFY(!toObject(*failed->body).contains("error"));

    // An exception in a tool is also reported as isError, not as a crash.
    const auto thrown = call(d, rpc("tools/call", {{"name", "throws"}}));
    const auto thrownResult = toObject(*thrown->body).value("result").toObject();
    QVERIFY(thrownResult.value("isError").toBool());
    QVERIFY(
        thrownResult.value("content").toArray().at(0).toObject().value("text").toString().contains(
            "boom"));

    // Unknown tool and malformed parameters are protocol errors (-32602).
    const auto unknown = call(d, rpc("tools/call", {{"name", "nope"}}));
    QCOMPARE(toObject(*unknown->body).value("error").toObject().value("code").toInt(), -32602);
    QVERIFY(toObject(*unknown->body)
                .value("error")
                .toObject()
                .value("message")
                .toString()
                .contains("nope"));

    const auto noName = call(d, rpc("tools/call", {}));
    QCOMPARE(toObject(*noName->body).value("error").toObject().value("code").toInt(), -32602);

    const auto badArgs = call(d, rpc("tools/call", {{"name", "echo"}, {"arguments", "string"}}));
    QCOMPARE(toObject(*badArgs->body).value("error").toObject().value("code").toInt(), -32602);
}

void TestMcp::dispatcherDeferredReplyTest()
{
    // A tool that answers later, the way a "wait for an event" tool will.
    McpToolRegistry reg;
    McpReply pending;
    reg.add({"later",
             "Replies when told to.",
             mcpObjectSchema(),
             [&pending](const QJsonObject &, McpReply reply) { pending = std::move(reply); }});
    const McpDispatcher d{reg, "MMapper", "1"};

    const auto r = call(d, rpc("tools/call", {{"name", "later"}}, 42));
    QVERIFY(!r->called); // nothing yet

    pending(McpToolResult::ok("done"));
    QVERIFY(r->called);
    QCOMPARE(r->callCount, 1);
    QCOMPARE(toObject(*r->body).value("id").toInt(), 42);

    // A second reply from a misbehaving tool must not respond twice.
    pending(McpToolResult::ok("again"));
    QCOMPARE(r->callCount, 1);
}

void TestMcp::dispatcherMalformedInputTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher d{reg, "MMapper", "1"};

    auto errorCode = [&d](const QByteArray &body) {
        const auto r = call(d, body);
        return r->body ? toObject(*r->body).value("error").toObject().value("code").toInt(0) : 0;
    };

    QCOMPARE(errorCode("{not json"), -32700);
    QCOMPARE(errorCode(""), -32700);
    // Batches are not allowed by MCP's HTTP transport.
    QCOMPARE(errorCode(R"([{"jsonrpc":"2.0","id":1,"method":"ping"}])"), -32600);
    // Qt's parser rejects top-level scalars outright, so these report a parse error.
    QCOMPARE(errorCode("42"), -32700);
    QCOMPARE(errorCode(R"({"id":1,"method":"ping"})"), -32600); // missing jsonrpc
    QCOMPARE(errorCode(R"({"jsonrpc":"1.0","id":1,"method":"ping"})"), -32600);
    QCOMPARE(errorCode(R"({"jsonrpc":"2.0","id":{"a":1},"method":"ping"})"), -32600);
    QCOMPARE(errorCode(R"({"jsonrpc":"2.0","id":1})"), -32600); // no method

    // String ids are valid and are echoed back unchanged.
    const auto r = call(d, R"({"jsonrpc":"2.0","id":"abc","method":"ping"})");
    QCOMPARE(toObject(*r->body).value("id").toString(), QString("abc"));
}

QTEST_MAIN(TestMcp)
