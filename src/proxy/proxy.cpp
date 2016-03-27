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

#include "proxy.h"
#include "telnetfilter.h"
#include "parser.h"
#include "mumexmlparser.h"
#include "mainwindow.h"
#include "parseevent.h"
#include "mmapper2pathmachine.h"
#include "prespammedpath.h"
#include "CGroup.h"
#include "configuration.h"
#include <QDebug>

ProxyThreader::ProxyThreader(Proxy* proxy):
    m_proxy(proxy)
{
}

ProxyThreader::~ProxyThreader()
{
  if (m_proxy) delete m_proxy;
  qDebug() << "Killing Proxy";
}

void ProxyThreader::run() {
  try {
    exec();
    qDebug() << "Starting Proxy";
  } catch (char const * error) {
//          cerr << error << endl;
    throw;
  }
}


Proxy::Proxy(MapData* md, Mmapper2PathMachine* pm, CommandEvaluator* ce, PrespammedPath* pp, CGroup* gm, QString & host, int & port, bool threaded, QObject *parent)
  : QObject(parent),
                               m_remoteHost(host),
                                            m_remotePort(port),
                                                        m_serverConnected(false),
                                                        m_filter(NULL), m_parser(NULL), m_parserXml(NULL),
                                                            m_mapData(md),
                                                                m_pathMachine(pm),
                                                                    m_commandEvaluator(ce),
                                                                        m_prespammedPath(pp),
                                                                        m_threaded(threaded),
                                                                            m_groupManager(gm)
{
    if (threaded)
      m_thread = new ProxyThreader(this);
    else
      m_thread = NULL;
#ifdef PROXY_STREAM_DEBUG_INPUT_TO_FILE
  QString fileName = "proxy_debug.dat";

  file = new QFile(fileName);

  if (!file->open(QFile::WriteOnly))
    return;

  debugStream = new QDataStream(file);
#endif
}

Proxy::~Proxy()
{
#ifdef PROXY_STREAM_DEBUG_INPUT_TO_FILE
  file->close();
#endif
  if (m_mudSocket) {
    m_mudSocket->disconnectFromHost();
    m_mudSocket->waitForDisconnected();
    m_mudSocket->deleteLater();
  }
  if (m_filter) delete m_filter;
  if (m_parser) delete m_parser;
  if (m_parserXml) delete m_parserXml;
}

void Proxy::start() {
    init();
}

