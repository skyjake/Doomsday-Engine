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

#include <de/Protocol>
#include <de/RecordPacket>
#include <QList>

namespace de {
namespace shell {

/**
 * Packet with one or more log entries.
 */
class LogEntryPacket : public Packet
{
public:
    LogEntryPacket();
    ~LogEntryPacket();

    void clear();
    void execute() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(Block const &block);

private:
    QList<LogEntry *> _entries;
};

/**
 * Network protocol for communicating with a server.
 */
class Protocol : public de::Protocol
{
public:
    /// Type of provided packet is incorrect. @ingroup errors
    DENG2_ERROR(TypeError);

    enum PacketType
    {
        Unknown,
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
    PacketType recognize(Packet const *packet);

    /**
     * Constructs a console command packet.
     *
     * @param command  Command to execute on the server.
     *
     * @return Packet. Caller gets ownership.
     */
    RecordPacket *newCommand(String const &command);

    String command(Packet const &commandPacket);
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_PROTOCOL_H
