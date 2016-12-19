#ifndef WEBSOCKETCANVAS_H
#define WEBSOCKETCANVAS_H

#include <QObject>
#include <QPointer>
#include <QWebSocketServer>
#include <QWebSocket>

class WebSocketCanvas : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketCanvas(QObject *parent = 0);

signals:

public slots:
    void incomingWebSocketConnection();
    void update(const QImage &image);

private:
    QPointer<QWebSocketServer> m_webSocketServer;
    QPointer<QWebSocket> m_webSocket;

};

#endif // WEBSOCKETCANVAS_H
