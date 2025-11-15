// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002-2005 by Tomas Mecir - kmuddy@kmuddy.com
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com>

#include "ClientTelnet.h"

#include "../configuration/configuration.h"
#include "../global/io.h"
#include "../global/utils.h"
#include "../proxy/TextCodec.h"
#include "../proxy/connectionlistener.h"

#include <limits>
#include <tuple>

#include <QApplication>
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
    QObject::connect(&socket, &VirtualSocket::connected, &m_dummy, [this]() { onConnected(); });
    QObject::connect(&socket, &VirtualSocket::disconnected, &m_dummy, [this]() {
        onDisconnected();
    });

    QObject::connect(&socket, &QIODevice::readyRead, &m_dummy, [this]() { onReadyRead(); });
}

ClientTelnet::~ClientTelnet()
{
    m_socket.disconnectFromHost();
}

void ClientTelnet::connectToHost(ConnectionListener *listener)
{
    auto peerSocket = std::make_unique<VirtualSocket>();
    m_socket.connectToPeer(peerSocket.get());
    listener->startClient(std::move(peerSocket));
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

    current.width = width;
    current.height = height;

    if (getOptions().myOptionState[OPT_NAWS]) {
        sendWindowSizeChanged(width, height);
    }
}

void ClientTelnet::onReadyRead()
{
    std::ignore = io::readAllAvailable(m_socket, m_buffer, [this](const QByteArray &byteArray) {
        assert(!byteArray.isEmpty());
        onReadInternal(TelnetIacBytes{byteArray});
    });
}

void ClientTelnet::virt_sendToMapper(const RawBytes &data, bool goAhead)
{
    assert(getEncoding() == CharacterEncodingEnum::UTF8);
    QString out = QString::fromUtf8(data.getQByteArray());

    static constexpr const QChar BEL = mmqt::QC_ALERT;
    if (out.contains(BEL)) {
        out.remove(BEL);
        QApplication::beep();
    }

    m_output.sendToUser(out);
}

void ClientTelnet::virt_receiveEchoMode(const bool mode)
{
    m_output.echoModeChanged(mode);
}
