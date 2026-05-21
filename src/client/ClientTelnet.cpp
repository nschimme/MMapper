// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002-2005 by Tomas Mecir - kmuddy@kmuddy.com
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "ClientTelnet.h"

#include "../configuration/configuration.h"
#include "../global/Version.h"
#include "../global/io.h"
#include "../global/utils.h"
#include "../proxy/TextCodec.h"
#include "../proxy/connectionlistener.h"

#include <limits>
#include <tuple>

#include <QByteArray>
#include <QHostAddress>
#include <QMessageLogContext>
#include <QObject>
#include <QString>

ClientTelnetOutputs::~ClientTelnetOutputs() = default;

ClientTelnet::ClientTelnet(ClientTelnetOutputs &output)
    : AbstractTelnet(TextCodecStrategyEnum::FORCE_UTF_8, TelnetTermTypeBytes{"MMapper"})
    , m_output{output}
{
    auto &socket = m_socket;
    QObject::connect(&socket, &VirtualSocket::sig_connected, &m_dummy, [this]() { onConnected(); });
    QObject::connect(&socket, &VirtualSocket::sig_disconnected, &m_dummy, [this]() {
        onDisconnected();
    });

    QObject::connect(&socket, &QIODevice::readyRead, &m_dummy, [this]() { onReadyRead(); });
}

ClientTelnet::~ClientTelnet()
{
    m_socket.disconnectFromHost();
}

void ClientTelnet::connectToHost(ConnectionListener &listener)
{
    auto peerSocket = std::make_unique<VirtualSocket>();
    m_socket.connectToPeer(peerSocket.get());
    listener.startClient(std::move(peerSocket));
    onConnected();
}

void ClientTelnet::onConnected()
{
    reset();
    m_output.connected();
}

void ClientTelnet::disconnectFromHost()
{
    m_socket.disconnectFromHost();
}

void ClientTelnet::onDisconnected()
{
    reset();
    m_output.echoModeChanged(true);
    m_output.disconnected();
}

void ClientTelnet::sendToMud(const QString &data)
{
    submitOverTelnet(data, false);
}

void ClientTelnet::virt_sendRawData(const TelnetIacBytes &data)
{
    m_socket.write(data.getQByteArray());
}

void ClientTelnet::onWindowSizeChanged(const int width, const int height)
{
    auto &current = m_currentNaws;
    if (current.width == width && current.height == height) {
        return;
    }

    // remember the size - we'll need it if NAWS is currently disabled but will
    // be enabled. Also remember it if no connection exists at the moment;
    // we won't be called again when connecting
    current.width = width;
    current.height = height;

    if (getOptions().myOptionState[OPT_NAWS]) {
        // only if we have negotiated this option
        sendWindowSizeChanged(width, height);
    }
}

void ClientTelnet::onReadyRead()
{
    // REVISIT: check return value?
    std::ignore = io::readAllAvailable(m_socket, m_buffer, [this](const QByteArray &byteArray) {
        assert(!byteArray.isEmpty());
        onReadInternal(TelnetIacBytes{byteArray});
    });
}

void ClientTelnet::virt_sendToMapper(const RawBytes &data, bool /*goAhead*/)
{
    // The encoding for the built-in client is always Utf8.
    assert(getEncoding() == CharacterEncodingEnum::UTF8);
    QString out = QString::fromUtf8(data.getQByteArray());

    // REVISIT: Why is virt_sendToMapper() calling sendToUser()? One needs to be renamed?
    m_output.sendToUser(out);
}

void ClientTelnet::virt_receiveEchoMode(const bool mode)
{
    m_output.echoModeChanged(mode);
}

void ClientTelnet::virt_receiveNewEnvironSend(const QList<RawBytes> &vars,
                                              const QList<RawBytes> &userVars)
{
    const bool sendAll = vars.isEmpty() && userVars.isEmpty();

    QMap<RawBytes, RawBytes> myVars;
    QMap<RawBytes, RawBytes> myUserVars;

    bool clientNameRequested = sendAll;
    bool clientVersionRequested = sendAll;
    bool charsetRequested = sendAll;
    bool mttsRequested = sendAll;

    for (const auto &v : vars) {
        if (v == RawBytes("CLIENT_NAME")) {
            clientNameRequested = true;
        } else if (v == RawBytes("CLIENT_VERSION")) {
            clientVersionRequested = true;
        } else if (v == RawBytes("CHARSET")) {
            charsetRequested = true;
        } else if (v == RawBytes("MTTS")) {
            mttsRequested = true;
        }
    }

    if (clientNameRequested) {
        myVars[RawBytes("CLIENT_NAME")] = RawBytes("MMapper");
    }
    if (clientVersionRequested) {
        myVars[RawBytes("CLIENT_VERSION")] = RawBytes(getMMapperVersion());
    }
    if (charsetRequested) {
        myVars[RawBytes("CHARSET")] = RawBytes("UTF-8");
    }
    if (mttsRequested) {
        // bitvector = ANSI(1) | UTF-8(4) | 256 COLORS(8) | MNES(512) = 525
        myVars[RawBytes("MTTS")] = RawBytes("525");
    }

    if (!myVars.isEmpty() || !myUserVars.isEmpty()) {
        sendNewEnvironIs(myVars, myUserVars);
    }
}
