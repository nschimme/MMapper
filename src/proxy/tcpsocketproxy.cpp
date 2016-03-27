#include "tcpsocketproxy.h"

#include <QTcpSocket>

TcpSocketProxy::TcpSocketProxy(MapData* md, Mmapper2PathMachine* pm, CommandEvaluator* ce, PrespammedPath* pp, CGroup* gm, QTcpSocket *tcpSocket, QString & host, int & port, bool threaded, QObject *parent) :
    Proxy(md, pm, ce, pp, gm, host, port, threaded, parent),
    m_userSocket(tcpSocket),
    m_parent(parent)
{

    connect(m_userSocket, SIGNAL(disconnected()), this, SLOT(userTerminatedConnection()) );
    connect(m_userSocket, SIGNAL(readyRead()), this, SLOT(processUserStream()) );
}

TcpSocketProxy::~TcpSocketProxy() {
    if (m_userSocket) {
      m_userSocket->disconnectFromHost();
      m_userSocket->deleteLater();
    }
}

void TcpSocketProxy::processUserStream() {
//  emit log("Proxy", "Procesing user input ...");
    emit analyzeUserStream(m_userSocket->readAll());
}


void TcpSocketProxy::sendToUser(const QByteArray& ba)
{
  if (m_userSocket)
  {
    m_userSocket->write(ba.data(), ba.size());
    m_userSocket->flush();

  }
}
