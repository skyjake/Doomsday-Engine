/** @file serverfinder.cpp  Looks up servers via beacon.
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

#include "de/shell/ServerFinder"
#include <de/App>
#include <de/Beacon>
#include <de/CommandLine>
#include <de/LogBuffer>
#include <de/NumberValue>
#include <de/Reader>
#include <de/TextValue>

#include <QMap>
#include <QTimer>

namespace de { namespace shell {

static TimeSpan MSG_EXPIRATION_SECS = 4;

DENG2_PIMPL_NOREF(ServerFinder)
{
    Beacon beacon;
    struct Found
    {
        shell::ServerInfo message;
        Time at;

        Found() : at(Time()) {}
    };
    QMap<Address, Found> servers;

    Impl() : beacon(DEFAULT_PORT) {}

    bool removeExpired()
    {
        bool changed = false;

        QMutableMapIterator<Address, Found> iter(servers);
        while (iter.hasNext())
        {
            Found &found = iter.next().value();
            if (found.at.since() > MSG_EXPIRATION_SECS)
            {
                iter.remove();
                changed = true;
            }
        }

        return changed;
    }
};

ServerFinder::ServerFinder() : d(new Impl)
{
    try
    {
        connect(&d->beacon, SIGNAL(found(de::Address, de::Block)), this, SLOT(found(de::Address, de::Block)));
        QTimer::singleShot(1000, this, SLOT(expire()));

        if (!App::appExists() || !App::commandLine().has("-nodiscovery"))
        {
            d->beacon.discover(0 /* no timeout */, 2);
        }
    }
    catch (Beacon::PortError const &er)
    {
        LOG_WARNING("Automatic server discovery is not available:\n") << er.asText();
    }
}

ServerFinder::~ServerFinder()
{
    d.reset();
}

void ServerFinder::clear()
{
    d->servers.clear();
}

QList<Address> ServerFinder::foundServers() const
{
    return d->servers.keys();
}

String ServerFinder::name(Address const &server) const
{
    return messageFromServer(server).name();
}

int ServerFinder::playerCount(Address const &server) const
{
    return messageFromServer(server).playerCount();
}

int ServerFinder::maxPlayers(Address const &server) const
{
    return messageFromServer(server).maxPlayers();
}

ServerInfo ServerFinder::messageFromServer(Address const &address) const
{
    Address addr = shell::checkPort(address);
    if (!d->servers.contains(addr))
    {
        /// @throws NotFoundError @a address not found in the registry of server responses.
        throw NotFoundError("ServerFinder::messageFromServer",
                            "No message from server " + addr.asText());
    }
    return d->servers[addr].message;
}

void ServerFinder::found(Address host, Block block)
{
    // Normalize the local host address.
    if (host.isLocal()) host.setHost(QHostAddress::LocalHost);

    try
    {
        LOG_TRACE("Received a server message from %s with %i bytes",
                  host << block.size());

        Record info;
        Reader(block).withHeader() >> info;

        ServerInfo receivedInfo(info);
        receivedInfo.setAddress(host);

        Address const from = receivedInfo.address(); // port validated

        // Replace or insert the information for this host.
        Impl::Found found;
        if (d->servers.contains(from))
        {
            d->servers[from].message = receivedInfo;
            d->servers[from].at = Time();
        }
        else
        {
            found.message = receivedInfo;
            d->servers.insert(from, found);
        }

        //qDebug() << "Server found:\n" << receivedInfo.asText().toLatin1().constData();

        emit updated();
    }
    catch (Error const &)
    {
        // Remove the message that failed to deserialize.
        if (d->servers.contains(host))
        {
            d->servers.remove(host);
        }
    }
}

void ServerFinder::expire()
{
    if (d->removeExpired())
    {
        emit updated();
    }
    QTimer::singleShot(1000, this, SLOT(expire()));
}

}} // namespace de::shell
