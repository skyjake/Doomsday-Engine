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

#include "de/shell/Protocol"
#include <de/LogBuffer>
#include <de/ArrayValue>
#include <de/TextValue>
#include <de/Reader>
#include <de/Writer>

namespace de { namespace shell {

static String const PT_COMMAND    = "shell.command";
static String const PT_LEXICON    = "shell.lexicon";
static String const PT_GAME_STATE = "shell.game.state";

// ChallengePacket -----------------------------------------------------------

static Packet::Type const CHALLENGE_PACKET_TYPE = Packet::typeFromString("Psw?");

ChallengePacket::ChallengePacket() : Packet(CHALLENGE_PACKET_TYPE)
{}

Packet *ChallengePacket::fromBlock(Block const &block)
{
    return constructFromBlock<ChallengePacket>(block, CHALLENGE_PACKET_TYPE);
}

// LogEntryPacket ------------------------------------------------------------

static Packet::Type const LOG_ENTRY_PACKET_TYPE = Packet::typeFromString("LgEn");

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

void LogEntryPacket::add(LogEntry const &entry)
{
    _entries.append(new LogEntry(entry));
}

LogEntryPacket::Entries const &LogEntryPacket::entries() const
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

Packet *LogEntryPacket::fromBlock(Block const &block)
{
    return constructFromBlock<LogEntryPacket>(block, LOG_ENTRY_PACKET_TYPE);
}

// PlayerInfoPacket ----------------------------------------------------------

static Packet::Type const PLAYER_INFO_PACKET_TYPE = Packet::typeFromString("PlrI");

DE_PIMPL_NOREF(PlayerInfoPacket)
{
    Players players;
};

PlayerInfoPacket::PlayerInfoPacket()
    : Packet(PLAYER_INFO_PACKET_TYPE), d(new Impl)
{}

void PlayerInfoPacket::add(Player const &player)
{
    d->players.insert(player.number, player);
}

dsize PlayerInfoPacket::count() const
{
    return d->players.size();
}

PlayerInfoPacket::Player const &PlayerInfoPacket::player(int number) const
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

Packet *PlayerInfoPacket::fromBlock(Block const &block)
{
    return constructFromBlock<PlayerInfoPacket>(block, PLAYER_INFO_PACKET_TYPE);
}

// MapOutlinePacket ----------------------------------------------------------

static Packet::Type const MAP_OUTLINE_PACKET_TYPE = Packet::typeFromString("MpOL");

DE_PIMPL_NOREF(MapOutlinePacket)
{
    List<Line> lines;
};

MapOutlinePacket::MapOutlinePacket()
    : Packet(MAP_OUTLINE_PACKET_TYPE), d(new Impl)
{}

void MapOutlinePacket::clear()
{
    d->lines.clear();
}

void MapOutlinePacket::addLine(Vec2i const &vertex1, Vec2i const &vertex2, LineType type)
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

MapOutlinePacket::Line const &MapOutlinePacket::line(int index) const
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

Packet *MapOutlinePacket::fromBlock(Block const &block)
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

Protocol::PacketType Protocol::recognize(Packet const *packet)
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
    if (RecordPacket const *rec = maybeAs<RecordPacket>(packet))
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

Block Protocol::passwordResponse(String const &plainPassword)
{
    Block response;
    response += "Shell";
    response += Block(plainPassword).md5Hash(); // MD5 isn't actually secure
    /// @todo Calculate SHA-256 digest instead.
    return response;
}

RecordPacket *Protocol::newCommand(String const &command)
{
    RecordPacket *cmd = new RecordPacket(PT_COMMAND);
    cmd->record().addText("execute", command);
    return cmd;
}

static RecordPacket const &asRecordPacket(Packet const &packet, Protocol::PacketType type)
{
    RecordPacket const *rec = dynamic_cast<RecordPacket const *>(&packet);
    DE_ASSERT(rec != nullptr);
    DE_ASSERT(Protocol::recognize(&packet) == type);
    DE_UNUSED(type);
    return *rec;
}

String Protocol::command(Packet const &commandPacket)
{
    RecordPacket const &rec = asRecordPacket(commandPacket, Command);
    return rec["execute"].value().asText();
}

RecordPacket *Protocol::newConsoleLexicon(Lexicon const &lexicon)
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

Lexicon Protocol::lexicon(Packet const &consoleLexiconPacket)
{
    RecordPacket const &rec = asRecordPacket(consoleLexiconPacket, ConsoleLexicon);
    Lexicon lexicon;
    DE_FOR_EACH_CONST(ArrayValue::Elements, i, rec["terms"].array().elements())
    {
        lexicon.addTerm((*i)->asText());
    }
    lexicon.setAdditionalWordChars(rec.valueAsText("extraChars"));
    return lexicon;
}

RecordPacket *Protocol::newGameState(String const &mode,
                                     String const &rules,
                                     String const &mapId,
                                     String const &mapTitle)
{
    RecordPacket *gs = new RecordPacket(PT_GAME_STATE);
    Record &r = gs->record();
    r.addText("mode", mode);
    r.addText("rules", rules);
    r.addText("mapId", mapId);
    r.addText("mapTitle", mapTitle);
    return gs;
}

}} // namespace de::shell
