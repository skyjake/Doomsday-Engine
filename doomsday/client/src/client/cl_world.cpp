/** @file cl_world.cpp  Clientside world management.
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

#include "de_base.h"
#include "client/cl_world.h"

#include "client/cl_def.h"
#include "client/cl_player.h"

#include "api_map.h"
#include "api_materialarchive.h"

#include "network/net_msg.h"
#include "network/protocol.h"

#include "Surface"
#include "world/map.h"
#include "world/p_players.h"
#include "world/thinkers.h"

#include <de/memoryzone.h>
#include <QVector>
#include <cmath>

using namespace de;

struct ClPlaneMover
{
    thinker_t thinker;
    int sectorIndex;
    clplanetype_t type;
    int property; ///< Floor or ceiling
    int dmuPlane;
    coord_t destination;
    float speed;
};

/**
 * Plane mover. Makes changes in planes using DMU.
 */
void ClPlaneMover_Thinker(ClPlaneMover *mover);

struct ClPolyMover
{
    thinker_t thinker;
    int number;
    Polyobj *polyobj;
    bool move;
    bool rotate;
};

void ClPolyMover_Thinker(ClPolyMover *mover);

static MaterialArchive *serverMaterials;

typedef QVector<int> IndexTransTable;
static IndexTransTable xlatMobjType;
static IndexTransTable xlatMobjState;

void Cl_ReadServerMaterials()
{
    LOG_AS("Cl_ReadServerMaterials");

    if(!serverMaterials)
    {
        serverMaterials = MaterialArchive_NewEmpty(false /*no segment check*/);
    }
    MaterialArchive_Read(serverMaterials, msgReader, -1 /*no forced version*/);

    LOGDEV_NET_VERBOSE("Received %i materials") << MaterialArchive_Count(serverMaterials);
}

void Cl_ReadServerMobjTypeIDs()
{
    LOG_AS("Cl_ReadServerMobjTypeIDs");

    StringArray *ar = StringArray_New();
    StringArray_Read(ar, msgReader);

    LOGDEV_NET_VERBOSE("Received %i mobj type IDs") << StringArray_Size(ar);

    xlatMobjType.resize(StringArray_Size(ar));

    // Translate the type IDs to local.
    for(int i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjType[i] = Def_GetMobjNum(StringArray_At(ar, i));
        if(xlatMobjType[i] < 0)
        {
            LOG_NET_WARNING("Could not find '%s' in local thing definitions")
                    << StringArray_At(ar, i);
        }
    }

    StringArray_Delete(ar);
}

void Cl_ReadServerMobjStateIDs()
{
    LOG_AS("Cl_ReadServerMobjStateIDs");

    StringArray *ar = StringArray_New();
    StringArray_Read(ar, msgReader);

    LOGDEV_NET_VERBOSE("Received %i mobj state IDs") << StringArray_Size(ar);

    xlatMobjState.resize(StringArray_Size(ar));

    // Translate the type IDs to local.
    for(int i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjState[i] = Def_GetStateNum(StringArray_At(ar, i));
        if(xlatMobjState[i] < 0)
        {
            LOG_NET_WARNING("Could not find '%s' in local state definitions")
                    << StringArray_At(ar, i);
        }
    }

    StringArray_Delete(ar);
}

static Material *Cl_FindLocalMaterial(materialarchive_serialid_t archId)
{    
    if(!serverMaterials)
    {
        // Can't do it.
        LOGDEV_NET_WARNING("Cannot translate serial id %i, server has not sent its materials!") << archId;
        return 0;
    }
    return (Material *) MaterialArchive_Find(serverMaterials, archId, 0);
}

int Cl_LocalMobjType(int serverMobjType)
{
    if(serverMobjType < 0 || serverMobjType >= xlatMobjType.size())
        return 0; // Invalid type.
    return xlatMobjType[serverMobjType];
}

int Cl_LocalMobjState(int serverMobjState)
{
    if(serverMobjState < 0 || serverMobjState >= xlatMobjState.size())
        return 0; // Invalid state.
    return xlatMobjState[serverMobjState];
}

bool Map::isValidClPlane(int i)
{
    if(!clActivePlanes[i]) return false;
    return (clActivePlanes[i]->thinker.function == reinterpret_cast<thinkfunc_t>(ClPlaneMover_Thinker));
}

