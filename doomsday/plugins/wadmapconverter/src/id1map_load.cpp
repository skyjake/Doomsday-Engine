/** @file id1map_load.cpp  id Tech 1 map format reader.
 *
 * Load and translation of the id Tech 1 map format data.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "id1map_load.h"
#include "id1map_datatypes.h"
#include <de/libcore.h>

size_t ElementSizeForMapLumpType(Id1Map::Format mapFormat, MapLumpType type)
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

/**
 * Translate the line definition flags for Doomsday.
 */
static void interpretLineDefFlags(mline_t &l, Id1Map::Format mapFormat)
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
        if(l.flags & ML_INVALID)
            l.flags &= DOOM_VALIDMASK;
    }

    if(l.flags & ML_BLOCKING)
    {
        l.ddFlags |= DDLF_BLOCKING;
        l.flags &= ~ML_BLOCKING;
    }

    if(l.flags & ML_DONTPEGTOP)
    {
        l.ddFlags |= DDLF_DONTPEGTOP;
        l.flags &= ~ML_DONTPEGTOP;
    }

    if(l.flags & ML_DONTPEGBOTTOM)
    {
        l.ddFlags |= DDLF_DONTPEGBOTTOM;
        l.flags &= ~ML_DONTPEGBOTTOM;
    }

#undef DOOM_VALIDMASK
#undef ML_INVALID
#undef ML_DONTPEGBOTTOM
#undef ML_DONTPEGTOP
#undef ML_BLOCKING
}

void MLine_Read(mline_t &l, Id1Map &map, Reader &reader)
{
    int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.v[0] = (idx == 0xFFFF? -1 : idx);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.v[1] = (idx == 0xFFFF? -1 : idx);

    l.flags = SHORT( Reader_ReadInt16(&reader) );
    l.dType = SHORT( Reader_ReadInt16(&reader) );
    l.dTag  = SHORT( Reader_ReadInt16(&reader) );

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.sides[RIGHT] = (idx == 0xFFFF? -1 : idx);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.sides[LEFT] = (idx == 0xFFFF? -1 : idx);

    l.aFlags       = 0;
    l.validCount   = 0;
    l.ddFlags      = 0;

    interpretLineDefFlags(l, map.format());
}

void MLine64_Read(mline_t &l, Id1Map &map, Reader &reader)
{
    int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.v[0] = (idx == 0xFFFF? -1 : idx);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.v[1] = (idx == 0xFFFF? -1 : idx);

    l.flags = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.d64drawFlags = Reader_ReadByte(&reader);
    l.d64texFlags  = Reader_ReadByte(&reader);
    l.d64type      = Reader_ReadByte(&reader);
    l.d64useType   = Reader_ReadByte(&reader);
    l.d64tag       = SHORT( Reader_ReadInt16(&reader) );

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.sides[RIGHT] = (idx == 0xFFFF? -1 : idx);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.sides[LEFT] = (idx == 0xFFFF? -1 : idx);

    l.aFlags       = 0;
    l.validCount   = 0;
    l.ddFlags      = 0;

    interpretLineDefFlags(l, map.format());
}

void MLineHx_Read(mline_t &l, Id1Map &map, Reader &reader)
{
    int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.v[0] = (idx == 0xFFFF? -1 : idx);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.v[1] = (idx == 0xFFFF? -1 : idx);

    l.flags = SHORT( Reader_ReadInt16(&reader) );
    l.xType    = Reader_ReadByte(&reader);
    l.xArgs[0] = Reader_ReadByte(&reader);
    l.xArgs[1] = Reader_ReadByte(&reader);
    l.xArgs[2] = Reader_ReadByte(&reader);
    l.xArgs[3] = Reader_ReadByte(&reader);
    l.xArgs[4] = Reader_ReadByte(&reader);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.sides[RIGHT] = (idx == 0xFFFF? -1 : idx);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    l.sides[LEFT] = (idx == 0xFFFF? -1 : idx);

    l.aFlags       = 0;
    l.validCount   = 0;
    l.ddFlags      = 0;

    interpretLineDefFlags(l, map.format());
}

