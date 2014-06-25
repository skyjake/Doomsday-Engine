/** @file id1map.cpp  id Tech 1 map format reader.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "wadmapconverter.h"
#include "id1map_util.h"
#include <de/libcore.h>
#include <de/Error>
#include <de/Log>
#include <de/Time>
#include <de/memory.h>
#include <de/timer.h>
#include <QVector>

using namespace de;

namespace wadimp {
namespace internal {

/// Sizes of the map data structures in the arrived map formats (in bytes).
#define SIZEOF_64VERTEX         (4 * 2)
#define SIZEOF_VERTEX           (2 * 2)
#define SIZEOF_64THING          (2 * 7)
#define SIZEOF_XTHING           (2 * 7 + 1 * 6)
#define SIZEOF_THING            (2 * 5)
#define SIZEOF_XLINEDEF         (2 * 5 + 1 * 6)
#define SIZEOF_64LINEDEF        (2 * 6 + 1 * 4)
#define SIZEOF_LINEDEF          (2 * 7)
#define SIZEOF_64SIDEDEF        (2 * 6)
#define SIZEOF_SIDEDEF          (2 * 3 + 8 * 3)
#define SIZEOF_64SECTOR         (2 * 12)
#define SIZEOF_SECTOR           (2 * 5 + 8 * 2)
#define SIZEOF_LIGHT            (1 * 6)

struct SideDef
{
    int                 index;
    int16_t             offset[2];
    Id1Map::MaterialId  topMaterial;
    Id1Map::MaterialId  bottomMaterial;
    Id1Map::MaterialId  middleMaterial;
    int                 sector;

    void read(reader_s &reader, Id1Map &map)
    {
        offset[VX] = SHORT( Reader_ReadInt16(&reader) );
        offset[VY] = SHORT( Reader_ReadInt16(&reader) );

        char name[9];
        Reader_Read(&reader, name, 8); name[8] = '\0';
        topMaterial    = map.toMaterialId(name, Id1Map::WallMaterials);

        Reader_Read(&reader, name, 8); name[8] = '\0';
        bottomMaterial = map.toMaterialId(name, Id1Map::WallMaterials);

        Reader_Read(&reader, name, 8); name[8] = '\0';
        middleMaterial = map.toMaterialId(name, Id1Map::WallMaterials);

        int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sector = (idx == 0xFFFF? -1 : idx);
    }

    void read_Doom64(reader_s &reader, Id1Map &map)
    {
        offset[VX] = SHORT( Reader_ReadInt16(&reader) );
        offset[VY] = SHORT( Reader_ReadInt16(&reader) );

        int idx;
        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        topMaterial    = map.toMaterialId(idx, Id1Map::WallMaterials);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        bottomMaterial = map.toMaterialId(idx, Id1Map::WallMaterials);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        middleMaterial = map.toMaterialId(idx, Id1Map::WallMaterials);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sector = (idx == 0xFFFF? -1 : idx);
    }
};

/**
 * @defgroup lineAnalysisFlags  Line Analysis flags
 */
///@{
#define LAF_POLYOBJ             0x1 ///< Line defines a polyobj segment.
///@}

#define PO_LINE_START           (1) ///< Polyobj line start special.
#define PO_LINE_EXPLICIT        (5)

#define SEQTYPE_NUMSEQ          (10)

struct LineDef
{
    enum Side {
        Front,
        Back
    };

    int             index;
    int             v[2];
    int             sides[2];
    int16_t         flags; ///< MF_* flags.

    // Analysis data:
    int16_t         aFlags;

    // DOOM format members:
    int16_t         dType;
    int16_t         dTag;

    // Hexen format members:
    int8_t          xType;
    int8_t          xArgs[5];

    // DOOM64 format members:
    int8_t          d64drawFlags;
    int8_t          d64texFlags;
    int8_t          d64type;
    int8_t          d64useType;
    int16_t         d64tag;

    int             ddFlags;
    uint            validCount; ///< Used for polyobj line collection.

    void read(reader_s &reader, Id1Map &map)
    {
        int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        v[0] = (idx == 0xFFFF? -1 : idx);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        v[1] = (idx == 0xFFFF? -1 : idx);

        flags = SHORT( Reader_ReadInt16(&reader) );
        dType = SHORT( Reader_ReadInt16(&reader) );
        dTag  = SHORT( Reader_ReadInt16(&reader) );

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sides[Front] = (idx == 0xFFFF? -1 : idx);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sides[Back] = (idx == 0xFFFF? -1 : idx);

        aFlags       = 0;
        validCount   = 0;
        ddFlags      = 0;

        xlatFlags(map.format());
    }

    void read_Doom64(reader_s &reader, Id1Map &map)
    {
        int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        v[0] = (idx == 0xFFFF? -1 : idx);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        v[1] = (idx == 0xFFFF? -1 : idx);

        flags = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        d64drawFlags = Reader_ReadByte(&reader);
        d64texFlags  = Reader_ReadByte(&reader);
        d64type      = Reader_ReadByte(&reader);
        d64useType   = Reader_ReadByte(&reader);
        d64tag       = SHORT( Reader_ReadInt16(&reader) );

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sides[Front] = (idx == 0xFFFF? -1 : idx);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sides[Back] = (idx == 0xFFFF? -1 : idx);

        aFlags       = 0;
        validCount   = 0;
        ddFlags      = 0;

        xlatFlags(map.format());
    }

    void read_Hexen(reader_s &reader, Id1Map &map)
    {
        int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        v[0] = (idx == 0xFFFF? -1 : idx);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        v[1] = (idx == 0xFFFF? -1 : idx);

        flags = SHORT( Reader_ReadInt16(&reader) );
        xType    = Reader_ReadByte(&reader);
        xArgs[0] = Reader_ReadByte(&reader);
        xArgs[1] = Reader_ReadByte(&reader);
        xArgs[2] = Reader_ReadByte(&reader);
        xArgs[3] = Reader_ReadByte(&reader);
        xArgs[4] = Reader_ReadByte(&reader);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sides[Front] = (idx == 0xFFFF? -1 : idx);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        sides[Back] = (idx == 0xFFFF? -1 : idx);

        aFlags       = 0;
        validCount   = 0;
        ddFlags      = 0;

        xlatFlags(map.format());
    }

