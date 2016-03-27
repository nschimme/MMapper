/************************************************************************
**
** Authors:   Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve),
**            Marek Krejza <krejza@gmail.com> (Caligor),
**            Nils Schimmelmann <nschimme@gmail.com> (Jahara)
**
** This file is part of the MMapper project. 
** Maintained by Nils Schimmelmann <nschimme@gmail.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the:
** Free Software Foundation, Inc.
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
************************************************************************/

#include "connectionlistener.h"
#include "proxy.h"
#include "configuration.h"
#include "websocketproxy.h"
#include "tcpsocketproxy.h"
#include <QWebSocket>
#include <QTcpSocket>

ConnectionListener::ConnectionListener(MapData* md, Mmapper2PathMachine* pm, CommandEvaluator* ce, PrespammedPath* pp, CGroup* gm, QObject *parent)
  : QTcpServer(parent)
{
  m_accept = true;

  m_mapData = md;
  m_pathMachine = pm;
  m_commandEvaluator = ce;
  m_prespammedPath = pp;
  m_groupManager = gm;

  connect(this, SIGNAL(log(const QString&, const QString&)), parent, SLOT(log(const QString&, const QString&)));
}

ConnectionListener::~ConnectionListener() {
    disconnect();
    deleteLater();
    m_webSocketServer->disconnect();
    m_webSocketServer->deleteLater();
}

bool ConnectionListener::start() {
    setMaxPendingConnections (1);
    if (!listen(QHostAddress::Any, Config().m_localPort)) {
        return false;
    }

    m_webSocketServer = new QWebSocketServer("MMapper WebSocket", QWebSocketServer::NonSecureMode, this);
    m_webSocketServer->setMaxPendingConnections(1);
    if (!m_webSocketServer->listen(QHostAddress::LocalHost, 4243)) {
        return false;
    }

    connect(m_webSocketServer, SIGNAL(newConnection()), this, SLOT(incomingWebSocketConnection()));
    return true;
}

void ConnectionListener::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket* tcpSocket = new QTcpSocket(this);
    tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
    if (!tcpSocket->setSocketDescriptor(socketDescriptor))
    {
       emit log ("Listener", "New connection: error." + tcpSocket->errorString());
       delete tcpSocket;
    }
    addPendingConnection(tcpSocket);
}

void ConnectionListener::addPendingConnection(QTcpSocket *tcpSocket)
{
  if (m_accept)
  {
    emit log ("Listener", "New connection: accepted.");
    doNotAcceptNewConnections();
    TcpSocketProxy *proxy = new TcpSocketProxy(m_mapData, m_pathMachine, m_commandEvaluator, m_prespammedPath, m_groupManager, tcpSocket, m_remoteHost, m_remotePort, true, this);
    connect (proxy, SIGNAL(log(const QString&, const QString&)), parent(), SLOT(log(const QString&, const QString&)));
    proxy->start();
  }
  else
  {
    emit log ("Listener", "New connection: rejected.");
    tcpSocket->write("You can't connect more than once!!!\r\n",37);
    tcpSocket->flush();
    tcpSocket->disconnectFromHost();
    tcpSocket->deleteLater();
  }
}

void ConnectionListener::incomingWebSocketConnection()
{
    QWebSocket* webSocket = m_webSocketServer->nextPendingConnection();
    if (m_accept)
    {
      emit log ("Listener", "New web socket connection: accepted.");
      doNotAcceptNewConnections();
      WebSocketProxy *proxy = new WebSocketProxy(m_mapData, m_pathMachine, m_commandEvaluator, m_prespammedPath, m_groupManager, webSocket, m_remoteHost, m_remotePort, true, this);
      connect (proxy, SIGNAL(log(const QString&, const QString&)), parent(), SLOT(log(const QString&, const QString&)));
      proxy->start();
    }
    else
    {
        emit log ("Listener", "New web socket connection: rejected.");
        webSocket->sendTextMessage("You can't connect more than once!!!\r\n");
        webSocket->flush();
        webSocket->close(QWebSocketProtocol::CloseCodeNormal, "You can't connect more than once!!!");
        webSocket->deleteLater();
    }
}

void ConnectionListener::doNotAcceptNewConnections()
{
    emit log ("Listener", "Stopping to accept new connections");
  m_accept = false;
}

void ConnectionListener::doAcceptNewConnections()
{
    emit log ("Listener", "Will accept new connections");
  m_accept = true;
}


