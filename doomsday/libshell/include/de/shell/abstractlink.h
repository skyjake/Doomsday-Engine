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

#ifndef LIBSHELL_ABSTRACTLINK_H
#define LIBSHELL_ABSTRACTLINK_H

#include "libshell.h"
#include <de/Address>
#include <de/Socket>
#include <de/Time>
#include <de/Transmitter>
#include <QObject>

namespace de {
namespace shell {

/**
 * Abstract network connection to a server.
 *
 * Derived implementations must provide a protocol for incoming messages.
 */
class LIBSHELL_PUBLIC AbstractLink : public QObject, public Transmitter
{
    Q_OBJECT

public:
    enum Status
    {
        Disconnected,
        Connecting,
        Connected
    };

public:
    AbstractLink();

    /**
     * Opens a connection to a server over the network.
     *
     * @param domain   Domain/IP address of the server.
     * @param timeout  Keep trying until this much time has passed.
     */
    virtual void connectDomain(String const &domain, TimeDelta const &timeout = 0);

    /**
     * Opens a connection to a server over the network.
     *
     * @param address  Address of the server.
     */
    virtual void connectHost(Address const &address);

    /**
     * Takes over an existing socket.
     *
     * @param openSocket  Socket. AbstractLink takes ownership.
     */
    virtual void takeOver(Socket *openSocket);

    /**
     * Closes the connection.
     */
    virtual void disconnect();

    /**
     * Peer address of the link. The address may be a null address if the IP
     * address hasn't been resolved yet.
     */
    Address address() const;

    /**
     * Current status of the connection.
     */
    Status status() const;

    /**
     * Returns the time when the link was successfully connected.
     */
    Time connectedAt() const;

    /**
     * Returns the next received packet. The packet has been interpreted
     * using the virtual interpret() method.
     *
     * @return Received packet. Ownership given to caller. Returns @c NULL if
     * there are no more packets ready.
     */
    Packet *nextPacket();

    // Transmitter.
    void send(IByteArray const &data);

protected:
    virtual Packet *interpret(Message const &msg) = 0;

    /**
     * Called immediately after a connection has been formed.
     */
    virtual void initiateCommunications() = 0;

protected slots:
    void socketConnected();
    void socketDisconnected();

signals:
    void addressResolved();
    void connected();
    void disconnected();
    void packetsReady();

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_ABSTRACTLINK_H
