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

#pragma once

#include "de/lexicon.h"
#include "de/protocol.h"
#include "de/recordpacket.h"
#include "de/vector.h"
#include "de/keymap.h"
#include "dd_share.h"

/**
 * Server protocol version number.
 * @deprecated Will be replaced with the libcore serialization protocol version.
 */
#define SV_VERSION          24

// Packet types.
// PKT = sent by anyone
// PSV = only sent by server
// PCL = only sent by client
enum {
    // Messages and responses.
    PCL_HELLO               = 0,
    PKT_OK                  = 1,
    PKT_CANCEL              = 2, // unused?
    PKT_PLAYER_INFO         = 3,
    PKT_CHAT                = 4,
    PSV_FINALE              = 5,
    PKT_PING                = 6,
    PSV_HANDSHAKE           = 7,
    PSV_SERVER_CLOSE        = 8,
    PSV_FRAME               = 9, // obsolete
    PSV_PLAYER_EXIT         = 10,
    PSV_CONSOLE_TEXT        = 11,
    PCL_ACK_SHAKE           = 12,
    PSV_SYNC                = 13,
    PSV_MATERIAL_ARCHIVE    = 14,
    PCL_FINALE_REQUEST      = 15,
    PKT_LOGIN               = 16,
    PCL_ACK_SETS            = 17,
    PKT_COORDS              = 18,
    PKT_DEMOCAM             = 19,
    PKT_DEMOCAM_RESUME      = 20,
    PCL_HELLO2              = 21, // Includes game ID
    PSV_FRAME2              = 22, // Frame packet v2
    PSV_FIRST_FRAME2        = 23, // First PSV_FRAME2 after map change
    PSV_SOUND2              = 24, // unused?
    PSV_STOP_SOUND          = 25,
    PCL_ACKS                = 26,
    PSV_PLAYER_FIX_OBSOLETE = 27, // Fix angles/pos/mom (without console number).
    PCL_ACK_PLAYER_FIX      = 28, // Acknowledge player fix. /* 28 */
    PKT_COMMAND2            = 29,
    PSV_PLAYER_FIX          = 30, // Fix angles/pos/mom.
    PCL_GOODBYE             = 31,
    PSV_MOBJ_TYPE_ID_LIST   = 32,
    PSV_MOBJ_STATE_ID_LIST  = 33,

    // Game specific events.
    PKT_GAME_MARKER = DDPT_FIRST_GAME_EVENT, // 64
};

// Use the number defined in dd_share.h for sound packets.
// This is for backwards compatibility.
#define PSV_SOUND           71     /* DDPT_SOUND */

// Prefer adding new flags inside the deltas instead of adding new delta types.
typedef enum {
    DT_MOBJ = 0,
    DT_PLAYER = 1,
    //DT_SECTOR_R6 = 2, // 2 bytes for flags.
    DT_SIDE_SOUND = 3,
    DT_POLY = 4,
    DT_LUMP = 5,
    DT_SOUND = 6, // No emitter
    DT_MOBJ_SOUND = 7,
    DT_SECTOR_SOUND = 8,
    DT_POLY_SOUND = 9,
    DT_SECTOR = 10, // Flags in a packed long.

    // Special types: (only in the PSV_FRAME2 packet when written to message)
    DT_NULL_MOBJ = 11, // Mobj was removed (just type and ID).
    DT_CREATE_MOBJ = 12, // Regular DT_MOBJ, but the mobj was just created.

    DT_SIDE = 13, // Flags in a packed long.

    NUM_DELTA_TYPES
} deltatype_t;

// These DD-flags are packed (i.e. included in mobj deltas).
#define DDMF_PACK_MASK      0x3cfff1ff

// Mobj delta flags. These are used to determine what a delta contains.
// (Which parts of a delta mobj_t are used.)
#define MDF_ORIGIN_X            0x0001
#define MDF_ORIGIN_Y            0x0002
#define MDF_ORIGIN_Z            0x0004
#define MDF_ORIGIN              0x0007
#define MDF_MOM_X               0x0008
#define MDF_MOM_Y               0x0010
#define MDF_MOM_Z               0x0020
#define MDF_MOM                 0x0038
#define MDF_ANGLE               0x0040
#define MDF_HEALTH              0x0080
#define MDF_MORE_FLAGS          0x0100 // A byte of extra flags follows.
#define MDF_SELSPEC             0x0200 // Only during transfer.
#define MDF_SELECTOR            0x0400
#define MDF_STATE               0x0800
#define MDF_RADIUS              0x1000
#define MDF_HEIGHT              0x2000
#define MDF_FLAGS               0x4000
#define MDF_FLOORCLIP           0x8000
#define MDF_EVERYTHING          (MDF_ORIGIN | MDF_MOM | MDF_ANGLE | MDF_SELECTOR | MDF_STATE |\
                                 MDF_RADIUS | MDF_HEIGHT | MDF_FLAGS | MDF_HEALTH | MDF_FLOORCLIP)

// Extra flags for the Extra Flags byte.
#define MDFE_FAST_MOM           0x01 // Momentum has 10.6 bits (+/- 512)
#define MDFE_TRANSLUCENCY       0x02
#define MDFE_Z_FLOOR            0x04 // Mobj z is on the floor.
#define MDFE_Z_CEILING          0x08 // Mobj z+hgt is in the ceiling.
#define MDFE_FADETARGET         0x10
#define MDFE_TYPE               0x20 // Mobj type.

