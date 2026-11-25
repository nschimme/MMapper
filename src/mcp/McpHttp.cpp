// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpHttp.h"

#include <algorithm>

#include <QList>

QByteArray McpHttpRequest::header(const QByteArray &lowerCaseName) const
{
    const auto it = headers.find(lowerCaseName);
    return it == headers.end() ? QByteArray{} : it->second;
}

namespace {

NODISCARD McpHttpError makeError(const int status, const char *const reason)
{
    return McpHttpError{status, QByteArray{reason}};
}

NODISCARD bool isTokenChar(const char c)
{
    // RFC 9110 "tchar"
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    static constexpr const char extra[] = "!#$%&'*+-.^_`|~";
    return std::find(std::begin(extra), std::end(extra) - 1, c) != std::end(extra) - 1;
}

NODISCARD bool isValidToken(const QByteArray &s)
{
    return !s.isEmpty() && std::all_of(s.begin(), s.end(), isTokenChar);
}

} // namespace

std::optional<McpHttpParser::Result> McpHttpParser::next()
{
    if (m_failed) {
        return std::nullopt;
    }

    auto fail = [this](const int status, const char *const reason) -> Result {
        m_failed = true;
        m_buffer.clear();
        return makeError(status, reason);
    };

    const auto headerEnd = m_buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (static_cast<size_t>(m_buffer.size()) > m_limits.maxHeaderBytes) {
            return fail(431, "request headers too large");
        }
        return std::nullopt;
    }
    if (static_cast<size_t>(headerEnd) > m_limits.maxHeaderBytes) {
        return fail(431, "request headers too large");
    }

    const QByteArray head = m_buffer.left(headerEnd);
    const QList<QByteArray> lines = head.split('\n');

    // Request line
    QByteArray requestLine = lines.front();
    if (requestLine.endsWith('\r')) {
        requestLine.chop(1);
    }
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() != 3 || !isValidToken(parts[0]) || parts[1].isEmpty()) {
        return fail(400, "malformed request line");
    }
    if (parts[2] != "HTTP/1.1" && parts[2] != "HTTP/1.0") {
        return fail(505, "HTTP version not supported");
    }

    McpHttpRequest req;
    req.method = parts[0].toUpper();
    req.target = parts[1];
    req.keepAlive = (parts[2] == "HTTP/1.1");

    // Headers
    for (qsizetype i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i];
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.isEmpty()) {
            continue;
        }
        if (line.front() == ' ' || line.front() == '\t') {
            return fail(400, "folded headers are not supported");
        }
        const auto colon = line.indexOf(':');
        if (colon <= 0) {
            return fail(400, "malformed header");
        }
        const QByteArray name = line.left(colon);
        if (!isValidToken(name)) {
            return fail(400, "malformed header name");
        }
        const QByteArray value = line.mid(colon + 1).trimmed();
        const QByteArray key = name.toLower();
        if (key == "content-length" && req.headers.count(key) != 0) {
            return fail(400, "duplicate Content-Length");
        }
        req.headers[key] = value;
    }

    if (req.headers.count("transfer-encoding") != 0) {
        return fail(501, "chunked or encoded request bodies are not supported");
    }
    if (req.headers.count("expect") != 0) {
        return fail(417, "Expect is not supported");
    }

    const QByteArray connection = req.header("connection").toLower();
    if (connection.contains("close")) {
        req.keepAlive = false;
    } else if (connection.contains("keep-alive")) {
        req.keepAlive = true;
    }

    // Body: framed by Content-Length only.
    size_t bodyLength = 0;
    if (const auto it = req.headers.find("content-length"); it != req.headers.end()) {
        bool ok = false;
        const qulonglong n = it->second.toULongLong(&ok);
        if (!ok || it->second.startsWith('+') || it->second.startsWith('-')) {
            return fail(400, "invalid Content-Length");
        }
        if (n > m_limits.maxBodyBytes) {
            return fail(413, "request body too large");
        }
        bodyLength = static_cast<size_t>(n);
    } else if (req.method == "POST" || req.method == "PUT") {
        return fail(411, "Content-Length required");
    }

    const size_t headSize = static_cast<size_t>(headerEnd) + 4;
    if (static_cast<size_t>(m_buffer.size()) < headSize + bodyLength) {
        return std::nullopt; // need more bytes
    }

    req.body = m_buffer.mid(static_cast<qsizetype>(headSize), static_cast<qsizetype>(bodyLength));
    m_buffer.remove(0, static_cast<qsizetype>(headSize + bodyLength));
    return Result{std::move(req)};
}

QByteArray mcpHttpReasonPhrase(const int status)
{
    switch (status) {
    case 200:
        return "OK";
    case 202:
        return "Accepted";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 411:
        return "Length Required";
    case 413:
        return "Payload Too Large";
    case 415:
        return "Unsupported Media Type";
    case 417:
        return "Expectation Failed";
    case 431:
        return "Request Header Fields Too Large";
    case 500:
        return "Internal Server Error";
    case 501:
        return "Not Implemented";
    case 504:
        return "Gateway Timeout";
    case 505:
        return "HTTP Version Not Supported";
    default:
        return "Unknown";
    }
}

QByteArray mcpHttpResponse(const int status,
                           const QByteArray &contentType,
                           const QByteArray &body,
                           const bool keepAlive,
                           const std::map<QByteArray, QByteArray> &extraHeaders)
{
    const bool bodyAllowed = status != 202 && status != 204;

    QByteArray out;
    out += "HTTP/1.1 " + QByteArray::number(status) + " " + mcpHttpReasonPhrase(status) + "\r\n";
    if (bodyAllowed && !contentType.isEmpty()) {
        out += "Content-Type: " + contentType + "\r\n";
    }
    out += "Content-Length: " + QByteArray::number(bodyAllowed ? body.size() : 0) + "\r\n";
    out += keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    out += "Cache-Control: no-store\r\n";
    for (const auto &[name, value] : extraHeaders) {
        out += name + ": " + value + "\r\n";
    }
    out += "\r\n";
    if (bodyAllowed) {
        out += body;
    }
    return out;
}
