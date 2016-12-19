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
    if (!m_webSocket) {
        m_webSocket = m_webSocketServer->nextPendingConnection();

        QImage image(QString(":/icons/m.png"));
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG"); // writes image into ba in PNG format
        qDebug() << "buffer is of size" << ba.length();

        m_webSocket->sendBinaryMessage(ba);
        m_webSocket->flush();
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