bool Map::isValidClPolyobj(int i)
{
    if(!clActivePolyobjs[i]) return false;
    return (clActivePolyobjs[i]->thinker.function == reinterpret_cast<thinkfunc_t>(ClPolyMover_Thinker));
}

void Map::initClMovers()
{
    zap(clActivePlanes);
    zap(clActivePolyobjs);
}

void Map::resetClMovers()
{
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(isValidClPlane(i))
        {
            thinkers().remove(clActivePlanes[i]->thinker);
        }
        if(isValidClPolyobj(i))
        {
            thinkers().remove(clActivePolyobjs[i]->thinker);
        }
    }
}

void Cl_WorldInit()
{
    if(App_World().hasMap())
    {
        App_World().map().initClMovers();
    }

    serverMaterials = 0;
    zap(xlatMobjType);
    zap(xlatMobjState);
}

void Cl_WorldReset()
{
    if(serverMaterials)
    {
        MaterialArchive_Delete(serverMaterials);
        serverMaterials = 0;
    }

    xlatMobjType.clear();
    xlatMobjState.clear();

    if(App_World().hasMap())
    {
        App_World().map().resetClMovers();
    }
}

int Map::clPlaneIndex(ClPlaneMover *mover)
{
    if(!clActivePlanes) return -1;

    /// @todo Optimize lookup.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePlanes[i] == mover)
            return i;
    }
    return -1;
}

int Map::clPolyobjIndex(ClPolyMover *mover)
{
    if(!clActivePolyobjs) return -1;

    /// @todo Optimize lookup.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePolyobjs[i] == mover)
            return i;
    }
    return -1;
}

void Map::deleteClPlane(ClPlaneMover *mover)
{
    LOG_AS("Map::deleteClPlane");

    int index = clPlaneIndex(mover);
    if(index < 0)
    {
        LOG_MAP_VERBOSE("Mover in sector #%i not removed!") << mover->sectorIndex;
        return;
    }

    LOG_MAP_XVERBOSE("Removing mover [%i] (sector: #%i)") << index << mover->sectorIndex;
    thinkers().remove(mover->thinker);
}

void Map::deleteClPolyobj(ClPolyMover *mover)
{
    LOG_AS("Map::deleteClPolyobj");

    int index = clPolyobjIndex(mover);
    if(index < 0)
    {
        LOG_MAP_VERBOSE("Mover not removed!");
        return;
    }

    LOG_MAP_XVERBOSE("Removing mover [%i]") << index;
    thinkers().remove(mover->thinker);
}

void ClPlaneMover_Thinker(ClPlaneMover *mover)
{
    LOG_AS("Cl_MoverThinker");

    Map &map = App_World().map(); /// @todo Do not assume mover is from the CURRENT map.

    // Can we think yet?
    if(!Cl_GameReady()) return;

#ifdef _DEBUG
    if(map.clPlaneIndex(mover) < 0)
    {
        LOG_MAP_WARNING("Running a mover that is not in activemovers!");
    }
#endif

    // The move is cancelled if the consolePlayer becomes obstructed.
    bool const freeMove = ClPlayer_IsFreeToMove(consolePlayer);
    float const fspeed = mover->speed;

    // How's the gap?
    bool remove = false;
    coord_t const original = P_GetDouble(DMU_SECTOR, mover->sectorIndex, mover->property);
    if(de::abs(fspeed) > 0 && de::abs(mover->destination - original) > de::abs(fspeed))
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

    LOGDEV_MAP_XVERBOSE_DEBUGONLY("plane height %f in sector #%i",
            P_GetDouble(DMU_SECTOR, mover->sectorIndex, mover->property)
            << mover->sectorIndex);

    // Let the game know of this.
    if(gx.SectorHeightChangeNotification)
    {
        gx.SectorHeightChangeNotification(mover->sectorIndex);
    }

    // Make sure the client didn't get stuck as a result of this move.
    if(freeMove != ClPlayer_IsFreeToMove(consolePlayer))
    {
        LOG_MAP_VERBOSE("move blocked in sector #%i, undoing move") << mover->sectorIndex;

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
            LOG_MAP_VERBOSE("finished in sector #%i") << mover->sectorIndex;

            // It stops.
            P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->dmuPlane | DMU_SPEED, 0);

            map.deleteClPlane(mover);
        }
    }
}

