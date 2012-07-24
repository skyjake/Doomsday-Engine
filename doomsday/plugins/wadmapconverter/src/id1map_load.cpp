/**
 * @file id1map_load.cpp @ingroup wadmapconverter
 * 
 * Load and translation of the id tech 1 map format data structures.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include <de/libdeng2.h>

#define mapFormat               DENG_PLUGIN_GLOBAL(mapFormat)
#define map                     DENG_PLUGIN_GLOBAL(map)

static const ddstring_t* findMaterialInDictionary(MaterialDictId id)
{
    return StringPool_String(map->materials, id);
}

static MaterialDictId addMaterialToDictionary(const char* name, MaterialDictGroup group)
{
    DENG2_ASSERT(name);

    // Are we yet to instantiate the dictionary itself?
    if(!map->materials)
    {
        map->materials = StringPool_New();
    }

    // Prepare the encoded URI for insertion into the dictionary.
    ddstring_t* uriCString;
    if(mapFormat == MF_DOOM64)
    {
        // Doom64 maps reference materials with unique ids.
        int uniqueId = *((int*) name);
        //char name[9];
        //sprintf(name, "UNK%05i", uniqueId); name[8] = '\0';

        Uri* uri = Materials_ComposeUri(DD_MaterialForTextureUniqueId((group == MG_PLANE? TN_FLATS : TN_TEXTURES), uniqueId));
        uriCString = Uri_Compose(uri);
        Uri_Delete(uri);
    }
    else
    {
        // In original DOOM, texture name references beginning with the
        // hypen '-' character are always treated as meaning "no reference"
        // or "invalid texture" and surfaces using them were not drawn.
        if(group != MG_PLANE && !stricmp(name, "-"))
        {
            return 0; // Not a valid id.
        }

        // Material paths must be encoded.
        ddstring_t path;
        Str_PercentEncode(Str_Set(Str_Init(&path), name));

        Uri* uri = Uri_NewWithPath2(Str_Text(&path), RC_NULL);
        Uri_SetScheme(uri, group == MG_PLANE? MN_FLATS_NAME : MN_TEXTURES_NAME);
        Str_Free(&path);

        uriCString = Uri_Compose(uri);
        Uri_Delete(uri);
    }

    // Intern this material URI in the dictionary.
    MaterialDictId internId = StringPool_Intern(map->materials, uriCString);

    // We're done (phew!).
    Str_Delete(uriCString);
    return internId;
}

static void freeMapData(void)
{
    if(map->vertexes)
    {
        free(map->vertexes);
        map->vertexes = NULL;
    }

    if(map->lines)
    {
        free(map->lines);
        map->lines = NULL;
    }

    if(map->sides)
    {
        free(map->sides);
        map->sides = NULL;
    }

    if(map->sectors)
    {
        free(map->sectors);
        map->sectors = NULL;
    }

    if(map->things)
    {
        free(map->things);
        map->things = NULL;
    }

    if(map->polyobjs)
    {
        for(uint i = 0; i < map->numPolyobjs; ++i)
        {
            mpolyobj_t* po = map->polyobjs[i];
            free(po->lineIndices);
            free(po);
        }
        free(map->polyobjs);
        map->polyobjs = NULL;
    }

    if(map->lights)
    {
        free(map->lights);
        map->lights = NULL;
    }

    /*if(map->macros)
    {
        free(map->macros);
        map->macros = NULL;
    }*/

    if(map->materials)
    {
        StringPool_Clear(map->materials);
        StringPool_Delete(map->materials);
        map->materials = 0;
    }
}

static bool loadVertexes(Reader* reader, size_t lumpLength)
{
    DENG2_ASSERT(reader);

    ID1MAP_TRACE("Processing vertexes...");
    switch(mapFormat)
    {
    default:
    case MF_DOOM: {
        uint numElements = lumpLength / SIZEOF_VERTEX;
        for(uint n = 0; n < numElements; ++n)
        {
            map->vertexes[n * 2]     = coord_t( SHORT(Reader_ReadInt16(reader)) );
            map->vertexes[n * 2 + 1] = coord_t( SHORT(Reader_ReadInt16(reader)) );
        }
        break; }

    case MF_DOOM64: {
        uint numElements = lumpLength / SIZEOF_64VERTEX;
        for(uint n = 0; n < numElements; ++n)
        {
            map->vertexes[n * 2]     = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
            map->vertexes[n * 2 + 1] = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
        }
        break; }
    }

    return true;
}

