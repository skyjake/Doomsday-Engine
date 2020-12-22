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

#include "de/beacon.h"

#include "de/garbage.h"
#include "de/logbuffer.h"
#include "de/loop.h"
#include "de/keymap.h"
#include "de/reader.h"
#include "de/timer.h"
#include "de/writer.h"

#include <the_Foundation/address.h>
#include <the_Foundation/datagram.h>

namespace de {

/*
 * Revisions:
 *
 * 1.0: Initial version.
 * 1.1: Advertised message is compressed with zlib (deflate).
 * 1.2: No port number prefixed.
 */
static const char *discoveryMessage = "Doomsday Beacon 1.2";

DE_PIMPL(Beacon)
{
    Rangeui16              udpPorts;
    duint16                listenPort;
    Block                  message;
    tF::ref<iDatagram>     socket;
    std::unique_ptr<Timer> timer;
    Time                   discoveryEndsAt;
    KeyMap<Address, Block>    found;
    List<tF::ref<iAddress>> broadcastAddresses;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        if (socket)
        {
            iDisconnect(Datagram, socket, message, socket, readIncoming);
            iDisconnect(Datagram, socket, message, socket, readDiscoveryReply);
        }
    }

    void continueDiscovery()
    {
        if (!timer || !socket) return;

        // Time to stop discovering?
        if (discoveryEndsAt.isValid() && Time() > discoveryEndsAt)
        {
            timer->stop();

            DE_NOTIFY_PUBLIC(Finished, i) { i->beaconFinished(); }

            socket.reset();
            trash(timer.release());
            listenPort = 0;
            return;
        }

        const Block block(discoveryMessage);

        LOG_NET_XVERBOSE("Broadcasting %u bytes", block.size());

        // Send a new broadcast to the whole listening range of the beacons.
        for (const auto &addr : broadcastAddresses)
        {
            send_Datagram(socket, block, addr);
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

    static void readDiscoveryReply(iAny *, iDatagram * sock)
    {
        Loop::mainCall([sock]() {
            LOG_AS("Beacon");
            auto *    d = reinterpret_cast<Beacon::Impl *>(userData_Object(sock));
            iAddress *from;
            while (Block block = Block::take(receive_Datagram(sock, &from)))
            {
                try
                {
                    if (block != discoveryMessage)
                    {
                        // Remove the service listening port from the beginning.
                        //                        duint16 listenPort = 0;
                        //                        Reader(block) >> listenPort;
                        //                        block.remove(0, 2);
                        block = block.decompressed();
                        if (block)
                        {
                            const Address host(from);
                            d->found.insert(host, block);
                            DE_FOR_OBSERVERS(i, d->self().audienceForDiscovery())
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

Beacon::Beacon(const Rangeui16 &udpPorts) : d(new Impl(this))
{
    d->udpPorts = udpPorts;
    d->listenPort = 0;
}

duint16 Beacon::port() const
{
    return d->listenPort;
}

void Beacon::start()
{
    DE_ASSERT(!d->socket);

    d->socket = tF::make_ref(new_Datagram());
    setUserData_Object(d->socket, d);
    iConnect(Datagram, d->socket, message, d->socket, Impl::readIncoming);

    for (duint p = d->udpPorts.start; p < d->udpPorts.end; ++p)
    {
        if (open_Datagram(d->socket, duint16(p)))
        {
            d->listenPort = duint16(p);
            return;
        }
    }

    /// @throws PortError Could not open the UDP port.
    throw PortError(
        "Beacon::start",
        stringf("Could not bind to UDP ports %u...%u", d->udpPorts.start, d->udpPorts.end));
}

void Beacon::setMessage(const IByteArray &advertisedMessage)
{
    d->message = Block(advertisedMessage).compressed();
}

void Beacon::stop()
{
    d->socket.reset();
    if (d->timer) d->timer->stop();
    d->listenPort = 0;
}

void Beacon::discover(const TimeSpan& timeOut, const TimeSpan& interval)
{
    if (d->timer) return; // Already discovering.

    DE_ASSERT(!d->socket);

    d->socket.reset(new_Datagram());
    setUserData_Object(d->socket, d);
    iConnect(Datagram, d->socket, message, d->socket, Impl::readDiscoveryReply);

    for (duint p = d->udpPorts.start; p < d->udpPorts.end; ++p)
    {
        if (open_Datagram(d->socket, duint16(p)))
        {
            // Got a port open successfully.
            d->listenPort = duint16(p);
            break;
        }
        }
    if (!isOpen_Datagram(d->socket))
        {
            /// @throws PortError Could not open the UDP port.
        throw PortError(
            "Beacon::discover",
            stringf("Could not bind to UDP ports %u...%u", d->udpPorts.start, d->udpPorts.end));
    }

    d->found.clear();

    // Set up the broadcast range in advance.
    d->broadcastAddresses.clear();
    for (duint p = d->udpPorts.start; p < d->udpPorts.end; ++p)
    {
        d->broadcastAddresses << tF::make_ref(newBroadcast_Address(duint16(p)));
    }

    // Time-out timer.
    if (timeOut > 0.0)
    {
        d->discoveryEndsAt = Time() + timeOut;
    }
    else
    {
        d->discoveryEndsAt = Time::invalidTime(); // continues indefinitely
    }
    d->timer.reset(new Timer);
    *d->timer += [this]() { d->continueDiscovery(); };
    d->timer->start(interval);
    d->continueDiscovery();
}

List<Address> Beacon::foundHosts() const
{
    return map<List<Address>>(d->found, [](const std::pair<Address, Block> &v){
        return v.first;
    });
}

Block Beacon::messageFromHost(const Address &host) const
{
    if (!d->found.contains(host)) return Block();
    return d->found[host];
}

} // namespace de
