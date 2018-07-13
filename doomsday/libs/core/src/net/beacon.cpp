/** @file beacon.cpp  Presence service based on UDP broadcasts.
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

#include "de/Beacon"

#include "de/Garbage"
#include "de/LogBuffer"
#include "de/Loop"
#include "de/Map"
#include "de/Reader"
#include "de/Timer"
#include "de/Writer"

#include <c_plus/address.h>
#include <c_plus/datagram.h>

namespace de {

/**
 * Maximum number of Beacon UDP ports in simultaneous use at one machine, i.e.,
 * maximum number of servers on one machine.
 */
static const duint16 MAX_LISTEN_RANGE = 16;

static const duint16 MIN_PORT = 49152; // start of IANA private range

// 1.0: Initial version.
// 1.1: Advertised message is compressed with zlib (deflate).
static const char *discoveryMessage = "Doomsday Beacon 1.1";

DE_PIMPL(Beacon)
{
    duint16 port;
    duint16 servicePort;
    Block message;
    cplus::ref<iDatagram> socket;
    std::unique_ptr<Timer> timer;
    Time discoveryEndsAt;
    Map<Address, Block> found;

    Impl(Public *i) : Base(i) {}

    void continueDiscovery()
    {
        DE_ASSERT(timer);
        DE_ASSERT(socket);

        // Time to stop discovering?
        if (discoveryEndsAt.isValid() && Time() > discoveryEndsAt)
        {
            timer->stop();

            DE_FOR_PUBLIC_AUDIENCE2(Finished, i) { i->beaconFinished(); }

            socket.reset();
            trash(timer.release());
            return;
        }

        Block block(discoveryMessage);

        LOG_NET_XVERBOSE("Broadcasting %u bytes", block.size());

        // Send a new broadcast to the whole listening range of the beacons.
        for (duint16 i = 0; i < MAX_LISTEN_RANGE; ++i)
        {
            send_Datagram(socket, block, cplus::ref<iAddress>{newBroadcast_Address(port + i)});
        }
    }

    static void readIncoming(iAny *, iDatagram *sock)
{
        Loop::mainCall([sock]() {
    LOG_AS("Beacon");
            auto *d = reinterpret_cast<Beacon::Impl *>(userData_Object(sock));
            iAddress *from;
            while (const Block block = Block::take(receive_Datagram(sock, &from)))
    {
                LOG_NET_XVERBOSE("Received %i bytes from %s",
                                 block.size() << String::take(toString_Address(from)));
        if (block == discoveryMessage)
        {
            // Send a reply.
                    send_Datagram(sock, d->message, from);
        }
                iRelease(from);
    }
        });
}

    static void readDiscoveryReply(iAny *, iDatagram *sock)
{
        Loop::mainCall([sock]() {
    LOG_AS("Beacon");
            auto *d = reinterpret_cast<Beacon::Impl *>(userData_Object(sock));

            iAddress *from;
            while (Block block = Block::take(receive_Datagram(sock, &from)))
    {
        try
        {
                    if (block != discoveryMessage)
                    {
            // Remove the service listening port from the beginning.
            duint16 listenPort = 0;
            Reader(block) >> listenPort;
            block.remove(0, 2);
            block = block.decompressed();
                        if (block)
                        {
                            const Address host(cstr_String(hostName_Address(from)), listenPort);
            d->found.insert(host, block);

                            DE_FOR_EACH_OBSERVER(i, d->self().audienceForDiscovery())
                            {
                                i->beaconFoundHost(host, block);
                            }
                        }
                    }
        }
                catch (const Error &)
        {
            // Bogus reply message, ignore.
        }
                iRelease(from);
    }
        });
}

    DE_PIMPL_AUDIENCES(Discovery, Finished)
};

DE_AUDIENCE_METHODS(Beacon, Discovery, Finished)

Beacon::Beacon(duint16 port) : d(new Impl(this))
{
    d->port = port;
}

duint16 Beacon::port() const
{
    return d->port;
}

void Beacon::start(duint16 serviceListenPort)
{
    DE_ASSERT(!d->socket);

    d->servicePort = serviceListenPort;

    d->socket.reset(new_Datagram());
    setUserData_Object(d->socket, d);
    iConnect(Datagram, d->socket, message, d->socket, Impl::readIncoming);

    for (duint16 attempt = 0; attempt < MAX_LISTEN_RANGE; ++attempt)
    {
        if (open_Datagram(d->socket,
                          d->port ? d->port + attempt : duint16(Rangeui{MIN_PORT, 65536}.random())))
        {
            d->port = d->port + attempt;
            return;
        }
    }

    /// @throws PortError Could not open the UDP port.
    throw PortError("Beacon::start", "Could not bind to UDP port " + String::asText(d->port));
}

void Beacon::setMessage(IByteArray const &advertisedMessage)
{
    d->message.clear();

    // Begin with the service listening port.
    Writer(d->message) << d->servicePort;

    d->message += Block(advertisedMessage).compressed();

    //qDebug() << "Beacon message:" << advertisedMessage.size() << d->message.size();
}

void Beacon::stop()
{
    d->socket.reset();
}

void Beacon::discover(const TimeSpan& timeOut, const TimeSpan& interval)
{
    if (d->timer) return; // Already discovering.

    DE_ASSERT(!d->socket);

    d->socket.reset(new_Datagram());
    setUserData_Object(d->socket, d);
    iConnect(Datagram, d->socket, message, d->socket, Impl::readDiscoveryReply);

    Rangeui ports{max(d->port, MIN_PORT), 65536};

    // Choose a semi-random port for listening to replies from servers' beacons.
    int tries = 10;
    for (;;)
    {
        if (open_Datagram(d->socket, duint16(ports.random())))
        {
            // Got a port open successfully.
            break;
        }
        if (!--tries)
        {
            /// @throws PortError Could not open the UDP port.
            throw PortError("Beacon::start", "Could not bind to UDP port " + String::asText(d->port));
        }
    }

    d->found.clear();

    // Time-out timer.
    if (timeOut > 0.0)
    {
        d->discoveryEndsAt = Time() + timeOut;
    }
    else
    {
        d->discoveryEndsAt = Time::invalidTime();
    }
    d->timer.reset(new Timer);
    *d->timer += [this](){ d->continueDiscovery(); };
    d->timer->start(interval);

    d->continueDiscovery();
}

List<Address> Beacon::foundHosts() const
{
    return map<List<Address>>(d->found, [](const std::pair<Address, Block> &v){
        return v.first;
    });
}

Block Beacon::messageFromHost(Address const &host) const
{
    if (!d->found.contains(host)) return Block();
    return d->found[host];
}

} // namespace de