/**
 * Translate the line definition flags for Doomsday.
 */
static void interpretLineDefFlags(mline_t* l)
{
#define ML_BLOCKING             1 ///< Solid, is an obstacle.
#define ML_TWOSIDED             4 ///< Backside will not be present at all if not two sided.
#define ML_DONTPEGTOP           8 ///< Upper texture unpegged.
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
    if(mapFormat == MF_DOOM)
    {
        if(l->flags & ML_INVALID)
            l->flags &= DOOM_VALIDMASK;
    }

    if(l->flags & ML_BLOCKING)
    {
        l->ddFlags |= DDLF_BLOCKING;
        l->flags &= ~ML_BLOCKING;
    }

    if(l->flags & ML_TWOSIDED)
    {
        l->flags &= ~ML_TWOSIDED;
    }

    if(l->flags & ML_DONTPEGTOP)
    {
        l->ddFlags |= DDLF_DONTPEGTOP;
        l->flags &= ~ML_DONTPEGTOP;
    }

    if(l->flags & ML_DONTPEGBOTTOM)
    {
        l->ddFlags |= DDLF_DONTPEGBOTTOM;
        l->flags &= ~ML_DONTPEGBOTTOM;
    }

#undef DOOM_VALIDMASK
#undef ML_INVALID
#undef ML_DONTPEGBOTTOM
#undef ML_DONTPEGTOP
#undef ML_TWOSIDED
#undef ML_BLOCKING
}

static bool loadLineDefs(Reader* reader, size_t lumpLength)
{
    DENG2_ASSERT(reader);

    ID1MAP_TRACE("Processing line definitions...");
    switch(mapFormat)
    {
    default:
    case MF_DOOM: {
        uint numElements = lumpLength / SIZEOF_LINEDEF;
        for(uint n = 0; n < numElements; ++n)
        {
            mline_t* l = &map->lines[n];
            int idx;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->v[0] = 0;
            else              l->v[0] = idx + 1;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->v[1] = 0;
            else              l->v[1] = idx + 1;

            l->flags = SHORT( Reader_ReadInt16(reader) );
            l->dType = SHORT( Reader_ReadInt16(reader) );
            l->dTag  = SHORT( Reader_ReadInt16(reader) );

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->sides[RIGHT] = 0;
            else              l->sides[RIGHT] = idx + 1;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->sides[LEFT] = 0;
            else              l->sides[LEFT] = idx + 1;

            l->aFlags       = 0;
            l->validCount   = 0;
            l->ddFlags      = 0;

            interpretLineDefFlags(l);
        }
        break; }

    case MF_DOOM64: {
        uint numElements = lumpLength / SIZEOF_64LINEDEF;
        for(uint n = 0; n < numElements; ++n)
        {
            mline_t* l = &map->lines[n];
            int idx;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->v[0] = 0;
            else              l->v[0] = idx + 1;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->v[1] = 0;
            else              l->v[1] = idx + 1;

            l->flags = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            l->d64drawFlags = Reader_ReadByte(reader);
            l->d64texFlags  = Reader_ReadByte(reader);
            l->d64type      = Reader_ReadByte(reader);
            l->d64useType   = Reader_ReadByte(reader);
            l->d64tag       = SHORT( Reader_ReadInt16(reader) );

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->sides[RIGHT] = 0;
            else              l->sides[RIGHT] = idx + 1;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->sides[LEFT] = 0;
            else              l->sides[LEFT] = idx + 1;

            l->aFlags       = 0;
            l->validCount   = 0;
            l->ddFlags      = 0;

            interpretLineDefFlags(l);
        }
        break; }

    case MF_HEXEN: {
        uint numElements = lumpLength / SIZEOF_XLINEDEF;
        for(uint n = 0; n < numElements; ++n)
        {
            mline_t* l = &map->lines[n];
            int idx;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->v[0] = 0;
            else              l->v[0] = idx + 1;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->v[1] = 0;
            else              l->v[1] = idx + 1;

            l->flags = SHORT( Reader_ReadInt16(reader) );
            l->xType    = Reader_ReadByte(reader);
            l->xArgs[0] = Reader_ReadByte(reader);
            l->xArgs[1] = Reader_ReadByte(reader);
            l->xArgs[2] = Reader_ReadByte(reader);
            l->xArgs[3] = Reader_ReadByte(reader);
            l->xArgs[4] = Reader_ReadByte(reader);

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->sides[RIGHT] = 0;
            else              l->sides[RIGHT] = idx + 1;

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) l->sides[LEFT] = 0;
            else              l->sides[LEFT] = idx + 1;

            l->aFlags       = 0;
            l->validCount   = 0;
            l->ddFlags      = 0;

            interpretLineDefFlags(l);
        }
        break; }
    }

    return true;
}

