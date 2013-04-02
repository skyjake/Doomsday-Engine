/** @file cl_world.cpp Clientside world management.
 * @ingroup client
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_filesys.h"
#include "de_defs.h"
#include "de_misc.h"
#include "api_map.h"
#include "api_materialarchive.h"

#include "map/gamemap.h"
#include "client/cl_world.h"
#include "r_util.h"

#define MAX_TRANSLATIONS    16384

#define MVF_CEILING         0x1 // Move ceiling.
#define MVF_SET_FLOORPIC    0x2 // Set floor texture when move done.

typedef struct clplane_s {
    thinker_t   thinker;
    uint        sectorIndex;
    clplanetype_t type;
    int         property; // floor or ceiling
    int         dmuPlane;
    coord_t     destination;
    float       speed;
} clplane_t;

typedef struct clpolyobj_s {
    thinker_t   thinker;
    uint        number;
    Polyobj*    polyobj;
    boolean     move;
    boolean     rotate;
} clpolyobj_t;

typedef struct {
    int size;
    int* serverToLocal;
} indextranstable_t;

void Cl_MoverThinker(clplane_t* mover);
void Cl_PolyMoverThinker(clpolyobj_t* mover);

static MaterialArchive* serverMaterials;
static indextranstable_t xlatMobjType;
static indextranstable_t xlatMobjState;

void Cl_ReadServerMaterials(void)
{
    if(!serverMaterials)
    {
        serverMaterials = MaterialArchive_NewEmpty(false /*no segment check*/);
    }
    MaterialArchive_Read(serverMaterials, msgReader, -1 /*no forced version*/);

#ifdef _DEBUG
    Con_Message("Cl_ReadServerMaterials: Received %i materials.", MaterialArchive_Count(serverMaterials));
#endif
}

static void setTableSize(indextranstable_t* table, int size)
{
    if(size > 0)
    {
        table->serverToLocal = (int*) M_Realloc(table->serverToLocal,
                                                sizeof(*table->serverToLocal) * size);
    }
    else
    {
        M_Free(table->serverToLocal);
        table->serverToLocal = 0;
    }
    table->size = size;
}

void Cl_ReadServerMobjTypeIDs(void)
{
    int i;
    StringArray* ar = StringArray_New();
    StringArray_Read(ar, msgReader);
#ifdef _DEBUG
    Con_Message("Cl_ReadServerMobjTypeIDs: Received %i mobj type IDs.", StringArray_Size(ar));
#endif

    setTableSize(&xlatMobjType, StringArray_Size(ar));

    // Translate the type IDs to local.
    for(i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjType.serverToLocal[i] = Def_GetMobjNum(StringArray_At(ar, i));
        if(xlatMobjType.serverToLocal[i] < 0)
        {
            Con_Message("Could not find '%s' in local thing definitions.",
                        StringArray_At(ar, i));
        }
    }

    StringArray_Delete(ar);
}

void Cl_ReadServerMobjStateIDs(void)
{
    int i;
    StringArray* ar = StringArray_New();
    StringArray_Read(ar, msgReader);
#ifdef _DEBUG
    Con_Message("Cl_ReadServerMobjStateIDs: Received %i mobj state IDs.", StringArray_Size(ar));
#endif

    setTableSize(&xlatMobjState, StringArray_Size(ar));

    // Translate the type IDs to local.
    for(i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjState.serverToLocal[i] = Def_GetStateNum(StringArray_At(ar, i));
        if(xlatMobjState.serverToLocal[i] < 0)
        {
            Con_Message("Could not find '%s' in local state definitions.",
                        StringArray_At(ar, i));
        }
    }

    StringArray_Delete(ar);
}

static Material *Cl_FindLocalMaterial(materialarchive_serialid_t archId)
{
    if(!serverMaterials)
    {
        // Can't do it.
        Con_Message("Cl_FindLocalMaterial: Cannot translate serial id %i, server has not sent its materials!", archId);
        return 0;
    }
    return (Material *)MaterialArchive_Find(serverMaterials, archId, 0);
}

int Cl_LocalMobjType(int serverMobjType)
{
    if(serverMobjType < 0 || serverMobjType >= xlatMobjType.size)
        return 0; // Invalid type.
    return xlatMobjType.serverToLocal[serverMobjType];
}

