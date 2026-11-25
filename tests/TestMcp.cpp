// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestMcp.h"

#include "../src/mcp/McpAccess.h"
#include "../src/mcp/McpDispatcher.h"
#include "../src/mcp/McpEventLog.h"
#include "../src/mcp/McpHttp.h"
#include "../src/mcp/McpServer.h"
#include "../src/mcp/McpToolRegistry.h"
#include "../src/mcp/McpTools.h"
#include "../src/mcp/McpWaiters.h"

#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
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

// ---- tool helpers ----

struct NODISCARD ToolReply final
{
    bool done = false;
    bool isError = false;
    QString text;
    QJsonObject data;
};

// Runs a tool and waits (with the event loop running) until it has answered.
ToolReply callTool(const McpToolRegistry &reg,
                   const QString &name,
                   const QJsonObject &args,
                   const int waitMs = 2000)
{
    const McpTool *const tool = reg.find(name);
    ToolReply out;
    if (tool == nullptr) {
        return out;
    }
    auto reply = std::make_shared<ToolReply>();
    tool->handler(args, [reply](McpToolResult r) {
        reply->done = true;
        reply->isError = r.isError;
        reply->text = r.text;
        reply->data = r.structured ? r.structured->toObject() : QJsonObject{};
    });
    QTest::qWaitFor([&reply]() { return reply->done; }, waitMs);
    return *reply;
}