static bool loadSideDefs(Reader* reader, size_t lumpLength)
{
    DENG2_ASSERT(reader);

    ID1MAP_TRACE("Processing side definitions...");
    switch(mapFormat)
    {
    default:
    case MF_DOOM: {
        uint numElements = lumpLength / SIZEOF_SIDEDEF;
        for(uint n = 0; n < numElements; ++n)
        {
            mside_t* s = &map->sides[n];
            char name[9];
            int idx;

            s->offset[VX] = SHORT( Reader_ReadInt16(reader) );
            s->offset[VY] = SHORT( Reader_ReadInt16(reader) );

            Reader_Read(reader, name, 8); name[8] = '\0';
            s->topMaterial    = addMaterialToDictionary(name, MG_WALL);

            Reader_Read(reader, name, 8); name[8] = '\0';
            s->bottomMaterial = addMaterialToDictionary(name, MG_WALL);

            Reader_Read(reader, name, 8); name[8] = '\0';
            s->middleMaterial = addMaterialToDictionary(name, MG_WALL);

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) s->sector = 0;
            else              s->sector = idx + 1;
        }
        break; }

    case MF_DOOM64: {
        uint numElements = lumpLength / SIZEOF_64SIDEDEF;
        for(uint n = 0; n < numElements; ++n)
        {
            mside_t* s = &map->sides[n];
            int idx;

            s->offset[VX] = SHORT( Reader_ReadInt16(reader) );
            s->offset[VY] = SHORT( Reader_ReadInt16(reader) );

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->topMaterial    = addMaterialToDictionary((const char*) &idx, MG_WALL);

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->bottomMaterial = addMaterialToDictionary((const char*) &idx, MG_WALL);

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->middleMaterial = addMaterialToDictionary((const char*) &idx, MG_WALL);

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            if(idx == 0xFFFF) s->sector = 0;
            else              s->sector = idx + 1;
        }
        break; }
    }

    return true;
}

static bool loadSectors(Reader* reader, size_t lumpLength)
{
    DENG2_ASSERT(reader);

    ID1MAP_TRACE("Processing sectors...");
    switch(mapFormat)
    {
    default: {
        uint numElements = lumpLength / SIZEOF_SECTOR;
        for(uint n = 0; n < numElements; ++n)
        {
            msector_t* s = &map->sectors[n];
            char name[9];

            s->floorHeight  = SHORT( Reader_ReadInt16(reader) );
            s->ceilHeight   = SHORT( Reader_ReadInt16(reader) );

            Reader_Read(reader, name, 8); name[8] = '\0';
            s->floorMaterial= addMaterialToDictionary(name, MG_PLANE);

            Reader_Read(reader, name, 8); name[8] = '\0';
            s->ceilMaterial = addMaterialToDictionary(name, MG_PLANE);

            s->lightLevel   = SHORT( Reader_ReadInt16(reader) );
            s->type         = SHORT( Reader_ReadInt16(reader) );
            s->tag          = SHORT( Reader_ReadInt16(reader) );
        }
        break; }

    case MF_DOOM64: {
        uint numElements = lumpLength / SIZEOF_64SECTOR;
        for(uint n = 0; n < numElements; ++n)
        {
            msector_t* s = &map->sectors[n];
            int idx;

            s->floorHeight  = SHORT( Reader_ReadInt16(reader));
            s->ceilHeight   = SHORT( Reader_ReadInt16(reader) );

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->floorMaterial= addMaterialToDictionary((const char*) &idx, MG_PLANE);

            idx = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->ceilMaterial = addMaterialToDictionary((const char*) &idx, MG_PLANE);

            s->d64ceilingColor  = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->d64floorColor    = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->d64unknownColor  = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->d64wallTopColor  = USHORT( uint16_t(Reader_ReadInt16(reader)) );
            s->d64wallBottomColor = USHORT( uint16_t(Reader_ReadInt16(reader)) );

            s->type     = SHORT( Reader_ReadInt16(reader) );
            s->tag      = SHORT( Reader_ReadInt16(reader) );
            s->d64flags = USHORT( uint16_t(Reader_ReadInt16(reader)) );

            s->lightLevel = 160; ///?
        }
        break; }
    }

    return true;
}