    /**
     * Translate the line flags for Doomsday.
     */
    void xlatFlags(Id1Map::Format mapFormat)
    {
#define ML_BLOCKING              1 ///< Solid, is an obstacle.
#define ML_DONTPEGTOP            8 ///< Upper texture unpegged.
#define ML_DONTPEGBOTTOM        16 ///< Lower texture unpegged.

/// If set ALL flags NOT in DOOM v1.9 will be zeroed upon map load.
#define ML_INVALID              2048
#define DOOM_VALIDMASK          0x000001ff

        /**
         * Zero unused flags if ML_INVALID is set.
         *
         * @attention "This has been found to be necessary because of errors
         *  in Ultimate DOOM's E2M7, where around 1000 linedefs have
         *  the value 0xFE00 masked into the flags value.
         *  There could potentially be many more maps with this problem,
         *  as it is well-known that Hellmaker wads set all bits in
         *  mapthings that it does not understand."
         *  Thanks to Quasar for the heads up.
         *
         * Only valid for DOOM format maps.
         */
        if(mapFormat == Id1Map::DoomFormat)
        {
            if(flags & ML_INVALID)
                flags &= DOOM_VALIDMASK;
        }

        if(flags & ML_BLOCKING)
        {
            ddFlags |= DDLF_BLOCKING;
            flags &= ~ML_BLOCKING;
        }

        if(flags & ML_DONTPEGTOP)
        {
            ddFlags |= DDLF_DONTPEGTOP;
            flags &= ~ML_DONTPEGTOP;
        }

        if(flags & ML_DONTPEGBOTTOM)
        {
            ddFlags |= DDLF_DONTPEGBOTTOM;
            flags &= ~ML_DONTPEGBOTTOM;
        }

#undef DOOM_VALIDMASK
#undef ML_INVALID
#undef ML_DONTPEGBOTTOM
#undef ML_DONTPEGTOP
#undef ML_BLOCKING
    }
};

struct SectorDef
{
    int             index;
    int16_t         floorHeight;
    int16_t         ceilHeight;
    int16_t         lightLevel;
    int16_t         type;
    int16_t         tag;
    Id1Map::MaterialId  floorMaterial;
    Id1Map::MaterialId  ceilMaterial;

    // DOOM64 format members:
    int16_t         d64flags;
    uint16_t        d64floorColor;
    uint16_t        d64ceilingColor;
    uint16_t        d64unknownColor;
    uint16_t        d64wallTopColor;
    uint16_t        d64wallBottomColor;

    void read(reader_s &reader, Id1Map &map)
    {
        floorHeight  = SHORT( Reader_ReadInt16(&reader) );
        ceilHeight   = SHORT( Reader_ReadInt16(&reader) );

        char name[9];
        Reader_Read(&reader, name, 8); name[8] = '\0';
        floorMaterial= map.toMaterialId(name, Id1Map::PlaneMaterials);

        Reader_Read(&reader, name, 8); name[8] = '\0';
        ceilMaterial = map.toMaterialId(name, Id1Map::PlaneMaterials);

        lightLevel   = SHORT( Reader_ReadInt16(&reader) );
        type         = SHORT( Reader_ReadInt16(&reader) );
        tag          = SHORT( Reader_ReadInt16(&reader) );
    }

    void read_Doom64(reader_s &reader, Id1Map &map)
    {
        floorHeight  = SHORT( Reader_ReadInt16(&reader));
        ceilHeight   = SHORT( Reader_ReadInt16(&reader) );

        int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        floorMaterial= map.toMaterialId(idx, Id1Map::PlaneMaterials);

        idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        ceilMaterial = map.toMaterialId(idx, Id1Map::PlaneMaterials);

        d64ceilingColor    = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        d64floorColor      = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        d64unknownColor    = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        d64wallTopColor    = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
        d64wallBottomColor = USHORT( uint16_t(Reader_ReadInt16(&reader)) );

        type     = SHORT( Reader_ReadInt16(&reader) );
        tag      = SHORT( Reader_ReadInt16(&reader) );
        d64flags = USHORT( uint16_t(Reader_ReadInt16(&reader)) );

        lightLevel = 160; ///?
    }
};

// Thing DoomEdNums for polyobj anchors/spawn spots.
#define PO_ANCHOR_DOOMEDNUM     (3000)
#define PO_SPAWN_DOOMEDNUM      (3001)
#define PO_SPAWNCRUSH_DOOMEDNUM (3002)

/// @todo Get these from a game api header.
#define MTF_Z_FLOOR         0x20000000 ///< Spawn relative to floor height.
#define MTF_Z_CEIL          0x40000000 ///< Spawn relative to ceiling height (minus thing height).
#define MTF_Z_RANDOM        0x80000000 ///< Random point between floor and ceiling.

#define ANG45               0x20000000

struct Thing
{
    int             index;
    int16_t         origin[3];
    angle_t         angle;
    int16_t         doomEdNum;
    int32_t         flags;
    int32_t         skillModes;

    // Hexen format members:
    int16_t         xTID;
    int8_t          xSpecial;
    int8_t          xArgs[5];

    // DOOM64 format members:
    int16_t         d64TID;

