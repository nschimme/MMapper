#include "websocketcanvas.h"

#include <QWebSocket>
#include <QImage>
#include <QBuffer>
#include <QDebug>

WebSocketCanvas::WebSocketCanvas(QObject *parent) : QObject(parent)
{
    m_webSocketServer = new QWebSocketServer("MMapper Map WebSocket", QWebSocketServer::NonSecureMode, this);
    m_webSocketServer->setMaxPendingConnections(1);
    if (!m_webSocketServer->listen(QHostAddress::LocalHost, 4343)) {
    }

    connect(m_webSocketServer, SIGNAL(newConnection()), this, SLOT(incomingWebSocketConnection()));
}


void WebSocketCanvas::incomingWebSocketConnection()
{
    if (m_webSocketServer->isListening()) {
        qDebug() << "new connecting arrived to web socket";
        m_webSocket = m_webSocketServer->nextPendingConnection();
        connect(m_webSocket, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
        m_webSocketServer->pauseAccepting();

        QImage image(QString(":/icons/m.png"));
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG"); // writes image into ba in PNG format
        qDebug() << "m.png is of size" << ba.length();

        m_webSocket->sendBinaryMessage(ba);
        m_webSocket->flush();
    }
}

void WebSocketCanvas::clientDisconnected() {
    if (m_webSocket) {
        m_webSocket->disconnect();
        m_webSocket->deleteLater();
        m_webSocketServer->resumeAccepting();
        qDebug() << "killed old web socket";
    }
}


void WebSocketCanvas::update(const QImage &image)
{
    if (m_webSocket) {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG"); // writes image into ba in PNG format
        qDebug() << "buffer is of size" << ba.length();

        m_webSocket->sendBinaryMessage(ba);
        m_webSocket->flush();

    }
}
