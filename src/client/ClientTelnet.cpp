// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002-2005 by Tomas Mecir - kmuddy@kmuddy.com
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "ClientTelnet.h"

#include "../configuration/configuration.h"
#include "../global/io.h"
#include "../global/utils.h"
#include "../proxy/TextCodec.h"
#include "../proxy/AbstractSocket.h"

#include <limits>
#include <tuple>

#include <QApplication>
#include <QByteArray>
#include <QHostAddress>
#include <QMessageLogContext>
#include <QObject>
#include <QString>

ClientTelnetOutputs::~ClientTelnetOutputs() = default;

ClientTelnet::ClientTelnet(ClientTelnetOutputs &output, std::unique_ptr<AbstractSocket> socket)
    : QObject(nullptr)
    , AbstractTelnet(TextCodecStrategyEnum::FORCE_UTF_8, TelnetTermTypeBytes{"MMapper"})
    , m_output{output}
    , m_socket(std::move(socket))
{
    QObject::connect(m_socket.get(), &AbstractSocket::connected, this, &ClientTelnet::onConnected);
    QObject::connect(m_socket.get(), &AbstractSocket::disconnected, this, &ClientTelnet::onDisconnected);
    QObject::connect(m_socket.get(), &AbstractSocket::readyRead, this, &ClientTelnet::onReadyRead);
    QObject::connect(m_socket.get(), &AbstractSocket::errorOccurred, this, &ClientTelnet::onError);
}

ClientTelnet::~ClientTelnet()
{
    m_socket->disconnectFromHost();
}

void ClientTelnet::connectToHost()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        return;
    }

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_socket->connectToHost(QHostAddress(QHostAddress::LocalHost).toString(), getConfig().connection.localPort);
    m_socket->waitForConnected(3000);
}

void ClientTelnet::onConnected()
{
    reset();
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, true);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
    m_output.connected();
}

void ClientTelnet::disconnectFromHost()
{
    m_socket->disconnectFromHost();
}

void ClientTelnet::onDisconnected()
{
    reset();
    m_output.echoModeChanged(true);
    m_output.disconnected();
}

void ClientTelnet::onError(QAbstractSocket::SocketError error)
{
    if (error == QAbstractSocket::SocketError::RemoteHostClosedError) {
        // The connection closing isn't an error
        return;
    }

    QString err = m_socket->errorString();
    m_socket->abort();
    m_output.socketError(err);
}

void ClientTelnet::sendToMud(const QString &data)
{
    submitOverTelnet(data, false);
}

void ClientTelnet::virt_sendRawData(const TelnetIacBytes &data)
{
    m_socket->write(data.getQByteArray());
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
    const auto data = m_socket->readAll();
    if (!data.isEmpty()) {
        onReadInternal(TelnetIacBytes{data});
    }
}

void ClientTelnet::virt_sendToMapper(const RawBytes &data, bool /*goAhead*/)
{
    // The encoding for the built-in client is always Utf8.
    assert(getEncoding() == CharacterEncodingEnum::UTF8);
    QString out = QString::fromUtf8(data.getQByteArray());

    // Replace BEL character with an application beep
    // REVISIT: This seems like the WRONG place to do this.
    static constexpr const QChar BEL = mmqt::QC_ALERT;
    if (out.contains(BEL)) {
        out.remove(BEL);
        QApplication::beep();
    }

    // REVISIT: Why is virt_sendToMapper() calling sendToUser()? One needs to be renamed?
    m_output.sendToUser(out);
}

void ClientTelnet::virt_receiveEchoMode(const bool mode)
{
    m_output.echoModeChanged(mode);
}
