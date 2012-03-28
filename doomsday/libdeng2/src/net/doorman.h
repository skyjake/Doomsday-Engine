/*
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_DOORMAN_H
#define LIBDENG2_DOORMAN_H

#include "de/libdeng2.h"
#include <QTcpServer>
#include <QThread>

namespace de {
namespace internal {

/**
 * Overrides QTcpServer's handling of incoming connections.
 */
class TcpServer : public QTcpServer
{
    Q_OBJECT

public:
    TcpServer() : QTcpServer() {}

    void incomingConnection(int handle) {
        qDebug() << "TcpServer: Incoming connection available, handle" << handle;
        emit takeIncomingSocketDesc(handle);
    }

signals:
    void takeIncomingSocketDesc(int handle);
};

/**
 * Separate thread for listening to incoming connections. This is needed
 * when the application's main event loop is not available.
 */
class Doorman : public QThread
{
    Q_OBJECT

public:
    Doorman(duint16 port) : _port(port), _socket(0), _shouldStop(false) {}
    void run();
    void stopAndWait() {
        _shouldStop = true;
        wait();
    }

signals:
    void incomingSocketDesc(int handle);

private:
    duint16 _port;
    TcpServer* _socket;
    volatile bool _shouldStop;
};

} // namespace internal
} // namespace de

#endif // LIBDENG2_DOORMAN_H