static bool loadThings(Reader* reader, size_t lumpLength)
{
/// @todo Get these from a game api header.
#define MTF_Z_FLOOR         0x20000000 ///< Spawn relative to floor height.
#define MTF_Z_CEIL          0x40000000 ///< Spawn relative to ceiling height (minus thing height).
#define MTF_Z_RANDOM        0x80000000 ///< Random point between floor and ceiling.

#define ANG45               0x20000000

    DENG2_ASSERT(reader);

    ID1MAP_TRACE("Processing things...");
    switch(mapFormat)
    {
    default:
    case MF_DOOM: {
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

        uint numElements = lumpLength / SIZEOF_THING;
        for(uint n = 0; n < numElements; ++n)
        {
            mthing_t* t = &map->things[n];

            t->origin[VX]   = SHORT( Reader_ReadInt16(reader) );
            t->origin[VY]   = SHORT( Reader_ReadInt16(reader) );
            t->origin[VZ]   = 0;
            t->angle        = ANG45 * (SHORT( Reader_ReadInt16(reader) ) / 45);
            t->doomEdNum    = SHORT( Reader_ReadInt16(reader) );
            t->flags        = SHORT( Reader_ReadInt16(reader) );

            t->skillModes = 0;
            if(t->flags & MTF_EASY)
                t->skillModes |= 0x00000001 | 0x00000002;
            if(t->flags & MTF_MEDIUM)
                t->skillModes |= 0x00000004;
            if(t->flags & MTF_HARD)
                t->skillModes |= 0x00000008 | 0x00000010;

            t->flags &= ~MASK_UNKNOWN_THING_FLAGS;
            // DOOM format things spawn on the floor by default unless their
            // type-specific flags override.
            t->flags |= MTF_Z_FLOOR;
        }

#undef MASK_UNKNOWN_THING_FLAGS
#undef MTF_FRIENDLY
#undef MTF_NOTCOOP
#undef MTF_NOTDM
#undef MTF_NOTSINGLE
#undef MTF_AMBUSH
#undef MTF_HARD
#undef MTF_MEDIUM
#undef MTF_EASY
        break; }

    case MF_DOOM64: {
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

        uint numElements = lumpLength / SIZEOF_64THING;
        for(uint n = 0; n < numElements; ++n)
        {
            mthing_t* t = &map->things[n];

            t->origin[VX]   = SHORT( Reader_ReadInt16(reader) );
            t->origin[VY]   = SHORT( Reader_ReadInt16(reader) );
            t->origin[VZ]   = SHORT( Reader_ReadInt16(reader) );
            t->angle        = ANG45 * (SHORT( Reader_ReadInt16(reader) ) / 45);
            t->doomEdNum    = SHORT( Reader_ReadInt16(reader) );
            t->flags        = SHORT( Reader_ReadInt16(reader) );

            t->skillModes = 0;
            if(t->flags & MTF_EASY)
                t->skillModes |= 0x00000001;
            if(t->flags & MTF_MEDIUM)
                t->skillModes |= 0x00000002;
            if(t->flags & MTF_HARD)
                t->skillModes |= 0x00000004 | 0x00000008;

            t->flags &= ~MASK_UNKNOWN_THING_FLAGS;
            // DOOM64 format things spawn relative to the floor by default
            // unless their type-specific flags override.
            t->flags |= MTF_Z_FLOOR;

            t->d64TID = SHORT( Reader_ReadInt16(reader) );
        }

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
        break; }

    case MF_HEXEN: {
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

        uint numElements = lumpLength / SIZEOF_XTHING;
        for(uint n = 0; n < numElements; ++n)
        {
            mthing_t* t = &map->things[n];

            t->xTID         = SHORT( Reader_ReadInt16(reader) );
            t->origin[VX]   = SHORT( Reader_ReadInt16(reader) );
            t->origin[VY]   = SHORT( Reader_ReadInt16(reader) );
            t->origin[VZ]   = SHORT( Reader_ReadInt16(reader) );
            t->angle        = SHORT( Reader_ReadInt16(reader) );
            t->doomEdNum    = SHORT( Reader_ReadInt16(reader) );

            /**
             * For some reason, the Hexen format stores polyobject tags in
             * the angle field in THINGS. Thus, we cannot translate the
             * angle until we know whether it is a polyobject type or not.
             */
            if(t->doomEdNum != PO_ANCHOR_DOOMEDNUM &&
               t->doomEdNum != PO_SPAWN_DOOMEDNUM &&
               t->doomEdNum != PO_SPAWNCRUSH_DOOMEDNUM)
                t->angle = ANG45 * (t->angle / 45);

            t->flags = SHORT( Reader_ReadInt16(reader) );

            t->skillModes = 0;
            if(t->flags & MTF_EASY)
                t->skillModes |= 0x00000001 | 0x00000002;
            if(t->flags & MTF_MEDIUM)
                t->skillModes |= 0x00000004;
            if(t->flags & MTF_HARD)
                t->skillModes |= 0x00000008 | 0x00000010;

            t->flags &= ~MASK_UNKNOWN_THING_FLAGS;
            /**
             * Translate flags:
             */
            // Game type logic is inverted.
            t->flags ^= (MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH);

            // HEXEN format things spawn relative to the floor by default
            // unless their type-specific flags override.
            t->flags |= MTF_Z_FLOOR;

            t->xSpecial = Reader_ReadByte(reader);
            t->xArgs[0] = Reader_ReadByte(reader);
            t->xArgs[1] = Reader_ReadByte(reader);
            t->xArgs[2] = Reader_ReadByte(reader);
            t->xArgs[3] = Reader_ReadByte(reader);
            t->xArgs[4] = Reader_ReadByte(reader);
        }

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
        break; }
    }

    return true;

#undef MTF_Z_RANDOM
#undef MTF_Z_CEIL
#undef MTF_Z_FLOOR
}

