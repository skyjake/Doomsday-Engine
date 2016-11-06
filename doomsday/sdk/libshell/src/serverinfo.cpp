/** @file serverinfo.cpp  Information about a multiplayer server.
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

#include "de/shell/ServerInfo"
#include <de/TextValue>
#include <de/Version>
#include <de/data/json.h>

namespace de {
namespace shell {

static String const VAR_VERSION                 ("version");
static String const VAR_PROTOCOL_VERSION        ("proto");
static String const VAR_COMPATIBILITY_VERSION   ("compat");
static String const VAR_HOST                    ("host");
static String const VAR_NAME                    ("name");
static String const VAR_DESCRIPTION             ("desc");
static String const VAR_PLUGIN                  ("plugin");
static String const VAR_PACKAGES                ("pkgs");
static String const VAR_GAME_ID                 ("game");
static String const VAR_GAME_CONFIG             ("conf");
static String const VAR_MAP                     ("map");
static String const VAR_PLAYERS                 ("players");
static String const VAR_MAX_PLAYERS             ("plmax");
static String const VAR_FLAGS                   ("flags");

ServerInfo::ServerInfo()
{
    set(VAR_VERSION, Version::currentBuild().asText());
    set(VAR_PROTOCOL_VERSION, DENG2_PROTOCOL_LATEST);
    addArray(VAR_PLAYERS);
}

ServerInfo::ServerInfo(Record const &rec)
    : Record(rec)
{}

ServerInfo::ServerInfo(ServerInfo &&moved)
    : Record(std::move(moved))
{}

ServerInfo &ServerInfo::operator = (ServerInfo &&moved)
{
    Record::operator = (moved);
    return *this;
}

Version ServerInfo::version() const
{
    return Version(gets(VAR_VERSION));
}

ProtocolVersion ServerInfo::protocolVersion() const
{
    return ProtocolVersion(geti(VAR_PROTOCOL_VERSION));
}

int ServerInfo::compatibilityVersion() const
{
    return geti(VAR_COMPATIBILITY_VERSION);
}

ServerInfo &ServerInfo::setCompatibilityVersion(int compatVersion)
{
    set(VAR_COMPATIBILITY_VERSION, compatVersion);
    return *this;
}

Address ServerInfo::address() const
{
    return Address::parse(gets(VAR_HOST));
}

ServerInfo &ServerInfo::setAddress(Address const &address)
{
    set(VAR_HOST, address.asText());
    return *this;
}

String ServerInfo::name() const
{
    return gets(VAR_NAME);
}

ServerInfo &ServerInfo::setName(String const &name)
{
    set(VAR_NAME, name);
    return *this;
}

String ServerInfo::description() const
{
    return gets(VAR_DESCRIPTION, "");
}

ServerInfo &ServerInfo::setDescription(const String &description)
{
    set(VAR_DESCRIPTION, description);
    return *this;
}

String ServerInfo::pluginDescription() const
{
    return gets(VAR_PLUGIN, "");
}

ServerInfo &ServerInfo::setPluginDescription(String const &pluginDescription)
{
    set(VAR_PLUGIN, pluginDescription);
    return *this;
}

StringList ServerInfo::packages() const
{
    return getStringList(VAR_PACKAGES);
}

ServerInfo &ServerInfo::setPackages(StringList const &packages)
{
    ArrayValue &pkgs = addArray(VAR_PACKAGES).value<ArrayValue>();
    for (String const &p : packages)
    {
        pkgs << TextValue(p);
    }
    return *this;
}

String ServerInfo::gameId() const
{
    return gets(VAR_GAME_ID, "");
}

ServerInfo &ServerInfo::setGameId(String const &gameId)
{
    set(VAR_GAME_ID, gameId);
    return *this;
}

String ServerInfo::gameConfig() const
{
    return gets(VAR_GAME_CONFIG, "");
}

ServerInfo &ServerInfo::setGameConfig(String const &gameConfig)
{
    set(VAR_GAME_CONFIG, gameConfig);
    return *this;
}

String ServerInfo::map() const
{
    return gets(VAR_MAP, "");
}

ServerInfo &ServerInfo::setMap(String const &map)
{
    set(VAR_MAP, map);
    return *this;
}

StringList ServerInfo::players() const
{
    return getStringList(VAR_PLAYERS);
}

ServerInfo &ServerInfo::addPlayer(String const &playerName)
{
    ArrayValue &players = member(VAR_PLAYERS).value<ArrayValue>();
    players.add(playerName);
    return *this;
}

ServerInfo &ServerInfo::removePlayer(String const &playerName)
{
    ArrayValue &players = member(VAR_PLAYERS).value<ArrayValue>();
    for (int i = 0; i < int(players.size()); ++i)
    {
        if (players.at(i).asText() == playerName)
        {
            players.remove(i);
            break;
        }
    }
    return *this;
}

int ServerInfo::maxPlayers() const
{
    return geti(VAR_MAX_PLAYERS);
}

ServerInfo &ServerInfo::setMaxPlayers(int count)
{
    set(VAR_MAX_PLAYERS, count);
    return *this;
}

ServerInfo::Flags ServerInfo::flags() const
{
    return Flags(geti(VAR_FLAGS, DefaultFlags));
}

String ServerInfo::asStyledText() const
{
    return String();
}

Block ServerInfo::asJSON() const
{
    return composeJSON(*this);
}

ServerInfo &ServerInfo::setFlags(Flags const &flags)
{
    set(VAR_FLAGS, duint32(flags));
    return *this;
}

} // namespace shell
} // namespace de
