#ifndef TcpSocketProxy_H
#define TcpSocketProxy_H

#include <QTcpSocket>
#include "proxy.h"

class TcpSocketProxy  : public Proxy
{
public:
    TcpSocketProxy(MapData*, Mmapper2PathMachine*, CommandEvaluator*, PrespammedPath*, CGroup*, QTcpSocket *, QString & host, int & port, bool threaded, QObject *parent);
    ~TcpSocketProxy();

public slots:
    void processUserStream();
    void sendToUser(const QByteArray& ba);

private:
    Q_OBJECT

    QPointer<QTcpSocket> m_userSocket;
    QObject *m_parent;

};

#endif // TcpSocketProxy_H
