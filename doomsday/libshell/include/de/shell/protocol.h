/** @file protocol.h  Network protocol for communicating with a server.
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

#ifndef LIBSHELL_PROTOCOL_H
#define LIBSHELL_PROTOCOL_H

#include "libshell.h"
#include "Lexicon"
#include <de/Protocol>
#include <de/RecordPacket>
#include <de/Vector>
#include <QList>

namespace de {
namespace shell {

/**
 * Password challenge.
 */
class LIBSHELL_PUBLIC ChallengePacket : public Packet
{
public:
    ChallengePacket();
    static Packet *fromBlock(Block const &block);
};

/**
 * Packet with one or more log entries.
 */
class LIBSHELL_PUBLIC LogEntryPacket : public Packet
{
public:
    typedef QList<LogEntry *> Entries;

public:
    LogEntryPacket();
    ~LogEntryPacket();

    void clear();   
    bool isEmpty() const;

    /**
     * Adds a log entry to the packet.
     * @param entry  Log entry.
     */
    void add(LogEntry const &entry);

    Entries const &entries() const;

    /**
     * Adds all the entries into the application's log buffer.
     */
    void execute() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(Block const &block);

private:
    Entries _entries;
};

/**
 * Packet containing an outline of a map's lines.
 *
 * The contained information is not intended to be a 100% accurate or complete
 * representation of a map. It is only meant to be used as an informative
 * visualization for the shell user (2D outline of the map).
 */
class LIBSHELL_PUBLIC MapOutlinePacket : public Packet
{
public:
    enum LineType
    {
        OneSidedLine = 0,
        TwoSidedLine = 1
    };
    struct Line
    {
        Vector2i start;
        Vector2i end;
        LineType type;
    };

    MapOutlinePacket();
    ~MapOutlinePacket();

    void clear();

    void addLine(Vector2i const &vertex1, Vector2i const &vertex2, LineType type);

    /**
     * Returns the number of lines.
     */
    int lineCount() const;

    /**
     * Returns a line in the outline.
     * @param index  Index of the line, in range [0, lineCount()).
     * @return Line specs.
     */
    Line const &line(int index) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(Block const &block);

private:
    DENG2_PRIVATE(d)
};

/**
 * Network protocol for communicating with a server.
 */
class LIBSHELL_PUBLIC Protocol : public de::Protocol
{
public:
    /// Type of provided packet is incorrect. @ingroup errors
    DENG2_ERROR(TypeError);

    enum PacketType
    {
        Unknown,
        PasswordChallenge,
        Command,        ///< Console command (only to server).
        LogEntries,     ///< Log entries.
        ConsoleLexicon, ///< Known words for command line completion.
        GameState,      ///< Current state of the game (mode, map).
        Leaderboard,    ///< Frags leaderboard.
        MapOutline,     ///< Sectors of the map for visual overview.
        PlayerPositions ///< Current player positions.
    };

public:
    Protocol();

    /**
     * Detects the type of a packet.
     *
     * @param packet  Any packet.
     *
     * @return Type of the packet.
     */
    static PacketType recognize(Packet const *packet);

    static Block passwordResponse(String const &plainPassword);

    /**
     * Constructs a console command packet.
     *
     * @param command  Command to execute on the server.
     *
     * @return Packet. Caller gets ownership.
     */
    RecordPacket *newCommand(String const &command);

    String command(Packet const &commandPacket);

    /**
     * Constructs a packet that defines all known terms of the console.
     *
     * @param lexicon  Lexicon.
     *
     * @return Packet. Caller gets ownership.
     */
    RecordPacket *newConsoleLexicon(Lexicon const &lexicon);

    Lexicon lexicon(Packet const &consoleLexiconPacket);

    /**
     * Constructs a packet that describes the current gameplay state.
     *
     * @param mode      Game mode (e.g., doom2).
     * @param rules     Name of the game rules (e.g., Deathmatch).
     * @param mapId     Identifier of the map (e.g., E1M3).
     * @param mapTitle  Title of the map (from mapinfo/defs).
     *
     * @return Packet. Caller gets ownership.
     */
    RecordPacket *newGameState(String const &mode,
                               String const &rules,
                               String const &mapId,
                               String const &mapTitle);
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_PROTOCOL_H