    void read(reader_s &reader, Id1Map & /*map*/)
    {
/**
 * DOOM Thing flags:
 */
#define MTF_EASY            0x00000001 ///< Can be spawned in Easy skill modes.
#define MTF_MEDIUM          0x00000002 ///< Can be spawned in Medium skill modes.
#define MTF_HARD            0x00000004 ///< Can be spawned in Hard skill modes.
#define MTF_DEAF            0x00000008 ///< Mobj will be deaf spawned deaf.
#define MTF_NOTSINGLE       0x00000010 ///< (BOOM) Can not be spawned in single player gamemodes.
#define MTF_NOTDM           0x00000020 ///< (BOOM) Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP         0x00000040 ///< (BOOM) Can not be spawned in the Co-op gameMode.
#define MTF_FRIENDLY        0x00000080 ///< (BOOM) friendly monster.

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_NOTDM|MTF_NOTCOOP|MTF_FRIENDLY))

        origin[VX]   = SHORT( Reader_ReadInt16(&reader) );
        origin[VY]   = SHORT( Reader_ReadInt16(&reader) );
        origin[VZ]   = 0;
        angle        = ANG45 * (SHORT( Reader_ReadInt16(&reader) ) / 45);
        doomEdNum    = SHORT( Reader_ReadInt16(&reader) );
        flags        = SHORT( Reader_ReadInt16(&reader) );

        skillModes = 0;
        if(flags & MTF_EASY)
            skillModes |= 0x00000001 | 0x00000002;
        if(flags & MTF_MEDIUM)
            skillModes |= 0x00000004;
        if(flags & MTF_HARD)
            skillModes |= 0x00000008 | 0x00000010;

        flags &= ~MASK_UNKNOWN_THING_FLAGS;
        // DOOM format things spawn on the floor by default unless their
        // type-specific flags override.
        flags |= MTF_Z_FLOOR;

#undef MASK_UNKNOWN_THING_FLAGS
#undef MTF_FRIENDLY
#undef MTF_NOTCOOP
#undef MTF_NOTDM
#undef MTF_NOTSINGLE
#undef MTF_AMBUSH
#undef MTF_HARD
#undef MTF_MEDIUM
#undef MTF_EASY
    }

    void read_Doom64(reader_s &reader, Id1Map & /*map*/)
    {
/**
 * DOOM64 Thing flags:
 */
#define MTF_EASY            0x00000001 ///< Appears in easy skill modes.
#define MTF_MEDIUM          0x00000002 ///< Appears in medium skill modes.
#define MTF_HARD            0x00000004 ///< Appears in hard skill modes.
#define MTF_DEAF            0x00000008 ///< Thing is deaf.
#define MTF_NOTSINGLE       0x00000010 ///< Appears in multiplayer game modes only.
#define MTF_DONTSPAWNATSTART 0x00000020 ///< Do not spawn this thing at map start.
#define MTF_SCRIPT_TOUCH    0x00000040 ///< Mobjs spawned from this spot will envoke a script when touched.
#define MTF_SCRIPT_DEATH    0x00000080 ///< Mobjs spawned from this spot will envoke a script on death.
#define MTF_SECRET          0x00000100 ///< A secret (bonus) item.
#define MTF_NOTARGET        0x00000200 ///< Mobjs spawned from this spot will not target their attacker when hurt.
#define MTF_NOTDM           0x00000400 ///< Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP         0x00000800 ///< Can not be spawned in the Co-op gameMode.

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_DONTSPAWNATSTART|MTF_SCRIPT_TOUCH|MTF_SCRIPT_DEATH|MTF_SECRET|MTF_NOTARGET|MTF_NOTDM|MTF_NOTCOOP))

        origin[VX]   = SHORT( Reader_ReadInt16(&reader) );
        origin[VY]   = SHORT( Reader_ReadInt16(&reader) );
        origin[VZ]   = SHORT( Reader_ReadInt16(&reader) );
        angle        = ANG45 * (SHORT( Reader_ReadInt16(&reader) ) / 45);
        doomEdNum    = SHORT( Reader_ReadInt16(&reader) );
        flags        = SHORT( Reader_ReadInt16(&reader) );

        skillModes = 0;
        if(flags & MTF_EASY)
            skillModes |= 0x00000001;
        if(flags & MTF_MEDIUM)
            skillModes |= 0x00000002;
        if(flags & MTF_HARD)
            skillModes |= 0x00000004 | 0x00000008;

        flags &= ~MASK_UNKNOWN_THING_FLAGS;
        // DOOM64 format things spawn relative to the floor by default
        // unless their type-specific flags override.
        flags |= MTF_Z_FLOOR;

        d64TID = SHORT( Reader_ReadInt16(&reader) );

#undef MASK_UNKNOWN_THING_FLAGS
#undef MTF_NOTCOOP
#undef MTF_NOTDM
#undef MTF_NOTARGET
#undef MTF_SECRET
#undef MTF_SCRIPT_DEATH
#undef MTF_SCRIPT_TOUCH
#undef MTF_DONTSPAWNATSTART
#undef MTF_NOTSINGLE
#undef MTF_DEAF
#undef MTF_HARD
#undef MTF_MEDIUM
#undef MTF_EASY
    }

    void read_Hexen(reader_s &reader, Id1Map & /*map*/)
    {
/**
 * Hexen Thing flags:
 */
#define MTF_EASY            0x00000001
#define MTF_MEDIUM          0x00000002
#define MTF_HARD            0x00000004
#define MTF_AMBUSH          0x00000008
#define MTF_DORMANT         0x00000010
#define MTF_FIGHTER         0x00000020
#define MTF_CLERIC          0x00000040
#define MTF_MAGE            0x00000080
#define MTF_GSINGLE         0x00000100
#define MTF_GCOOP           0x00000200
#define MTF_GDEATHMATCH     0x00000400
// The following are not currently used:
#define MTF_SHADOW          0x00000800 ///< (ZDOOM) Thing is 25% translucent.
#define MTF_INVISIBLE       0x00001000 ///< (ZDOOM) Makes the thing invisible.
#define MTF_FRIENDLY        0x00002000 ///< (ZDOOM) Friendly monster.
#define MTF_STILL           0x00004000 ///< (ZDOOM) Thing stands still (only useful for specific Strife monsters or friendlies).

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_AMBUSH|MTF_DORMANT|MTF_FIGHTER|MTF_CLERIC|MTF_MAGE|MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH|MTF_SHADOW|MTF_INVISIBLE|MTF_FRIENDLY|MTF_STILL))

        xTID         = SHORT( Reader_ReadInt16(&reader) );
        origin[VX]   = SHORT( Reader_ReadInt16(&reader) );
        origin[VY]   = SHORT( Reader_ReadInt16(&reader) );
        origin[VZ]   = SHORT( Reader_ReadInt16(&reader) );
        angle        = SHORT( Reader_ReadInt16(&reader) );
        doomEdNum    = SHORT( Reader_ReadInt16(&reader) );

        /**
         * For some reason, the Hexen format stores polyobject tags in
         * the angle field in THINGS. Thus, we cannot translate the
         * angle until we know whether it is a polyobject type or no
         */
        if(doomEdNum != PO_ANCHOR_DOOMEDNUM &&
           doomEdNum != PO_SPAWN_DOOMEDNUM &&
           doomEdNum != PO_SPAWNCRUSH_DOOMEDNUM)
        {
            angle = ANG45 * (angle / 45);
        }

        flags = SHORT( Reader_ReadInt16(&reader) );

        skillModes = 0;
        if(flags & MTF_EASY)
            skillModes |= 0x00000001 | 0x00000002;
        if(flags & MTF_MEDIUM)
            skillModes |= 0x00000004;
        if(flags & MTF_HARD)
            skillModes |= 0x00000008 | 0x00000010;

        flags &= ~MASK_UNKNOWN_THING_FLAGS;
        /**
         * Translate flags:
         */
        // Game type logic is inverted.
        flags ^= (MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH);

        // HEXEN format things spawn relative to the floor by default
        // unless their type-specific flags override.
        flags |= MTF_Z_FLOOR;

        xSpecial = Reader_ReadByte(&reader);
        xArgs[0] = Reader_ReadByte(&reader);
        xArgs[1] = Reader_ReadByte(&reader);
        xArgs[2] = Reader_ReadByte(&reader);
        xArgs[3] = Reader_ReadByte(&reader);
        xArgs[4] = Reader_ReadByte(&reader);

