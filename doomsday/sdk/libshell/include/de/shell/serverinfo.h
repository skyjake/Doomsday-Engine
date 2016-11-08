/** @file serverinfo.h  Information about a multiplayer server.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_SERVERINFO_H
#define LIBDOOMSDAY_SERVERINFO_H

#include "libshell.h"
#include <de/Record>
#include <de/Address>
#include <de/Version>

namespace de {
namespace shell {

/**
 * Information about a multiplayer server. @ingroup network
 */
class LIBSHELL_PUBLIC ServerInfo : public Record
{
public:
    enum Flag
    {
        AllowJoin    = 0x1,
        DefaultFlags = AllowJoin
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    ServerInfo();
    ServerInfo(ServerInfo const &other);
    ServerInfo(ServerInfo &&moved);
    ServerInfo(Record const &rec);
    ServerInfo &operator = (ServerInfo const &other);
    ServerInfo &operator = (ServerInfo &&moved);

    Version version() const;
    int compatibilityVersion() const;

    Address address() const;
    duint16 port() const;
    String name() const;
    String description() const;
    String pluginDescription() const;
    StringList packages() const;
    String gameId() const;
    String gameConfig() const;
    String map() const;
    StringList players() const;
    int playerCount() const;
    int maxPlayers() const;
    Flags flags() const;

    String asStyledText() const;
    Block asJSON() const;
    Record strippedForBroadcast() const;

    ServerInfo &setCompatibilityVersion(int compatVersion);
    ServerInfo &setAddress(Address const &address);
    ServerInfo &setName(String const &name);
    ServerInfo &setDescription(String const &description);
    ServerInfo &setPluginDescription(String const &pluginDescription);
    ServerInfo &setPackages(StringList const &packages);
    ServerInfo &setGameId(String const &gameId);
    ServerInfo &setGameConfig(String const &gameConfig);
    ServerInfo &setMap(String const &map);
    ServerInfo &addPlayer(String const &playerName);
    ServerInfo &removePlayer(String const &playerName);
    ServerInfo &setMaxPlayers(int count);
    ServerInfo &setFlags(Flags const &flags);

    //duint32 iwadDirectoryCRC32() const;

    //int             version;
    //char            name[64];
    //char            description[80];
    //int             numPlayers, maxPlayers;
    //char            canJoin;
    //char            address[64];
    //int             port;
    //unsigned short  ping;       ///< Milliseconds.
    //char            plugin[32]; ///< Game plugin and version.
    //char            gameIdentityKey[17];
    //char            gameConfig[40];
    //char            map[20];
    //char            clientNames[128];
    //unsigned int    loadedFilesCRC;
    //char            iwad[32];   ///< Obsolete.
    //char            pwads[128];
    //int             data[3];

    /**
     * Prints server/host information into the console. The header line is
     * printed if 'info' is NULL.
     */
    void printToLog(int indexNumber, bool includeHeader = false) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ServerInfo::Flags)

} // namespace shell
} // namespace de

#endif // SERVERINFO_H
