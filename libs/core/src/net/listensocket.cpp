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

#include "de/listensocket.h"
#include "de/address.h"
#include "de/socket.h"
#include "de/lockable.h"

#include <the_Foundation/service.h>

namespace de {

DE_PIMPL(ListenSocket)
{
    tF::ref<iService> service;
    duint16 port{0};
    LockableT<List<iSocket *>> incoming; ///< Incoming connections.

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        if (service) close_Service(service);
    }

    static void acceptNewConnection(iAny *, iService *sv, iSocket *sock)
    {
        Impl *d = static_cast<Impl *>(userData_Object(sv));
        {
            DE_GUARD_FOR(d->incoming, _);
            d->incoming.value << sock;
        }
        DE_FOR_OBSERVERS(i, d->self().audienceForIncoming())
        {
            i->incomingConnection(d->self());
        }
    }

    DE_PIMPL_AUDIENCE(Incoming)
};

DE_AUDIENCE_METHOD(ListenSocket, Incoming)

ListenSocket::ListenSocket(duint16 port) : d(new Impl(this))
{
    LOG_AS("ListenSocket");

    d->service = tF::make_ref(new_Service(port));
    d->port = port;
    setUserData_Object(d->service, d);

    if (!open_Service(d->service))
    {
        /// @throw OpenError Opening the socket failed.
        throw OpenError("ListenSocket", stringf("Port %u: %s", d->port, strerror(errno)));
    }

    iConnect(Service, d->service, incomingAccepted, d->service, Impl::acceptNewConnection);
}

//void ListenSocket::acceptNewConnection()
//{
//    LOG_AS("ListenSocket::acceptNewConnection");

//    d->incoming << d->socket->nextPendingConnection();

//    emit incomingConnection();
//}

Socket *ListenSocket::accept()
{
    iSocket *s;
    {
        DE_GUARD_FOR(d->incoming, _);
        if (d->incoming.value.empty())
        {
            return nullptr;
        }
        s = d->incoming.value.takeFirst();
    }
    LOG_NET_NOTE("Accepted new connection from %s") << String::take(toString_Address(address_Socket(s)));
    return new Socket(s); // taken
}

duint16 ListenSocket::port() const
{
    return d->port;
}

} // namespace de
