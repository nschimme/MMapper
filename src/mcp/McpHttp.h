#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"

#include <cstddef>
#include <map>
#include <optional>
#include <variant>

#include <QByteArray>

// A tiny HTTP/1.1 request parser and response writer, just enough for a JSON-RPC endpoint.
// It never touches a socket: callers feed it bytes and take complete requests out, so partial
// reads and pipelined requests are easy to test.
//
// Deliberately unsupported: chunked request bodies, "Expect: 100-continue", upgrades, and
// anything but Content-Length framing. Those are rejected, not guessed at.

struct NODISCARD McpHttpRequest final
{
    QByteArray method;                        // upper-case, e.g. "POST"
    QByteArray target;                        // request target, e.g. "/mcp"
    std::map<QByteArray, QByteArray> headers; // lower-case names
    QByteArray body;
    bool keepAlive = true;

    NODISCARD QByteArray header(const QByteArray &lowerCaseName) const;
};

// The request was malformed or over a limit. The connection must be closed after replying.
struct NODISCARD McpHttpError final
{
    int status = 400;
    QByteArray reason;
};

class NODISCARD McpHttpParser final
{
public:
    struct NODISCARD Limits final
    {
        size_t maxHeaderBytes = 16 * 1024;
        size_t maxBodyBytes = 1024 * 1024;
    };

    using Result = std::variant<McpHttpRequest, McpHttpError>;

private:
    Limits m_limits;
    QByteArray m_buffer;
    bool m_failed = false;

public:
    McpHttpParser() = default;
    explicit McpHttpParser(const Limits &limits)
        : m_limits{limits}
    {}

    void feed(const QByteArray &bytes) { m_buffer.append(bytes); }

    // Returns the next complete request (or an error), or nullopt if more bytes are needed.
    // After an error the parser is finished and always returns nullopt.
    NODISCARD std::optional<Result> next();

    NODISCARD size_t bufferedBytes() const { return static_cast<size_t>(m_buffer.size()); }
};

NODISCARD QByteArray mcpHttpReasonPhrase(int status);

// Builds a complete response. Content-Length is always set; a 202/204 gets no body.
NODISCARD QByteArray mcpHttpResponse(int status,
                                     const QByteArray &contentType,
                                     const QByteArray &body,
                                     bool keepAlive,
                                     const std::map<QByteArray, QByteArray> &extraHeaders = {});