McpEvent makeEvent(McpEventLog &log, const QString &kind, const QString &text, QJsonObject data = {})
{
    const uint64_t seq = log.append(kind, text, data);
    return McpEvent{seq, kind, text, std::move(data)};
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

// ---- server helpers ----

struct HttpReply final
{
    int status = 0;
    QByteArray head;
    QByteArray body;

    NODISCARD QByteArray header(const QByteArray &lowerCaseName) const
    {
        for (const QByteArray &line : head.split('\n')) {
            const auto colon = line.indexOf(':');
            if (colon > 0 && line.left(colon).toLower() == lowerCaseName) {
                return line.mid(colon + 1).trimmed();
            }
        }
        return {};
    }
};

// Pulls every complete HTTP response out of `buffer`.
std::vector<HttpReply> takeReplies(QByteArray &buffer)
{
    std::vector<HttpReply> replies;
    for (;;) {
        const auto headEnd = buffer.indexOf("\r\n\r\n");
        if (headEnd < 0) {
            break;
        }
        HttpReply reply;
        reply.head = buffer.left(headEnd);
        reply.status = reply.head.mid(9, 3).toInt(); // "HTTP/1.1 200 OK"
        const qsizetype length = reply.header("content-length").toInt();
        if (buffer.size() < headEnd + 4 + length) {
            break;
        }
        reply.body = buffer.mid(headEnd + 4, length);
        buffer.remove(0, headEnd + 4 + length);
        replies.push_back(std::move(reply));
    }
    return replies;
}

// A client that talks to a McpServer living on the same thread, so every wait spins the
// event loop instead of blocking.
class TestClient final
{
public:
    QTcpSocket socket;
    QByteArray buffer;
    std::vector<HttpReply> replies;

    bool connectTo(const uint16_t port, const int timeoutMs = 2000)
    {
        socket.connectToHost(QHostAddress::LocalHost, port);
        return QTest::qWaitFor([this]() { return socket.state() == QAbstractSocket::ConnectedState; },
                               timeoutMs);
    }

    void send(const QByteArray &bytes) { socket.write(bytes); }

    // Waits until `count` replies have arrived in total.
    bool waitForReplies(const size_t count, const int timeoutMs = 3000)
    {
        return QTest::qWaitFor(
            [this, count]() {
                buffer += socket.readAll();
                for (auto &r : takeReplies(buffer)) {
                    replies.push_back(std::move(r));
                }
                return replies.size() >= count;
            },
            timeoutMs);
    }

    NODISCARD bool waitForClose(const int timeoutMs = 3000)
    {
        return QTest::qWaitFor(
            [this]() {
                std::ignore = socket.readAll();
                return socket.state() == QAbstractSocket::UnconnectedState;
            },
            timeoutMs);
    }
};

QByteArray postTo(const uint16_t port,
                  const QByteArray &body,
                  const QByteArray &token = "secret",
                  const QByteArray &extra = {})
{
    QByteArray req = "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:"
                     + QByteArray::number(static_cast<int>(port))
                     + "\r\nContent-Type: application/json\r\n";
    if (!token.isEmpty()) {
        req += "Authorization: Bearer " + token + "\r\n";
    }
    req += extra + "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    return req;
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

    // The payload counts towards the limit, and trimming stays consistent with appending.
    McpEventLog withData(1000, /*maxBytes*/ 8000);
    const QJsonObject big{{"json", QString(1000, 'x')}};
    for (int i = 0; i < 50; ++i) {
        std::ignore = withData.append("gmcp", "Char.Vitals", big);
    }
    QVERIFY(withData.size() < 50);
    QVERIFY(withData.size() >= 1);
    QCOMPARE(withData.readSince(0).events.back().seq, uint64_t{50});
    // Plain text events fit back in once the large ones have been trimmed away.
    for (int i = 0; i < 100; ++i) {
        std::ignore = withData.append("user_text", "short");
    }
    QCOMPARE(withData.readSince(0).events.back().seq, uint64_t{150});
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

void TestMcp::eventLogOnAppendTest()
{
    McpEventLog log;
    std::vector<uint64_t> seen;
    log.setOnAppend([&seen](const McpEvent &e) { seen.push_back(e.seq); });
    fill(log, 3);
    QCOMPARE(seen, (std::vector<uint64_t>{1, 2, 3}));

    // By the time the hook runs, the event can already be read back.
    McpEventLog other;
    bool readable = false;
    other.setOnAppend([&other, &readable](const McpEvent &e) {
        readable = other.readSince(e.seq - 1).events.size() == 1;
    });
    fill(other, 1);
    QVERIFY(readable);
}

// =============================== McpWaiters ===============================

void TestMcp::waitersMatchTest()
{
    McpWaiters waiters;
    std::vector<McpWaiters::Outcome> outcomes;
    const auto id = waiters.add([](const McpEvent &e) { return e.kind == "prompt"; },
                                5000,
                                [&outcomes](const McpWaiters::Outcome &o) {
                                    outcomes.push_back(o);
                                });
    QVERIFY(id != 0);
    QCOMPARE(waiters.pending(), size_t{1});

    McpEvent text;
    text.seq = 1;
    text.kind = "mud_text";
    waiters.onEvent(text);
    QVERIFY(outcomes.empty()); // not the one we wanted

    McpEvent prompt;
    prompt.seq = 2;
    prompt.kind = "prompt";
    waiters.onEvent(prompt);
    QCOMPARE(outcomes.size(), size_t{1});
    QVERIFY(outcomes[0].kind == McpWaiters::Outcome::KindEnum::Matched);
    QCOMPARE(outcomes[0].event->seq, uint64_t{2});
    QCOMPARE(waiters.pending(), size_t{0});

    // A waiter finishes exactly once, however many matching events follow.
    waiters.onEvent(prompt);
    QCOMPARE(outcomes.size(), size_t{1});
    QVERIFY(!waiters.cancel(id, "late"));
    QCOMPARE(outcomes.size(), size_t{1});
}

void TestMcp::waitersTimeoutTest()
{
    McpWaiters waiters;
    std::vector<McpWaiters::Outcome> outcomes;
    std::ignore = waiters.add([](const McpEvent &) { return false; },
                              50,
                              [&outcomes](const McpWaiters::Outcome &o) { outcomes.push_back(o); });
    QVERIFY(QTest::qWaitFor([&outcomes]() { return !outcomes.empty(); }, 2000));
    QCOMPARE(outcomes.size(), size_t{1});
    QVERIFY(outcomes[0].kind == McpWaiters::Outcome::KindEnum::TimedOut);
    QCOMPARE(waiters.pending(), size_t{0});

    // The timeout must not fire again, and a match after it must not call back again.
    QTest::qWait(100);
    McpEvent e;
    e.seq = 1;
    waiters.onEvent(e);
    QCOMPARE(outcomes.size(), size_t{1});
}

void TestMcp::waitersCancelTest()
{
    McpWaiters waiters;
    std::vector<McpWaiters::Outcome> outcomes;
    auto record = [&outcomes](const McpWaiters::Outcome &o) { outcomes.push_back(o); };
    const auto a = waiters.add([](const McpEvent &) { return false; }, 5000, record);
    std::ignore = waiters.add([](const McpEvent &) { return false; }, 5000, record);
    std::ignore = waiters.add([](const McpEvent &) { return false; }, 5000, record);

    QVERIFY(waiters.cancel(a, "the session closed"));
    QCOMPARE(outcomes.size(), size_t{1});
    QVERIFY(outcomes[0].kind == McpWaiters::Outcome::KindEnum::Cancelled);
    QCOMPARE(outcomes[0].reason, QString("the session closed"));

    waiters.cancelAll("shutting down");
    QCOMPARE(outcomes.size(), size_t{3});
    QCOMPARE(outcomes[2].reason, QString("shutting down"));
    QCOMPARE(waiters.pending(), size_t{0});
}

void TestMcp::waitersReentrancyTest()
{
    // A callback may add another waiter or cancel one; neither may corrupt the list.
    McpWaiters waiters;
    int firstDone = 0;
    int secondDone = 0;
    int thirdDone = 0;
    uint64_t third = 0;

    third = waiters.add([](const McpEvent &) { return false; },
                        5000,
                        [&thirdDone](const McpWaiters::Outcome &) { ++thirdDone; });
    std::ignore = waiters.add([](const McpEvent &) { return true; },
                              5000,
                              [&](const McpWaiters::Outcome &) {
                                  ++firstDone;
                                  std::ignore = waiters.cancel(third, "from a callback");
                                  std::ignore = waiters.add([](const McpEvent &) { return true; },
                                                            5000,
                                                            [&secondDone](
                                                                const McpWaiters::Outcome &) {
                                                                ++secondDone;
                                                            });
                              });

    McpEvent e;
    e.seq = 1;
    waiters.onEvent(e);
    QCOMPARE(firstDone, 1);
    QCOMPARE(thirdDone, 1);  // cancelled from inside the first callback
    QCOMPARE(secondDone, 0); // added during the event, so it did not see that event
    QCOMPARE(waiters.pending(), size_t{1});

    e.seq = 2;
    waiters.onEvent(e);
    QCOMPARE(secondDone, 1);
    QCOMPARE(waiters.pending(), size_t{0});
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
    QVERIFY(throwsInvalidArgument(
        [&]() { reg.add(McpTool{"no_description", "  ", objectSchema, handler}); }));
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

// ================================ McpAccess ================================

void TestMcp::accessTokenCompareTest()
{
    QVERIFY(mcpTokensEqual("abc", "abc"));
    QVERIFY(!mcpTokensEqual("abc", "abd"));
    QVERIFY(!mcpTokensEqual("abc", "ab"));
    QVERIFY(!mcpTokensEqual("abc", "abcd"));
    QVERIFY(!mcpTokensEqual("abc", ""));
    QVERIFY(mcpTokensEqual("", "")); // callers must separately refuse an empty token
}

void TestMcp::accessCheckTest()
{
    const McpAccessPolicy policy{"secret", 4280};

    auto make = [](const QByteArray &method = "POST",
                   const QByteArray &target = "/mcp",
                   std::map<QByteArray, QByteArray> headers = {}) {
        McpHttpRequest r;
        r.method = method;
        r.target = target;
        r.headers = {{"host", "127.0.0.1:4280"}, {"authorization", "Bearer secret"}};
        for (auto &[k, v] : headers) {
            if (v.isEmpty()) {
                r.headers.erase(k);
            } else {
                r.headers[k] = v;
            }
        }
        return r;
    };
    auto statusOf = [&policy](const McpHttpRequest &r) {
        const auto err = mcpCheckRequest(r, policy);
        return err ? err->status : 0;
    };

    QCOMPARE(statusOf(make()), 0);

    // Transport-level problems.
    QCOMPARE(statusOf(make("POST", "/other")), 404);
    QCOMPARE(statusOf(make("GET")), 405);
    QCOMPARE(statusOf(make("DELETE")), 405);
    QCOMPARE(mcpCheckRequest(make("GET"), policy)->headers.at("Allow"), QByteArray("POST"));

    // Token: required, exact, case-insensitive scheme.
    QCOMPARE(statusOf(make("POST", "/mcp", {{"authorization", ""}})), 401);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"authorization", "Bearer wrong"}})), 401);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"authorization", "Basic secret"}})), 401);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"authorization", "Bearer "}})), 401);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"authorization", "bearer secret"}})), 0);
    QCOMPARE(mcpCheckRequest(make("POST", "/mcp", {{"authorization", ""}}), policy)
                 ->headers.at("WWW-Authenticate"),
             QByteArray("Bearer"));
    QCOMPARE(mcpCheckRequest(make(), McpAccessPolicy{"", 4280}).has_value(), true); // no token set

    // Host: must be loopback and our port (blocks DNS rebinding).
    for (const char *host : {"127.0.0.1:4280", "localhost:4280", "[::1]:4280", "LOCALHOST:4280"}) {
        QCOMPARE(statusOf(make("POST", "/mcp", {{"host", host}})), 0);
    }
    for (const char *host : {"evil.com",
                             "evil.com:4280",
                             "127.0.0.1:9999",
                             "127.0.0.1",
                             "localhost.evil.com:4280",
                             "a@127.0.0.1:4280",
                             ""}) {
        QCOMPARE(statusOf(make("POST", "/mcp", {{"host", host}})), 403);
    }

    // Origin: absent is fine; present must be a loopback web origin.
    QCOMPARE(statusOf(make("POST", "/mcp", {{"origin", "http://localhost:4280"}})), 0);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"origin", "http://127.0.0.1:3000"}})), 0);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"origin", "https://evil.com"}})), 403);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"origin", "null"}})), 403);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"origin", "http://localhost.evil.com"}})), 403);

    // Protocol version: missing is fine, known is fine, unknown is a 400.
    QCOMPARE(statusOf(make("POST", "/mcp", {{"mcp-protocol-version", "2025-06-18"}})), 0);
    QCOMPARE(statusOf(make("POST", "/mcp", {{"mcp-protocol-version", "1999-01-01"}})), 400);

    // Accept is never used to reject.
    QCOMPARE(statusOf(make("POST", "/mcp", {{"accept", "text/html"}})), 0);
}