#undef MASK_UNKNOWN_THING_FLAGS
#undef MTF_STILL
#undef MTF_FRIENDLY
#undef MTF_INVISIBLE
#undef MTF_SHADOW
#undef MTF_GDEATHMATCH
#undef MTF_GCOOP
#undef MTF_GSINGLE
#undef MTF_MAGE
#undef MTF_CLERIC
#undef MTF_FIGHTER
#undef MTF_DORMANT
#undef MTF_AMBUSH
#undef MTF_HARD
#undef MTF_NORMAL
#undef MTF_EASY
    }
};

struct Polyobj
{
    typedef QVector<int> LineIndices;

    int             index;
    LineIndices     lineIndices;
    int             tag;
    int             seqType;
    int16_t         anchor[2];
};

struct TintColor
{
    int             index;
    float           rgb[3];
    int8_t          xx[3];

    void read_Doom64(reader_s &reader, Id1Map & /*map*/)
    {
        rgb[0] = Reader_ReadByte(&reader) / 255.f;
        rgb[1] = Reader_ReadByte(&reader) / 255.f;
        rgb[2] = Reader_ReadByte(&reader) / 255.f;
        xx[0]  = Reader_ReadByte(&reader);
        xx[1]  = Reader_ReadByte(&reader);
        xx[2]  = Reader_ReadByte(&reader);
    }
};

} // namespace internal

using namespace internal;

static reader_s *bufferLump(lumpnum_t lumpNum);
static void clearReadBuffer();

static uint validCount = 0; // Used for Polyobj LineDef collection.