int Cl_LocalMobjState(int serverMobjState)
{
    if(serverMobjState < 0 || serverMobjState >= xlatMobjState.size)
        return 0; // Invalid state.
    return xlatMobjState.serverToLocal[serverMobjState];
}

static boolean GameMap_IsValidClPlane(GameMap* map, int i)
{
    assert(map);
    if(!map->clActivePlanes[i]) return false;
    return (map->clActivePlanes[i]->thinker.function == reinterpret_cast<thinkfunc_t>(Cl_MoverThinker));
}

static boolean GameMap_IsValidClPolyobj(GameMap* map, int i)
{
    assert(map);
    if(!map->clActivePolyobjs[i]) return false;
    return (map->clActivePolyobjs[i]->thinker.function == reinterpret_cast<thinkfunc_t>(Cl_PolyMoverThinker));
}

void GameMap_InitClMovers(GameMap* map)
{
    assert(map);
    memset(map->clActivePlanes, 0, sizeof(map->clActivePlanes));
    memset(map->clActivePolyobjs, 0, sizeof(map->clActivePolyobjs));
}

void GameMap_ResetClMovers(GameMap* map)
{
    int i;
    assert(map);

    for(i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(GameMap_IsValidClPlane(map, i))
        {
            GameMap_ThinkerRemove(map, &map->clActivePlanes[i]->thinker);
        }
        if(GameMap_IsValidClPolyobj(map, i))
        {
            GameMap_ThinkerRemove(map, &map->clActivePolyobjs[i]->thinker);
        }
    }
}

/**
 * Clears the arrays that track active plane and polyobj mover thinkers.
 */
void Cl_WorldInit(void)
{
    if(theMap)
    {
        GameMap_InitClMovers(theMap);
    }

    serverMaterials = 0;
    memset(&xlatMobjType, 0, sizeof(xlatMobjType));
    memset(&xlatMobjState, 0, sizeof(xlatMobjState));
}

/**
 * Removes all the active movers.
 */
void Cl_WorldReset(void)
{
    if(serverMaterials)
    {
        MaterialArchive_Delete(serverMaterials);
        serverMaterials = 0;
    }

    setTableSize(&xlatMobjType, 0);
    setTableSize(&xlatMobjState, 0);

    if(theMap)
    {
        GameMap_ResetClMovers(theMap);
    }
}

int GameMap_ClPlaneIndex(GameMap* map, clplane_t* mover)
{
    int i;
    assert(map);
    if(!map->clActivePlanes) return -1;

    /// @todo Optimize lookup.
    for(i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(map->clActivePlanes[i] == mover)
            return i;
    }
    return -1;
}

int GameMap_ClPolyobjIndex(GameMap* map, clpolyobj_t* mover)
{
    int i;
    assert(map);
    if(!map->clActivePolyobjs) return -1;

    /// @todo Optimize lookup.
    for(i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(map->clActivePolyobjs[i] == mover)
            return i;
    }
    return -1;
}

void GameMap_DeleteClPlane(GameMap* map, clplane_t* mover)
{
    int index;
    assert(map);

    index = GameMap_ClPlaneIndex(map, mover);
    if(index < 0)
    {
        DEBUG_Message(("GameMap_DeleteClPlane: Mover in sector #%i not removed!\n", mover->sectorIndex));
        return;
    }

    DEBUG_Message(("GameMap_DeleteClPlane: Removing mover [%i] (sector: #%i).\n", index, mover->sectorIndex));
    GameMap_ThinkerRemove(map, &mover->thinker);
}

void GameMap_DeleteClPolyobj(GameMap* map, clpolyobj_t* mover)
{
    int index;
    assert(map);

    index = GameMap_ClPolyobjIndex(map, mover);
    if(index < 0)
    {
        DEBUG_Message(("GameMap_DeleteClPolyobj: Mover not removed!\n"));
        return;
    }

    DEBUG_Message(("GameMap_DeleteClPolyobj: Removing mover [%i].\n", index));
    GameMap_ThinkerRemove(map, &mover->thinker);
}