void TestMcp::connectGuardTest()
{
    // OFFLINE never connects anywhere, so any host is fine.
    QVERIFY(mcpConnectRefusal(true, "mume.org", 4242).isEmpty());
    // Otherwise only this computer is allowed.
    for (const char *host : {"127.0.0.1", "localhost", "LOCALHOST", "::1", "127.1.2.3"}) {
        QVERIFY2(mcpConnectRefusal(false, host, 4242).isEmpty(), host);
    }
    for (const char *host : {"mume.org", "192.168.1.5", "10.0.0.1", "localhost.evil.com", ""}) {
        const QString why = mcpConnectRefusal(false, host, 4242);
        QVERIFY2(!why.isEmpty(), host);
        QVERIFY(why.contains("OFFLINE"));
    }
    QVERIFY(mcpConnectRefusal(false, "mume.org", 4242).contains("mume.org:4242"));

    // The command that connects, however it is abbreviated, but nothing else.
    QVERIFY(mcpIsConnectCommand("_connect", '_'));
    QVERIFY(mcpIsConnectCommand("_conn", '_'));
    QVERIFY(mcpIsConnectCommand("  _CONNECT now", '_'));
    QVERIFY(mcpIsConnectCommand("/connect", '/')); // the prefix is configurable
    QVERIFY(!mcpIsConnectCommand("_con", '_'));    // "_config" starts this way; four letters needed
    QVERIFY(!mcpIsConnectCommand("_config", '_'));
    QVERIFY(!mcpIsConnectCommand("_disconnect", '_'));
    QVERIFY(!mcpIsConnectCommand("connect", '_')); // no prefix: plain game input
    QVERIFY(!mcpIsConnectCommand("_connectx", '_'));
    QVERIFY(!mcpIsConnectCommand("", '_'));
}