DENG2_PIMPL(Id1Map)
{
    Format format;

    QVector<coord_t> vertCoords; ///< Array of vertex coords [v0:X, vo:Y, v1:X, v1:Y, ..]

    typedef std::vector<LineDef> Lines;
    Lines lines;

    typedef std::vector<SideDef> Sides;
    Sides sides;

    typedef std::vector<SectorDef> Sectors;
    Sectors sectors;

    typedef std::vector<Thing> Things;
    Things things;

    typedef std::vector<TintColor> SurfaceTints;
    SurfaceTints surfaceTints;

    typedef std::list<Polyobj> Polyobjs;
    Polyobjs polyobjs;

    struct MaterialDict
    {
        StringPool dict;

        String const &find(MaterialId id) const
        {
            return dict.stringRef(id);
        }

        MaterialId toMaterialId(String name, MaterialGroup group)
        {
            // In original DOOM, texture name references beginning with the
            // hypen '-' character are always treated as meaning "no reference"
            // or "invalid texture" and surfaces using them were not drawn.
            if(group != PlaneMaterials && name[0] == '-')
            {
                return 0; // Not a valid id.
            }

            // Prepare the encoded URI for insertion into the dictionary.
            // Material paths must be encoded.
            AutoStr *path = Str_PercentEncode(AutoStr_FromText(name.toUtf8().constData()));
            Uri *uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
            Uri_SetScheme(uri, group == PlaneMaterials? "Flats" : "Textures");
            AutoStr *uriCString = Uri_Compose(uri);
            Uri_Delete(uri);

            // Intern this material URI in the dictionary.
            return dict.intern(String(Str_Text(uriCString)));
        }

        MaterialId toMaterialId(int uniqueId, MaterialGroup group)
        {
            // Prepare the encoded URI for insertion into the dictionary.
            AutoStr *uriCString;

            Uri *textureUrn = Uri_NewWithPath2(Str_Text(Str_Appendf(AutoStr_NewStd(), "urn:%s:%i", group == PlaneMaterials? "Flats" : "Textures", uniqueId)), RC_NULL);
            Uri *uri = Materials_ComposeUri(P_ToIndex(DD_MaterialForTextureUri(textureUrn)));
            uriCString = Uri_Compose(uri);
            Uri_Delete(uri);
            Uri_Delete(textureUrn);

            // Intern this material URI in the dictionary.
            return dict.intern(String(Str_Text(uriCString)));
        }
    } materials;

    Instance(Public *i)
        : Base(i)
        , format(UnknownFormat)
    {}

    inline int vertexCount() const {
        return vertCoords.count() / 2;
    }

    /// @todo fixme: A real performance killer...
    inline AutoStr *composeMaterialRef(MaterialId id) {
        return AutoStr_FromTextStd(materials.find(id).toUtf8().constData());
    }

    bool loadVertexes(reader_s &reader, int numElements)
    {
        LOGDEV_MAP_XVERBOSE("Processing vertexes...");
        for(int n = 0; n < numElements; ++n)
        {
            switch(format)
            {
            default:
            case DoomFormat:
                vertCoords[n * 2]     = coord_t( SHORT(Reader_ReadInt16(&reader)) );
                vertCoords[n * 2 + 1] = coord_t( SHORT(Reader_ReadInt16(&reader)) );
                break;

            case Doom64Format:
                vertCoords[n * 2]     = coord_t( FIX2FLT(LONG(Reader_ReadInt32(&reader))) );
                vertCoords[n * 2 + 1] = coord_t( FIX2FLT(LONG(Reader_ReadInt32(&reader))) );
                break;
            }
        }

        return true;
    }

    bool loadLineDefs(reader_s &reader, int numElements)
    {
        LOGDEV_MAP_XVERBOSE("Processing line definitions...");
        if(numElements)
        {
            lines.reserve(lines.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                lines.push_back(LineDef());
                LineDef &line = lines.back();

                line.index = n;

                switch(format)
                {
                default:
                case DoomFormat:   line.read(reader, self);        break;

                case Doom64Format: line.read_Doom64(reader, self); break;
                case HexenFormat:  line.read_Hexen(reader, self);  break;
                }
            }
        }

        return true;
    }

    bool loadSideDefs(reader_s &reader, int numElements)
    {
        LOGDEV_MAP_XVERBOSE("Processing side definitions...");
        if(numElements)
        {
            sides.reserve(sides.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                sides.push_back(SideDef());
                SideDef &side = sides.back();

                side.index = n;

                switch(format)
                {
                default:
                case DoomFormat:   side.read(reader, self);        break;

                case Doom64Format: side.read_Doom64(reader, self); break;
                }
            }
        }

        return true;
    }

    bool loadSectors(reader_s &reader, int numElements)
    {
        LOGDEV_MAP_XVERBOSE("Processing sectors...");
        if(numElements)
        {
            sectors.reserve(sectors.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                sectors.push_back(SectorDef());
                SectorDef &sector = sectors.back();

                sector.index = n;

                switch(format)
                {
                default:
                case DoomFormat:
                case HexenFormat:  sector.read(reader, self);        break;

                case Doom64Format: sector.read_Doom64(reader, self); break;
                }
            }
        }

        return true;
    }

    bool loadThings(reader_s &reader, int numElements)
    {
        LOGDEV_MAP_XVERBOSE("Processing things...");
        if(numElements)
        {
            things.reserve(things.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                things.push_back(Thing());
                Thing &thing = things.back();

                thing.index = n;

                switch(format)
                {
                default:
                case DoomFormat:   thing.read(reader, self);        break;

                case Doom64Format: thing.read_Doom64(reader, self); break;
                case HexenFormat:  thing.read_Hexen(reader, self);  break;
                }
            }
        }

        return true;
    }

    bool loadSurfaceTints(reader_s &reader, int numElements)
    {
        LOGDEV_MAP_XVERBOSE("Processing surface tints...");
        if(numElements)
        {
            surfaceTints.reserve(surfaceTints.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                surfaceTints.push_back(TintColor());
                TintColor &tint = surfaceTints.back();

                tint.index = n;
                tint.read_Doom64(reader, self);
            }
        }

        return true;
    }

    /**
     * Create a temporary polyobj.
     */
    Polyobj *createPolyobj(Polyobj::LineIndices const &lineIndices, int tag,
        int sequenceType, int16_t anchorX, int16_t anchorY)
    {
        // Allocate the new polyobj.
        polyobjs.push_back(Polyobj());
        Polyobj *po = &polyobjs.back();

        po->index       = polyobjs.size() - 1;
        po->tag         = tag;
        po->seqType     = sequenceType;
        po->anchor[VX]  = anchorX;
        po->anchor[VY]  = anchorY;
        po->lineIndices = lineIndices; // A copy is made.

        foreach(int lineIdx, po->lineIndices)
        {
            LineDef *line = &lines[lineIdx];

            // This line now belongs to a polyobj.
            line->aFlags |= LAF_POLYOBJ;

            /*
             * Due a logic error in hexen.exe, when the column drawer is presented
             * with polyobj segs built from two-sided linedefs; clipping is always
             * calculated using the pegging logic for single-sided linedefs.
             *
             * Here we emulate this behavior by automatically applying bottom unpegging
             * for two-sided linedefs.
             */
            if(line->sides[LineDef::Back] >= 0)
                line->ddFlags |= DDLF_DONTPEGBOTTOM;
        }

        return po;
    }

    /**
     * Find all linedefs marked as belonging to a polyobject with the given tag
     * and attempt to create a polyobject from them.
     *
     * @param tag  Line tag of linedefs to search for.
     *
     * @return  @c true = successfully created polyobj.
     */
    bool findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY)
    {
        Polyobj::LineIndices polyLines;

        // First look for a PO_LINE_START linedef set with this tag.
        DENG2_FOR_EACH(Lines, i, lines)
        {
            // Already belongs to another polyobj?
            if(i->aFlags & LAF_POLYOBJ) continue;

            if(!(i->xType == PO_LINE_START && i->xArgs[0] == tag)) continue;

            collectPolyobjLines(polyLines, i);
            if(!polyLines.isEmpty())
            {
                int8_t sequenceType = i->xArgs[2];
                if(sequenceType >= SEQTYPE_NUMSEQ) sequenceType = 0;

                createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
                return true;
            }
            return false;
        }

        // Perhaps a PO_LINE_EXPLICIT linedef set with this tag?
        for(int n = 0; ; ++n)
        {
            bool foundAnotherLine = false;

            DENG2_FOR_EACH(Lines, i, lines)
            {
                // Already belongs to another polyobj?
                if(i->aFlags & LAF_POLYOBJ) continue;

                if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
                {
                    if(i->xArgs[1] <= 0)
                    {
                        LOG_MAP_WARNING("Linedef missing (probably #%d) in explicit polyobj (tag:%d)") << n + 1 << tag;
                        return false;
                    }

                    if(i->xArgs[1] == n + 1)
                    {
                        // Add this line to the list.
                        polyLines.append( i - lines.begin() );
                        foundAnotherLine = true;

                        // Clear any special.
                        i->xType = 0;
                        i->xArgs[0] = 0;
                    }
                }
            }

            if(foundAnotherLine)
            {
                // Check if an explicit line order has been skipped.
                // A line has been skipped if there are any more explicit lines with
                // the current tag value.
                DENG2_FOR_EACH(Lines, i, lines)
                {
                    if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
                    {
                        LOG_MAP_WARNING("Linedef missing (#%d) in explicit polyobj (tag:%d)") << n << tag;
                        return false;
                    }
                }
            }
            else
            {
                // All lines have now been found.
                break;
            }
        }

        if(polyLines.isEmpty())
        {
            LOG_MAP_WARNING("Failed to locate a single line for polyobj (tag:%d)") << tag;
            return false;
        }

        LineDef *line = &lines[ polyLines.first() ];
        int8_t const sequenceType = line->xArgs[3];

        // Setup the mirror if it exists.
        line->xArgs[1] = line->xArgs[2];

        createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
        return true;
    }

    void analyze()
    {
        Time begunAt;

        if(format == HexenFormat)
        {
            LOGDEV_MAP_XVERBOSE("Locating polyobjs...");
            DENG2_FOR_EACH(Things, i, things)
            {
                // A polyobj anchor?
                if(i->doomEdNum == PO_ANCHOR_DOOMEDNUM)
                {
                    int const tag = i->angle;
                    findAndCreatePolyobj(tag, i->origin[VX], i->origin[VY]);
                }
            }
        }

        LOGDEV_MAP_MSG("Analyses completed in %.2f seconds") << begunAt.since();
    }

    void transferVertexes()
    {
        LOGDEV_MAP_XVERBOSE("Transfering vertexes...");
        int const numVertexes = vertexCount();
        int *indices = new int[numVertexes];
        for(int i = 0; i < numVertexes; ++i)
        {
            indices[i] = i;
        }
        MPE_VertexCreatev(numVertexes, vertCoords.constData(), indices, 0);
        delete[] indices;
    }

    void transferSectors()
    {
        LOGDEV_MAP_XVERBOSE("Transfering sectors...");

        DENG2_FOR_EACH(Sectors, i, sectors)
        {
            int idx = MPE_SectorCreate(float(i->lightLevel) / 255.0f, 1, 1, 1, i->index);

            MPE_PlaneCreate(idx, i->floorHeight, composeMaterialRef(i->floorMaterial),
                            0, 0, 1, 1, 1, 1, 0, 0, 1, -1);
            MPE_PlaneCreate(idx, i->ceilHeight, composeMaterialRef(i->ceilMaterial),
                            0, 0, 1, 1, 1, 1, 0, 0, -1, -1);

            MPE_GameObjProperty("XSector", idx, "Tag",    DDVT_SHORT, &i->tag);
            MPE_GameObjProperty("XSector", idx, "Type",   DDVT_SHORT, &i->type);

            if(format == Doom64Format)
            {
                MPE_GameObjProperty("XSector", idx, "Flags",          DDVT_SHORT, &i->d64flags);
                MPE_GameObjProperty("XSector", idx, "CeilingColor",   DDVT_SHORT, &i->d64ceilingColor);
                MPE_GameObjProperty("XSector", idx, "FloorColor",     DDVT_SHORT, &i->d64floorColor);
                MPE_GameObjProperty("XSector", idx, "UnknownColor",   DDVT_SHORT, &i->d64unknownColor);
                MPE_GameObjProperty("XSector", idx, "WallTopColor",   DDVT_SHORT, &i->d64wallTopColor);
                MPE_GameObjProperty("XSector", idx, "WallBottomColor", DDVT_SHORT, &i->d64wallBottomColor);
            }
        }
    }

    void transferLinesAndSides()
    {
        LOGDEV_MAP_XVERBOSE("Transfering lines and sides...");
        DENG2_FOR_EACH(Lines, i, lines)
        {
            SideDef *front = ((i)->sides[LineDef::Front] >= 0? &sides[(i)->sides[LineDef::Front]] : 0);
            SideDef *back  = ((i)->sides[LineDef::Back]  >= 0? &sides[(i)->sides[LineDef::Back]] : 0);

            int sideFlags = (format == Doom64Format? SDF_MIDDLE_STRETCH : 0);

            // Interpret the lack of a ML_TWOSIDED line flag to mean the
            // suppression of the side relative back sector.
            if(!(i->flags & 0x4 /*ML_TWOSIDED*/) && front && back)
                sideFlags |= SDF_SUPPRESS_BACK_SECTOR;

            int lineIdx = MPE_LineCreate(i->v[0], i->v[1], front? front->sector : -1,
                                         back? back->sector : -1, i->ddFlags, i->index);
            if(front)
            {
                MPE_LineAddSide(lineIdx, LineDef::Front, sideFlags,
                                composeMaterialRef(front->topMaterial),
                                front->offset[VX], front->offset[VY], 1, 1, 1,
                                composeMaterialRef(front->middleMaterial),
                                front->offset[VX], front->offset[VY], 1, 1, 1, 1,
                                composeMaterialRef(front->bottomMaterial),
                                front->offset[VX], front->offset[VY], 1, 1, 1,
                                front->index);
            }
            if(back)
            {
                MPE_LineAddSide(lineIdx, LineDef::Back, sideFlags,
                                composeMaterialRef(back->topMaterial),
                                back->offset[VX], back->offset[VY], 1, 1, 1,
                                composeMaterialRef(back->middleMaterial),
                                back->offset[VX], back->offset[VY], 1, 1, 1, 1,
                                composeMaterialRef(back->bottomMaterial),
                                back->offset[VX], back->offset[VY], 1, 1, 1,
                                back->index);
            }

            MPE_GameObjProperty("XLinedef", lineIdx, "Flags", DDVT_SHORT, &i->flags);

            switch(format)
            {
            default:
            case DoomFormat:
                MPE_GameObjProperty("XLinedef", lineIdx, "Type",  DDVT_SHORT, &i->dType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Tag",   DDVT_SHORT, &i->dTag);
                break;

            case Doom64Format:
                MPE_GameObjProperty("XLinedef", lineIdx, "DrawFlags", DDVT_BYTE,  &i->d64drawFlags);
                MPE_GameObjProperty("XLinedef", lineIdx, "TexFlags",  DDVT_BYTE,  &i->d64texFlags);
                MPE_GameObjProperty("XLinedef", lineIdx, "Type",      DDVT_BYTE,  &i->d64type);
                MPE_GameObjProperty("XLinedef", lineIdx, "UseType",   DDVT_BYTE,  &i->d64useType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Tag",       DDVT_SHORT, &i->d64tag);
                break;

            case HexenFormat:
                MPE_GameObjProperty("XLinedef", lineIdx, "Type", DDVT_BYTE, &i->xType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg0", DDVT_BYTE, &i->xArgs[0]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg1", DDVT_BYTE, &i->xArgs[1]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg2", DDVT_BYTE, &i->xArgs[2]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg3", DDVT_BYTE, &i->xArgs[3]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg4", DDVT_BYTE, &i->xArgs[4]);
                break;
            }
        }
    }

    void transferSurfaceTints()
    {
        if(surfaceTints.empty()) return;

        LOGDEV_MAP_XVERBOSE("Transfering surface tints...");
        DENG2_FOR_EACH(SurfaceTints, i, surfaceTints)
        {
            int idx = i - surfaceTints.begin();

            MPE_GameObjProperty("Light", idx, "ColorR",   DDVT_FLOAT, &i->rgb[0]);
            MPE_GameObjProperty("Light", idx, "ColorG",   DDVT_FLOAT, &i->rgb[1]);
            MPE_GameObjProperty("Light", idx, "ColorB",   DDVT_FLOAT, &i->rgb[2]);
            MPE_GameObjProperty("Light", idx, "XX0",      DDVT_BYTE,  &i->xx[0]);
            MPE_GameObjProperty("Light", idx, "XX1",      DDVT_BYTE,  &i->xx[1]);
            MPE_GameObjProperty("Light", idx, "XX2",      DDVT_BYTE,  &i->xx[2]);
        }
    }

    void transferPolyobjs()
    {
        if(polyobjs.empty()) return;

        LOGDEV_MAP_XVERBOSE("Transfering polyobjs...");
        DENG2_FOR_EACH(Polyobjs, i, polyobjs)
        {
            MPE_PolyobjCreate(i->lineIndices.constData(), i->lineIndices.count(),
                              i->tag, i->seqType,
                              coord_t(i->anchor[VX]), coord_t(i->anchor[VY]),
                              i->index);
        }
    }

    void transferThings()
    {
        if(things.empty()) return;

        LOGDEV_MAP_XVERBOSE("Transfering things...");
        DENG2_FOR_EACH(Things, i, things)
        {
            int idx = i - things.begin();

            MPE_GameObjProperty("Thing", idx, "X",            DDVT_SHORT, &i->origin[VX]);
            MPE_GameObjProperty("Thing", idx, "Y",            DDVT_SHORT, &i->origin[VY]);
            MPE_GameObjProperty("Thing", idx, "Z",            DDVT_SHORT, &i->origin[VZ]);
            MPE_GameObjProperty("Thing", idx, "Angle",        DDVT_ANGLE, &i->angle);
            MPE_GameObjProperty("Thing", idx, "DoomEdNum",    DDVT_SHORT, &i->doomEdNum);
            MPE_GameObjProperty("Thing", idx, "SkillModes",   DDVT_INT,   &i->skillModes);
            MPE_GameObjProperty("Thing", idx, "Flags",        DDVT_INT,   &i->flags);

            if(format == Doom64Format)
            {
                MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &i->d64TID);
            }
            else if(format == HexenFormat)
            {
                MPE_GameObjProperty("Thing", idx, "Special",  DDVT_BYTE,  &i->xSpecial);
                MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &i->xTID);
                MPE_GameObjProperty("Thing", idx, "Arg0",     DDVT_BYTE,  &i->xArgs[0]);
                MPE_GameObjProperty("Thing", idx, "Arg1",     DDVT_BYTE,  &i->xArgs[1]);
                MPE_GameObjProperty("Thing", idx, "Arg2",     DDVT_BYTE,  &i->xArgs[2]);
                MPE_GameObjProperty("Thing", idx, "Arg3",     DDVT_BYTE,  &i->xArgs[3]);
                MPE_GameObjProperty("Thing", idx, "Arg4",     DDVT_BYTE,  &i->xArgs[4]);
            }
        }
    }

    /**
     * @param lineList  @c NULL, will cause IterFindPolyLines to count the number
     *                  of lines in the polyobj.
     */
    void collectPolyobjLinesWorker(Polyobj::LineIndices &lineList,
        coord_t x, coord_t y)
    {
        DENG2_FOR_EACH(Lines, i, lines)
        {
            // Already belongs to another polyobj?
            if(i->aFlags & LAF_POLYOBJ) continue;

            // Have we already encounterd this?
            if(i->validCount == validCount) continue;

            coord_t v1[2] = { vertCoords[i->v[0] * 2],
                              vertCoords[i->v[0] * 2 + 1] };

            coord_t v2[2] = { vertCoords[i->v[1] * 2],
                              vertCoords[i->v[1] * 2 + 1] };

            if(de::fequal(v1[VX], x) && de::fequal(v1[VY], y))
            {
                i->validCount = validCount;
                lineList.append( i - lines.begin() );
                collectPolyobjLinesWorker(lineList, v2[VX], v2[VY]);
            }
        }
    }

    /**
     * @todo This terribly inefficent (naive) algorithm may need replacing
     *       (it is far outside an acceptable polynomial range!).
     */
    void collectPolyobjLines(Polyobj::LineIndices &lineList, Lines::iterator lineIt)
    {
        LineDef &line = *lineIt;
        line.xType    = 0;
        line.xArgs[0] = 0;

        coord_t v2[2] = { vertCoords[line.v[1] * 2],
                          vertCoords[line.v[1] * 2 + 1] };

        validCount++;
        // Insert the first line.
        lineList.append(lineIt - lines.begin());
        line.validCount = validCount;
        collectPolyobjLinesWorker(lineList, v2[VX], v2[VY]);
    }
};

