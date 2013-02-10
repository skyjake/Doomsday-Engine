/** @file protocol.cpp  Network protocol for communicating with a server.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/Protocol"
#include <de/LogBuffer>
#include <de/ArrayValue>
#include <de/TextValue>
#include <de/Reader>
#include <de/Writer>
#include <QList>

namespace de {
namespace shell {

static String const PT_COMMAND = "shell.command";
static String const PT_LEXICON = "shell.lexicon";
static String const PT_GAME_STATE = "shell.game.state";

// LogEntryPacket ------------------------------------------------------------

static char const *LOG_ENTRY_PACKET_TYPE = "LgEn";

LogEntryPacket::LogEntryPacket() : Packet(LOG_ENTRY_PACKET_TYPE)
{}

LogEntryPacket::~LogEntryPacket()
{
    clear();
}

void LogEntryPacket::clear()
{
    foreach(LogEntry *e, _entries) delete e;
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
    LogBuffer &buf = LogBuffer::appBuffer();
    foreach(LogEntry *e, _entries)
    {
        buf.add(new LogEntry(*e, LogEntry::Remote));
    }
}

void LogEntryPacket::operator >> (Writer &to) const
{
    Packet::operator >> (to);
    to.writeObjects(_entries);
}

void LogEntryPacket::operator << (Reader &from)
{
    _entries.clear();

    Packet::operator << (from);
    from.readObjects<LogEntry>(_entries);
}

Packet *LogEntryPacket::fromBlock(Block const &block)
{
    return constructFromBlock<LogEntryPacket>(block, LOG_ENTRY_PACKET_TYPE);
}

// MapOutlinePacket ----------------------------------------------------------

static char const *MAP_OUTLINE_PACKET_TYPE = "MpOL";

struct MapOutlinePacket::Instance
{
    QList<Line> lines;
};

MapOutlinePacket::MapOutlinePacket()
    : Packet(MAP_OUTLINE_PACKET_TYPE), d(new Instance)
{}

MapOutlinePacket::~MapOutlinePacket()
{
    delete d;
}

void MapOutlinePacket::clear()
{
    d->lines.clear();
}

void MapOutlinePacket::addLine(Vector2i const &vertex1, Vector2i const &vertex2, LineType type)
{
    Line ln;
    ln.start = vertex1;
    ln.end   = vertex2;
    ln.type  = type;
    d->lines.append(ln);
}

int MapOutlinePacket::lineCount() const
{
    return d->lines.size();
}

MapOutlinePacket::Line const &MapOutlinePacket::line(int index) const
{
    DENG2_ASSERT(index >= 0 && index < d->lines.size());
    return d->lines[index];
}

void MapOutlinePacket::operator >> (Writer &to) const
{
    Packet::operator >> (to);

    to << duint32(d->lines.size());
    foreach(Line const &ln, d->lines)
    {
        to << ln.start << ln.end << dbyte(ln.type);
    }
}

void MapOutlinePacket::operator << (Reader &from)
{
    clear();

    Packet::operator << (from);

    duint32 count;
    from >> count;
    while(count-- > 0)
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
    define(LogEntryPacket::fromBlock);
    define(MapOutlinePacket::fromBlock);
}

Protocol::PacketType Protocol::recognize(Packet const *packet)
{
    if(packet->type() == LOG_ENTRY_PACKET_TYPE)
    {
        DENG2_ASSERT(dynamic_cast<LogEntryPacket const *>(packet) != 0);
        return LogEntries;
    }

    if(packet->type() == MAP_OUTLINE_PACKET_TYPE)
    {
        DENG2_ASSERT(dynamic_cast<MapOutlinePacket const *>(packet) != 0);
        return MapOutline;
    }

    // One of the generic-format packets?
    RecordPacket const *rec = dynamic_cast<RecordPacket const *>(packet);
    if(rec)
    {
        if(rec->name() == PT_COMMAND)
        {
            return Command;
        }
        else if(rec->name() == PT_LEXICON)
        {
            return ConsoleLexicon;
        }
        else if(rec->name() == PT_GAME_STATE)
        {
            return GameState;
        }
    }
    return Unknown;
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
    DENG2_ASSERT(rec != 0);
    DENG2_ASSERT(Protocol::recognize(&packet) == type);
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
    ArrayValue &arr = lex->record().addArray("terms").value<ArrayValue>();
    foreach(String const &term, lexicon.terms())
    {
        arr << TextValue(term);
    }
    return lex;
}

Lexicon Protocol::lexicon(Packet const &consoleLexiconPacket)
{
    RecordPacket const &rec = asRecordPacket(consoleLexiconPacket, ConsoleLexicon);
    Lexicon lexicon;
    DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, rec["terms"].value<ArrayValue>().elements())
    {
        lexicon.addTerm((*i)->asText());
    }
    lexicon.setAdditionalWordChars(rec.valueAsText("extraChars"));
    return lexicon;
}

RecordPacket *Protocol::newGameState(String const &mode, String const &rules,
                                     String const &mapId, String const &mapTitle)
{
    RecordPacket *gs = new RecordPacket(PT_GAME_STATE);
    Record &r = gs->record();
    r.addText("mode", mode);
    r.addText("rules", rules);
    r.addText("mapId", mapId);
    r.addText("mapTitle", mapTitle);
    return gs;
}

} // namespace shell
} // namespace de