static bool loadLights(Reader* reader, size_t lumpLength)
{
    DENG2_ASSERT(reader);

    ID1MAP_TRACE("Processing lights...");

    uint numElements = lumpLength / SIZEOF_LIGHT;
    for(uint n = 0; n < numElements; ++n)
    {
        surfacetint_t* t = &map->lights[n];

        t->rgb[0]   = Reader_ReadByte(reader) / 255.f;
        t->rgb[1]   = Reader_ReadByte(reader) / 255.f;
        t->rgb[2]   = Reader_ReadByte(reader) / 255.f;
        t->xx[0]    = Reader_ReadByte(reader);
        t->xx[1]    = Reader_ReadByte(reader);
        t->xx[2]    = Reader_ReadByte(reader);
    }

    return true;
}

static uint8_t* readPtr;
static uint8_t* readBuffer = NULL;
static size_t readBufferSize = 0;

static char readInt8(Reader* r)
{
    if(!r) return 0;
    char value = *((const int8_t*) (readPtr));
    readPtr += 1;
    return value;
}

static short readInt16(Reader* r)
{
    if(!r) return 0;
    short value = *((const int16_t*) (readPtr));
    readPtr += 2;
    return value;
}

static int readInt32(Reader* r)
{
    if(!r) return 0;
    int value = *((const int32_t*) (readPtr));
    readPtr += 4;
    return value;
}

static float readFloat(Reader* r)
{
    DENG2_ASSERT(sizeof(float) == 4);
    if(!r) return 0;
    int32_t val = *((const int32_t*) (readPtr));
    float returnValue = 0;
    memcpy(&returnValue, &val, 4);
    return returnValue;
}

static void readData(Reader* r, char* data, int len)
{
    if(!r) return;
    memcpy(data, readPtr, len);
    readPtr += len;
}

/// @todo It should not be necessary to buffer the lump data here.
static Reader* bufferLump(MapLumpInfo* info)
{
    // Need to enlarge our read buffer?
    if(info->length > readBufferSize)
    {
        readBuffer = (uint8_t*)realloc(readBuffer, info->length);
        if(!readBuffer)
            Con_Error("WadMapConverter::bufferLump: Failed on (re)allocation of %lu bytes for "
                      "the read buffer.", (unsigned long) info->length);
        readBufferSize = info->length;
    }

    // Buffer the entire lump.
    W_ReadLump(info->lump, readBuffer);

    // Create the reader.
    readPtr = readBuffer;
    return Reader_NewWithCallbacks(readInt8, readInt16, readInt32, readFloat, readData);
}

static void clearReadBuffer(void)
{
    if(!readBuffer) return;
    free(readBuffer);
    readBuffer = 0;
    readBufferSize = 0;
}