Id1Map::Id1Map(Format format) : d(new Instance(this))
{
    d->format = format;
}

Id1Map::Format Id1Map::format() const
{
    return d->format;
}

String const &Id1Map::formatName(Format id) //static
{
    static String const names[1 + MapFormatCount] = {
        /* MF_UNKNOWN */ "Unknown",
        /* MF_DOOM    */ "id Tech 1 (Doom)",
        /* MF_HEXEN   */ "id Tech 1 (Hexen)",
        /* MF_DOOM64  */ "id Tech 1 (Doom64)"
    };
    if(id >= DoomFormat && id < MapFormatCount)
    {
        return names[1 + id];
    }
    return names[0];
}

Id1Map::MaterialId Id1Map::toMaterialId(String name, MaterialGroup group)
{
    return d->materials.toMaterialId(name, group);
}

Id1Map::MaterialId Id1Map::toMaterialId(int uniqueId, MaterialGroup group)
{
    return d->materials.toMaterialId(uniqueId, group);
}

void Id1Map::load(QMap<MapLumpType, lumpnum_t> const &lumps)
{
    typedef QMap<MapLumpType, lumpnum_t> DataLumps;

    // Allocate the vertices first as a large contiguous array suitable for
    // passing directly to Doomsday's MapEdit interface.
    uint vertexCount = W_LumpLength(lumps.find(ML_VERTEXES).value())
                     / ElementSizeForMapLumpType(d->format, ML_VERTEXES);
    d->vertCoords.resize(vertexCount * 2);

    DENG2_FOR_EACH_CONST(DataLumps, i, lumps)
    {
        MapLumpType type  = i.key();
        lumpnum_t lumpNum = i.value();

        size_t lumpLength = W_LumpLength(lumpNum);
        if(!lumpLength) continue;

        size_t elemSize = ElementSizeForMapLumpType(d->format, type);
        if(!elemSize) continue;

        // Process this data lump.
        uint elemCount = lumpLength / elemSize;
        reader_s *reader = bufferLump(lumpNum);
        switch(type)
        {
        default: break;

        case ML_VERTEXES: d->loadVertexes(*reader, elemCount);     break;
        case ML_LINEDEFS: d->loadLineDefs(*reader, elemCount);     break;
        case ML_SIDEDEFS: d->loadSideDefs(*reader, elemCount);     break;
        case ML_SECTORS:  d->loadSectors(*reader, elemCount);      break;
        case ML_THINGS:   d->loadThings(*reader, elemCount);       break;
        case ML_LIGHTS:   d->loadSurfaceTints(*reader, elemCount); break;
        }
        Reader_Delete(reader);
    }

    // We're done with the read buffer.
    clearReadBuffer();

    // Perform post load analyses.
    d->analyze();
}