ClPlaneMover *Map::newClPlane(int sectorIndex, clplanetype_t type, coord_t dest, float speed)
{
    LOG_AS("Map::newClPlane");

    int dmuPlane = (type == CPT_FLOOR ? DMU_FLOOR_OF_SECTOR : DMU_CEILING_OF_SECTOR);

    LOG_MAP_XVERBOSE("Sector #%i, type:%s, dest:%f, speed:%f")
            << sectorIndex << (type == CPT_FLOOR? "floor" : "ceiling")
            << dest << speed;

    if(sectorIndex < 0 || sectorIndex >= sectorCount())
    {
        DENG2_ASSERT(false); // Invalid Sector index.
        return 0;
    }

    // Remove any existing movers for the same plane.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(isValidClPlane(i) &&
           clActivePlanes[i]->sectorIndex == sectorIndex &&
           clActivePlanes[i]->type == type)
        {
            LOG_MAP_XVERBOSE("Removing existing mover #%i in sector #%i, type %s")
                    << i << sectorIndex << (type == CPT_FLOOR? "floor" : "ceiling");

            deleteClPlane(clActivePlanes[i]);
        }
    }

    // Add a new mover.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePlanes[i]) continue;

        LOG_MAP_XVERBOSE("New mover #%i") << i;

        // Allocate a new clplane_t thinker.
        ClPlaneMover *mov = clActivePlanes[i] = (ClPlaneMover *) Z_Calloc(sizeof(ClPlaneMover), PU_MAP, &clActivePlanes[i]);

        mov->thinker.function = reinterpret_cast<thinkfunc_t>(ClPlaneMover_Thinker);
        mov->type        = type;
        mov->sectorIndex = sectorIndex;
        mov->destination = dest;
        mov->speed       = speed;
        mov->property    = dmuPlane | DMU_HEIGHT;
        mov->dmuPlane    = dmuPlane;

        // Set the right sign for speed.
        if(mov->destination < P_GetDouble(DMU_SECTOR, sectorIndex, mov->property))
        {
            mov->speed = -mov->speed;
        }

        // Update speed and target height.
        P_SetDouble(DMU_SECTOR, sectorIndex, dmuPlane | DMU_TARGET_HEIGHT, dest);
        P_SetFloat(DMU_SECTOR, sectorIndex, dmuPlane | DMU_SPEED, speed);

        thinkers().add(mov->thinker, false /*not public*/);

        // Immediate move?
        if(de::fequal(speed, 0))
        {
            // This will remove the thinker immediately if the move is ok.
            ClPlaneMover_Thinker(mov);
        }
        return mov;
    }

    throw Error("Map::newClPlane", "Exhausted activemovers");
}

void ClPolyMover_Thinker(ClPolyMover *mover)
{
    DENG2_ASSERT(mover != 0);

    LOG_AS("Cl_PolyMoverThinker");

    Polyobj *po = mover->polyobj;
    if(mover->move)
    {
        // How much to go?
        Vector2d delta = Vector2d(po->dest) - Vector2d(po->origin);

        ddouble dist = M_ApproxDistance(delta.x, delta.y);
        if(dist <= po->speed || de::fequal(po->speed, 0))
        {
            // We'll arrive at the destination.
            mover->move = false;
        }
        else
        {
            // Adjust deltas to fit speed.
            delta = (delta / dist) * po->speed;
        }

        // Do the move.
        po->move(delta);
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
            LOG_MAP_XVERBOSE("Mover %i reached end of turn, destAngle=%i")
                    << mover->number << po->destAngle;

            // We'll arrive at the destination.
            mover->rotate = false;
        }
        else
        {
            // Adjust to speed.
            dist = /*FIX2FLT((int)*/ po->angleSpeed;
        }

        po->rotate(dist);
    }

    // Can we get rid of this mover?
    if(!mover->move && !mover->rotate)
    {
        /// @todo Do not assume the move is from the CURRENT map.
        App_World().map().deleteClPolyobj(mover);
    }
}

ClPolyMover *Map::clPolyobjByPolyobjIndex(int index)
{
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(!isValidClPolyobj(i)) continue;

        if(clActivePolyobjs[i]->number == index)
            return clActivePolyobjs[i];
    }

    return 0;
}

