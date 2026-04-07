/** @file serverfinder.h  Looks up servers via beacon.
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

#ifndef LIBSHELL_SERVERFINDER_H
#define LIBSHELL_SERVERFINDER_H

#include "de/serverinfo.h"
#include "de/address.h"

namespace de {

/**
 * Looks up servers via beacon. @ingroup shell
 */
class DE_PUBLIC ServerFinder
{
public:
    /// Specified server was not found. @ingroup errors
    DE_ERROR(NotFoundError);

    DE_AUDIENCE(Update, void foundServersUpdated())

public:
    ServerFinder();
    ~ServerFinder();

    /**
     * Forgets all servers found so far.
     */
    void clear();

    List<Address> foundServers() const;

    String name(const Address &server) const;
    int    playerCount(const Address &server) const;
    int    maxPlayers(const Address &server) const;

    /**
     * Returns the message sent by a server's beacon.
     *
     * @param address  Address of a found server.
     *
     * @return Server information.
     */
    ServerInfo messageFromServer(const Address &address) const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBSHELL_SERVERFINDER_H
