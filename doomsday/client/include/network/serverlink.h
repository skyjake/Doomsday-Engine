/** @file serverlink.h  Network connection to a server.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_SERVERLINK_H
#define CLIENT_SERVERLINK_H

#include <de/Transmitter>
#include <de/Address>
#include <de/Observers>
#include <de/shell/AbstractLink>
#include <QObject>
#include "network/net_main.h"

/**
 * Network connection to a server.
 * @ingroup network
 */
class ServerLink : public de::shell::AbstractLink
{
    Q_OBJECT

public:
    DENG2_DEFINE_AUDIENCE(DiscoveryUpdate, void linkDiscoveryUpdate(ServerLink const &link))

    DENG2_DEFINE_AUDIENCE(Join,  void networkGameJoined())
    DENG2_DEFINE_AUDIENCE(Leave, void networkGameLeft())

public:
    ServerLink();

    void clear();

    void connectDomain(de::String const &domain, de::TimeDelta const &timeout = 0);
    void connectHost(de::Address const &address);

    /**
     * Disconnect from the server.
     */
    void disconnect();

    /**
     * Attempts to connect to the specified address and asks for server
     * information if one happens to be running.
     *
     * @param domain
     */
    void discover(de::String const &domain);

    /**
     * Asks the master server for information about currently running servers.
     */
    void discoverUsingMaster();

    bool isDiscovering() const;

    enum FoundMaskFlag
    {
        Direct = 0x1,
        LocalNetwork = 0x2,
        MasterServer = 0x4,

        Any = Direct | LocalNetwork | MasterServer
    };
    Q_DECLARE_FLAGS(FoundMask, FoundMaskFlag)

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    int foundServerCount(FoundMask mask = Any) const;

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    QList<de::Address> foundServers(FoundMask mask = Any) const;

    bool isFound(de::Address const &host, FoundMask mask = Any) const;

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    bool foundServerInfo(de::Address const &host, serverinfo_t *info,
                         FoundMask mask = Any) const;

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    bool foundServerInfo(int index, serverinfo_t *info,
                         FoundMask mask = Any) const;

signals:
    void serversDiscovered();

public slots:
    void handleIncomingPackets();

protected slots:
    void localServersFound();
    void linkDisconnected();

protected:
    de::Packet *interpret(de::Message const &msg);
    void initiateCommunications();

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ServerLink::FoundMask)

#endif // CLIENT_LINK_H