// ================================ McpServer ================================

void TestMcp::serverRequestTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher dispatcher{reg, "MMapper", "1"};
    McpServer server{dispatcher, nullptr};
    QVERIFY(server.listen(0, "secret").isEmpty());
    QVERIFY(server.isListening());
    QVERIFY(server.port() != 0);

    TestClient c;
    QVERIFY(c.connectTo(server.port()));

    // A normal request.
    c.send(postTo(server.port(), rpc("tools/list", {})));
    QVERIFY(c.waitForReplies(1));
    QCOMPARE(c.replies[0].status, 200);
    QCOMPARE(c.replies[0].header("content-type"), QByteArray("application/json"));
    QCOMPARE(toObject(c.replies[0].body).value("result").toObject().value("tools").toArray().size(),
             3);

    // The same connection is reused (keep-alive); a notification gets 202 and no body.
    c.send(postTo(server.port(), R"({"jsonrpc":"2.0","method":"notifications/initialized"})"));
    QVERIFY(c.waitForReplies(2));
    QCOMPARE(c.replies[1].status, 202);
    QVERIFY(c.replies[1].body.isEmpty());

    // A JSON-RPC error is still HTTP 200.
    c.send(postTo(server.port(), rpc("no/such/method", {})));
    QVERIFY(c.waitForReplies(3));
    QCOMPARE(c.replies[2].status, 200);
    QVERIFY(toObject(c.replies[2].body).contains("error"));

    // Connection: close is honored.
    c.send(postTo(server.port(), rpc("ping", {}), "secret", "Connection: close\r\n"));
    QVERIFY(c.waitForReplies(4));
    QVERIFY(c.waitForClose());

    server.close();
    QVERIFY(!server.isListening());
    TestClient after;
    QVERIFY(!after.connectTo(server.port(), 300)); // nothing is listening any more
}

void TestMcp::serverRejectsBadRequestsTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher dispatcher{reg, "MMapper", "1"};
    McpServer server{dispatcher, nullptr};
    QVERIFY(server.listen(0, "secret").isEmpty());
    const uint16_t port = server.port();

    auto statusFor = [port](const QByteArray &request) {
        TestClient c;
        if (!c.connectTo(port)) {
            return -1;
        }
        c.send(request);
        return c.waitForReplies(1) ? c.replies[0].status : -2;
    };

    const QByteArray body = rpc("ping", {});
    QCOMPARE(statusFor(postTo(port, body, "")), 401);
    QCOMPARE(statusFor(postTo(port, body, "wrong")), 401);
    QCOMPARE(statusFor(postTo(port, body, "secret", "Origin: https://evil.com\r\n")), 403);
    QCOMPARE(statusFor("POST /mcp HTTP/1.1\r\nHost: evil.com\r\nAuthorization: Bearer secret\r\n"
                       "Content-Length: 0\r\n\r\n"),
             403);
    QCOMPARE(statusFor("GET /mcp HTTP/1.1\r\nHost: 127.0.0.1:"
                       + QByteArray::number(static_cast<int>(port))
                       + "\r\nAuthorization: Bearer secret\r\n\r\n"),
             405);
    QCOMPARE(statusFor("POST /elsewhere HTTP/1.1\r\nHost: 127.0.0.1:"
                       + QByteArray::number(static_cast<int>(port))
                       + "\r\nContent-Length: 0\r\n\r\n"),
             404);
    QCOMPARE(statusFor(
                 "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nTransfer-Encoding: chunked\r\n\r\n"),
             501);

    // After a framing error the server hangs up.
    TestClient broken;
    QVERIFY(broken.connectTo(port));
    broken.send("garbage\r\n\r\n");
    QVERIFY(broken.waitForReplies(1));
    QCOMPARE(broken.replies[0].status, 400);
    QVERIFY(broken.waitForClose());
}

