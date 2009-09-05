/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "../internal.h"
#include "../sdl.h"

using namespace de;

ListenSocket::ListenSocket(duint16 port) : _socket(0), _port(port)
{
    // Listening address.
    Address address(0, port);
    IPaddress ip;

    internal::convertAddress(address, &ip);

    if((_socket = SDLNet_TCP_Open(&ip)) == 0)
    {
        /// @throw OpenError Opening the socket failed.
        throw OpenError("ListenSocket::ListenSocket", SDLNet_GetError());
    }
}

ListenSocket::~ListenSocket()
{
    if(_socket)
    {
        SDLNet_TCP_Close( static_cast<TCPsocket>(_socket) );
    }
}

Socket* ListenSocket::accept()
{
    TCPsocket clientSocket = SDLNet_TCP_Accept(static_cast<TCPsocket>(_socket));
    if(!clientSocket)
    {
        return 0;
    }
    // We can use this constructor because we are Socket's friend.
    return new Socket(clientSocket);
}

duint16 ListenSocket::port() const
{
    return _port;
}
