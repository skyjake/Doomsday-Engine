/** @file serverinfo.h  Information about a multiplayer server.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "de/record.h"
#include "de/address.h"
#include "de/version.h"

namespace de {

// Default TCP/UDP port for servers to listen on.
static constexpr duint16 DEFAULT_PORT = 13209;

inline Address checkPort(const Address &address)
{
    if (address.port() == 0) return Address(address.hostName(), DEFAULT_PORT);
    return address;
}

/**
 * Information about a multiplayer server. @ingroup network
 */
class DE_PUBLIC ServerInfo
{
public:
    enum Flag { AllowJoin = 0x1, DefaultFlags = AllowJoin };

    ServerInfo();
    ServerInfo(const ServerInfo &other);
    ServerInfo(const Record &rec);
    ServerInfo &operator=(const ServerInfo &other);

    Version version() const;
    int     compatibilityVersion() const;

    Address    address() const;
    String     domainName() const;
    duint16    port() const;
    duint32    serverId() const;
    String     name() const;
    String     description() const;
    String     pluginDescription() const;
    StringList packages() const;
    String     gameId() const;
    String     gameConfig() const;
    String     map() const;
    StringList players() const;
    int        playerCount() const;
    int        maxPlayers() const;
    Flags      flags() const;

    String        asStyledText() const;
    Block         asJSON() const;
    const Record &asRecord() const;
    Record        strippedForBroadcast() const;

    ServerInfo &setCompatibilityVersion(int compatVersion);
    ServerInfo &setServerId(duint32 sid);
    ServerInfo &setAddress(const Address &address);
    ServerInfo &setDomainName(const String &domain);
    ServerInfo &setName(const String &name);
    ServerInfo &setDescription(const String &description);
    ServerInfo &setPluginDescription(const String &pluginDescription);
    ServerInfo &setPackages(StringList packages);
    ServerInfo &setGameId(const String &gameId);
    ServerInfo &setGameConfig(const String &gameConfig);
    ServerInfo &setMap(const String &map);
    ServerInfo &addPlayer(const String &playerName);
    ServerInfo &removePlayer(const String &playerName);
    ServerInfo &setMaxPlayers(int count);
    ServerInfo &setFlags(const Flags &flags);

    /**
     * Prints server/host information into the console. The header line is
     * printed if 'info' is NULL.
     */
    void printToLog(int indexNumber, bool includeHeader = false) const;

private:
    DE_PRIVATE(d)
};

} // namespace de