void TestMcp::serverPipeliningAndDeferredReplyTest()
{
    // "slow" replies after a delay; "fast" replies at once. Replies must still come back in
    // request order.
    McpToolRegistry reg;
    reg.add({"slow",
             "Replies after a delay.",
             mcpObjectSchema(),
             [](const QJsonObject &, McpReply reply) {
                 QTimer::singleShot(150, [reply]() { reply(McpToolResult::ok("slow")); });
             }});
    reg.addSync("fast", "Replies at once.", mcpObjectSchema(), [](const QJsonObject &) {
        return McpToolResult::ok("fast");
    });
    const McpDispatcher dispatcher{reg, "MMapper", "1"};
    McpServer server{dispatcher, nullptr};
    QVERIFY(server.listen(0, "secret").isEmpty());

    TestClient c;
    QVERIFY(c.connectTo(server.port()));
    // Both requests in one write.
    c.send(postTo(server.port(), rpc("tools/call", {{"name", "slow"}}, 1))
           + postTo(server.port(), rpc("tools/call", {{"name", "fast"}}, 2)));
    QVERIFY(c.waitForReplies(2));
    QCOMPARE(toObject(c.replies[0].body).value("id").toInt(), 1);
    QCOMPARE(toObject(c.replies[1].body).value("id").toInt(), 2);

    // The server going away while a reply is pending must not crash on the late reply.
    TestClient d;
    QVERIFY(d.connectTo(server.port()));
    d.send(postTo(server.port(), rpc("tools/call", {{"name", "slow"}}, 3)));
    QTest::qWait(30);
    server.close();
    QTest::qWait(250); // the timer fires into a reply closure whose connection is gone
    QVERIFY(true);
}

void TestMcp::serverReplyTimeoutTest()
{
    McpToolRegistry reg;
    McpReply stash;
    reg.add({"never",
             "Never replies.",
             mcpObjectSchema(),
             [&stash](const QJsonObject &, McpReply reply) { stash = std::move(reply); }});
    const McpDispatcher dispatcher{reg, "MMapper", "1"};
    McpServer::Options options;
    options.replyTimeoutMs = 100;
    McpServer server{dispatcher, nullptr, options};
    QVERIFY(server.listen(0, "secret").isEmpty());

    TestClient c;
    QVERIFY(c.connectTo(server.port()));
    c.send(postTo(server.port(), rpc("tools/call", {{"name", "never"}})));
    QVERIFY(c.waitForReplies(1));
    QCOMPARE(c.replies[0].status, 504);
    QVERIFY(c.waitForClose());

    // A late reply after the timeout is dropped quietly.
    stash(McpToolResult::ok("too late"));
}

