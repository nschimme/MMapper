// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpAccess.h"

#include "McpDispatcher.h"

#include <algorithm>

#include <QString>
#include <QUrl>

namespace {

NODISCARD McpHttpError makeError(const int status,
                                 const char *const reason,
                                 std::map<QByteArray, QByteArray> headers = {})
{
    return McpHttpError{status, QByteArray{reason}, std::move(headers)};
}

NODISCARD bool isLoopbackHostName(const QString &host)
{
    const QString h = host.toLower();
    return h == QStringLiteral("localhost") || h == QStringLiteral("127.0.0.1")
           || h == QStringLiteral("::1");
}

// "host", "host:port", "[::1]" or "[::1]:port"
NODISCARD bool isLoopbackHostHeader(const QByteArray &value, const uint16_t port)
{
    if (value.isEmpty()) {
        return false;
    }
    // QUrl does the bracket and port handling for us.
    const QUrl url{QStringLiteral("//") + QString::fromLatin1(value)};
    if (!url.isValid() || url.host().isEmpty() || !url.userName().isEmpty()
        || !url.path().isEmpty()) {
        return false;
    }
    const QString host = url.host(); // brackets removed
    if (!isLoopbackHostName(host)) {
        return false;
    }
    // A Host header without a port means port 80, which is never ours.
    return url.port(80) == static_cast<int>(port);
}

// An Origin looks like "http://localhost:4280"; "null" and everything else is rejected.
NODISCARD bool isLoopbackOrigin(const QByteArray &value)
{
    const QUrl url{QString::fromLatin1(value)};
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        return false;
    }
    return isLoopbackHostName(url.host());
}

} // namespace

bool mcpTokensEqual(const QByteArray &expected, const QByteArray &given)
{
    // Look at every byte of the longer string and fold the length difference in, so neither the
    // position of the first mismatch nor the length is revealed through timing.
    const auto longest = std::max(expected.size(), given.size());
    unsigned char diff = (expected.size() == given.size()) ? 0 : 1;
    for (qsizetype i = 0; i < longest; ++i) {
        const auto a = static_cast<unsigned char>(i < expected.size() ? expected[i] : 0);
        const auto b = static_cast<unsigned char>(i < given.size() ? given[i] : 0);
        diff = static_cast<unsigned char>(diff | (a ^ b));
    }
    return diff == 0;
}

std::optional<McpHttpError> mcpCheckRequest(const McpHttpRequest &request,
                                            const McpAccessPolicy &policy)
{
    if (request.target != "/mcp") {
        return makeError(404, "not found");
    }
    if (request.method != "POST") {
        // GET would open a server-to-client stream, which this server does not offer.
        return makeError(405, "only POST is supported", {{"Allow", "POST"}});
    }

    if (!isLoopbackHostHeader(request.header("host"), policy.port)) {
        return makeError(403, "invalid Host header");
    }
    if (const QByteArray origin = request.header("origin");
        !origin.isEmpty() && !isLoopbackOrigin(origin)) {
        return makeError(403, "invalid Origin header");
    }

    // "Bearer <token>"; the scheme name is case-insensitive.
    const QByteArray auth = request.header("authorization");
    static const QByteArray scheme = "bearer ";
    const bool hasBearer = auth.size() > scheme.size()
                           && auth.left(scheme.size()).toLower() == scheme;
    if (policy.token.isEmpty() || !hasBearer
        || !mcpTokensEqual(policy.token, auth.mid(scheme.size()).trimmed())) {
        return makeError(401, "missing or invalid bearer token", {{"WWW-Authenticate", "Bearer"}});
    }

    // A missing header is fine (older clients); one we don't speak is not.
    if (const QByteArray version = request.header("mcp-protocol-version");
        !version.isEmpty()
        && !McpDispatcher::isSupportedProtocolVersion(QString::fromLatin1(version))) {
        return makeError(400, "unsupported MCP-Protocol-Version");
    }

    return std::nullopt;
}