bool Proxy::init()
{
  connect (this, SIGNAL(doAcceptNewConnections()), parent(), SLOT(doAcceptNewConnections()));

  m_filter = new TelnetFilter(this);
  connect(this, SIGNAL(analyzeUserStream(const QByteArray&)), m_filter, SLOT(analyzeUserStream(const QByteArray&)));
  connect(this, SIGNAL(analyzeMudStream(const QByteArray&)), m_filter, SLOT(analyzeMudStream(const QByteArray&)));
  connect(m_filter, SIGNAL(sendToMud(const QByteArray&)), this, SLOT(sendToMud(const QByteArray&)));
  connect(m_filter, SIGNAL(sendToUser(const QByteArray&)), this, SLOT(sendToUser(const QByteArray&)));

  m_parser = new Parser(m_mapData, this);
  connect(m_filter, SIGNAL(parseNewMudInput(IncomingData&)), m_parser, SLOT(parseNewMudInput(IncomingData&)));
  connect(m_filter, SIGNAL(parseNewUserInput(IncomingData&)), m_parser, SLOT(parseNewUserInput(IncomingData&)));
  connect(m_parser, SIGNAL(sendToMud(const QByteArray&)), this, SLOT(sendToMud(const QByteArray&)));
  connect(m_parser, SIGNAL(sendToUser(const QByteArray&)), this, SLOT(sendToUser(const QByteArray&)));
  connect(m_parser, SIGNAL(setXmlMode()), m_filter, SLOT(setXmlMode()));

        //connect(m_parser, SIGNAL(event(ParseEvent* )), (QObject*)(Mmapper2PathMachine*)((MainWindow*)(m_parent->parent()))->getPathMachine(), SLOT(event(ParseEvent* )), Qt::QueuedConnection);
  connect(m_parser, SIGNAL(event(ParseEvent* )), m_pathMachine, SLOT(event(ParseEvent* )), Qt::QueuedConnection);
  connect(m_parser, SIGNAL(releaseAllPaths()), m_pathMachine, SLOT(releaseAllPaths()), Qt::QueuedConnection);
  connect(m_parser, SIGNAL(showPath(CommandQueue, bool)), m_prespammedPath, SLOT(setPath(CommandQueue, bool)), Qt::QueuedConnection);

  m_parserXml = new MumeXmlParser(m_mapData, this);
  connect(m_filter, SIGNAL(parseNewMudInputXml(IncomingData&)), m_parserXml, SLOT(parseNewMudInput(IncomingData&)));
  connect(m_filter, SIGNAL(parseNewUserInputXml(IncomingData&)), m_parserXml, SLOT(parseNewUserInput(IncomingData&)));
  connect(m_parserXml, SIGNAL(sendToMud(const QByteArray&)), this, SLOT(sendToMud(const QByteArray&)));
  connect(m_parserXml, SIGNAL(sendToUser(const QByteArray&)), this, SLOT(sendToUser(const QByteArray&)));
  connect(m_parserXml, SIGNAL(setNormalMode()), m_filter, SLOT(setNormalMode()));

        //connect(m_parserXml, SIGNAL(event(ParseEvent* )), (QObject*)(Mmapper2PathMachine*)((MainWindow*)(m_parent->parent()))->getPathMachine(), SLOT(event(ParseEvent* )), Qt::QueuedConnection);
  connect(m_parserXml, SIGNAL(event(ParseEvent* )), m_pathMachine, SLOT(event(ParseEvent* )), Qt::QueuedConnection);
  connect(m_parserXml, SIGNAL(releaseAllPaths()), m_pathMachine, SLOT(releaseAllPaths()), Qt::QueuedConnection);
  connect(m_parserXml, SIGNAL(showPath(CommandQueue, bool)), m_prespammedPath, SLOT(setPath(CommandQueue, bool)), Qt::QueuedConnection);

  //Group Manager Support
  //TODO: Add normal parser support
  connect(m_parserXml, SIGNAL(sendScoreLineEvent(QByteArray)), m_groupManager, SLOT(parseScoreInformation(QByteArray)), Qt::QueuedConnection);
  connect(m_parserXml, SIGNAL(sendPromptLineEvent(QByteArray)), m_groupManager, SLOT(parsePromptInformation(QByteArray)), Qt::QueuedConnection);
  connect(m_parserXml, SIGNAL(sendGroupTellEvent(QByteArray)), m_groupManager, SLOT(sendGTell(QByteArray)), Qt::QueuedConnection);
  // Group Tell
  connect(m_groupManager, SIGNAL(displayGroupTellEvent(const QByteArray&)), m_parserXml, SLOT(sendGTellToUser(const QByteArray&)), Qt::QueuedConnection);

  //m_userSocket->write("Connection to client established ...\r\n", 38);
  emit log("Proxy", "Connection to client established ...");

  QByteArray ba("\033[1;37;41mWelcome to MMapper!\033[0;37;41m   Type \033[1m_help\033[0m\033[37;41m for help or \033[1m_vote\033[0m\033[37;41m to vote!\033[0m\r\n");
  sendToUser(ba);

  m_mudSocket = new QTcpSocket(this);
  m_mudSocket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
  connect(m_mudSocket, SIGNAL(disconnected()), this, SLOT(mudTerminatedConnection()) );
  connect(m_mudSocket, SIGNAL(readyRead()), this, SLOT(processMudStream()) );

  m_mudSocket->connectToHost(m_remoteHost, m_remotePort, QIODevice::ReadWrite);
  if (!m_mudSocket->waitForConnected(5000))
  {
        //m_userSocket->write("Server not responding!!!\r\n", 26);
    emit log("Proxy", "Server not responding!!!");

    sendToUser("\r\nServer not responding!!!\r\n\r\nYou can explore world map offline or try to reconnect again...\r\n");
    sendToUser("\r\n>");
        //m_userSocket->flush();

    emit error(m_mudSocket->error());
    m_mudSocket->close();
    delete m_mudSocket;
    m_mudSocket = NULL;

    return true;

  }
  else
  {
    m_serverConnected = true;
#if __linux__
    // Linux needs to have this option set after the server has established a connection
    m_mudSocket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
#endif
    //m_userSocket->write("Connection to server established ...\r\n", 38);
    emit log("Proxy", "Connection to server established ...");
    //m_userSocket->flush();

    if (Config().m_mpi) {
      emit sendToMud(QByteArray("~$#EX1\n3\n"));
      emit log("Proxy", "Sent MUME Protocol Initiator XML request");
      m_filter->setXmlMode();
    }

    if (Config().m_IAC_prompt_parser) {
      //send IAC-GA prompt request
      QByteArray idprompt("~$#EP2\nG\n");
      emit sendToMud(idprompt);
    }

    return true;
  }
//return true;
  return false;
}


void Proxy::userTerminatedConnection()
{
  emit log("Proxy", "User terminated connection ...");
  m_thread->exit();
  emit doAcceptNewConnections();
}

void Proxy::mudTerminatedConnection()
{
  emit log("Proxy", "Mud terminated connection ...");
  sendToUser("\r\nServer closed the connection\r\n\r\nYou can explore world map offline or try to reconnect again...\r\n");
  sendToUser("\r\n>");
  //m_thread->exit();
}

void Proxy::processMudStream() {
  emit analyzeMudStream(m_mudSocket->readAll());
}


void Proxy::sendToMud(const QByteArray& ba)
{
  //emit log("Proxy", "Sending to MUD " + ba);
  if (m_mudSocket && m_serverConnected)
  {
    m_mudSocket->write(ba.data(), ba.size());
    m_mudSocket->flush();

  }
}