void TestMcp::serverLimitsAndListenFailureTest()
{
    const auto reg = makeRegistry();
    const McpDispatcher dispatcher{reg, "MMapper", "1"};

    // A busy port is an error message, not an exception, and leaves nothing listening.
    McpServer first{dispatcher, nullptr};
    QVERIFY(first.listen(0, "secret").isEmpty());
    McpServer second{dispatcher, nullptr};
    QVERIFY(!second.listen(first.port(), "secret").isEmpty());
    QVERIFY(!second.isListening());

    // No token, no server.
    QVERIFY(!second.listen(0, "").isEmpty());
    QVERIFY(!second.isListening());

    // Connections beyond the cap are dropped.
    McpServer::Options options;
    options.maxConnections = 2;
    McpServer capped{dispatcher, nullptr, options};
    QVERIFY(capped.listen(0, "secret").isEmpty());
    TestClient a;
    TestClient b;
    TestClient extra;
    QVERIFY(a.connectTo(capped.port()));
    QVERIFY(b.connectTo(capped.port()));
    QVERIFY(QTest::qWaitFor([&capped]() { return capped.connectionCount() == 2; }));
    std::ignore = extra.connectTo(capped.port());
    QVERIFY(extra.waitForClose());
    QCOMPARE(capped.connectionCount(), size_t{2});

    // Idle connections are closed.
    McpServer::Options quick;
    quick.idleTimeoutMs = 100;
    McpServer idle{dispatcher, nullptr, quick};
    QVERIFY(idle.listen(0, "secret").isEmpty());
    TestClient sleepy;
    QVERIFY(sleepy.connectTo(idle.port()));
    QVERIFY(sleepy.waitForClose());
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

// ============================= tools without MMapper =============================

void TestMcp::readEventsToolTest()
{
    McpEventLog log;
    McpWaiters waiters;
    McpToolRegistry reg;
    mcpRegisterEventTools(reg, log, waiters);

    makeEvent(log, "user_text", "one");
    makeEvent(log, "prompt", "");
    makeEvent(log, "user_text", "two");

    const auto all = callTool(reg, "read_events", {});
    QVERIFY(all.done && !all.isError);
    QCOMPARE(all.data.value("events").toArray().size(), 3);
    QCOMPARE(all.data.value("nextSeq").toInt(), 3);
    QCOMPARE(all.data.value("truncated").toBool(true), false);

    const auto texts = callTool(reg,
                                "read_events",
                                {{"kinds", QJsonArray{"user_text"}}, {"max", 1}});
    QCOMPARE(texts.data.value("events").toArray().size(), 1);

    const auto later = callTool(reg, "read_events", {{"since", 2}});
    QCOMPARE(later.data.value("events").toArray().size(), 1);

    QVERIFY(callTool(reg, "read_events", {{"since", -1}}).isError);
}

void TestMcp::waitForEventLoggedTest()
{
    McpEventLog log;
    McpWaiters waiters;
    log.setOnAppend([&waiters](const McpEvent &e) { waiters.onEvent(e); });
    McpToolRegistry reg;
    mcpRegisterEventTools(reg, log, waiters);

    makeEvent(log, "map_load", "a", {{"success", false}});
    makeEvent(log, "map_load", "b", {{"success", true}});
    makeEvent(log, "user_text", "hello   world");

    // Events that are already logged count, in order, and the answer carries the cursor.
    const auto hit = callTool(reg,
                              "wait_for_event",
                              {{"since", 0},
                               {"kinds", QJsonArray{"map_load"}},
                               {"data", QJsonObject{{"success", true}}}});
    QVERIFY(hit.done && !hit.isError);
    QCOMPARE(hit.data.value("status").toString(), QString("matched"));
    QCOMPARE(hit.data.value("event").toObject().value("text").toString(), QString("b"));
    QCOMPARE(hit.data.value("nextSeq").toInt(), 2);
    QCOMPARE(hit.data.value("truncated").toBool(true), false);

    const auto regex = callTool(reg,
                                "wait_for_event",
                                {{"since", 0}, {"text_regex", "hello\\s+world"}});
    QCOMPARE(regex.data.value("status").toString(), QString("matched"));
    QCOMPARE(regex.data.value("event").toObject().value("kind").toString(), QString("user_text"));

    // Nothing matches: it times out, and says where to continue from.
    const auto miss = callTool(reg,
                               "wait_for_event",
                               {{"since", 0},
                                {"kinds", QJsonArray{"map_load"}},
                                {"data", QJsonObject{{"success", 5}}},
                                {"timeout_ms", 100}});
    QVERIFY(miss.done && !miss.isError);
    QCOMPARE(miss.data.value("status").toString(), QString("timeout"));
    QCOMPARE(miss.data.value("nextSeq").toInt(), 3);
    QCOMPARE(waiters.pending(), size_t{0});
}

void TestMcp::waitForEventNewEventTest()
{
    McpEventLog log;
    McpWaiters waiters;
    log.setOnAppend([&waiters](const McpEvent &e) { waiters.onEvent(e); });
    McpToolRegistry reg;
    mcpRegisterEventTools(reg, log, waiters);

    makeEvent(log, "user_text", "already there");

    // Without `since` only events from now on count, so the one above must not match.
    QTimer::singleShot(30, [&log]() {
        makeEvent(log, "prompt", "");
        makeEvent(log, "user_text", "fresh");
    });
    const auto r = callTool(reg, "wait_for_event", {{"kinds", QJsonArray{"user_text"}}});
    QVERIFY(r.done && !r.isError);
    QCOMPARE(r.data.value("status").toString(), QString("matched"));
    QCOMPARE(r.data.value("event").toObject().value("text").toString(), QString("fresh"));
    QCOMPARE(r.data.value("nextSeq").toInt(), 3);
}

void TestMcp::waitForEventFutureCursorTest()
{
    McpEventLog log;
    McpWaiters waiters;
    log.setOnAppend([&waiters](const McpEvent &e) { waiters.onEvent(e); });
    McpToolRegistry reg;
    mcpRegisterEventTools(reg, log, waiters);

    makeEvent(log, "user_text", "one");

    // A cursor ahead of the log must not match the events that come before it.
    QTimer::singleShot(20, [&log]() { makeEvent(log, "user_text", "two"); });
    const auto r = callTool(reg,
                            "wait_for_event",
                            {{"since", 1000},
                             {"kinds", QJsonArray{"user_text"}},
                             {"timeout_ms", 150}});
    QVERIFY(r.done && !r.isError);
    QCOMPARE(r.data.value("status").toString(), QString("timeout"));
}

void TestMcp::waitForEventValidationTest()
{
    McpEventLog log;
    McpWaiters waiters;
    McpToolRegistry reg;
    mcpRegisterEventTools(reg, log, waiters);

    const auto bad = callTool(reg, "wait_for_event", {{"text_regex", "(["}});
    QVERIFY(bad.done);
    QVERIFY(bad.isError);
    QVERIFY(bad.text.contains("text_regex"));
    QCOMPARE(waiters.pending(), size_t{0});
}

void TestMcp::waitForEventTruncationTest()
{
    McpEventLog log{3, 1024 * 1024}; // keeps only the last three events
    McpWaiters waiters;
    log.setOnAppend([&waiters](const McpEvent &e) { waiters.onEvent(e); });
    McpToolRegistry reg;
    mcpRegisterEventTools(reg, log, waiters);

    for (int i = 0; i < 10; ++i) {
        makeEvent(log, "user_text", QString::number(i));
    }

    // The first events after the cursor are gone: a match is still found, but the caller is told.
    const auto matched = callTool(reg, "wait_for_event", {{"since", 0}});
    QCOMPARE(matched.data.value("status").toString(), QString("matched"));
    QCOMPARE(matched.data.value("truncated").toBool(false), true);

    // Events dropped while waiting are reported when the wait times out.
    McpEventLog log2{3, 1024 * 1024};
    McpWaiters waiters2;
    log2.setOnAppend([&waiters2](const McpEvent &e) { waiters2.onEvent(e); });
    McpToolRegistry reg2;
    mcpRegisterEventTools(reg2, log2, waiters2);
    makeEvent(log2, "user_text", "x");
    QTimer::singleShot(20, [&log2]() {
        for (int i = 0; i < 6; ++i) {
            makeEvent(log2, "other", "y");
        }
    });
    const auto timedOut = callTool(reg2,
                                   "wait_for_event",
                                   {{"since", 1},
                                    {"kinds", QJsonArray{"nothing"}},
                                    {"timeout_ms", 150}});
    QCOMPARE(timedOut.data.value("status").toString(), QString("timeout"));
    QCOMPARE(timedOut.data.value("truncated").toBool(false), true);
    QCOMPARE(timedOut.data.value("nextSeq").toInt(), 7);
}

void TestMcp::toolResultImageTest()
{
    McpToolRegistry reg;
    const QByteArray png = QByteArray::fromHex("89504e470d0a1a0a0102");
    reg.addSync("pic", "Returns an image.", mcpObjectSchema(), [png](const QJsonObject &) {
        return McpToolResult::image("2x2 PNG", png);
    });
    const McpDispatcher d{reg, "MMapper", "1"};

    const auto r = call(d, rpc("tools/call", {{"name", "pic"}}));
    QVERIFY(r->body);
    const auto result = toObject(*r->body).value("result").toObject();
    QCOMPARE(result.value("isError").toBool(true), false);
    const auto content = result.value("content").toArray();
    QCOMPARE(content.size(), 2);
    QCOMPARE(content[0].toObject().value("type").toString(), QString("text"));
    QCOMPARE(content[0].toObject().value("text").toString(), QString("2x2 PNG"));
    QCOMPARE(content[1].toObject().value("type").toString(), QString("image"));
    QCOMPARE(content[1].toObject().value("mimeType").toString(), QString("image/png"));
    QCOMPARE(QByteArray::fromBase64(content[1].toObject().value("data").toString().toLatin1()), png);
    QVERIFY(!result.contains("structuredContent"));
}

QTEST_MAIN(TestMcp)
