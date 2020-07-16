/** @file beacon.h  Presence service based on UDP broadcasts.
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

#ifndef LIBCORE_BEACON_H
#define LIBCORE_BEACON_H

#include "de/error.h"
#include "de/block.h"
#include "de/address.h"
#include "de/observers.h"

namespace de {

/**
 * UDP-based peer discovery mechanism.
 *
 * Beacon can work either in advertising or discovery mode. In advertising mode, one must
 * specify the port number when the Beacon is constructed. Beacon will attempt to communicate
 * using the specified port only. In discovery mode, a port is selected at random.
 *
 * @ingroup net
 */
class DE_PUBLIC Beacon
{
public:
    /// The UDP port was unavailable. @ingroup errors
    DE_ERROR(PortError);

    DE_AUDIENCE(Discovery, void beaconFoundHost(const Address &host, const Block &message))
    DE_AUDIENCE(Finished,  void beaconFinished())

public:
    /**
     * @param udpPorts  Range of ports that the Beacon will use for communications. The lowest
     *                  available port in this range is used. Having a larger range of ports
     *                  means that multiple Beacons can coexist (e.g., multiple servers on the
     *                  local computer). Note that discovery messages are broadcasted to the
     *                  entire range so it should not be overly large.
     */
    Beacon(const Rangeui16 &udpPorts);

    /**
     * Returns the port the beacon has chosen for communications.
     */
    duint16 port() const;

    /**
     * Changes the message to advertise.
     *
     * @param advertisedMessage  Message to send to requesters.
     */
    void setMessage(const IByteArray &advertisedMessage);

    /**
     * Starts the beacon with a message to give out.
     */
    void start();

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
    void discover(const TimeSpan& timeOut, const TimeSpan& interval = 1.0);

    List<Address> foundHosts() const;
    Block messageFromHost(const Address &host) const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_BEACON_H
