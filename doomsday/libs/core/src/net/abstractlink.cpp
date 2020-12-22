/** @file abstractlink.h  Network connection to a server.
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

#include "de/abstractlink.h"
#include "de/app.h"
#include "de/log.h"
#include "de/message.h"
#include "de/packet.h"
#include "de/serverinfo.h"
#include "de/socket.h"
#include "de/time.h"

namespace de {

DE_PIMPL(AbstractLink)
, DE_OBSERVES(Socket, StateChange)
{
    String   tryingToConnectToHost;
    Time     startedTryingAt;
    TimeSpan timeout;
    Address  peerAddress;
    Status   status;
    Time     connectedAt;

    std::unique_ptr<Socket> socket;

    Impl(Public *i)
        : Base(i)
        , status(Disconnected)
        , connectedAt(Time::invalidTime())
    {}
    
    ~Impl() override
    {
        if (socket) socket->audienceForStateChange() -= this;
    }
    
    void socketStateChanged(Socket &, Socket::SocketState state) override
    {
        switch (state)
        {
        case Socket::AddressResolved:
            DE_NOTIFY_PUBLIC(AddressResolved, i)
            {
                i->addressResolved();
            }
            break;

        case Socket::Connected:
            socketConnected();
            break;

        case Socket::Disconnected:
            socketDisconnected();
            break;
        }
    }

    void socketConnected()
    {
        LOG_AS("AbstractLink");
        LOG_NET_VERBOSE("Successfully connected to server %s") << socket->peerAddress();

        self().initiateCommunications();

        status      = Connected;
        connectedAt = Time();
        peerAddress = socket->peerAddress();

        DE_NOTIFY_PUBLIC(Connected, i) i->connected();
    }

    void socketDisconnected()
    {
        LOG_AS("AbstractLink");

        if (status == Connecting)
        {
            if (startedTryingAt.since() < timeout)
            {
                // Let's try again a bit later.
//                Timer::singleShot(500, d->socket.get(), SLOT(reconnect()));
                /// @todo Implement reconnect attempt.
                return;
            }
            socket->setQuiet(false);
        }
        else
        {
            if (!peerAddress.isNull())
            {
                LOG_NET_NOTE("Disconnected from %s") << peerAddress;
            }
            else
            {
                LOG_NET_NOTE("Disconnected");
            }
        }

        status = Disconnected;

        DE_NOTIFY_PUBLIC(Disconnected, i) i->disconnected();

        // Slots have now had an opportunity to observe the total
        // duration of the connection that has just ended.
        connectedAt = Time::invalidTime();
    }

    DE_PIMPL_AUDIENCES(Connected, Disconnected, PacketsReady, AddressResolved)
};

DE_AUDIENCE_METHODS(AbstractLink, Connected, Disconnected, PacketsReady, AddressResolved)

AbstractLink::AbstractLink() : d(new Impl(this))
{}

void AbstractLink::connectDomain(const String &domain, TimeSpan timeout)
{
    disconnect();

    d->socket.reset(new Socket);
    d->socket->audienceForStateChange() += d;
    d->socket->audienceForMessage() += [this]() {
        DE_NOTIFY(PacketsReady, i) i->packetsReady();
    };

    // Fallback to default port.
    d->tryingToConnectToHost = domain;
    d->socket->setQuiet(true); // we'll be retrying a few times
    d->socket->open(d->tryingToConnectToHost, DEFAULT_PORT);

    d->status          = Connecting;
    d->startedTryingAt = Time();
    d->timeout         = timeout;
}

void AbstractLink::connectHost(const Address &address)
{
    disconnect();

    d->peerAddress = address;
    d->socket.reset(new Socket);

    d->socket->audienceForStateChange() += d;
    d->socket->audienceForMessage() += [this]() {
        DE_NOTIFY(PacketsReady, i) i->packetsReady();
    };

    // Fallback to default port.
    if (!d->peerAddress.port())
    {
        d->peerAddress = Address(d->peerAddress.hostName(), DEFAULT_PORT);
    }
    d->socket->open(d->peerAddress);

    d->status          = Connecting;
    d->startedTryingAt = Time();
    d->timeout         = 0.0;
}

void AbstractLink::takeOver(Socket *openSocket)
{
    disconnect();

    d->peerAddress = openSocket->peerAddress();
    d->socket.reset(openSocket);

    // Note: socketConnected() not used because the socket is already open.
    d->socket->audienceForStateChange() += d;
    d->socket->audienceForMessage() += [this]() {
        DE_NOTIFY(PacketsReady, i) i->packetsReady();
    };

    d->status      = Connected;
    d->connectedAt = Time();
}

void AbstractLink::disconnect()
{
    if (d->status != Disconnected)
    {
        DE_ASSERT(d->socket.get() != nullptr);

        d->timeout = 0.0;
        d->socket->close(); // emits signal

        d->status = Disconnected;

        d->socket->audienceForStateChange() -= d;
//        QObject::disconnect(d->socket.get(), SIGNAL(addressResolved()), this, SIGNAL(addressResolved()));
//        QObject::disconnect(d->socket.get(), SIGNAL(connected()), this, SLOT(socketConnected()));
//        QObject::disconnect(d->socket.get(), SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
//        QObject::disconnect(d->socket.get(), SIGNAL(messagesReady()), this, SIGNAL(packetsReady()));
    }
}

Address AbstractLink::address() const
{
    if (!d->socket) return Address();
    if (d->socket->isOpen()) return d->socket->peerAddress();
    return d->peerAddress;
}

AbstractLink::Status AbstractLink::status() const
{
    return d->status;
}

Time AbstractLink::connectedAt() const
{
    return d->connectedAt;
}

Packet *AbstractLink::nextPacket()
{
    if (!d->socket->hasIncoming())
    {
        return nullptr;
    }
    std::unique_ptr<Message> data(d->socket->receive());
    Packet *packet = interpret(*data.get());
    if (packet) packet->setFrom(data->address());
    return packet;
}

void AbstractLink::send(const IByteArray &data)
{
    d->socket->send(data);
}

} // namespace de