/**
 * Plane mover. Makes changes in planes using DMU.
 */
void Cl_MoverThinker(clplane_t* mover)
{
    GameMap* map = theMap; /// @todo Do not assume mover is from the CURRENT map.
    coord_t original;
    boolean remove = false;
    boolean freeMove;
    float fspeed;

    // Can we think yet?
    if(!Cl_GameReady()) return;

#ifdef _DEBUG
    if(GameMap_ClPlaneIndex(map, mover) < 0)
    {
        Con_Message("Cl_MoverThinker: Running a mover that is not in activemovers!");
    }
#endif

    // The move is cancelled if the consolePlayer becomes obstructed.
    freeMove = ClPlayer_IsFreeToMove(consolePlayer);
    fspeed = mover->speed;

    // How's the gap?
    original = P_GetDouble(DMU_SECTOR, mover->sectorIndex, mover->property);
    if(fabs(fspeed) > 0 && fabs(mover->destination - original) > fabs(fspeed))
    {
        // Do the move.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->property, original + fspeed);
    }
    else
    {
        // We have reached the destination.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->property, mover->destination);

        // This thinker can now be removed.
        remove = true;
    }

    DEBUG_VERBOSE2_Message(("Cl_MoverThinker: plane height %f in sector #%i\n",
                            P_GetDouble(DMU_SECTOR, mover->sectorIndex, mover->property),
                            mover->sectorIndex));

    // Let the game know of this.
    if(gx.SectorHeightChangeNotification)
    {
        gx.SectorHeightChangeNotification(mover->sectorIndex);
    }

    // Make sure the client didn't get stuck as a result of this move.
    if(freeMove != ClPlayer_IsFreeToMove(consolePlayer))
    {
        DEBUG_Message(("Cl_MoverThinker: move blocked in sector %i, undoing\n", mover->sectorIndex));

        // Something was blocking the way! Go back to original height.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->property, original);

        if(gx.SectorHeightChangeNotification)
        {
            gx.SectorHeightChangeNotification(mover->sectorIndex);
        }
    }
    else
    {
        // Can we remove this thinker?
        if(remove)
        {
            DEBUG_Message(("Cl_MoverThinker: finished in %i\n", mover->sectorIndex));

            // It stops.
            P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->dmuPlane | DMU_SPEED, 0);

            GameMap_DeleteClPlane(map, mover);
        }
    }
}

clplane_t *GameMap_NewClPlane(GameMap *map, uint sectorIndex, clplanetype_t type, coord_t dest, float speed)
{
    DENG_ASSERT(map);

    int dmuPlane = (type == CPT_FLOOR ? DMU_FLOOR_OF_SECTOR : DMU_CEILING_OF_SECTOR);

    DEBUG_Message(("GameMap_NewClPlane: Sector #%i, type:%s, dest:%f, speed:%f\n",
                   sectorIndex, type==CPT_FLOOR? "floor" : "ceiling", dest, speed));

    if(int( sectorIndex ) >= map->_sectors.size())
    {
        DENG_ASSERT(false); // Invalid Sector index.
        return 0;
    }

    // Remove any existing movers for the same plane.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(GameMap_IsValidClPlane(map, i) &&
           map->clActivePlanes[i]->sectorIndex == sectorIndex &&
           map->clActivePlanes[i]->type == type)
        {
            DEBUG_Message(("GameMap_NewClPlane: Removing existing mover [%i] in sector #%i, type %s\n",
                           i, sectorIndex, type == CPT_FLOOR? "floor" : "ceiling"));

            GameMap_DeleteClPlane(map, map->clActivePlanes[i]);
        }
    }

    // Add a new mover.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(map->clActivePlanes[i]) continue;

        DEBUG_Message(("GameMap_NewClPlane: ...new mover [%i]\n", i));

        // Allocate a new clplane_t thinker.
        clplane_t *mov = map->clActivePlanes[i] = (clplane_t *) Z_Calloc(sizeof(clplane_t), PU_MAP, &map->clActivePlanes[i]);
        mov->thinker.function = reinterpret_cast<thinkfunc_t>(Cl_MoverThinker);
        mov->type = type;
        mov->sectorIndex = sectorIndex;
        mov->destination = dest;
        mov->speed = speed;
        mov->property = dmuPlane | DMU_HEIGHT;
        mov->dmuPlane = dmuPlane;

        // Set the right sign for speed.
        if(mov->destination < P_GetDouble(DMU_SECTOR, sectorIndex, mov->property))
            mov->speed = -mov->speed;

        // Update speed and target height.
        P_SetDouble(DMU_SECTOR, sectorIndex, dmuPlane | DMU_TARGET_HEIGHT, dest);
        P_SetFloat(DMU_SECTOR, sectorIndex, dmuPlane | DMU_SPEED, speed);

        GameMap_ThinkerAdd(map, &mov->thinker, false /*not public*/);

        // Immediate move?
        if(FEQUAL(speed, 0))
        {
            // This will remove the thinker immediately if the move is ok.
            Cl_MoverThinker(mov);
        }
        return mov;
    }

    Con_Error("GameMap_NewClPlane: Exhausted activemovers.");
    exit(1); // Unreachable.
}