int LoadMap(MapLumpInfo* lumpInfos[NUM_MAPLUMP_TYPES])
{
    size_t elmSize;

    DENG2_ASSERT(lumpInfos);

    memset(map, 0, sizeof(*map));

    /**
     * Determine how many map data objects we'll need of each type.
     */
    // Verts.
    elmSize = (mapFormat == MF_DOOM64? SIZEOF_64VERTEX : SIZEOF_VERTEX);
    map->numVertexes = lumpInfos[ML_VERTEXES]->length / elmSize;

    // Things.
    if(lumpInfos[ML_THINGS])
    {
        elmSize = (mapFormat == MF_DOOM64? SIZEOF_64THING : mapFormat == MF_HEXEN? SIZEOF_XTHING : SIZEOF_THING);
        map->numThings = lumpInfos[ML_THINGS]->length / elmSize;
    }

    // Lines.
    elmSize = (mapFormat == MF_DOOM64? SIZEOF_64LINEDEF : mapFormat == MF_HEXEN? SIZEOF_XLINEDEF : SIZEOF_LINEDEF);
    map->numLines = lumpInfos[ML_LINEDEFS]->length / elmSize;

    // Sides.
    elmSize = (mapFormat == MF_DOOM64? SIZEOF_64SIDEDEF : SIZEOF_SIDEDEF);
    map->numSides = lumpInfos[ML_SIDEDEFS]->length / elmSize;

    // Sectors.
    elmSize = (mapFormat == MF_DOOM64? SIZEOF_64SECTOR : SIZEOF_SECTOR);
    map->numSectors = lumpInfos[ML_SECTORS]->length / elmSize;

    if(lumpInfos[ML_LIGHTS])
    {
        elmSize = SIZEOF_LIGHT;
        map->numLights = lumpInfos[ML_LIGHTS]->length / elmSize;
    }

    /**
     * Allocate the temporary map data objects used during conversion.
     */
    map->vertexes =   (coord_t*)malloc(map->numVertexes * 2 * sizeof(*map->vertexes));
    map->lines    =   (mline_t*)malloc(map->numLines * sizeof(mline_t));
    map->sides    =   (mside_t*)malloc(map->numSides * sizeof(mside_t));
    map->sectors  = (msector_t*)malloc(map->numSectors * sizeof(msector_t));   
    map->things   =  (mthing_t*)malloc(map->numThings * sizeof(mthing_t));
    if(map->numLights)
    {
        map->lights = (surfacetint_t*)malloc(map->numLights * sizeof(surfacetint_t));
    }

    for(uint i = 0; i < (uint)NUM_MAPLUMP_TYPES; ++i)
    {
        MapLumpInfo* info = lumpInfos[i];
        Reader* reader;

        if(!info) continue;

        // Process this data lump.
        switch(info->type)
        {
        case ML_VERTEXES:
            reader = bufferLump(info);
            loadVertexes(reader, info->length);
            Reader_Delete(reader);
            break;

        case ML_LINEDEFS:
            reader = bufferLump(info);
            loadLineDefs(reader, info->length);
            Reader_Delete(reader);
            break;

        case ML_SIDEDEFS:
            reader = bufferLump(info);
            loadSideDefs(reader, info->length);
            Reader_Delete(reader);
            break;

        case ML_SECTORS:
            reader = bufferLump(info);
            loadSectors(reader, info->length);
            Reader_Delete(reader);
            break;

        case ML_THINGS:
            if(info->length)
            {
                reader = bufferLump(info);
                loadThings(reader, info->length);
                Reader_Delete(reader);
            }
            break;

        case ML_LIGHTS:
            if(info->length)
            {
                reader = bufferLump(info);
                loadLights(reader, info->length);
                Reader_Delete(reader);
            }
            break;

        case ML_MACROS:
            /// @todo Write me!
            break;

        default: break;
        }
    }

    // We're done with the read buffer.
    clearReadBuffer();

    return false; // Success.
}

static void transferVertexes(void)
{
    ID1MAP_TRACE("Transfering vertexes...");
    MPE_VertexCreatev(map->numVertexes, map->vertexes, NULL);
}

