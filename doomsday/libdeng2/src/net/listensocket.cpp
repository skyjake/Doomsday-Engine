/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/ListenSocket"
#include "de/Address"
#include "de/Socket"

#include <QCoreApplication>
#include <QThread>

using namespace de;

class TcpServer : public QTcpServer
{
public:
    TcpServer() : QTcpServer() {}

    void incomingConnection(int handle)
    {
        qDebug() << "TcpServer: Incoming connection available, handle" << handle;
    }
};

class Doorman : public QThread
{
public:
    Doorman(duint16 port) : _port(port), _socket(0), _shouldStop(false) {}
    void run();
    void stopAndWait() {
        _shouldStop = true;
        wait();
    }

private:
    duint16 _port;
    TcpServer* _socket;
    volatile bool _shouldStop;
};

void Doorman::run()
{
    _socket = new TcpServer;
    _socket->listen(QHostAddress::Any, _port);
    Q_ASSERT(_socket->isListening());

    while(!_shouldStop)
    {
        if(_socket->waitForNewConnection(100))
        {
            // Got a new connection!

        }
    }

    delete _socket;
}

struct ListenSocket::Instance
{
    /// Pointer to the internal socket data.
    Doorman* doorman;

    duint16 port;

    /// Incoming connections.
    QList<QTcpSocket*> incoming;

    Instance() : doorman(0), port(0) {}
    ~Instance() {
        doorman->stopAndWait();
        delete doorman;
    }
};

ListenSocket::ListenSocket(duint16 port)
{
    LOG_AS("ListenSocket");

    d = new Instance;
    d->doorman = new Doorman(port);
    d->doorman->start();

/*    if(!d->socket->listen(QHostAddress::Any, d->port))
    {
        /// @throw OpenError Opening the socket failed.
        throw OpenError("ListenSocket", "Port " + QString::number(d->port) + ": " +
                        d->socket->errorString());
    }
*/

    //connect(d->socket, SIGNAL(newConnection()), this, SLOT(acceptNewConnection()));
}

ListenSocket::~ListenSocket()
{
    delete d;
}

void ListenSocket::acceptNewConnection()
{
    LOG_AS("ListenSocket::acceptNewConnection");
/*
    QTcpSocket* s = d->socket->nextPendingConnection();
    d->incoming.append(s);

    LOG_MSG("Accepted new connection from %s.") << s->peerAddress().toString();

    emit incomingConnection();*/
}

Socket* ListenSocket::accept()
{
    if(d->incoming.empty())
    {
        return 0;
    }
    // We can use this constructor because we are Socket's friend.
    return new Socket(d->incoming.takeFirst());
}

duint16 ListenSocket::port() const
{
    return d->port;
}
