/*
 * The Doomsday Engine Project -- dengsv
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "client.h"

using namespace de;

Client::Client(const de::Address& address) : de::MuxLink(address) 
{
    grantRights();
}

Client::Client(de::Socket* socket) : de::MuxLink(socket) 
{
    grantRights();
}

void Client::grantRights()
{
    /// Local clients get the admin rights automatically.
    if(peerAddress().matches(Address("127.0.0.1")))
    {
        rights.set(ADMIN_BIT);
    }
}