ClPolyMover *Map::newClPolyobj(int polyobjIndex)
{
    LOG_AS("Map::newClPolyobj");

    // Take the first unused slot.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePolyobjs[i]) continue;

        LOG_MAP_XVERBOSE("New polymover [%i] for polyobj #%i.") << i << polyobjIndex;

        ClPolyMover *mover = clActivePolyobjs[i] = (ClPolyMover *) Z_Calloc(sizeof(ClPolyMover), PU_MAP, &clActivePolyobjs[i]);

        mover->thinker.function = reinterpret_cast<thinkfunc_t>(ClPolyMover_Thinker);
        mover->polyobj = polyobjs().at(polyobjIndex);
        mover->number  = polyobjIndex;

        thinkers().add(mover->thinker, false /*not public*/);

        return mover;
    }

    return 0; // Not successful.
}

ClPolyMover *Cl_FindOrMakeActivePoly(uint polyobjIndex)
{
    ClPolyMover *mover = App_World().map().clPolyobjByPolyobjIndex(polyobjIndex);
    if(mover) return mover;
    // Not found; make a new one.
    return App_World().map().newClPolyobj(polyobjIndex);
}

void Cl_SetPolyMover(uint number, int move, int rotate)
{
    ClPolyMover *mover = Cl_FindOrMakeActivePoly(number);
    if(!mover)
    {
        LOGDEV_NET_WARNING("Out of polymovers");
        return;
    }

    // Flag for moving.
    if(move) mover->move = true;
    if(rotate) mover->rotate = true;
}

ClPlaneMover *Map::clPlaneBySectorIndex(int sectorIndex, clplanetype_t type)
{
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(!isValidClPlane(i)) continue;
        if(clActivePlanes[i]->sectorIndex != sectorIndex) continue;
        if(clActivePlanes[i]->type != type) continue;

        // Found it!
        return clActivePlanes[i];
    }
    return 0;
}

