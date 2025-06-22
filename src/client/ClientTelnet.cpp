// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002-2005 by Tomas Mecir - kmuddy@kmuddy.com
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "ClientTelnet.h"

#include "../configuration/configuration.h"
#include "../global/io.h"
#include "../global/utils.h"
#include "../proxy/TextCodec.h"
#include "../global/Version.h" // For getMMapperVersion() for MTTS

#include <limits>
#include <tuple>

#include <QApplication> // For beep
#include <QDebug> // For qDebug if m_debug is true
#include <QByteArray>
#include <QHostAddress>
#include <QMessageLogContext>
#include <QObject>
#include <QString>

ClientTelnetOutputs::~ClientTelnetOutputs() = default;

ClientTelnet::ClientTelnet(ClientTelnetOutputs &output)
    : AbstractTelnet(TextCodecStrategyEnum::FORCE_UTF_8, TelnetTermTypeBytes{"MMAPPER"}) // Default TTYPE name
    , m_output{output}
{
    // Ensure m_debug is initialized from config
    m_debug = getConfig().debug.telnet;

    auto &socket = m_socket;
    QObject::connect(&socket, &QAbstractSocket::connected, &m_dummy, [this]() { onConnected(); });
    QObject::connect(&socket, &QAbstractSocket::disconnected, &m_dummy, [this]() {
        onDisconnected();
    });

    QObject::connect(&socket, &QIODevice::readyRead, &m_dummy, [this]() { onReadyRead(); });
    QObject::connect(&socket,
                     QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                     &m_dummy,
                     [this](QAbstractSocket::SocketError err) { onError(err); });
}

ClientTelnet::~ClientTelnet()
{
    m_socket.disconnectFromHost();
}

void ClientTelnet::connectToHost()
{
    auto &socket = m_socket;
    if (socket.state() == QAbstractSocket::ConnectedState) {
        return;
    }

    if (socket.state() != QAbstractSocket::UnconnectedState) {
        socket.abort();
    }

    socket.connectToHost(QHostAddress::LocalHost, getConfig().connection.localPort);
    socket.waitForConnected(3000);
}

void ClientTelnet::onConnected()
{
    reset(); // Resets AbstractTelnet state
    m_ttypeSendCount = 0; // Reset TTYPE sequence counter

    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, true);
    m_socket.setSocketOption(QAbstractSocket::KeepAliveOption, true);

    // Announce capabilities
    sendTelnetOption(TN_WILL, OPT_TERMINAL_TYPE);
    sendTelnetOption(TN_WILL, OPT_NAWS);
    sendTelnetOption(TN_WILL, OPT_SUPPRESS_GA);
    sendTelnetOption(TN_WILL, OPT_CHARSET); // We'll offer UTF-8 when asked via SB CHARSET REQUEST
    sendTelnetOption(TN_WONT, OPT_MNES);   // MMapper internal client does not support MNES for now
    // sendTelnetOption(TN_WONT, OPT_ECHO); // We prefer server to echo. If it WONTs, we will.

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

void ClientTelnet::onError(QAbstractSocket::SocketError error)
{
    if (error == QAbstractSocket::RemoteHostClosedError) {
        // The connection closing isn't an error
        return;
    }

    QString err = m_socket.errorString();
    m_socket.abort();
    m_output.socketError(err);
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

// MNES and MTTS handling for ClientTelnet
void ClientTelnet::virt_receiveMnesSubnegotiation(const AppendBuffer &subnegotiation_data)
{
    if (getDebug()) {
        qDebug() << "ClientTelnet: Received MNES subnegotiation from MUD (ignoring):" << subnegotiation_data;
    }
    // MMapper's internal client doesn't process incoming MNES data yet.
}

void ClientTelnet::virt_mnesStateChanged(bool us_will_mnes, bool them_will_mnes)
{
    // us_will_mnes is our WILL state (myOptionState[OPT_MNES])
    // them_will_mnes is MUD's WILL state (hisOptionState[OPT_MNES])
    if (getDebug()) {
        qDebug() << "ClientTelnet: MNES state changed. We WILL MNES:" << us_will_mnes
                 << "MUD WILL MNES:" << them_will_mnes;
    }
    // Since we send WONT MNES on connect, us_will_mnes should be false.
}

void ClientTelnet::virt_receiveMttsReport(const TelnetTermTypeBytes &report_data)
{
    if (getDebug()) {
        qDebug() << "ClientTelnet: Received MTTS Report from MUD (ignoring):" << report_data;
    }
    // MMapper's internal client doesn't parse MUD's MTTS report for its own use yet.
}

void ClientTelnet::virt_receivedTerminalTypeSendRequest()
{
    // MUD sent `IAC SB TTYPE SEND IAC SE`. ClientTelnet needs to respond.
    if (getDebug()) {
        qDebug() << "ClientTelnet: MUD requests TTYPE. Sequence count:" << m_ttypeSendCount;
    }

    TelnetTermTypeBytes response;
    switch (m_ttypeSendCount) {
    case 0:
        response = TelnetTermTypeBytes{"MMAPPER"}; // Client Name
        break;
    case 1:
        response = TelnetTermTypeBytes{"XTERM"};   // Terminal Type
        break;
    case 2: // First MTTS response
    case 3: // Second MTTS response (cycling)
        {
            // Construct MMapper client's MTTS bitvector
            // ANSI (1), VT100 (2), UTF-8 (4), 256 COLORS (8) are common capabilities.
            // OSC COLOR PALETTE (32) if supported.
            // TRUECOLOR (256) if supported.
            // SSL (2048) if connection is SSL/TLS (not tracked by ClientTelnet directly here, assume no for now).
            // MNES (512) - we send WONT MNES, so don't include.
            // MSLP (1024) - not supported.
            // MOUSE TRACKING (16) - not applicable for internal client.
            // SCREEN READER (64) - not applicable.
            // PROXY (128) - Not a proxy in this mode.
            unsigned int mtts_bits = 1 /*ANSI*/ | 2 /*VT100*/ | 4 /*UTF-8*/ | 8 /*256COLORS*/;
            // Add more bits if ClientTelnet supports them, e.g. Truecolor, OSC Palette
            // if (m_supportsTrueColor) mtts_bits |= 256;
            // if (m_supportsOscPalette) mtts_bits |= 32;

            response = TelnetTermTypeBytes{QString("MTTS %1").arg(mtts_bits).toUtf8()};
        }
        break;
    default:
        // Repeat last MTTS report
        unsigned int mtts_bits = 1 | 2 | 4 | 8;
        response = TelnetTermTypeBytes{QString("MTTS %1").arg(mtts_bits).toUtf8()};
        break;
    }

    if (getDebug()) {
        qDebug() << "ClientTelnet: Sending TTYPE IS (count" << m_ttypeSendCount << "):" << response;
    }
    sendTerminalType(response); // AbstractTelnet method to send IAC SB TTYPE IS <response> IAC SE
    m_ttypeSendCount++;
}