// Player delta flags.
#define PDF_MOBJ                0x0001
#define PDF_FORWARDMOVE         0x0002
#define PDF_SIDEMOVE            0x0004
#define PDF_ANGLE               0x0008
#define PDF_TURNDELTA           0x0010
#define PDF_FRICTION            0x0020
#define PDF_EXTRALIGHT          0x0040 // Plus fixedcolormap (same byte).
#define PDF_FILTER              0x0080
//#define PDF_CLYAW               0x1000 // Sent in the player num byte.
//#define PDF_CLPITCH             0x2000 // Sent in the player num byte.
#define PDF_PSPRITES            0x4000 // Sent in the player num byte.

// Written separately, stored in playerdelta flags 2 highest bytes.
#define PSDF_STATEPTR           0x01
#define PSDF_OFFSET             0x08
#define PSDF_LIGHT              0x20
#define PSDF_ALPHA              0x40
#define PSDF_STATE              0x80

#define SDF_FLOOR_MATERIAL      0x00000001
#define SDF_CEILING_MATERIAL    0x00000002
#define SDF_LIGHT               0x00000004
#define SDF_FLOOR_TARGET        0x00000008
#define SDF_FLOOR_SPEED         0x00000010
#define SDF_CEILING_TARGET      0x00000020
#define SDF_CEILING_SPEED       0x00000040
#define SDF_FLOOR_TEXMOVE       0x00000080
//#define SDF_CEILING_TEXMOVE     0x00000100 // obsolete
#define SDF_COLOR_RED           0x00000200
#define SDF_COLOR_GREEN         0x00000400
#define SDF_COLOR_BLUE          0x00000800
#define SDF_FLOOR_SPEED_44      0x00001000 // Used for sent deltas.
#define SDF_CEILING_SPEED_44    0x00002000 // Used for sent deltas.
#define SDF_FLOOR_HEIGHT        0x00004000
#define SDF_CEILING_HEIGHT      0x00008000
#define SDF_FLOOR_COLOR_RED     0x00010000
#define SDF_FLOOR_COLOR_GREEN   0x00020000
#define SDF_FLOOR_COLOR_BLUE    0x00040000
#define SDF_CEIL_COLOR_RED      0x00080000
#define SDF_CEIL_COLOR_GREEN    0x00100000
#define SDF_CEIL_COLOR_BLUE     0x00200000

#define SIDF_TOP_MATERIAL       0x0001
#define SIDF_MID_MATERIAL       0x0002
#define SIDF_BOTTOM_MATERIAL    0x0004
#define SIDF_LINE_FLAGS         0x0008
#define SIDF_TOP_COLOR_RED      0x0010
#define SIDF_TOP_COLOR_GREEN    0x0020
#define SIDF_TOP_COLOR_BLUE     0x0040
#define SIDF_MID_COLOR_RED      0x0080
#define SIDF_MID_COLOR_GREEN    0x0100
#define SIDF_MID_COLOR_BLUE     0x0200
#define SIDF_MID_COLOR_ALPHA    0x0400
#define SIDF_BOTTOM_COLOR_RED   0x0800
#define SIDF_BOTTOM_COLOR_GREEN 0x1000
#define SIDF_BOTTOM_COLOR_BLUE  0x2000
#define SIDF_MID_BLENDMODE      0x4000
#define SIDF_FLAGS              0x8000

#define PODF_DEST_X             0x01
#define PODF_DEST_Y             0x02
#define PODF_SPEED              0x04
#define PODF_DEST_ANGLE         0x08
#define PODF_ANGSPEED           0x10
#define PODF_PERPETUAL_ROTATE   0x20 // Special flag.

#define LDF_INFO                0x01

// Sound delta flags.
#define SNDDF_VOLUME            0x01 // 0=stop, 1=full, >1=no att.
#define SNDDF_REPEAT            0x02 // Start repeating sound.
#define SNDDF_PLANE_FLOOR       0x04 // Play sound from a sector's floor.
#define SNDDF_PLANE_CEILING     0x08 // Play sound from a sector's ceiling.
#define SNDDF_SIDE_TOP          0x10 // Play sound from a side's top part.
#define SNDDF_SIDE_MIDDLE       0x20 // Play sound from a side's middle part.
#define SNDDF_SIDE_BOTTOM       0x40 // Play sound from a side's bottom part.

/**
 * @defgroup soundPacketFlags  Sound Packet Flags
 * Used with PSV_SOUND packets.
 * @ingroup flags
 */
///@{
#define SNDF_ORIGIN             0x01 ///< Sound has an origin.
#define SNDF_SECTOR             0x02 ///< Originates from a degenmobj.
#define SNDF_PLAYER             0x04 ///< Originates from a player.
#define SNDF_VOLUME             0x08 ///< Volume included.
#define SNDF_ID                 0x10 ///< Mobj ID of the origin.
#define SNDF_REPEATING          0x20 ///< Repeat sound indefinitely.
#define SNDF_SHORT_SOUND_ID     0x40 ///< Sound ID is a short.
///@}

/**
 * @defgroup stopSoundPacketFlags  Stop Sound Packet Flags
 * Used with PSV_STOP_SOUND packets.
 * @ingroup flags
 */
///@{
#define STOPSNDF_SOUND_ID       0x01
#define STOPSNDF_ID             0x02
#define STOPSNDF_SECTOR         0x04
///@}

/// Largest message sendable using the protocol.
#define PROTOCOL_MAX_DATAGRAM_SIZE (1 << 22) // 4 MB

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