void MSide_Read(mside_t &s, Id1Map &map, Reader &reader)
{
    s.offset[VX] = SHORT( Reader_ReadInt16(&reader) );
    s.offset[VY] = SHORT( Reader_ReadInt16(&reader) );

    char name[9];
    Reader_Read(&reader, name, 8); name[8] = '\0';
    s.topMaterial    = map.toMaterialId(name, Id1Map::WallMaterials);

    Reader_Read(&reader, name, 8); name[8] = '\0';
    s.bottomMaterial = map.toMaterialId(name, Id1Map::WallMaterials);

    Reader_Read(&reader, name, 8); name[8] = '\0';
    s.middleMaterial = map.toMaterialId(name, Id1Map::WallMaterials);

    int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.sector = (idx == 0xFFFF? -1 : idx);
}

void MSide64_Read(mside_t &s, Id1Map &map, Reader &reader)
{
    s.offset[VX] = SHORT( Reader_ReadInt16(&reader) );
    s.offset[VY] = SHORT( Reader_ReadInt16(&reader) );

    int idx;
    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.topMaterial    = map.toMaterialId(idx, Id1Map::WallMaterials);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.bottomMaterial = map.toMaterialId(idx, Id1Map::WallMaterials);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.middleMaterial = map.toMaterialId(idx, Id1Map::WallMaterials);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.sector = (idx == 0xFFFF? -1 : idx);
}

void MSector_Read(msector_t &s, Id1Map &map, Reader &reader)
{
    s.floorHeight  = SHORT( Reader_ReadInt16(&reader) );
    s.ceilHeight   = SHORT( Reader_ReadInt16(&reader) );

    char name[9];
    Reader_Read(&reader, name, 8); name[8] = '\0';
    s.floorMaterial= map.toMaterialId(name, Id1Map::PlaneMaterials);

    Reader_Read(&reader, name, 8); name[8] = '\0';
    s.ceilMaterial = map.toMaterialId(name, Id1Map::PlaneMaterials);

    s.lightLevel   = SHORT( Reader_ReadInt16(&reader) );
    s.type         = SHORT( Reader_ReadInt16(&reader) );
    s.tag          = SHORT( Reader_ReadInt16(&reader) );
}

void MSector64_Read(msector_t &s, Id1Map &map, Reader &reader)
{
    s.floorHeight  = SHORT( Reader_ReadInt16(&reader));
    s.ceilHeight   = SHORT( Reader_ReadInt16(&reader) );

    int idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.floorMaterial= map.toMaterialId(idx, Id1Map::PlaneMaterials);

    idx = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.ceilMaterial = map.toMaterialId(idx, Id1Map::PlaneMaterials);

    s.d64ceilingColor  = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.d64floorColor    = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.d64unknownColor  = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.d64wallTopColor  = USHORT( uint16_t(Reader_ReadInt16(&reader)) );
    s.d64wallBottomColor = USHORT( uint16_t(Reader_ReadInt16(&reader)) );

    s.type     = SHORT( Reader_ReadInt16(&reader) );
    s.tag      = SHORT( Reader_ReadInt16(&reader) );
    s.d64flags = USHORT( uint16_t(Reader_ReadInt16(&reader)) );

    s.lightLevel = 160; ///?
}

/// @todo Get these from a game api header.
#define MTF_Z_FLOOR         0x20000000 ///< Spawn relative to floor height.
#define MTF_Z_CEIL          0x40000000 ///< Spawn relative to ceiling height (minus thing height).
#define MTF_Z_RANDOM        0x80000000 ///< Random point between floor and ceiling.

#define ANG45               0x20000000

void MThing_Read(mthing_t &t, Id1Map & /*map*/, Reader &reader)
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

    t.origin[VX]   = SHORT( Reader_ReadInt16(&reader) );
    t.origin[VY]   = SHORT( Reader_ReadInt16(&reader) );
    t.origin[VZ]   = 0;
    t.angle        = ANG45 * (SHORT( Reader_ReadInt16(&reader) ) / 45);
    t.doomEdNum    = SHORT( Reader_ReadInt16(&reader) );
    t.flags        = SHORT( Reader_ReadInt16(&reader) );

    t.skillModes = 0;
    if(t.flags & MTF_EASY)
        t.skillModes |= 0x00000001 | 0x00000002;
    if(t.flags & MTF_MEDIUM)
        t.skillModes |= 0x00000004;
    if(t.flags & MTF_HARD)
        t.skillModes |= 0x00000008 | 0x00000010;

    t.flags &= ~MASK_UNKNOWN_THING_FLAGS;
    // DOOM format things spawn on the floor by default unless their
    // type-specific flags override.
    t.flags |= MTF_Z_FLOOR;

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

