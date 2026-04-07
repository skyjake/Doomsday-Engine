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

#include "de/serverfinder.h"

#include "de/app.h"
#include "de/beacon.h"
#include "de/commandline.h"
#include "de/logbuffer.h"
#include "de/loop.h"
#include "de/keymap.h"
#include "de/numbervalue.h"
#include "de/reader.h"
#include "de/textvalue.h"
#include "de/timer.h"

namespace de {

static constexpr TimeSpan MSG_EXPIRATION_SECS = 4.0_s;

DE_PIMPL(ServerFinder)
, DE_OBSERVES(Beacon, Discovery)
{
    struct Found {
        ServerInfo message;
        Time at;
    };

    Beacon beacon;
    KeyMap<Address, Found> servers;
    Timer expiration;

    Impl(Public * i)
        : Base(i)
        , beacon({DEFAULT_PORT, DEFAULT_PORT + 16})
    {
        expiration.setInterval(1.0_s);
        expiration.audienceForTrigger() += [this]() { expire(); };
    }

    void beaconFoundHost(const Address &host, const Block &block) override
    {
        // Normalize the local host address.
        //if (host.isLocal()) host.setHost(QHostAddress::LocalHost);

        try
        {
            LOG_TRACE("Received a server message from %s with %i bytes",
                      host << block.size());

            Record inf;
            Reader(block).withHeader() >> inf;
            ServerInfo receivedInfo(inf);

            // We don't need to know the sender's Beacon UDP port.
            if (host.isLocal())
            {
                // This gives us a network address with the widest available scope 
                // instead of some internal IP address where the packet may have been
                // received from.
                receivedInfo.setAddress(Address::localNetworkInterface(receivedInfo.port()));
            }
            else
            {
                receivedInfo.setAddress(Address(host.hostName(), receivedInfo.port()));
            }

            const Address from = receivedInfo.address(); // port validated

            // Replace or insert the information for this host.
            Impl::Found found;
            if (servers.contains(from))
            {
                servers[from].message = receivedInfo;
                servers[from].at = Time();
            }
            else
            {
                found.message = receivedInfo;
                servers.insert(from, found);
            }

            //qDebug() << "Server found:\n" << receivedInfo.asText().toLatin1().constData();

            DE_NOTIFY_PUBLIC(Update, i)
            {
                i->foundServersUpdated();
            }
        }
        catch (const Error &)
        {
            // Remove the message that failed to deserialize.
            if (servers.contains(host))
            {
                servers.remove(host);
            }
        }
    }

    bool removeExpired()
    {
        bool changed = false;
        for (auto iter = servers.begin(); iter != servers.end(); )
        {
            Found &found = iter->second;
            if (found.at.since() > MSG_EXPIRATION_SECS)
            {
                iter = servers.erase(iter);
                changed = true;
            }
            else
            {
                ++iter;
            }
        }
        return changed;
    }

    void expire()
    {
        if (removeExpired())
        {
            DE_NOTIFY_PUBLIC(Update, i) { i->foundServersUpdated(); }
        }
    }

    DE_PIMPL_AUDIENCE(Update)
};

DE_AUDIENCE_METHOD(ServerFinder, Update)

ServerFinder::ServerFinder() : d(new Impl(this))
{
    try
    {
        d->beacon.audienceForDiscovery() += d;
        d->expiration.start();

        if (!App::appExists() || !App::commandLine().has("-nodiscovery"))
        {
            d->beacon.discover(0.0 /* no timeout */, 2.0);
        }
    }
    catch (const Beacon::PortError &er)
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

List<Address> ServerFinder::foundServers() const
{
    return map<List<Address>>(d->servers,
                              [](const decltype(d->servers)::value_type &v) { return v.first; });
}

String ServerFinder::name(const Address &server) const
{
    return messageFromServer(server).name();
}

int ServerFinder::playerCount(const Address &server) const
{
    return messageFromServer(server).playerCount();
}

int ServerFinder::maxPlayers(const Address &server) const
{
    return messageFromServer(server).maxPlayers();
}

ServerInfo ServerFinder::messageFromServer(const Address &address) const
{
    Address addr = checkPort(address);
    if (!d->servers.contains(addr))
    {
        /// @throws NotFoundError @a address not found in the registry of server responses.
        throw NotFoundError("ServerFinder::messageFromServer",
                            "No message from server " + addr.asText());
    }
    return d->servers[addr].message;
}

} // namespace de
