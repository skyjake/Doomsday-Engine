/** @file beacon.h  Presence service based on UDP broadcasts.
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

#ifndef LIBDENG2_BEACON_H
#define LIBDENG2_BEACON_H

#include <QObject>
#include "../Error"
#include "../Block"
#include "../Address"

namespace de {

/**
 * UDP-based peer discovery mechanism.
 * @ingroup net
 */
class DENG2_PUBLIC Beacon : public QObject
{
    Q_OBJECT

public:
    /// The UDP port was unavailable. @ingroup errors
    DENG2_ERROR(PortError);

public:
    Beacon(duint16 port);

    /**
     * Port the beacon uses for listening.
     * @return  Port.
     */
    duint16 port() const;

    /**
     * Starts the beacon with a message to give out.
     *
     * @param serviceListenPort
     *      TCP port that the advertised service listens on. Recipients will
     *      pair this with the IP address to form a full address.
     */
    void start(duint16 serviceListenPort);

    /**
     * Changes the message to advertise.
     *
     * @param advertisedMessage  Message to send to requesters.
     */
    void setMessage(IByteArray const &advertisedMessage);

    /**
     * Stops the beacon.
     */
    void stop();

    /**
     * Looks for any beacons on all accessible networks.
     *
     * @param timeOut   Maximum time to spend discovering. If the timeout
     *                  is zero or negative, discovery will not end.
     * @param interval  Interval between query broadcasts.
     */
    void discover(TimeDelta const &timeOut, TimeDelta const &interval = TimeDelta(1.0));

    QList<Address> foundHosts() const;
    Block messageFromHost(Address const &host) const;

protected slots:
    void readIncoming();
    void readDiscoveryReply();
    void continueDiscovery();

signals:
    void found(de::Address host, de::Block message);
    void finished();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_BEACON_H