void MThing64_Read(mthing_t &t, Id1Map & /*map*/, Reader &reader)
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

    t.origin[VX]   = SHORT( Reader_ReadInt16(&reader) );
    t.origin[VY]   = SHORT( Reader_ReadInt16(&reader) );
    t.origin[VZ]   = SHORT( Reader_ReadInt16(&reader) );
    t.angle        = ANG45 * (SHORT( Reader_ReadInt16(&reader) ) / 45);
    t.doomEdNum    = SHORT( Reader_ReadInt16(&reader) );
    t.flags        = SHORT( Reader_ReadInt16(&reader) );

    t.skillModes = 0;
    if(t.flags & MTF_EASY)
        t.skillModes |= 0x00000001;
    if(t.flags & MTF_MEDIUM)
        t.skillModes |= 0x00000002;
    if(t.flags & MTF_HARD)
        t.skillModes |= 0x00000004 | 0x00000008;

    t.flags &= ~MASK_UNKNOWN_THING_FLAGS;
    // DOOM64 format things spawn relative to the floor by default
    // unless their type-specific flags override.
    t.flags |= MTF_Z_FLOOR;

    t.d64TID = SHORT( Reader_ReadInt16(&reader) );

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

void MThingHx_Read(mthing_t &t, Id1Map & /*map*/, Reader &reader)
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

    t.xTID         = SHORT( Reader_ReadInt16(&reader) );
    t.origin[VX]   = SHORT( Reader_ReadInt16(&reader) );
    t.origin[VY]   = SHORT( Reader_ReadInt16(&reader) );
    t.origin[VZ]   = SHORT( Reader_ReadInt16(&reader) );
    t.angle        = SHORT( Reader_ReadInt16(&reader) );
    t.doomEdNum    = SHORT( Reader_ReadInt16(&reader) );

    /**
     * For some reason, the Hexen format stores polyobject tags in
     * the angle field in THINGS. Thus, we cannot translate the
     * angle until we know whether it is a polyobject type or not.
     */
    if(t.doomEdNum != PO_ANCHOR_DOOMEDNUM &&
       t.doomEdNum != PO_SPAWN_DOOMEDNUM &&
       t.doomEdNum != PO_SPAWNCRUSH_DOOMEDNUM)
    {
        t.angle = ANG45 * (t.angle / 45);
    }

    t.flags = SHORT( Reader_ReadInt16(&reader) );

    t.skillModes = 0;
    if(t.flags & MTF_EASY)
        t.skillModes |= 0x00000001 | 0x00000002;
    if(t.flags & MTF_MEDIUM)
        t.skillModes |= 0x00000004;
    if(t.flags & MTF_HARD)
        t.skillModes |= 0x00000008 | 0x00000010;

    t.flags &= ~MASK_UNKNOWN_THING_FLAGS;
    /**
     * Translate flags:
     */
    // Game type logic is inverted.
    t.flags ^= (MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH);

    // HEXEN format things spawn relative to the floor by default
    // unless their type-specific flags override.
    t.flags |= MTF_Z_FLOOR;

    t.xSpecial = Reader_ReadByte(&reader);
    t.xArgs[0] = Reader_ReadByte(&reader);
    t.xArgs[1] = Reader_ReadByte(&reader);
    t.xArgs[2] = Reader_ReadByte(&reader);
    t.xArgs[3] = Reader_ReadByte(&reader);
    t.xArgs[4] = Reader_ReadByte(&reader);

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

void SurfaceTint_Read(surfacetint_t &t, Id1Map & /*map*/, Reader &reader)
{
    t.rgb[0] = Reader_ReadByte(&reader) / 255.f;
    t.rgb[1] = Reader_ReadByte(&reader) / 255.f;
    t.rgb[2] = Reader_ReadByte(&reader) / 255.f;
    t.xx[0]  = Reader_ReadByte(&reader);
    t.xx[1]  = Reader_ReadByte(&reader);
    t.xx[2]  = Reader_ReadByte(&reader);
}
