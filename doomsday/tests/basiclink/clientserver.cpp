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

#include "clientserver.h"
#include "../testapp.h"
#include <stdexcept>

Server::Server() : _entry(SERVER_PORT)
{
    connect(&_entry, SIGNAL(incomingConnection()), this, SLOT(sendResponse()));
}

void Server::sendResponse()
{
    LOG_AS("Server::sendResponse");

    Link link(_entry.accept());
    Block packet;
    Writer(packet) << "Hello world!";

    LOG_MESSAGE("Sending...");

    link << packet;

    LOG_INFO("Quitting.");

    App::app().stop();
}

Client::Client(const Address& serverAddress) : _link(serverAddress)
{
    connect(&_link, SIGNAL(messagesReady()), this, SLOT(handleIncoming()));
}

void Client::handleIncoming()
{
    LOG_AS("Client::handleIncoming");

    QScopedPointer<IByteArray> data(_link.receive());
    String str;
    Reader(*data) >> str;

    LOG_MESSAGE("Received '%s'") << str;

    LOG_INFO("Quitting.");

    App::app().stop();
}