void Cl_ReadSectorDelta(int /*deltaType*/)
{
    /// @todo Do not assume the CURRENT map.
    Map &map = App_World().map();

#define PLN_FLOOR   0
#define PLN_CEILING 1

    float height[2] = { 0, 0 };
    float target[2] = { 0, 0 };
    float speed[2]  = { 0, 0 };

    // Sector index number.
    int const index = Reader_ReadUInt16(msgReader);
    DENG2_ASSERT(index < map.sectorCount());
    Sector *sec = map.sectors().at(index);

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

    if(df & (SDF_COLOR_RED | SDF_COLOR_GREEN | SDF_COLOR_BLUE))
    {
        Vector3f newColor = sec->lightColor();
        if(df & SDF_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->setLightColor(newColor);
    }

    if(df & (SDF_FLOOR_COLOR_RED | SDF_FLOOR_COLOR_GREEN | SDF_FLOOR_COLOR_BLUE))
    {
        Vector3f newColor = sec->floorSurface().tintColor();
        if(df & SDF_FLOOR_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_FLOOR_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_FLOOR_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->floorSurface().setTintColor(newColor);
    }

    if(df & (SDF_CEIL_COLOR_RED | SDF_CEIL_COLOR_GREEN | SDF_CEIL_COLOR_BLUE))
    {
        Vector3f newColor = sec->ceilingSurface().tintColor();
        if(df & SDF_CEIL_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_CEIL_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_CEIL_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->ceilingSurface().setTintColor(newColor);
    }

    // The whole delta has now been read.

    // Do we need to start any moving planes?
    if(df & SDF_FLOOR_HEIGHT)
    {
        map.newClPlane(index, CPT_FLOOR, height[PLN_FLOOR], 0);
    }
    else if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
    {
        map.newClPlane(index, CPT_FLOOR, target[PLN_FLOOR], speed[PLN_FLOOR]);
    }

    if(df & SDF_CEILING_HEIGHT)
    {
        map.newClPlane(index, CPT_CEILING, height[PLN_CEILING], 0);
    }
    else if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
    {
        map.newClPlane(index, CPT_CEILING, target[PLN_CEILING], speed[PLN_CEILING]);
    }

#undef PLN_CEILING
#undef PLN_FLOOR
}

void Cl_ReadSideDelta(int /*deltaType*/)
{
    /// @todo Do not assume the CURRENT map.
    Map &map = App_World().map();

    int const index = Reader_ReadUInt16(msgReader);
    int const df    = Reader_ReadPackedUInt32(msgReader); // Flags.

    LineSide *side = map.sideByIndex(index);
    DENG2_ASSERT(side != 0);

    if(df & SIDF_TOP_MATERIAL)
    {
        int matIndex = Reader_ReadPackedUInt16(msgReader);
        side->top().setMaterial(Cl_FindLocalMaterial(matIndex));
    }

    if(df & SIDF_MID_MATERIAL)
    {
        int matIndex = Reader_ReadPackedUInt16(msgReader);
        side->middle().setMaterial(Cl_FindLocalMaterial(matIndex));
    }

    if(df & SIDF_BOTTOM_MATERIAL)
    {
        int matIndex = Reader_ReadPackedUInt16(msgReader);
        side->bottom().setMaterial(Cl_FindLocalMaterial(matIndex));
    }

    if(df & SIDF_LINE_FLAGS)
    {
        // The delta includes the entire lowest byte.
        int lineFlags = Reader_ReadByte(msgReader);
        Line &line = side->line();
        line.setFlags((line.flags() & ~0xff) | lineFlags, de::ReplaceFlags);
    }

    if(df & (SIDF_TOP_COLOR_RED | SIDF_TOP_COLOR_GREEN | SIDF_TOP_COLOR_BLUE))
    {
        Vector3f newColor = side->top().tintColor();
        if(df & SIDF_TOP_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_TOP_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_TOP_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        side->top().setTintColor(newColor);
    }

    if(df & (SIDF_MID_COLOR_RED | SIDF_MID_COLOR_GREEN | SIDF_MID_COLOR_BLUE))
    {
        Vector3f newColor = side->middle().tintColor();
        if(df & SIDF_MID_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_MID_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_MID_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        side->middle().setTintColor(newColor);
    }
    if(df & SIDF_MID_COLOR_ALPHA)
    {
        side->middle().setOpacity(Reader_ReadByte(msgReader) / 255.f);
    }

    if(df & (SIDF_BOTTOM_COLOR_RED | SIDF_BOTTOM_COLOR_GREEN | SIDF_BOTTOM_COLOR_BLUE))
    {
        Vector3f newColor = side->bottom().tintColor();
        if(df & SIDF_BOTTOM_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_BOTTOM_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_BOTTOM_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        side->bottom().setTintColor(newColor);
    }

    if(df & SIDF_MID_BLENDMODE)
    {
        side->middle().setBlendMode(blendmode_t(Reader_ReadInt32(msgReader)));
    }

    if(df & SIDF_FLAGS)
    {
        // The delta includes the entire lowest byte.
        int sideFlags = Reader_ReadByte(msgReader);
        side->setFlags((side->flags() & ~0xff) | sideFlags, de::ReplaceFlags);
    }
}

void Cl_ReadPolyDelta()
{
    /// @todo Do not assume the CURRENT map.
    Map &map = App_World().map();

    int const index = Reader_ReadPackedUInt16(msgReader);
    int const df    = Reader_ReadByte(msgReader); // Flags.

    DENG2_ASSERT(index < map.polyobjCount());
    Polyobj *po = map.polyobjs().at(index);

    if(df & PODF_DEST_X)
    {
        po->dest[VX] = Reader_ReadFloat(msgReader);
    }

    if(df & PODF_DEST_Y)
    {
        po->dest[VY] = Reader_ReadFloat(msgReader);
    }

    if(df & PODF_SPEED)
    {
        po->speed = Reader_ReadFloat(msgReader);
    }

    if(df & PODF_DEST_ANGLE)
    {
        po->destAngle = ((angle_t)Reader_ReadInt16(msgReader)) << 16;
    }

    if(df & PODF_ANGSPEED)
    {
        po->angleSpeed = ((angle_t)Reader_ReadInt16(msgReader)) << 16;
    }

    if(df & PODF_PERPETUAL_ROTATE)
    {
        po->destAngle = -1;
    }

    // Update the polyobj's mover thinkers.
    Cl_SetPolyMover(index, df & (PODF_DEST_X | PODF_DEST_Y | PODF_SPEED),
                    df & (PODF_DEST_ANGLE | PODF_ANGSPEED |
                          PODF_PERPETUAL_ROTATE));
}