static void transferSectors(void)
{
    ID1MAP_TRACE("Transfering sectors...");
    for(uint i = 0; i < map->numSectors; ++i)
    {
        msector_t* sec = &map->sectors[i];
        uint sectorIDX;

        sectorIDX = MPE_SectorCreate(float(sec->lightLevel) / 255.0f, 1, 1, 1);

        MPE_PlaneCreate(sectorIDX, sec->floorHeight, findMaterialInDictionary(sec->floorMaterial),
                        0, 0, 1, 1, 1, 1, 0, 0, 1);
        MPE_PlaneCreate(sectorIDX, sec->ceilHeight, findMaterialInDictionary(sec->ceilMaterial),
                        0, 0, 1, 1, 1, 1, 0, 0, -1);

        MPE_GameObjProperty("XSector", i, "Tag",    DDVT_SHORT, &sec->tag);
        MPE_GameObjProperty("XSector", i, "Type",   DDVT_SHORT, &sec->type);

        if(mapFormat == MF_DOOM64)
        {
            MPE_GameObjProperty("XSector", i, "Flags",          DDVT_SHORT, &sec->d64flags);
            MPE_GameObjProperty("XSector", i, "CeilingColor",   DDVT_SHORT, &sec->d64ceilingColor);
            MPE_GameObjProperty("XSector", i, "FloorColor",     DDVT_SHORT, &sec->d64floorColor);
            MPE_GameObjProperty("XSector", i, "UnknownColor",   DDVT_SHORT, &sec->d64unknownColor);
            MPE_GameObjProperty("XSector", i, "WallTopColor",   DDVT_SHORT, &sec->d64wallTopColor);
            MPE_GameObjProperty("XSector", i, "WallBottomColor", DDVT_SHORT, &sec->d64wallBottomColor);
        }
    }
}

