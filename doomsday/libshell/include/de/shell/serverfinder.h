/** @file serverfinder.h  Looks up servers via beacon.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBSHELL_SERVERFINDER_H
#define LIBSHELL_SERVERFINDER_H

#include "libshell.h"
#include <QObject>
#include <de/Error>
#include <de/Address>
#include <de/Record>

namespace de {
namespace shell {

/**
 * Looks up servers via beacon.
 */
class LIBSHELL_PUBLIC ServerFinder : public QObject
{
    Q_OBJECT

public:
    /// Specified server was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

public:
    ServerFinder();
    ~ServerFinder();

    /**
     * Forgets all servers found so far.
     */
    void clear();

    QList<Address> foundServers() const;

    String name(Address const &server) const;
    int playerCount(Address const &server) const;
    int maxPlayers(Address const &server) const;

    /**
     * Returns the message sent by a server's beacon.
     *
     * @param address  Address of a found server.
     *
     * @return Reference to a record. The reference is valid until start() is
     * called.
     */
    Record const &messageFromServer(Address const &address) const;

protected slots:
    void found(de::Address address, de::Block info);
    void expire();

signals:
    void updated();

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_SERVERFINDER_H
