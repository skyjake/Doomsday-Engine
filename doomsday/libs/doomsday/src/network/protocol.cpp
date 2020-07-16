/** @file libshell/src/protocol.cpp  Network protocol for communicating with a server.
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

#include "doomsday/network/protocol.h"

#include <de/logbuffer.h>
#include <de/arrayvalue.h>
#include <de/textvalue.h>
#include <de/reader.h>
#include <de/writer.h>

namespace network {

static const String PT_COMMAND    = "shell.command";
static const String PT_LEXICON    = "shell.lexicon";
static const String PT_GAME_STATE = "shell.game.state";

// ChallengePacket -----------------------------------------------------------

static const Packet::Type CHALLENGE_PACKET_TYPE = Packet::typeFromString("Psw?");

ChallengePacket::ChallengePacket() : Packet(CHALLENGE_PACKET_TYPE)
{}

Packet *ChallengePacket::fromBlock(const Block &block)
{
    return constructFromBlock<ChallengePacket>(block, CHALLENGE_PACKET_TYPE);
}

// LogEntryPacket ------------------------------------------------------------

static const Packet::Type LOG_ENTRY_PACKET_TYPE = Packet::typeFromString("LgEn");

LogEntryPacket::LogEntryPacket() : Packet(LOG_ENTRY_PACKET_TYPE)
{}

LogEntryPacket::~LogEntryPacket()
{
    clear();
}

void LogEntryPacket::clear()
{
    for (LogEntry *e : _entries) delete e;
    _entries.clear();
}

bool LogEntryPacket::isEmpty() const
{
    return _entries.isEmpty();
}

void LogEntryPacket::add(const LogEntry &entry)
{
    _entries.append(new LogEntry(entry));
}

const LogEntryPacket::Entries &LogEntryPacket::entries() const
{
    return _entries;
}

void LogEntryPacket::execute() const
{
    // Copies of all entries in the packet are added to the LogBuffer.
    LogBuffer &buf = LogBuffer::get();
    for (LogEntry *e : _entries)
    {
        buf.add(new LogEntry(*e, LogEntry::Remote));
    }
}

void LogEntryPacket::operator >> (Writer &to) const
{
    Packet::operator>>(to);
    to.writeObjects(_entries);
}

void LogEntryPacket::operator << (Reader &from)
{
    _entries.clear();

    Packet::operator<<(from);
    from.readObjects<LogEntry>(_entries);
}

Packet *LogEntryPacket::fromBlock(const Block &block)
{
    return constructFromBlock<LogEntryPacket>(block, LOG_ENTRY_PACKET_TYPE);
}

// PlayerInfoPacket ----------------------------------------------------------

static const Packet::Type PLAYER_INFO_PACKET_TYPE = Packet::typeFromString("PlrI");

DE_PIMPL_NOREF(PlayerInfoPacket)
{
    Players players;
};

PlayerInfoPacket::PlayerInfoPacket()
    : Packet(PLAYER_INFO_PACKET_TYPE), d(new Impl)
{}

void PlayerInfoPacket::add(const Player &player)
{
    d->players.insert(player.number, player);
}

dsize PlayerInfoPacket::count() const
{
    return d->players.size();
}

const PlayerInfoPacket::Player &PlayerInfoPacket::player(int number) const
{
    DE_ASSERT(d->players.contains(number));
    return d->players[number];
}

PlayerInfoPacket::Players PlayerInfoPacket::players() const
{
    return d->players;
}

void PlayerInfoPacket::operator>>(Writer &to) const
{
    Packet::operator>>(to);

    to << duint32(d->players.size());
    for (const auto &i : d->players)
    {
        const Player &p = i.second;
        to << dbyte(p.number) << p.position << p.name << p.color;
    }
}

void PlayerInfoPacket::operator<<(Reader &from)
{
    d->players.clear();

    Packet::operator<<(from);

    duint32 count;
    from >> count;
    while (count-- > 0)
    {
        Player p;
        from.readAs<dbyte>(p.number) >> p.position >> p.name >> p.color;
        d->players.insert(p.number, p);
    }
}

Packet *PlayerInfoPacket::fromBlock(const Block &block)
{
    return constructFromBlock<PlayerInfoPacket>(block, PLAYER_INFO_PACKET_TYPE);
}

// MapOutlinePacket ----------------------------------------------------------

static const Packet::Type MAP_OUTLINE_PACKET_TYPE = Packet::typeFromString("MpOL");

DE_PIMPL_NOREF(MapOutlinePacket)
{
    List<Line> lines;
};

MapOutlinePacket::MapOutlinePacket()
    : Packet(MAP_OUTLINE_PACKET_TYPE), d(new Impl)
{}

MapOutlinePacket::MapOutlinePacket(const MapOutlinePacket &other)
    : Packet(MAP_OUTLINE_PACKET_TYPE), d(new Impl)
{
    d->lines = other.d->lines;
}

MapOutlinePacket &MapOutlinePacket::operator=(const MapOutlinePacket &other)
{
    d->lines = other.d->lines;
    return *this;
}

void MapOutlinePacket::clear()
{
    d->lines.clear();
}

void MapOutlinePacket::addLine(const Vec2i &vertex1, const Vec2i &vertex2, LineType type)
{
    Line ln;
    ln.start = vertex1;
    ln.end   = vertex2;
    ln.type  = type;
    d->lines.append(ln);
}

int MapOutlinePacket::lineCount() const
{
    return d->lines.sizei();
}

const MapOutlinePacket::Line &MapOutlinePacket::line(int index) const
{
    DE_ASSERT(index >= 0 && index < d->lines.sizei());
    return d->lines[index];
}

void MapOutlinePacket::operator >> (Writer &to) const
{
    Packet::operator>>(to);

    to << duint32(d->lines.size());
    for (const Line &ln : d->lines)
    {
        to << ln.start << ln.end << dbyte(ln.type);
    }
}

void MapOutlinePacket::operator << (Reader &from)
{
    clear();

    Packet::operator<<(from);

    duint32 count;
    from >> count;
    while (count-- > 0)
    {
        Line ln;
        from >> ln.start >> ln.end;
        from.readAs<dbyte>(ln.type);
        d->lines.append(ln);
    }
}

Packet *MapOutlinePacket::fromBlock(const Block &block)
{
    return constructFromBlock<MapOutlinePacket>(block, MAP_OUTLINE_PACKET_TYPE);
}

// Protocol ------------------------------------------------------------------

Protocol::Protocol()
{
    define(ChallengePacket::fromBlock);
    define(LogEntryPacket::fromBlock);
    define(MapOutlinePacket::fromBlock);
    define(PlayerInfoPacket::fromBlock);
}

Protocol::PacketType Protocol::recognize(const Packet *packet)
{
    if (packet->type() == CHALLENGE_PACKET_TYPE)
    {
        DE_ASSERT(is<ChallengePacket>(packet));
        return PasswordChallenge;
    }

    if (packet->type() == LOG_ENTRY_PACKET_TYPE)
    {
        DE_ASSERT(is<LogEntryPacket>(packet));
        return LogEntries;
    }

    if (packet->type() == MAP_OUTLINE_PACKET_TYPE)
    {
        DE_ASSERT(is<MapOutlinePacket>(packet));
        return MapOutline;
    }

    if (packet->type() == PLAYER_INFO_PACKET_TYPE)
    {
        DE_ASSERT(is<PlayerInfoPacket>(packet));
        return PlayerInfo;
    }

    // One of the generic-format packets?
    if (const RecordPacket *rec = maybeAs<RecordPacket>(packet))
    {
        if (rec->name() == PT_COMMAND)
        {
            return Command;
        }
        else if (rec->name() == PT_LEXICON)
        {
            return ConsoleLexicon;
        }
        else if (rec->name() == PT_GAME_STATE)
        {
            return GameState;
        }
    }
    return Unknown;
}

Block Protocol::passwordResponse(const String &plainPassword)
{
    Block response;
    response += "Shell";
    response += Block(plainPassword).md5Hash(); // MD5 isn't actually secure
    /// @todo Calculate SHA-256 digest instead.
    return response;
}

RecordPacket *Protocol::newCommand(const String &command)
{
    RecordPacket *cmd = new RecordPacket(PT_COMMAND);
    cmd->record().addText("execute", command);
    return cmd;
}

static const RecordPacket &asRecordPacket(const Packet &packet, Protocol::PacketType type)
{
    const RecordPacket *rec = dynamic_cast<const RecordPacket *>(&packet);
    DE_ASSERT(rec != nullptr);
    DE_ASSERT(Protocol::recognize(&packet) == type);
    DE_UNUSED(type);
    return *rec;
}

String Protocol::command(const Packet &commandPacket)
{
    const RecordPacket &rec = asRecordPacket(commandPacket, Command);
    return rec["execute"].value().asText();
}

RecordPacket *Protocol::newConsoleLexicon(const Lexicon &lexicon)
{
    RecordPacket *lex = new RecordPacket(PT_LEXICON);
    lex->record().addText("extraChars", lexicon.additionalWordChars());
    ArrayValue &arr = lex->record().addArray("terms").array();
    for (const String &term : lexicon.terms())
    {
        arr << TextValue(term);
    }
    return lex;
}

Lexicon Protocol::lexicon(const Packet &consoleLexiconPacket)
{
    const RecordPacket &rec = asRecordPacket(consoleLexiconPacket, ConsoleLexicon);
    Lexicon lexicon;
    DE_FOR_EACH_CONST(ArrayValue::Elements, i, rec["terms"].array().elements())
    {
        lexicon.addTerm((*i)->asText());
    }
    lexicon.setAdditionalWordChars(rec.valueAsText("extraChars"));
    return lexicon;
}

RecordPacket *Protocol::newGameState(const String &mode,
                                     const String &rules,
                                     const String &mapId,
                                     const String &mapTitle)
{
    RecordPacket *gs = new RecordPacket(PT_GAME_STATE);
    Record &r = gs->record();
    r.addText("mode", mode);
    r.addText("rules", rules);
    r.addText("mapId", mapId);
    r.addText("mapTitle", mapTitle);
    return gs;
}

} // namespace network