static void transferLinesAndSides(void)
{
    ID1MAP_TRACE("Transfering lines and sides...");
    for(uint i = 0; i < map->numLines; ++i)
    {
        mline_t* l = &map->lines[i];

        uint frontIdx = 0;
        mside_t* front = (l->sides[RIGHT] != 0? &map->sides[l->sides[RIGHT]-1] : NULL);
        if(front)
        {
            frontIdx =
                MPE_SidedefCreate((mapFormat == MF_DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  findMaterialInDictionary(front->topMaterial),
                                  front->offset[VX], front->offset[VY], 1, 1, 1,
                                  findMaterialInDictionary(front->middleMaterial),
                                  front->offset[VX], front->offset[VY], 1, 1, 1, 1,
                                  findMaterialInDictionary(front->bottomMaterial),
                                  front->offset[VX], front->offset[VY], 1, 1, 1);
        }

        uint backIdx = 0;
        mside_t* back = (l->sides[LEFT] != 0? &map->sides[l->sides[LEFT]-1] : NULL);
        if(back)
        {
            backIdx =
                MPE_SidedefCreate((mapFormat == MF_DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  findMaterialInDictionary(back->topMaterial),
                                  back->offset[VX], back->offset[VY], 1, 1, 1,
                                  findMaterialInDictionary(back->middleMaterial),
                                  back->offset[VX], back->offset[VY], 1, 1, 1, 1,
                                  findMaterialInDictionary(back->bottomMaterial),
                                  back->offset[VX], back->offset[VY], 1, 1, 1);
        }

        MPE_LinedefCreate(l->v[0], l->v[1], front? front->sector : 0,
                          back? back->sector : 0, frontIdx, backIdx, l->ddFlags);

        MPE_GameObjProperty("XLinedef", i, "Flags", DDVT_SHORT, &l->flags);

        switch(mapFormat)
        {
        default:
        case MF_DOOM:
            MPE_GameObjProperty("XLinedef", i, "Type",  DDVT_SHORT, &l->dType);
            MPE_GameObjProperty("XLinedef", i, "Tag",   DDVT_SHORT, &l->dTag);
            break;

        case MF_DOOM64:
            MPE_GameObjProperty("XLinedef", i, "DrawFlags", DDVT_BYTE,  &l->d64drawFlags);
            MPE_GameObjProperty("XLinedef", i, "TexFlags",  DDVT_BYTE,  &l->d64texFlags);
            MPE_GameObjProperty("XLinedef", i, "Type",      DDVT_BYTE,  &l->d64type);
            MPE_GameObjProperty("XLinedef", i, "UseType",   DDVT_BYTE,  &l->d64useType);
            MPE_GameObjProperty("XLinedef", i, "Tag",       DDVT_SHORT, &l->d64tag);
            break;

        case MF_HEXEN:
            MPE_GameObjProperty("XLinedef", i, "Type", DDVT_BYTE, &l->xType);
            MPE_GameObjProperty("XLinedef", i, "Arg0", DDVT_BYTE, &l->xArgs[0]);
            MPE_GameObjProperty("XLinedef", i, "Arg1", DDVT_BYTE, &l->xArgs[1]);
            MPE_GameObjProperty("XLinedef", i, "Arg2", DDVT_BYTE, &l->xArgs[2]);
            MPE_GameObjProperty("XLinedef", i, "Arg3", DDVT_BYTE, &l->xArgs[3]);
            MPE_GameObjProperty("XLinedef", i, "Arg4", DDVT_BYTE, &l->xArgs[4]);
            break;
        }
    }
}

static void transferLights(void)
{
    ID1MAP_TRACE("Transfering lights...");
    for(uint i = 0; i < map->numLights; ++i)
    {
        surfacetint_t* l = &map->lights[i];

        MPE_GameObjProperty("Light", i, "ColorR",   DDVT_FLOAT, &l->rgb[0]);
        MPE_GameObjProperty("Light", i, "ColorG",   DDVT_FLOAT, &l->rgb[1]);
        MPE_GameObjProperty("Light", i, "ColorB",   DDVT_FLOAT, &l->rgb[2]);
        MPE_GameObjProperty("Light", i, "XX0",      DDVT_BYTE,  &l->xx[0]);
        MPE_GameObjProperty("Light", i, "XX1",      DDVT_BYTE,  &l->xx[1]);
        MPE_GameObjProperty("Light", i, "XX2",      DDVT_BYTE,  &l->xx[2]);
    }
}

static void transferPolyobjs(void)
{
    ID1MAP_TRACE("Transfering polyobjs...");
    for(uint i = 0; i < map->numPolyobjs; ++i)
    {
        mpolyobj_t* po = map->polyobjs[i];
        MPE_PolyobjCreate(po->lineIndices, po->lineCount, po->tag, po->seqType,
                          coord_t(po->anchor[VX]), coord_t(po->anchor[VY]));
    }
}

static void transferThings(void)
{
    ID1MAP_TRACE("Transfering things...");
    for(uint i = 0; i < map->numThings; ++i)
    {
        mthing_t* th = &map->things[i];

        MPE_GameObjProperty("Thing", i, "X",            DDVT_SHORT, &th->origin[VX]);
        MPE_GameObjProperty("Thing", i, "Y",            DDVT_SHORT, &th->origin[VY]);
        MPE_GameObjProperty("Thing", i, "Z",            DDVT_SHORT, &th->origin[VZ]);
        MPE_GameObjProperty("Thing", i, "Angle",        DDVT_ANGLE, &th->angle);
        MPE_GameObjProperty("Thing", i, "DoomEdNum",    DDVT_SHORT, &th->doomEdNum);
        MPE_GameObjProperty("Thing", i, "SkillModes",   DDVT_INT,   &th->skillModes);
        MPE_GameObjProperty("Thing", i, "Flags",        DDVT_INT,   &th->flags);

        if(mapFormat == MF_DOOM64)
        {
            MPE_GameObjProperty("Thing", i, "ID",       DDVT_SHORT, &th->d64TID);
        }
        else if(mapFormat == MF_HEXEN)
        {
            MPE_GameObjProperty("Thing", i, "Special",  DDVT_BYTE,  &th->xSpecial);
            MPE_GameObjProperty("Thing", i, "ID",       DDVT_SHORT, &th->xTID);
            MPE_GameObjProperty("Thing", i, "Arg0",     DDVT_BYTE,  &th->xArgs[0]);
            MPE_GameObjProperty("Thing", i, "Arg1",     DDVT_BYTE,  &th->xArgs[1]);
            MPE_GameObjProperty("Thing", i, "Arg2",     DDVT_BYTE,  &th->xArgs[2]);
            MPE_GameObjProperty("Thing", i, "Arg3",     DDVT_BYTE,  &th->xArgs[3]);
            MPE_GameObjProperty("Thing", i, "Arg4",     DDVT_BYTE,  &th->xArgs[4]);
        }
    }
}

int TransferMap(void)
{
    uint startTime = Sys_GetRealTime();

    ID1MAP_TRACE("TransferMap");

    MPE_Begin("");
    transferVertexes();
    transferSectors();
    transferLinesAndSides();
    transferLights();
    transferPolyobjs();
    transferThings();

    // We have now finished with our local for-conversion map representation.
    freeMapData();

    // Inform Doomsday we have finished with this map.
    MPE_End();

    VERBOSE2( Con_Message("WadMapConverter::TransferMap: Done in %.2f seconds.\n",
                          (Sys_GetRealTime() - startTime) / 1000.0f) )

    return false; // Success.
}
