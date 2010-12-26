/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENTSERVER_H
#define CLIENTSERVER_H

#include <de/data.h>
#include <de/net.h>

using namespace de;

#define SERVER_PORT     23546

// Server object.
class Server : public QObject
{
    Q_OBJECT

public:
    Server();

public slots:
    void sendResponse();

private:
    ListenSocket _entry;
};

// Client object.
class Client : public QObject
{
    Q_OBJECT

public:
    Client(const Address& serverAddress);

public slots:
    void handleIncoming();

private:
    Socket _socket;
};

#endif // CLIENTSERVER_H