void Cl_PolyMoverThinker(clpolyobj_t* mover)
{
    Polyobj* po;
    float dx, dy, dist;
    assert(mover);

    po = mover->polyobj;
    if(mover->move)
    {
        // How much to go?
        dx = po->dest[VX] - po->origin[VX];
        dy = po->dest[VY] - po->origin[VY];
        dist = M_ApproxDistance(dx, dy);
        if(dist <= po->speed || FEQUAL(po->speed, 0))
        {
            // We'll arrive at the destination.
            mover->move = false;
        }
        else
        {
            // Adjust deltas to fit speed.
            dx = po->speed * (dx / dist);
            dy = po->speed * (dy / dist);
        }

        // Do the move.
        P_PolyobjMoveXY(P_PolyobjByID(mover->number), dx, dy);
    }

    if(mover->rotate)
    {
        // How much to go?
        int dist = po->destAngle - po->angle;
        int speed = po->angleSpeed;

        //dist = FIX2FLT(po->destAngle - po->angle);
        //if(!po->angleSpeed || dist > 0   /*(abs(FLT2FIX(dist) >> 4) <= abs(((signed) po->angleSpeed) >> 4)*/
        //    /* && po->destAngle != -1*/) || !po->angleSpeed)
        if(!po->angleSpeed || ABS(dist >> 2) <= ABS(speed >> 2))
        {
            DEBUG_Message(("Cl_PolyMoverThinker: Mover %i reached end of turn, destAngle=%x.\n",
                           mover->number, po->destAngle));

            // We'll arrive at the destination.
            mover->rotate = false;
        }
        else
        {
            // Adjust to speed.
            dist = /*FIX2FLT((int)*/ po->angleSpeed;
        }

        P_PolyobjRotate(P_PolyobjByID(mover->number), dist);
    }

    // Can we get rid of this mover?
    if(!mover->move && !mover->rotate)
    {
        /// @todo Do not assume the move is from the CURRENT map.
        GameMap_DeleteClPolyobj(theMap, mover);
    }
}

clpolyobj_t* GameMap_ClPolyobjByPolyobjIndex(GameMap* map, uint index)
{
    int i;
    assert(map);
    for(i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(!GameMap_IsValidClPolyobj(map, i)) continue;

        if(map->clActivePolyobjs[i]->number == index)
            return map->clActivePolyobjs[i];
    }
    return NULL;
}

/**
 * @note Assumes there is no existing ClPolyobj for Polyobj @a index.
 */
clpolyobj_t* GameMap_NewClPolyobj(GameMap* map, uint polyobjIndex)
{
    clpolyobj_t* mover;
    int i;
    assert(map);

    // Take the first unused slot.
    for(i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(map->clActivePolyobjs[i]) continue;

        DEBUG_Message(("GameMap_NewClPolyobj: New polymover [%i] for polyobj #%i.\n", i, polyobjIndex));

        map->clActivePolyobjs[i] = mover = (clpolyobj_t *) Z_Calloc(sizeof(clpolyobj_t), PU_MAP, &map->clActivePolyobjs[i]);
        mover->thinker.function = reinterpret_cast<thinkfunc_t>(Cl_PolyMoverThinker);
        mover->polyobj = map->polyObjs[polyobjIndex];
        mover->number = polyobjIndex;
        GameMap_ThinkerAdd(map, &mover->thinker, false /*not public*/);
        return mover;
    }

    // Not successful.
    return NULL;
}

