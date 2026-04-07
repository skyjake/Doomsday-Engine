/** @file serverlink.h  Network connection to a server.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <doomsday/network/protocol.h>
#include <de/transmitter.h>
#include <de/address.h>
#include <de/observers.h>
#include <de/abstractlink.h>
#include <de/serverinfo.h>
#include <de/packagedownloader.h>
#include "network/net_main.h"

/**
 * Network connection to a server.
 * @ingroup network
 */
class ServerLink : public de::AbstractLink
{
public:
    DE_AUDIENCE(Discovery,    void serversDiscovered(const ServerLink &link))
    DE_AUDIENCE(PingResponse, void pingResponse(const de::Address &, de::TimeSpan))
    DE_AUDIENCE(MapOutline,   void mapOutlineReceived(const de::Address &, const network::MapOutlinePacket &))

    DE_AUDIENCE(Join,  void networkGameJoined())
    DE_AUDIENCE(Leave, void networkGameLeft())

    enum Flag {
        DiscoverLocalServers = 0x1,
        ManualConnectionOnly = 0,
    };

    static ServerLink &get();

public:
    ServerLink(de::Flags flags = DiscoverLocalServers);

    de::PackageDownloader &packageDownloader();

    void clear();

    /**
     * Connect to a server after loading the game running on the server and making
     * sure that all of the server's gameplay-affecting packages have been loaded
     * locally.
     *
     * @param info  Server to join. This should be one of the servers that have
     *              previously been found via discovery.
     */
    void connectToServerAndChangeGameAsync(const de::ServerInfo& info);

    /**
     * Acquire a game profile that describes the game on a multiplayer server.
     * If information about the server at @a address is not currently available, a
     * discovery query is sent to the address.
     *
     * After the server's profile is available, a callback is made via Dispatch.
     *
     * @param address        Server address.
     * @param resultHandler  Callback for receiving the server profile. ServerLink
     *                       retains ownership of the profile. The profile is @c nullptr
     *                       if server discovery fails and the profile is not available.
     *                       The callback is called in the main thread (from the app
     *                       event loop).
     */
    void acquireServerProfileAsync(const de::Address &address,
                                   const std::function<void (const GameProfile *)>& resultHandler);

    void acquireServerProfileAsync(const de::String &domain,
                                   std::function<void (de::Address, const GameProfile *)> resultHandler);

    void requestMapOutline(const de::Address &address);

    void ping(const de::Address &address);

    void connectDomain(const de::String &domain, de::TimeSpan timeout = 0.0) override;
    void connectHost(const de::Address &address) override;

    /**
     * Disconnect from the server.
     */
    void disconnect() override;

    /**
     * Attempt to connect to the specified address and asks for server information if one
     * happens to be running.
     *
     * @param domain
     */
    void discover(const de::String &domain);

    /**
     * Ask the master server for information about currently running servers.
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
    using FoundMask = de::Flags;

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    int foundServerCount(FoundMask mask = Any) const;

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    de::List<de::Address> foundServers(FoundMask mask = Any) const;

    bool isFound(const de::Address &host, FoundMask mask = Any) const;

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    bool foundServerInfo(const de::Address &host, de::ServerInfo &info,
                         FoundMask mask = Any) const;

    /**
     * @param mask  Defines the sources that are enabled when querying for found servers.
     */
    bool foundServerInfo(int index, de::ServerInfo &info,
                         FoundMask mask = Any) const;

    bool isServerOnLocalNetwork(const de::Address &host) const;

public:
    void handleIncomingPackets();

protected:
    void localServersFound();
    void linkDisconnected();

    de::Packet *interpret(const de::Message &msg) override;
    void initiateCommunications() override;

private:
    DE_PRIVATE(d)
};

#endif // CLIENT_LINK_H
