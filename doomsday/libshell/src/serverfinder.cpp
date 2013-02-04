/** @file serverfinder.cpp  Looks up servers via beacon.
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

#include "de/shell/ServerFinder"
#include <de/Beacon>
#include <de/Reader>
#include <QMap>

namespace de {
namespace shell {

struct ServerFinder::Instance
{
    Beacon beacon;
    QMap<Address, Record *> messages;

    Instance() : beacon(53209) {}

    ~Instance()
    {
        clearMessages();
    }

    void clearMessages()
    {
        foreach(Record *msg, messages.values()) delete msg;
        messages.clear();
    }
};

ServerFinder::ServerFinder() : d(new Instance)
{
    connect(&d->beacon, SIGNAL(found(de::Address, de::Block)), this, SLOT(found(de::Address, de::Block)));
    connect(&d->beacon, SIGNAL(finished()), this, SIGNAL(finished()));
}

ServerFinder::~ServerFinder()
{
    delete d;
}

void ServerFinder::start()
{
    d->clearMessages();
    d->beacon.discover(20);
}

QList<Address> ServerFinder::foundServers() const
{
    return d->beacon.foundHosts();
}

Record const &ServerFinder::messageFromServer(Address const &address) const
{
    return *d->messages[address];
}

void ServerFinder::found(Address host, Block block)
{
    try
    {
        LOG_DEBUG("Received a server message from %s with %i bytes")
                << host << block.size();

        // Replace or insert the information for this host.
        Record *rec;
        if(d->messages.contains(host))
        {
            rec = d->messages[host];
        }
        else
        {
            d->messages.insert(host, rec = new Record);
        }
        Reader(block).withHeader() >> *rec;

        emit updated();
    }
    catch(Error const &)
    {}
}

} // namespace shell
} // namespace de
