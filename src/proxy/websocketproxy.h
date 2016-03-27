#ifndef WebSocketProxy_H
#define WebSocketProxy_H

#include "proxy.h"
#include <QWebSocket>

class WebSocketProxy : public Proxy
{
public:
    WebSocketProxy(MapData*, Mmapper2PathMachine*, CommandEvaluator*, PrespammedPath*, CGroup*, QWebSocket *, QString & host, int & port, bool threaded, QObject *parent);
    ~WebSocketProxy();

public slots:
    void processTextUserStream(const QString &);
    void processBinaryUserStream(const QByteArray &);
    void sendToUser(const QByteArray& ba);

private:
    Q_OBJECT

    QPointer<QWebSocket> m_webSocket;
};

#endif // WebSocketProxy_H
