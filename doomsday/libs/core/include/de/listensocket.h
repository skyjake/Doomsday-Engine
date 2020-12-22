/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_LISTENSOCKET_H
#define LIBCORE_LISTENSOCKET_H

#include "de/observers.h"

namespace de {

class Socket;

/**
 * TCP/IP server socket.  It can only be used for accepting incoming TCP/IP
 * connections.  Normal communications using a listen socket are not possible.
 *
 * @ingroup net
 */
class DE_PUBLIC ListenSocket
{
public:
    /// Opening the socket failed. @ingroup errors
    DE_ERROR(OpenError);

    /**
     * Notifies when a new incoming connection is available.
     * Call accept() to get the Socket object.
     */
    DE_AUDIENCE(Incoming, void incomingConnection(ListenSocket &))

public:
    /// Open a listen socket on the specified port.
    ListenSocket(duint16 port);

    /// Returns the port the socket is listening on.
    duint16 port() const;

    /// Returns an incoming connection. Caller takes ownership of
    /// the returned object.
    Socket *accept();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_LISTENSOCKET_H