void Id1Map::transfer(uri_s const &uri)
{
    LOG_AS("Id1Map");

    Time begunAt;

    MPE_Begin(&uri);
        d->transferVertexes();
        d->transferSectors();
        d->transferLinesAndSides();
        d->transferSurfaceTints();
        d->transferPolyobjs();
        d->transferThings();
    MPE_End();

    LOGDEV_MAP_VERBOSE("Transfer completed in %.2f seconds") << begunAt.since();
}

size_t Id1Map::ElementSizeForMapLumpType(Id1Map::Format mapFormat, MapLumpType type) // static
{
    switch(type)
    {
    default: return 0;

    case ML_VERTEXES:
        return (mapFormat == Id1Map::Doom64Format? SIZEOF_64VERTEX : SIZEOF_VERTEX);

    case ML_LINEDEFS:
        return (mapFormat == Id1Map::Doom64Format? SIZEOF_64LINEDEF :
                mapFormat == Id1Map::HexenFormat ? SIZEOF_XLINEDEF  : SIZEOF_LINEDEF);

    case ML_SIDEDEFS:
        return (mapFormat == Id1Map::Doom64Format? SIZEOF_64SIDEDEF : SIZEOF_SIDEDEF);

    case ML_SECTORS:
        return (mapFormat == Id1Map::Doom64Format? SIZEOF_64SECTOR : SIZEOF_SECTOR);

    case ML_THINGS:
        return (mapFormat == Id1Map::Doom64Format? SIZEOF_64THING :
                mapFormat == Id1Map::HexenFormat ? SIZEOF_XTHING  : SIZEOF_THING);

    case ML_LIGHTS:
        return SIZEOF_LIGHT;
    }
}