clpolyobj_t* Cl_FindOrMakeActivePoly(uint polyobjIndex)
{
    GameMap* map = theMap;
    clpolyobj_t* mover = GameMap_ClPolyobjByPolyobjIndex(map, polyobjIndex);
    if(mover) return mover;
    // Not found; make a new one.
    return GameMap_NewClPolyobj(map, polyobjIndex);
}

void Cl_SetPolyMover(uint number, int move, int rotate)
{
    clpolyobj_t* mover = Cl_FindOrMakeActivePoly(number);
    if(!mover)
    {
        Con_Message("Cl_SetPolyMover: Out of polymovers.");
        return;
    }

    // Flag for moving.
    if(move) mover->move = true;
    if(rotate) mover->rotate = true;
}

clplane_t* GameMap_ClPlaneBySectorIndex(GameMap* map, uint sectorIndex, clplanetype_t type)
{
    int i;
    assert(map);

    for(i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(!GameMap_IsValidClPlane(map, i)) continue;
        if(map->clActivePlanes[i]->sectorIndex != sectorIndex) continue;
        if(map->clActivePlanes[i]->type != type) continue;

        // Found it!
        return map->clActivePlanes[i];
    }
    return NULL;
}

/**
 * Reads a sector delta from the PSV_FRAME2 message buffer and applies it
 * to the world.
 */
void Cl_ReadSectorDelta2(int deltaType, boolean /*skip*/)
{
    DENG_UNUSED(deltaType);

    /// @todo Do not assume the CURRENT map.
    GameMap *map = theMap;

#define PLN_FLOOR   0
#define PLN_CEILING 1

    float height[2] = { 0, 0 };
    float target[2] = { 0, 0 };
    float speed[2]  = { 0, 0 };

    // Sector index number.
    ushort num = Reader_ReadUInt16(msgReader);

    Sector *sec = 0;
#ifdef _DEBUG
    if(num >= GameMap_SectorCount(theMap))
    {
        // This is worrisome.
        Con_Error("Cl_ReadSectorDelta2: Sector %i out of range.\n", num);
    }
#endif
    sec = GameMap_Sector(theMap, num);

    // Flags.
    int df = Reader_ReadPackedUInt32(msgReader);

    if(df & SDF_FLOOR_MATERIAL)
    {
        P_SetPtrp(sec, DMU_FLOOR_OF_SECTOR | DMU_MATERIAL,
                  Cl_FindLocalMaterial(Reader_ReadPackedUInt16(msgReader)));
    }
    if(df & SDF_CEILING_MATERIAL)
    {
        P_SetPtrp(sec, DMU_CEILING_OF_SECTOR | DMU_MATERIAL,
                  Cl_FindLocalMaterial(Reader_ReadPackedUInt16(msgReader)));
    }

    if(df & SDF_LIGHT)
        P_SetFloatp(sec, DMU_LIGHT_LEVEL, Reader_ReadByte(msgReader) / 255.0f);

    if(df & SDF_FLOOR_HEIGHT)
        height[PLN_FLOOR] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_CEILING_HEIGHT)
        height[PLN_CEILING] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_FLOOR_TARGET)
        target[PLN_FLOOR] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_FLOOR_SPEED)
        speed[PLN_FLOOR] = FIX2FLT(Reader_ReadByte(msgReader) << (df & SDF_FLOOR_SPEED_44 ? 12 : 15));
    if(df & SDF_CEILING_TARGET)
        target[PLN_CEILING] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_CEILING_SPEED)
        speed[PLN_CEILING] = FIX2FLT(Reader_ReadByte(msgReader) << (df & SDF_CEILING_SPEED_44 ? 12 : 15));

    if(df & SDF_COLOR_RED)
        sec->_lightColor[0] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SDF_COLOR_GREEN)
        sec->_lightColor[1] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SDF_COLOR_BLUE)
        sec->_lightColor[2] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SDF_FLOOR_COLOR_RED)
        sec->floorSurface().setColorRed(Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_FLOOR_COLOR_GREEN)
        sec->floorSurface().setColorGreen(Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_FLOOR_COLOR_BLUE)
        sec->floorSurface().setColorBlue(Reader_ReadByte(msgReader) / 255.f);

    if(df & SDF_CEIL_COLOR_RED)
        sec->ceilingSurface().setColorRed(Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_CEIL_COLOR_GREEN)
        sec->ceilingSurface().setColorGreen(Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_CEIL_COLOR_BLUE)
        sec->ceilingSurface().setColorBlue(Reader_ReadByte(msgReader) / 255.f);

    // The whole delta has now been read.

    // Do we need to start any moving planes?
    if(df & SDF_FLOOR_HEIGHT)
    {
        GameMap_NewClPlane(map, num, CPT_FLOOR, height[PLN_FLOOR], 0);
    }
    else if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
    {
        GameMap_NewClPlane(map, num, CPT_FLOOR, target[PLN_FLOOR], speed[PLN_FLOOR]);
    }

    if(df & SDF_CEILING_HEIGHT)
    {
        GameMap_NewClPlane(map, num, CPT_CEILING, height[PLN_CEILING], 0);
    }
    else if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
    {
        GameMap_NewClPlane(map, num, CPT_CEILING, target[PLN_CEILING], speed[PLN_CEILING]);
    }

