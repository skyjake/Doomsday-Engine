/** @file abstractlink.h  Network connection to a server.
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

#include "de/shell/AbstractLink"
#include <de/Message>
#include <de/Socket>
#include <de/Time>
#include <de/Log>
#include <de/Packet>
#include <QTimer>

namespace de {
namespace shell {

DENG2_PIMPL(AbstractLink)
{
    String tryingToConnectToHost;
    Time startedTryingAt;
    TimeDelta timeout;
    Address peerAddress;
    std::auto_ptr<Socket> socket;
    Status status;
    Time connectedAt;

    Instance(Public *i)
        : Base(i),
          status(Disconnected),
          connectedAt(Time::invalidTime()) {}
};

AbstractLink::AbstractLink() : d(new Instance(this))
{}

AbstractLink::~AbstractLink()
{
    delete d;
}

void AbstractLink::connectDomain(String const &domain, TimeDelta const &timeout)
{
    disconnect();

    d->socket.reset(new Socket);

    connect(d->socket.get(), SIGNAL(addressResolved()), this, SIGNAL(addressResolved()));
    connect(d->socket.get(), SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(d->socket.get(), SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
    connect(d->socket.get(), SIGNAL(messagesReady()), this, SIGNAL(packetsReady()));

    // Fallback to default port.
    d->tryingToConnectToHost = domain;
    d->socket->setQuiet(true); // we'll be retrying a few times
    d->socket->connectToDomain(d->tryingToConnectToHost, 13209 /* default port */);

    d->status = Connecting;
    d->startedTryingAt = Time();
    d->timeout = timeout;
}

void AbstractLink::connectHost(Address const &address)
{
    disconnect();

    d->peerAddress = address;
    d->socket.reset(new Socket);

    connect(d->socket.get(), SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(d->socket.get(), SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
    connect(d->socket.get(), SIGNAL(messagesReady()), this, SIGNAL(packetsReady()));

    // Fallback to default port.
    if(!d->peerAddress.port()) d->peerAddress.setPort(13209);

    d->socket->connect(d->peerAddress);

    d->status = Connecting;
    d->startedTryingAt = Time();
    d->timeout = 0;
}

void AbstractLink::takeOver(Socket *openSocket)
{
    disconnect();

    d->peerAddress = openSocket->peerAddress();
    d->socket.reset(openSocket);

    // Note: socketConnected() not used because the socket is already open.
    connect(d->socket.get(), SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
    connect(d->socket.get(), SIGNAL(messagesReady()), this, SIGNAL(packetsReady()));

    d->status = Connected;
    d->connectedAt = Time();
}

void AbstractLink::disconnect()
{
    if(d->status != Disconnected)
    {
        DENG2_ASSERT(d->socket.get() != 0);

        d->timeout = 0;
        d->socket->close(); // emits signal

        QObject::disconnect(d->socket.get(), SIGNAL(addressResolved()), this, SIGNAL(addressResolved()));
        QObject::disconnect(d->socket.get(), SIGNAL(connected()), this, SLOT(socketConnected()));
        QObject::disconnect(d->socket.get(), SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
        QObject::disconnect(d->socket.get(), SIGNAL(messagesReady()), this, SIGNAL(packetsReady()));
    }

    DENG2_ASSERT(d->status == Disconnected);
}

Address AbstractLink::address() const
{
    if(d->socket->isOpen()) return d->socket->peerAddress();
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
    if(!d->socket->hasIncoming()) return 0;

    std::auto_ptr<Message> data(d->socket->receive());
    Packet *packet = interpret(*data.get());
    if(packet) packet->setFrom(data->address());
    return packet;
}

void AbstractLink::send(IByteArray const &data)
{
    d->socket->send(data);
}

void AbstractLink::socketConnected()
{
    LOG_AS("AbstractLink");
    LOG_VERBOSE("Successfully connected to server %s") << d->socket->peerAddress();

    initiateCommunications();

    d->status = Connected;
    d->connectedAt = Time();
    d->peerAddress = d->socket->peerAddress();

    emit connected();
}

void AbstractLink::socketDisconnected()
{
    LOG_AS("AbstractLink");

    if(d->status == Connecting)
    {
        if(d->startedTryingAt.since() < d->timeout)
        {
            // Let's try again a bit later.
            QTimer::singleShot(500, d->socket.get(), SLOT(reconnect()));
            return;
        }
        d->socket->setQuiet(false);
    }

    if(!d->peerAddress.isNull())
    {
        LOG_INFO("Disconnected from %s") << d->peerAddress;
    }
    else
    {
        LOG_INFO("Disconnected");
    }

    d->status = Disconnected;

    emit disconnected();

    // Slots have now had an opportunity to observe the total
    // duration of the connection that has just ended.
    d->connectedAt = Time::invalidTime();
}

} // namespace shell
} // namespace de