static uint8_t *readPtr;
static uint8_t *readBuffer;
static size_t readBufferSize;

static char readInt8(reader_s *r)
{
    if(!r) return 0;
    char value = *((int8_t const *) (readPtr));
    readPtr += 1;
    return value;
}

static short readInt16(reader_s *r)
{
    if(!r) return 0;
    short value = *((int16_t const *) (readPtr));
    readPtr += 2;
    return value;
}

static int readInt32(reader_s *r)
{
    if(!r) return 0;
    int value = *((int32_t const *) (readPtr));
    readPtr += 4;
    return value;
}

static float readFloat(reader_s *r)
{
    DENG2_ASSERT(sizeof(float) == 4);
    if(!r) return 0;
    int32_t val = *((int32_t const *) (readPtr));
    float returnValue = 0;
    std::memcpy(&returnValue, &val, 4);
    return returnValue;
}

static void readData(reader_s *r, char *data, int len)
{
    if(!r) return;
    std::memcpy(data, readPtr, len);
    readPtr += len;
}

/// @todo It should not be necessary to buffer the lump data here.
static reader_s *bufferLump(lumpnum_t lumpNum)
{
    // Need to enlarge our read buffer?
    size_t lumpLength = W_LumpLength(lumpNum);
    if(lumpLength > readBufferSize)
    {
        readBuffer = (uint8_t *)M_Realloc(readBuffer, lumpLength);
        readBufferSize = lumpLength;
    }

    // Buffer the entire lump.
    W_ReadLump(lumpNum, readBuffer);

    // Create the reader.
    readPtr = readBuffer;
    return Reader_NewWithCallbacks(readInt8, readInt16, readInt32, readFloat, readData);
}

static void clearReadBuffer()
{
    if(!readBuffer) return;
    M_Free(readBuffer);
    readBuffer = 0;
    readBufferSize = 0;
}

} // namespace wadimp
