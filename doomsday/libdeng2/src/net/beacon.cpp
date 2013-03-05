/** @file beacon.cpp  Presence service based on UDP broadcasts.
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

#include "de/Beacon"
#include "de/Reader"
#include "de/Writer"
#include <QUdpSocket>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QTimer>
#include <QMap>

namespace de {

/**
 * Maximum number of Beacon UDP ports in simultaneous use at one machine, i.e.,
 * maximum number of servers on one machine.
 */
static duint16 const MAX_LISTEN_RANGE = 16;

static char const *discoveryMessage = "Doomsday Beacon 1.0";

struct Beacon::Instance
{
    duint16 port;
    duint16 servicePort;
    QUdpSocket *socket;
    Block message;
    QTimer *timer;
    Time discoveryEndsAt;
    QMap<Address, Block> found;

    Instance() : socket(0), timer(0)
    {}

    ~Instance()
    {
        delete socket;
        delete timer;
    }
};

Beacon::Beacon(duint16 port) : d(new Instance)
{
    d->port = port;
}

Beacon::~Beacon()
{}

duint16 Beacon::port() const
{
    return d->port;
}

void Beacon::start(duint16 serviceListenPort)
{
    DENG2_ASSERT(!d->socket);

    d->servicePort = serviceListenPort;

    d->socket = new QUdpSocket;
    connect(d->socket, SIGNAL(readyRead()), this, SLOT(readIncoming()));

    for(duint16 attempt = 0; attempt < MAX_LISTEN_RANGE; ++attempt)
    {
        if(d->socket->bind(d->port + attempt, QUdpSocket::DontShareAddress))
        {
            d->port = d->port + attempt;
            return;
        }
    }

    /// @throws PortError Could not open the UDP port.
    throw PortError("Beacon::start", "Could not bind to UDP port " + String::number(d->port));
}

void Beacon::setMessage(IByteArray const &advertisedMessage)
{
    d->message.clear();

    // Begin with the service listening port.
    Writer(d->message) << d->servicePort;

    d->message += Block(advertisedMessage);
}

void Beacon::stop()
{
    delete d->socket;
    d->socket = 0;
}

void Beacon::discover(TimeDelta const &timeOut, TimeDelta const &interval)
{
    if(d->timer) return; // Already discovering.

    DENG2_ASSERT(!d->socket);

    d->socket = new QUdpSocket;
    connect(d->socket, SIGNAL(readyRead()), this, SLOT(readDiscoveryReply()));

    // Choose a semi-random port for listening to replies from servers' beacons.
    int tries = 10;
    forever
    {
        if(d->socket->bind(d->port + 1 + qrand() % 0x4000, QUdpSocket::DontShareAddress))
        {
            // Got a port open successfully.
            break;
        }
        if(!--tries)
        {
            /// @throws PortError Could not open the UDP port.
            throw PortError("Beacon::start", "Could not bind to UDP port " + String::number(d->port));
        }
    }

    d->found.clear();

    // Time-out timer.
    if(timeOut > 0.0)
    {
        d->discoveryEndsAt = Time() + timeOut;
    }
    else
    {
        d->discoveryEndsAt = Time::invalidTime();
    }
    d->timer = new QTimer;
    connect(d->timer, SIGNAL(timeout()), this, SLOT(continueDiscovery()));
    d->timer->start(interval.asMilliSeconds());

    continueDiscovery();
}

QList<Address> Beacon::foundHosts() const
{
    return d->found.keys();
}

Block Beacon::messageFromHost(Address const &host) const
{
    if(!d->found.contains(host)) return Block();
    return d->found[host];
}

void Beacon::readIncoming()
{
    LOG_AS("Beacon");

    if(!d->socket) return;

    while(d->socket->hasPendingDatagrams())
    {
        QHostAddress from;
        quint16 port = 0;
        Block block(d->socket->pendingDatagramSize());
        d->socket->readDatagram(reinterpret_cast<char *>(block.data()),
                                block.size(), &from, &port);

        LOG_TRACE("Received %i bytes from %s port %i") << block.size() << from.toString() << port;

        if(block == discoveryMessage)
        {
            // Send a reply.
            d->socket->writeDatagram(d->message, from, port);
        }
    }
}

void Beacon::readDiscoveryReply()
{
    LOG_AS("Beacon");

    if(!d->socket) return;

    while(d->socket->hasPendingDatagrams())
    {
        QHostAddress from;
        quint16 port = 0;
        Block block(d->socket->pendingDatagramSize());
        d->socket->readDatagram(reinterpret_cast<char *>(block.data()),
                                block.size(), &from, &port);

        if(block == discoveryMessage)
            continue;

        try
        {
            // Remove the service listening port from the beginning.
            duint16 listenPort = 0;
            Reader(block) >> listenPort;
            block.remove(0, 2);

            Address host(from, listenPort);
            d->found.insert(host, block);

            emit found(host, block);
        }
        catch(Error const &)
        {
            // Bogus message, ignore.
        }
    }
}

void Beacon::continueDiscovery()
{
    DENG2_ASSERT(d->socket);
    DENG2_ASSERT(d->timer);

    // Time to stop discovering?
    if(d->discoveryEndsAt.isValid() && Time() > d->discoveryEndsAt)
    {
        d->timer->stop();

        emit finished();

        d->socket->deleteLater();
        d->socket = 0;

        d->timer->deleteLater();
        d->timer = 0;
        return;
    }

    Block block(discoveryMessage);

    LOG_TRACE("Broadcasting %i bytes") << block.size();

    // Send a new broadcast to the whole listening range of the beacons.
    for(duint16 range = 0; range < MAX_LISTEN_RANGE; ++range)
    {
        d->socket->writeDatagram(block,
                                 QHostAddress::Broadcast,
                                 d->port + range);
    }
}

} // namespace de
