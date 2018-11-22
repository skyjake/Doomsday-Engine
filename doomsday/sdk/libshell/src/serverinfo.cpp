/** @file serverinfo.cpp  Information about a multiplayer server.
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

#include "de/shell/ServerInfo"
#include <de/TextValue>
#include <de/Version>
#include <de/Log>
#include <de/data/json.h>

namespace de { namespace shell {

static String const VAR_VERSION                 ("ver");
static String const VAR_COMPATIBILITY_VERSION   ("cver");
static String const VAR_HOST                    ("host");
static String const VAR_DOMAIN                  ("dom");
static String const VAR_PORT                    ("port");
static String const VAR_NAME                    ("name");
static String const VAR_DESCRIPTION             ("desc");
static String const VAR_PLUGIN                  ("plugin");
static String const VAR_PACKAGES                ("pkgs");
static String const VAR_GAME_ID                 ("game");
static String const VAR_GAME_CONFIG             ("cfg");
static String const VAR_MAP                     ("map");
static String const VAR_PLAYERS                 ("plrs");
static String const VAR_PLAYER_COUNT            ("pnum");
static String const VAR_MAX_PLAYERS             ("pmax");
static String const VAR_FLAGS                   ("flags");

DENG2_PIMPL(ServerInfo)
{
    std::shared_ptr<Record> info;

    Impl(Public *i) : Base(i)
    {}

    void detach()
    {
        if (info.use_count() > 1)
        {
            // Duplicate our own copy of it.
            info.reset(new Record(*info));
            DENG2_ASSERT(info.use_count() == 1);
        }
    }

    void checkValid()
    {
        if (!info->has(VAR_PLAYERS)) info->addArray(VAR_PLAYERS);
        if (info->has(VAR_HOST))
        {
            if (self().address().port() != self().port())
            {
                self().setAddress(Address(self().address().host(), self().port()));
            }
        }
    }
};

ServerInfo::ServerInfo()
    : d(new Impl(this))
{
    d->info.reset(new Record);
    d->info->set(VAR_VERSION, Version::currentBuild().fullNumber());
    d->info->addArray(VAR_PLAYERS);
}

ServerInfo::ServerInfo(ServerInfo const &other)
    : d(new Impl(this))
{
    d->info = other.d->info;
}

ServerInfo::ServerInfo(Record const &rec)
    : d(new Impl(this))
{
    d->info.reset(new Record(rec));
    d->checkValid();
}

ServerInfo &ServerInfo::operator=(ServerInfo const &other)
{
    d->info = other.d->info;
    return *this;
}

Version ServerInfo::version() const
{
    return Version(d->info->gets(VAR_VERSION));
}

int ServerInfo::compatibilityVersion() const
{
    return d->info->geti(VAR_COMPATIBILITY_VERSION);
}

ServerInfo &ServerInfo::setCompatibilityVersion(int compatVersion)
{
    d->info->set(VAR_COMPATIBILITY_VERSION, compatVersion);
    return *this;
}

Address ServerInfo::address() const
{
    if (d->info->has(VAR_HOST))
    {
        return Address::parse(d->info->gets(VAR_HOST));
    }
    return Address();
}

String ServerInfo::domainName() const
{
    return d->info->gets(VAR_DOMAIN, "");
}

ServerInfo &ServerInfo::setAddress(Address const &address)
{
    d->detach();
    d->info->set(VAR_HOST, address.asText());
    d->info->set(VAR_PORT, address.port() ? address.port() : shell::DEFAULT_PORT);
    d->checkValid();
    return *this;
}

duint16 ServerInfo::port() const
{
    return duint16(d->info->geti(VAR_PORT, shell::DEFAULT_PORT));
}

String ServerInfo::name() const
{
    return d->info->gets(VAR_NAME, "");
}

ServerInfo &ServerInfo::setName(String const &name)
{
    d->detach();
    d->info->set(VAR_NAME, name);
    return *this;
}

String ServerInfo::description() const
{
    return d->info->gets(VAR_DESCRIPTION, "");
}

ServerInfo &ServerInfo::setDescription(const String &description)
{
    d->detach();
    d->info->set(VAR_DESCRIPTION, description);
    return *this;
}

String ServerInfo::pluginDescription() const
{
    return d->info->gets(VAR_PLUGIN, "");
}

ServerInfo &ServerInfo::setPluginDescription(String const &pluginDescription)
{
    d->detach();
    d->info->set(VAR_PLUGIN, pluginDescription);
    return *this;
}

StringList ServerInfo::packages() const
{
    return d->info->getStringList(VAR_PACKAGES);
}

ServerInfo &ServerInfo::setPackages(StringList packages)
{
    d->detach();
    ArrayValue &pkgs = d->info->addArray(VAR_PACKAGES).value<ArrayValue>();
    for (String const &p : packages)
    {
        pkgs << TextValue(p);
    }
    return *this;
}

String ServerInfo::gameId() const
{
    return d->info->gets(VAR_GAME_ID, "");
}

ServerInfo &ServerInfo::setGameId(String const &gameId)
{
    d->detach();
    d->info->set(VAR_GAME_ID, gameId);
    return *this;
}

String ServerInfo::gameConfig() const
{
    return d->info->gets(VAR_GAME_CONFIG, "");
}

ServerInfo &ServerInfo::setGameConfig(String const &gameConfig)
{
    d->detach();
    d->info->set(VAR_GAME_CONFIG, gameConfig);
    return *this;
}

String ServerInfo::map() const
{
    return d->info->gets(VAR_MAP, "");
}

ServerInfo &ServerInfo::setMap(String const &map)
{
    d->detach();
    d->info->set(VAR_MAP, map);
    return *this;
}

StringList ServerInfo::players() const
{
    return d->info->getStringList(VAR_PLAYERS);
}

int ServerInfo::playerCount() const
{
    return d->info->geti(VAR_PLAYER_COUNT, 0);
}

ServerInfo &ServerInfo::addPlayer(String const &playerName)
{
    d->detach();
    ArrayValue &players = d->info->member(VAR_PLAYERS).value<ArrayValue>();
    players.add(playerName);
    d->info->set(VAR_PLAYER_COUNT, players.size());
    return *this;
}

ServerInfo &ServerInfo::removePlayer(String const &playerName)
{
    d->detach();
    ArrayValue &players = d->info->member(VAR_PLAYERS).value<ArrayValue>();
    for (int i = 0; i < int(players.size()); ++i)
    {
        if (players.at(i).asText() == playerName)
        {
            players.remove(i);
            d->info->set(VAR_PLAYER_COUNT, players.size());
            break;
        }
    }
    return *this;
}

int ServerInfo::maxPlayers() const
{
    return d->info->geti(VAR_MAX_PLAYERS);
}

ServerInfo &ServerInfo::setMaxPlayers(int count)
{
    d->detach();
    d->info->set(VAR_MAX_PLAYERS, count);
    return *this;
}

ServerInfo::Flags ServerInfo::flags() const
{
    return Flags(d->info->geti(VAR_FLAGS, DefaultFlags));
}

String ServerInfo::asStyledText() const
{
#define TABBED(A, B) _E(Ta)_E(l) "  " A _E(.) " " _E(\t) B "\n"

    auto const playerNames = players();

    return String(_E(b) "%1" _E(.) "\n%2\n" _E(T`)
                  TABBED("Address:", "%6")
                  TABBED("Joinable:", "%5")
                  TABBED("Players:", "%3 / %4%11")
                  TABBED("Game:", "%7\n%8\n%10 %9")
                  TABBED("Packages:", "%12")
                  /*TABBED("Ping:", "%8 ms (approx)")*/)
            .arg(name())
            .arg(description())
            .arg(playerNames.size())
            .arg(maxPlayers())
            .arg(flags().testFlag(AllowJoin)? "Yes" : "No") // 5
            .arg(address().asText())
            //.arg(sv->ping)
            .arg(pluginDescription())
            .arg(gameId())
            .arg(gameConfig())
            .arg(map()) // 10
            .arg(!playerNames.isEmpty()? String(_E(2) " (%1)" _E(.)).arg(String::join(playerNames, " ")) : "")
            .arg(String::join(packages(), " "));

