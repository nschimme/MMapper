#include "websocketproxy.h"

WebSocketProxy::WebSocketProxy(MapData* md, Mmapper2PathMachine* pm, CommandEvaluator* ce, PrespammedPath* pp, CGroup* gm, QWebSocket *webSocket, QString & host, int & port, bool threaded, QObject *parent) :
    Proxy(md, pm, ce, pp, gm, host, port, threaded, parent),
    m_webSocket(webSocket)
{
    connect(m_webSocket, SIGNAL(disconnected()), this, SLOT(userTerminatedConnection()) );
    connect(m_webSocket, SIGNAL(textMessageReceived(const QString &)), this, SLOT(processTextUserStream(const QString &)) );
    connect(m_webSocket, SIGNAL(binaryMessageReceived(const QByteArray &)), this, SLOT(processBinaryUserStream(const QByteArray &)) );

}

WebSocketProxy::~WebSocketProxy() {
    if (m_webSocket) {
      m_webSocket->disconnect();
      m_webSocket->deleteLater();
    }
}

void WebSocketProxy::processTextUserStream(const QString &textMessage) {
    //emit log("WebSocketProxy", "Procesing text user input ..." + textMessage);
    processBinaryUserStream(textMessage.toLatin1());
}

void WebSocketProxy::processBinaryUserStream(const QByteArray &message) {
   //emit log("WebSocketProxy", "Procesing binary user input ..." + message);
   emit analyzeUserStream(message);
}

void WebSocketProxy::sendToUser(const QByteArray& ba)
{
  if (m_webSocket)
  {
    m_webSocket->sendBinaryMessage(ba);
    m_webSocket->flush();

  }
}
