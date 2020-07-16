/** @file network/protocol.h  Network protocol for communicating with a server.
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

#ifndef LIBSHELL_PROTOCOL_H
#define LIBSHELL_PROTOCOL_H

#include "de/lexicon.h"
#include "de/protocol.h"
#include "de/recordpacket.h"
#include "de/vector.h"
#include "de/keymap.h"

namespace network {

using namespace de;

/**
 * Password challenge.
 * @ingroup shell
 */
class DE_PUBLIC ChallengePacket : public Packet
{
public:
    ChallengePacket();
    static Packet *fromBlock(const Block &block);
};

/**
 * Packet with one or more log entries.
 * @ingroup shell
 */
class DE_PUBLIC LogEntryPacket : public Packet
{
public:
    typedef List<LogEntry *> Entries;

public:
    LogEntryPacket();
    ~LogEntryPacket();

    void clear();
    bool isEmpty() const;

    /**
     * Adds a log entry to the packet.
     * @param entry  Log entry.
     */
    void add(const LogEntry &entry);

    const Entries &entries() const;

    /**
     * Adds all the entries into the application's log buffer.
     */
    void execute() const;

    // Implements ISerializable.
    void operator>>(Writer &to) const;
    void operator<<(Reader &from);

    static Packet *fromBlock(const Block &block);

private:
    Entries _entries;
};

/**
 * Packet containing information about the players' current positions, colors,
 * and names. @ingroup shell
 */
class DE_PUBLIC PlayerInfoPacket : public Packet
{
public:
    struct Player {
        int    number;
        Vec2i  position;
        String name;
        Vec3ub color;

        Player(int num = 0, const Vec2i &pos = Vec2i(), const String &plrName = "",
               const Vec3ub &plrColor = Vec3ub())
            : number(num)
            , position(pos)
            , name(plrName)
            , color(plrColor)
        {}
    };

    typedef KeyMap<int, Player> Players;

public:
    PlayerInfoPacket();

    void add(const Player &player);

    dsize         count() const;
    const Player &player(int number) const;
    Players       players() const;

    // Implements ISerializable.
    void operator>>(Writer &to) const;
    void operator<<(Reader &from);

    static Packet *fromBlock(const Block &block);

private:
    DE_PRIVATE(d)
};

/**
 * Packet containing an outline of a map's lines. @ingroup shell
 *
 * The contained information is not intended to be a 100% accurate or complete
 * representation of a map. It is only meant to be used as an informative
 * visualization for the shell user (2D outline of the map).
 */
class DE_PUBLIC MapOutlinePacket : public Packet
{
public:
    enum LineType {
        OneSidedLine = 0,
        TwoSidedLine = 1,
    };
    struct Line {
        Vec2i start;
        Vec2i end;
        LineType type;
    };

    MapOutlinePacket();
    MapOutlinePacket(const MapOutlinePacket &);
    MapOutlinePacket &operator=(const MapOutlinePacket &);

    void clear();

    void addLine(const Vec2i &vertex1, const Vec2i &vertex2, LineType type);

    /**
     * Returns the number of lines.
     */
    int lineCount() const;

    /**
     * Returns a line in the outline.
     * @param index  Index of the line, in range [0, lineCount()).
     * @return Line specs.
     */
    const Line &line(int index) const;

    // Implements ISerializable.
    void operator>>(Writer &to) const;
    void operator<<(Reader &from);

    static Packet *fromBlock(const Block &block);

private:
    DE_PRIVATE(d)
};

/**
 * Network protocol for communicating with a server. @ingroup shell
 */
class DE_PUBLIC Protocol : public de::Protocol
{
public:
    /// Type of provided packet is incorrect. @ingroup errors
    DE_ERROR(TypeError);

    enum PacketType {
        Unknown,
        PasswordChallenge,
        Command,        ///< Console command (only to server).
        LogEntries,     ///< Log entries.
        ConsoleLexicon, ///< Known words for command line completion.
        GameState,      ///< Current state of the game (mode, map).
        Leaderboard,    ///< Frags leaderboard.
        MapOutline,     ///< Sectors of the map for visual overview.
        PlayerInfo      ///< Current player names, colors, positions.
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
    static PacketType recognize(const Packet *packet);

    static Block passwordResponse(const String &plainPassword);

    /**
     * Constructs a console command packet.
     *
     * @param command  Command to execute on the server.
     *
     * @return Packet. Caller gets ownership.
     */
    RecordPacket *newCommand(const String &command);

    String command(const Packet &commandPacket);

    /**
     * Constructs a packet that defines all known terms of the console.
     *
     * @param lexicon  Lexicon.
     *
     * @return Packet. Caller gets ownership.
     */
    RecordPacket *newConsoleLexicon(const Lexicon &lexicon);

    Lexicon lexicon(const Packet &consoleLexiconPacket);

    /**
     * Constructs a packet that describes the current gameplay state.
     *
     * @param mode      Game mode (e.g., doom2).
     * @param rules     Game rule keywords (e.g., "dm skill2 jump").
     * @param mapId     Identifier of the map (e.g., E1M3).
     * @param mapTitle  Title of the map (from mapinfo/defs).
     *
     * @return Packet. Caller gets ownership.
     */
    RecordPacket *newGameState(const String &mode, const String &rules, const String &mapId,
                               const String &mapTitle);
};

} // namespace network

#endif // LIBSHELL_PROTOCOL_H
