/** @file shelluser.cpp  Remote user of a shell connection.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "shelluser.h"
#include <de/shell/Protocol>
#include <de/shell/Lexicon>
#include <de/LogSink>
#include <de/Log>
#include <de/LogBuffer>

#include "con_main.h"
#include "dd_main.h"
#include "games.h"
#include "Game"
#include "def_main.h"
#include "map/gamemap.h"

using namespace de;

DENG2_PIMPL(ShellUser), public LogSink
{
    /// Log entries to be sent are collected here.
    shell::LogEntryPacket logEntryPacket;

    Instance(Public &i) : Base(i)
    {
        // We will send all log entries to a shell user.
        LogBuffer::appBuffer().addSink(*this);
    }

    ~Instance()
    {
        LogBuffer::appBuffer().removeSink(*this);
    }

    LogSink &operator << (LogEntry const &entry)
    {
        logEntryPacket.add(entry);
        return *this;
    }

    LogSink &operator << (String const &)
    {
        return *this;
    }

    /**
     * Sends the accumulated log entries over the link.
     */
    void flush()
    {
        if(!logEntryPacket.isEmpty() && self.status() == shell::Link::Connected)
        {
            self << logEntryPacket;
            logEntryPacket.clear();
        }
    }
};

ShellUser::ShellUser(Socket *socket) : shell::Link(socket), d(new Instance(*this))
{
    connect(this, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
}

static int addToTerms(knownword_t const *word, void *parameters)
{
    shell::Lexicon *lexi = reinterpret_cast<shell::Lexicon *>(parameters);
    lexi->addTerm(Str_Text(Con_KnownWordToString(word)));
    return 0;
}

void ShellUser::sendInitialUpdate()
{
    // Console lexicon.
    shell::Lexicon lexi;
    Con_IterateKnownWords(0, WT_ANY, addToTerms, &lexi);
    lexi.setAdditionalWordChars("-_.");
    QScopedPointer<RecordPacket> packet(protocol().newConsoleLexicon(lexi));
    *this << *packet;

    sendGameState();
    sendMapOutline();
}

void ShellUser::sendGameState()
{
    de::Game &game = App_CurrentGame();
    String mode = (App_GameLoaded()? Str_Text(game.identityKey()) : "");

    /**
     * @todo The server is not the right place to compose a packet about
     * game state. Work needed:
     * - World class that contains the game world as a whole
     * - WorldFactory that produces world and map related instances
     * - Game plugins can extend the world with their own code (games can
     *   provide a Factory of their own for constructing world/map instances)
     *
     * The server should just ask the World for the information for the game
     * state packet.
     */

    /// @todo This information needs to come form the Game Rules.
    int deathmatch = Con_GetInteger("server-game-deathmatch");
    String rules = (!deathmatch    ? "Coop" :
                    deathmatch == 1? "Deathmatch"  :
                                     "Deathmatch II");

    String mapId;
    String mapTitle;

    // Check the map's information from definitions.
    /// @todo DD_GetVariable() is not an appropriate place to ask for this --
    /// should be moved to the GameMap class. (Ditto for the ID query below.)
    char const *name = reinterpret_cast<char const *>(DD_GetVariable(DD_MAP_NAME));
    if(name) mapTitle = name;

    // Check the map's information from definitions.
    if(theMap)
    {
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(theMap->uri);
        if(mapInfo)
        {
            mapId = Str_Text(Uri_ToString(mapInfo->uri));
        }
    }

    QScopedPointer<RecordPacket> packet(protocol().newGameState(mode, rules, mapId, mapTitle));
    *this << *packet;
}

void ShellUser::sendMapOutline()
{
    if(!theMap) return;

    QScopedPointer<shell::MapOutlinePacket> packet(new shell::MapOutlinePacket);

    for(uint i = 0; i < theMap->lineDefCount(); ++i)
    {
        LineDef const &line = theMap->lineDefs[i];
        packet->addLine(
                Vector2i(line.v[0]->origin()[VX], line.v[0]->origin()[VY]),
                Vector2i(line.v[1]->origin()[VX], line.v[1]->origin()[VY]),
                (line.sides[0].sector && line.sides[1].sector)?
                    shell::MapOutlinePacket::TwoSidedLine :
                    shell::MapOutlinePacket::OneSidedLine);
    }

    *this << *packet;
}

void ShellUser::handleIncomingPackets()
{
    forever
    {
        QScopedPointer<Packet> packet(nextPacket());
        if(packet.isNull()) break;

        try
        {
            switch(protocol().recognize(packet.data()))
            {
            case shell::Protocol::Command:
                Con_Execute(CMDS_CONSOLE, protocol().command(*packet).toUtf8().constData(), false, true);
                break;

            default:
                break;
            }
        }
        catch(Error const &er)
        {
            LOG_WARNING("Error while processing packet from %s:\n%s") << packet->from() << er.asText();
        }
    }
}