#undef PLN_CEILING
#undef PLN_FLOOR
}

/**
 * Reads a side delta from the message buffer and applies it to the world.
 */
void Cl_ReadSideDelta2(int deltaType, boolean skip)
{
    DENG_UNUSED(deltaType);

    ushort num;

    int df, topMat = 0, midMat = 0, botMat = 0;
    int blendmode = 0;
    byte lineFlags = 0, sideFlags = 0;
    float toprgb[3] = {0,0,0}, midrgba[4] = {0,0,0,0};
    float bottomrgb[3] = {0,0,0};

    // First read all the data.
    num = Reader_ReadUInt16(msgReader);

    // Flags.
    df = Reader_ReadPackedUInt32(msgReader);

    if(df & SIDF_TOP_MATERIAL)
        topMat = Reader_ReadPackedUInt16(msgReader);
    if(df & SIDF_MID_MATERIAL)
        midMat = Reader_ReadPackedUInt16(msgReader);
    if(df & SIDF_BOTTOM_MATERIAL)
        botMat = Reader_ReadPackedUInt16(msgReader);
    if(df & SIDF_LINE_FLAGS)
        lineFlags = Reader_ReadByte(msgReader);

    if(df & SIDF_TOP_COLOR_RED)
        toprgb[CR] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_TOP_COLOR_GREEN)
        toprgb[CG] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_TOP_COLOR_BLUE)
        toprgb[CB] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SIDF_MID_COLOR_RED)
        midrgba[CR] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_MID_COLOR_GREEN)
        midrgba[CG] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_MID_COLOR_BLUE)
        midrgba[CB] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_MID_COLOR_ALPHA)
        midrgba[CA] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SIDF_BOTTOM_COLOR_RED)
        bottomrgb[CR] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        bottomrgb[CG] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        bottomrgb[CB] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SIDF_MID_BLENDMODE)
        blendmode = Reader_ReadInt32(msgReader);

    if(df & SIDF_FLAGS)
        sideFlags = Reader_ReadByte(msgReader);

    // Must we skip this?
    if(skip)
        return;

#ifdef _DEBUG
    if(num >= GameMap_SideDefCount(theMap))
    {
        // This is worrisome.
        Con_Error("Cl_ReadSideDelta2: Side %i out of range.\n", num);
    }
