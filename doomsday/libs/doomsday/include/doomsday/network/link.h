/** @file link.h  Network connection to a server.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_LINK_H
#define LIBSHELL_LINK_H

#include "../libdoomsday.h"
#include "protocol.h"
#include <de/address.h>
#include <de/socket.h>
#include <de/time.h>
#include <de/transmitter.h>
#include <de/abstractlink.h>

namespace network {

using namespace de;

/**
 * Network connection to a server using the shell protocol.
 *
 * @ingroup shell
 */
class LIBDOOMSDAY_PUBLIC Link : public AbstractLink
{
public:
    DE_ERROR(ConnectError);

    /**
     * Opens a connection to a server over the network.
     *
     * @param domain   Domain/IP address of the server.
     * @param timeout  Keep trying until this much time has passed.
     */
    Link(const String &domain, TimeSpan timeout = 0.0);

    /**
     * Opens a connection to a server over the network.
     *
     * @param address  Address of the server.
     */
    Link(const Address &address);

    /**
     * Takes over an existing socket.
     *
     * @param openSocket  Socket. Link takes ownership.
     */
    Link(Socket *openSocket);

    /**
     * Shell protocol for constructing and interpreting packets.
     */
    Protocol &protocol();

    /**
     * Opens the connection. This is an asynchronous operation. One can observe the
     * state of the link via signals.
     */
    void connectLink();

protected:
    Packet *interpret(const Message &msg);
    void    initiateCommunications();

private:
    DE_PRIVATE(d)
};

} // namespace network

#endif // LIBSHELL_LINK_H
