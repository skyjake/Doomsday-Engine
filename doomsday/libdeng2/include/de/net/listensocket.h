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

#ifndef LIBDENG2_LISTENSOCKET_H
#define LIBDENG2_LISTENSOCKET_H

#include "../deng.h"

namespace de
{
    class Socket;

    /**
     * TCP/IP server socket.  It can only be used for accepting incoming TCP/IP
     * connections.  Normal communications using a listen socket are not possible.
     *
     * @ingroup net
     */
    class LIBDENG2_API ListenSocket
    {
    public:
        /// Opening the socket failed. @ingroup errors
        DEFINE_ERROR(OpenError);
    
    public:
        /// Open a listen socket on the specified port.
        ListenSocket(duint16 port);

        /// Close the listen socket.
        virtual ~ListenSocket();

        /// Accept a new incoming connection.  @return A Socket object
        /// that represents the new client, or NULL if there were no
        /// pending connection requests.
        Socket* accept();
        
        /// Returns the port the socket is listening on.
        duint16 port() const;
    
    private:
        /// Pointer to the internal socket data.
        void* _socket;
        
        duint16 _port;
    };
}

#endif /* LIBDENG2_LISTENSOCKET_H */