#endif

    SideDef *sideDef = GameMap_SideDef(theMap, num);

    if(df & SIDF_TOP_MATERIAL)
    {
        sideDef->top().setMaterial(Cl_FindLocalMaterial(topMat));
    }
    if(df & SIDF_MID_MATERIAL)
    {
        sideDef->middle().setMaterial(Cl_FindLocalMaterial(midMat));
    }
    if(df & SIDF_BOTTOM_MATERIAL)
    {
        sideDef->bottom().setMaterial(Cl_FindLocalMaterial(botMat));
    }

    if(df & SIDF_TOP_COLOR_RED)
        sideDef->top().setColorRed(toprgb[CR]);
    if(df & SIDF_TOP_COLOR_GREEN)
        sideDef->top().setColorGreen(toprgb[CG]);
    if(df & SIDF_TOP_COLOR_BLUE)
        sideDef->top().setColorBlue(toprgb[CB]);

    if(df & SIDF_MID_COLOR_RED)
        sideDef->middle().setColorRed(midrgba[CR]);
    if(df & SIDF_MID_COLOR_GREEN)
        sideDef->middle().setColorGreen(midrgba[CG]);
    if(df & SIDF_MID_COLOR_BLUE)
        sideDef->middle().setColorBlue(midrgba[CB]);
    if(df & SIDF_MID_COLOR_ALPHA)
        sideDef->middle().setAlpha(midrgba[CA]);

    if(df & SIDF_BOTTOM_COLOR_RED)
        sideDef->bottom().setColorRed(bottomrgb[CR]);
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        sideDef->bottom().setColorGreen(bottomrgb[CG]);
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        sideDef->bottom().setColorBlue(bottomrgb[CB]);

    if(df & SIDF_MID_BLENDMODE)
        sideDef->middle().setBlendMode(blendmode_t(blendmode));

    if(df & SIDF_FLAGS)
    {
        // The delta includes the entire lowest byte.
        sideDef->_flags &= ~0xff;
        sideDef->_flags |= sideFlags;
    }

    if(df & SIDF_LINE_FLAGS)
    {
        LineDef &line = sideDef->line();
        // The delta includes the entire lowest byte.
        line._flags &= ~0xff;
        line._flags |= lineFlags;
    }
}

/**
 * Reads a poly delta from the message buffer and applies it to
 * the world.
 */
void Cl_ReadPolyDelta2(boolean skip)
{
    int                 df;
    unsigned short      num;
    Polyobj            *po;
    float               destX = 0, destY = 0;
    float               speed = 0;
    int                 destAngle = 0, angleSpeed = 0;

    num = Reader_ReadPackedUInt16(msgReader);

    // Flags.
    df = Reader_ReadByte(msgReader);

    if(df & PODF_DEST_X)
        destX = Reader_ReadFloat(msgReader);
    if(df & PODF_DEST_Y)
        destY = Reader_ReadFloat(msgReader);
    if(df & PODF_SPEED)
        speed = Reader_ReadFloat(msgReader);
    if(df & PODF_DEST_ANGLE)
        destAngle = ((angle_t)Reader_ReadInt16(msgReader)) << 16;
    if(df & PODF_ANGSPEED)
        angleSpeed = ((angle_t)Reader_ReadInt16(msgReader)) << 16;

/*#ifdef _DEBUG
    Con_Message("Cl_ReadPolyDelta2: PO %i, angle %f, speed %f", num, FIX2FLT(destAngle), FIX2FLT(angleSpeed));
#endif*/

    if(skip)
        return;

#ifdef _DEBUG
    if(num >= GameMap_PolyobjCount(theMap))
    {
        // This is worrisome.
        Con_Error("Cl_ReadPolyDelta2: PO %i out of range.\n", num);
    }
#endif

    po = GameMap_PolyobjByID(theMap, num);

    if(df & PODF_DEST_X)
        po->dest[VX] = destX;
    if(df & PODF_DEST_Y)
        po->dest[VY] = destY;
    if(df & PODF_SPEED)
        po->speed = speed;
    if(df & PODF_DEST_ANGLE)
        po->destAngle = destAngle;
    if(df & PODF_ANGSPEED)
        po->angleSpeed = angleSpeed;
    if(df & PODF_PERPETUAL_ROTATE)
        po->destAngle = -1;

    // Update the polyobj's mover thinkers.
    Cl_SetPolyMover(num, df & (PODF_DEST_X | PODF_DEST_Y | PODF_SPEED),
                    df & (PODF_DEST_ANGLE | PODF_ANGSPEED |
                          PODF_PERPETUAL_ROTATE));
}