#undef TABBED
}

Block ServerInfo::asJSON() const
{
    return composeJSON(*d->info);
}

const Record &ServerInfo::asRecord() const
{
    return *d->info;
}

Record ServerInfo::strippedForBroadcast() const
{
    Record stripped(*d->info);
    delete stripped.tryRemove(VAR_HOST);     // address in network msg
    delete stripped.tryRemove(VAR_PLUGIN);   // gameId+version is enough
    delete stripped.tryRemove(VAR_PLAYERS);  // count is enough
    delete stripped.tryRemove(VAR_PACKAGES); // queried before connecting
    return stripped;
}

ServerInfo &ServerInfo::setDomainName(String const &domain)
{
    d->detach();
    d->info->set(VAR_DOMAIN, domain);
    return *this;
}

ServerInfo &ServerInfo::setFlags(Flags const &flags)
{
    d->detach();
    d->info->set(VAR_FLAGS, duint32(flags));
    return *this;
}

void ServerInfo::printToLog(int indexNumber, bool includeHeader) const
{
    /// @todo Update table for de::Log. -jk

    if (includeHeader)
    {
        LOG_NET_MSG(_E(m)"    %-20s P/M  L Ver:  Game:            Location:") << "Name:";
    }

    auto const plrs = players();

    LOG_NET_MSG(_E(m)"%-2i: %-20s %i/%-2i %c %-5i %-16s %s")
            << indexNumber
            << name()
            << playerCount()
            << maxPlayers()
            << (flags().testFlag(AllowJoin)? ' ' : '*')
            << compatibilityVersion()
            << pluginDescription()
            << address().asText();
    LOG_NET_MSG("    %s %-40s") << map() << description();
    LOG_NET_MSG("    %s %s") << gameId() << gameConfig();

    // Optional: PWADs in use.
    LOG_NET_MSG("    Packages: " _E(>) "%s") << String::join(packages(), "\n");

    // Optional: names of players.
    if (!plrs.isEmpty())
    {
        LOG_NET_MSG("    Players: " _E(>) "%s") << String::join(plrs, "\n");
    }
}

}} // namespace de::shell
