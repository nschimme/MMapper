#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "GmcpModule.h"

#include <QByteArray>

class GmcpMessage;
class Proxy;

class NODISCARD ProxyMudConnectionApi final
{
private:
    Proxy &m_proxy;

public:
    explicit ProxyMudConnectionApi(Proxy &proxy)
        : m_proxy{proxy}
    {}

public:
    NODISCARD bool isConnected() const;
    void connectToMud();
    void disconnectFromMud();
};

class NODISCARD ProxyUserGmcpApi final
{
private:
    Proxy &m_proxy;

public:
    explicit ProxyUserGmcpApi(Proxy &proxy)
        : m_proxy{proxy}
    {}

public:
    NODISCARD bool isUserGmcpModuleEnabled(const GmcpModuleTypeEnum &mod) const;
    void gmcpToUser(const GmcpMessage &msg);
};

class NODISCARD ProxyMudGmcpApi final
{
private:
    Proxy &m_proxy;

public:
    explicit ProxyMudGmcpApi(Proxy &proxy)
        : m_proxy{proxy}
    {}

public:
    NODISCARD bool isMudGmcpModuleEnabled(const GmcpModuleTypeEnum &mod) const;
    void gmcpToMud(const GmcpMessage &msg);
};

// Drives a proxy from the outside, at the same points where real input arrives: what the user
// types, and what the MUD sends once its telnet stream has been decoded. For tests and tools.
class NODISCARD ProxyControlApi final
{
private:
    Proxy &m_proxy;

public:
    explicit ProxyControlApi(Proxy &proxy)
        : m_proxy{proxy}
    {}

public:
    // As if the user typed `line` and pressed Enter (so "_" commands and game input both work).
    void sendUserInput(const QByteArray &line);
    // As if the MUD sent these bytes; goAhead marks the end of a prompt.
    void injectMudBytes(const QByteArray &bytes, bool goAhead);
    // As if the MUD sent this GMCP message.
    void injectMudGmcp(const GmcpMessage &msg);
    // Whether the MUD has agreed to use GMCP; until it has, GMCP sent to it is silently dropped.
    NODISCARD bool isMudGmcpNegotiated() const;
};
