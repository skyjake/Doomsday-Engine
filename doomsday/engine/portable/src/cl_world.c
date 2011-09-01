/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Clientside World Management
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "dd_world.h"
#include "de_filesys.h"

#include "r_util.h"

// MACROS ------------------------------------------------------------------

#define MAX_MOVERS          1024 // Definitely enough!
#define MAX_TRANSLATIONS    16384

#define MVF_CEILING         0x1 // Move ceiling.
#define MVF_SET_FLOORPIC    0x2 // Set floor texture when move done.

// TYPES -------------------------------------------------------------------

typedef struct {
    thinker_t   thinker;
    uint        sectornum;
    clmovertype_t type;
    int         property; // floor or ceiling
    int         dmuPlane;
    float       destination;
    float       speed;
} mover_t;

typedef struct {
    thinker_t   thinker;
    uint        number;
    polyobj_t  *poly;
    boolean     move;
    boolean     rotate;
} polymover_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Cl_MoverThinker(mover_t *mover);
void Cl_PolyMoverThinker(polymover_t* mover);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mover_t *activemovers[MAX_MOVERS];
static polymover_t *activepolys[MAX_MOVERS];
static short *xlat_lump; // obsolete

// CODE --------------------------------------------------------------------

/**
 * Allocates and inits the lump translation array. Clients use this
 * to make sure lump references are correct (in case the server and the
 * client are using different WAD configurations and the lump index
 * numbers happen to differ).
 *
 * \fixme A bit questionable? Why not allow the clients to download
 * data from the server in ambiguous cases?
 *
 * \note Only used by the obsolete cl_oldworld functions.
 */
void Cl_InitTranslations(void)
{
    xlat_lump = Z_Malloc(sizeof(short) * MAX_TRANSLATIONS, PU_APPSTATIC, 0);
    memset(xlat_lump, 0, sizeof(short) * MAX_TRANSLATIONS);
    { int i, numLumps = F_LumpCount();
    for(i = 0; i < numLumps; ++i)
        xlat_lump[i] = i; // Identity translation.
    }
}

void Cl_SetLumpTranslation(lumpnum_t lumpNum, char *name)
{
    if(lumpNum < 0 || lumpNum >= MAX_TRANSLATIONS)
        return; // Can't do it, sir! We just don't have the power!!

    xlat_lump[lumpNum] = F_CheckLumpNumForName(name, true);
    if(xlat_lump[lumpNum] < 0)
    {
        VERBOSE(Con_Message("Cl_SetLumpTranslation: %s not found.\n", name));
        xlat_lump[lumpNum] = 0;
    }
}

/**
 * This is a fail-safe operation.
 */
lumpnum_t Cl_TranslateLump(lumpnum_t lumpNum)
{
    if(lumpNum < 0 || lumpNum >= MAX_TRANSLATIONS)
        return 0;
    return xlat_lump[lumpNum];
}

static boolean Cl_IsMoverValid(int i)
{
    if(!activemovers[i]) return false;
    return (activemovers[i]->thinker.function == Cl_MoverThinker);
}

static boolean Cl_IsPolyValid(int i)
{
    if(!activepolys[i]) return false;
    return (activepolys[i]->thinker.function == Cl_PolyMoverThinker);
}

/**
 * Clears the arrays that track active plane and polyobj mover thinkers.
 */
void Cl_InitMovers(void)
{
    memset(activemovers, 0, sizeof(activemovers));
    memset(activepolys, 0, sizeof(activepolys));
}

void Cl_RemoveActiveMover(mover_t *mover)
{
    int                 i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activemovers[i] == mover)
        {
#ifdef _DEBUG
            Con_Message("Cl_RemoveActiveMover: Removing mover [%i] in sector %i.\n", i, mover->sectornum);
#endif
            P_ThinkerRemove(&mover->thinker);
            return;
        }

#ifdef _DEBUG
    Con_Message("Cl_RemoveActiveMover: Mover in sector %i not removed!\n", mover->sectornum);
#endif
}

/**
 * Removes the given polymover from the active polys array.
 */
void Cl_RemoveActivePoly(polymover_t *mover)
{
    int                 i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activepolys[i] == mover)
        {
            P_ThinkerRemove(&mover->thinker);
            break;
        }
}

/**
 * Plane mover. Makes changes in planes using DMU.
 */
void Cl_MoverThinker(mover_t *mover)
{
    float               original;
    boolean             remove = false;
    boolean             freeMove;
    float               fspeed;

    if(!Cl_GameReady())
        return; // Can we think yet?

#ifdef _DEBUG
    {
        int i, gotIt = false;
        for(i = 0; i < MAX_MOVERS; ++i)
        {
            if(activemovers[i] == mover)
                gotIt = true;
        }
        if(!gotIt)
            Con_Message("Cl_MoverThinker: Running a mover that is not in activemovers!\n");
    }
#endif

    // The move is cancelled if the consolePlayer becomes obstructed.
    freeMove = ClPlayer_IsFreeToMove(consolePlayer);
    fspeed = mover->speed;

    // How's the gap?
    original = P_GetFloat(DMU_SECTOR, mover->sectornum, mover->property);
    if(fabs(mover->destination - original) > fabs(fspeed))
    {
        // Do the move.
        P_SetFloat(DMU_SECTOR, mover->sectornum, mover->property, original + fspeed);
    }
    else
    {
        // We have reached the destination.
        P_SetFloat(DMU_SECTOR, mover->sectornum, mover->property, mover->destination);

        // This thinker can now be removed.
        remove = true;
    }

#ifdef _DEBUG
    VERBOSE2( Con_Message("Cl_MoverThinker: plane height %f in sector %i\n",
                P_GetFloat(DMU_SECTOR, mover->sectornum, mover->property),
                mover->sectornum) );
#endif

    // Make sure the client didn't get stuck as a result of this move.
    if(freeMove != ClPlayer_IsFreeToMove(consolePlayer))
    {
#ifdef _DEBUG
        Con_Message("Cl_MoverThinker: move blocked in sector %i, undoing\n", mover->sectornum);
#endif

        // Something was blocking the way! Go back to original height.
        P_SetFloat(DMU_SECTOR, mover->sectornum, mover->property, original);
    }
    else
    {
        // The move was valid, so let the game know of this.
        if(gx.SectorHeightChangeNotification)
        {
            gx.SectorHeightChangeNotification(mover->sectornum);
        }

        // Can we remove this thinker?
        if(remove)
        {
#ifdef _DEBUG
            /*VERBOSE2*/( Con_Message("Cl_MoverThinker: finished in %i\n", mover->sectornum) );
#endif
            // It stops.
            P_SetFloat(DMU_SECTOR, mover->sectornum, mover->dmuPlane | DMU_SPEED, 0);

            Cl_RemoveActiveMover(mover);
        }
    }
}

void Cl_AddMover(uint sectornum, clmovertype_t type, float dest, float speed)
{
    int                 i;
    mover_t            *mov;
    int                 dmuPlane = (type == MVT_FLOOR ? DMU_FLOOR_OF_SECTOR
                                                      : DMU_CEILING_OF_SECTOR);
#ifdef _DEBUG
    /*VERBOSE2*/( Con_Message("Cl_AddMover: Sector=%i, type=%s, dest=%f, speed=%f\n",
                          sectornum, type==MVT_FLOOR? "floor" : "ceiling",
                          dest, speed) );
#endif

    if(sectornum >= numSectors)
        return;

    // Remove any existing movers for the same plane.
    for(i = 0; i < MAX_MOVERS; ++i)
    {
        if(Cl_IsMoverValid(i) &&
           activemovers[i]->sectornum == sectornum &&
           activemovers[i]->type == type)
        {
#ifdef _DEBUG
            Con_Message("Cl_AddMover: Removing existing mover [%i] in sector %i, type %s\n", i, sectornum,
                        type == MVT_FLOOR? "floor" : "ceiling");
#endif
            Cl_RemoveActiveMover(activemovers[i]);
        }
    }

    // Add a new mover.
    for(i = 0; i < MAX_MOVERS; ++i)
        if(!activemovers[i])
        {
#ifdef _DEBUG
            Con_Message("Cl_AddMover: ...new mover [%i]\n", i);
#endif
            // Allocate a new mover_t thinker.
            mov = activemovers[i] = Z_Calloc(sizeof(mover_t), PU_MAP, &activemovers[i]);
            mov->thinker.function = Cl_MoverThinker;
            mov->type = type;
            mov->sectornum = sectornum;
            mov->destination = dest;
            mov->speed = speed;
            mov->property = dmuPlane | DMU_HEIGHT;
            mov->dmuPlane = dmuPlane;

            // Set the right sign for speed.
            if(mov->destination < P_GetFloat(DMU_SECTOR, sectornum, mov->property))
                mov->speed = -mov->speed;

            // Update speed and target height.
            P_SetFloat(DMU_SECTOR, sectornum, dmuPlane | DMU_TARGET_HEIGHT, dest);
            P_SetFloat(DMU_SECTOR, sectornum, dmuPlane | DMU_SPEED, speed);

            P_ThinkerAdd(&mov->thinker, false /*not public*/);
            break;
        }
}

void Cl_PolyMoverThinker(polymover_t* mover)
{
    polyobj_t*          poly = mover->poly;
    float               dx, dy;
    float               dist;

    if(mover->move)
    {
        // How much to go?
        dx = poly->dest[VX] - poly->pos[VX];
        dy = poly->dest[VY] - poly->pos[VY];
        dist = P_ApproxDistance(dx, dy);
        if(dist <= poly->speed || FEQUAL(poly->speed, 0))
        {
            // We'll arrive at the destination.
            mover->move = false;
        }
        else
        {
            // Adjust deltas to fit speed.
            dx = poly->speed * (dx / dist);
            dy = poly->speed * (dy / dist);
        }

        // Do the move.
        P_PolyobjMove(P_GetPolyobj(mover->number | 0x80000000), dx, dy);
    }

    if(mover->rotate)
    {
        // How much to go?
        int dist = poly->destAngle - poly->angle;
        int speed = poly->angleSpeed;

        //dist = FIX2FLT(poly->destAngle - poly->angle);
        //if(!poly->angleSpeed || dist > 0   /*(abs(FLT2FIX(dist) >> 4) <= abs(((signed) poly->angleSpeed) >> 4)*/
        //    /* && poly->destAngle != -1*/) || !poly->angleSpeed)
        if(!poly->angleSpeed || ABS(dist >> 2) <= ABS(speed >> 2))
        {
#ifdef _DEBUG
            Con_Message("Cl_PolyMoverThinker: Mover %i reached end of turn, destAngle=%x.\n", mover->number, poly->destAngle);
#endif
            // We'll arrive at the destination.
            mover->rotate = false;
        }
        else
        {
            // Adjust to speed.
            dist = /*FIX2FLT((int)*/ poly->angleSpeed;
        }

/*#ifdef _DEBUG
        Con_Message("%f\n", dist);
#endif*/

        P_PolyobjRotate(P_GetPolyobj(mover->number | 0x80000000), dist);
    }

    // Can we get rid of this mover?
    if(!mover->move && !mover->rotate)
        Cl_RemoveActivePoly(mover);
}

polymover_t* Cl_FindOrMakeActivePoly(uint number)
{
    int i;
    int available = -1;
    polymover_t* mover;

    for(i = 0; i < MAX_MOVERS; ++i)
    {
        if(available < 0 && !activepolys[i])
            available = i;

        if(Cl_IsPolyValid(i) && activepolys[i]->number == number)
            return activepolys[i];
    }

    // Not found, make a new one.
    if(available >= 0)
    {
#ifdef _DEBUG
        Con_Message("Cl_FindOrMakeActivePoly: New polymover [%i] in polyobj %i.\n", available, number);
#endif
        activepolys[available] = mover = Z_Calloc(sizeof(polymover_t), PU_MAP, &activepolys[available]);
        mover->thinker.function = Cl_PolyMoverThinker;
        mover->poly = polyObjs[number];
        mover->number = number;
        P_ThinkerAdd(&mover->thinker, false /*not public*/);
        return mover;
    }

    // Not successful.
    return NULL;
}

void Cl_SetPolyMover(uint number, int move, int rotate)
{
    polymover_t* mover = Cl_FindOrMakeActivePoly(number);
    if(!mover)
    {
        Con_Message("Cl_SetPolyMover: Out of polymovers.\n");
        return;
    }
    // Flag for moving.
    if(move)
        mover->move = true;
    if(rotate)
        mover->rotate = true;
}

/**
 * Removes all the active movers.
 */
void Cl_RemoveMovers(void)
{
    int                 i;

    for(i = 0; i < MAX_MOVERS; ++i)
    {
        if(Cl_IsMoverValid(i))
        {
            P_ThinkerRemove(&activemovers[i]->thinker);
        }
        if(Cl_IsPolyValid(i))
        {
            P_ThinkerRemove(&activepolys[i]->thinker);
        }
    }
}

mover_t *Cl_GetActiveMover(uint sectornum, clmovertype_t type)
{
    int                 i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(Cl_IsMoverValid(i) &&
           activemovers[i]->sectornum == sectornum &&
           activemovers[i]->type == type)
        {
            return activemovers[i];
        }
    return NULL;
}

/**
 * @return              @c false, if the end marker is found (lump
 *                      index zero).
 */
int Cl_ReadLumpDelta(void)
{
    lumpnum_t           num = (lumpnum_t) Reader_ReadPackedUInt16(msgReader);
    char                name[9];

    if(!num)
        return false; // No more.

    // Read the name of the lump.
    memset(name, 0, sizeof(name));
    Reader_Read(msgReader, name, 8);

    VERBOSE(Con_Printf("LumpTranslate: %i => %s\n", num, name));

    // Set up translation.
    Cl_SetLumpTranslation(num, name);
    return true;
}

/**
 * Reads a sector delta from the PSV_FRAME2 message buffer and applies it
 * to the world.
 */
void Cl_ReadSectorDelta2(int deltaType, boolean skip)
{
    static sector_t     dummy; // Used when skipping.
    static plane_t*     dummyPlaneArray[2];
    static plane_t      dummyPlanes[2];

    unsigned short      num;
    sector_t*           sec;
    int                 df;
    boolean             wasChanged = false;

    // Set up the dummy.
    dummyPlaneArray[0] = &dummyPlanes[0];
    dummyPlaneArray[1] = &dummyPlanes[1];
    dummy.planes = dummyPlaneArray;

    // Sector index number.
    num = Reader_ReadUInt16(msgReader);

    // Flags.
    /*if(deltaType == DT_SECTOR_R6)
    {
        // The R6 protocol reserves two bytes for the flags.
        df = Reader_ReadUInt16(msgReader);
    }
    else*/
    //{
    df = Reader_ReadPackedUInt32(msgReader);
    //}

    if(!skip)
    {
#ifdef _DEBUG
        if(num >= numSectors)
        {
            // This is worrisome.
            Con_Error("Cl_ReadSectorDelta2: Sector %i out of range.\n", num);
        }
#endif
        sec = SECTOR_PTR(num);
    }
    else
    {
        // Read the data into the dummy if we're skipping.
        sec = &dummy;
    }

    if(df & SDF_FLOOR_MATERIAL)
    {
        material_t* mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(Reader_ReadPackedUInt16(msgReader));
        Surface_SetMaterial(&sec->SP_floorsurface, mat);
    }
    if(df & SDF_CEILING_MATERIAL)
    {
        material_t* mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(Reader_ReadPackedUInt16(msgReader));
        Surface_SetMaterial(&sec->SP_ceilsurface, mat);
    }

    if(df & SDF_LIGHT)
        sec->lightLevel = Reader_ReadByte(msgReader) / 255.0f;
    if(df & SDF_FLOOR_HEIGHT)
    {
        P_SetFloatp(sec, DMU_FLOOR_OF_SECTOR | DMU_HEIGHT, FIX2FLT(Reader_ReadInt16(msgReader) << 16));
        wasChanged = true;

        /*
        if(!skip)
        {
            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Absolute floor height=%f\n",
                                num, sec->SP_floorheight) );
        }
        */
    }
    if(df & SDF_CEILING_HEIGHT)
    {
        fixed_t height = Reader_ReadInt16(msgReader) << 16;
        P_SetFloatp(sec, DMU_CEILING_OF_SECTOR | DMU_HEIGHT, FIX2FLT(height));
        wasChanged = true;

        /*
            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Absolute ceiling height=%f%s\n",
                                num, sec->SP_ceilheight, skip? " --SKIPPED!--" : "") );
        }*/
    }
    if(df & SDF_FLOOR_TARGET)
    {
        fixed_t height = Reader_ReadInt16(msgReader) << 16;
        P_SetFloatp(sec, DMU_FLOOR_OF_SECTOR | DMU_TARGET_HEIGHT, FIX2FLT(height));

        /*if(!skip)
        {
            sec->planes[PLN_FLOOR]->target = FIX2FLT(height);

            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Floor target=%f\n",
                                num, sec->planes[PLN_FLOOR]->target) );
        }*/
    }
    if(df & SDF_FLOOR_SPEED)
    {
        fixed_t speed = Reader_ReadByte(msgReader);
        //if(!skip)
        {
            P_SetFloatp(sec, DMU_FLOOR_OF_SECTOR | DMU_SPEED,
                        FIX2FLT(speed << (df & SDF_FLOOR_SPEED_44 ? 12 : 15)));

            /*VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Floor speed=%f\n",
                                num, sec->planes[PLN_FLOOR]->speed) );*/
        }
    }
#if 0
    if(df & SDF_FLOOR_TEXMOVE)
    {   // Old clients might include these.
        /*fixed_t moveX = */ Msg_ReadShort() /* << 8*/;
        /*fixed_t moveY = */ Msg_ReadShort() /* << 8*/;
    }
#endif
    if(df & SDF_CEILING_TARGET)
    {
        fixed_t target = Reader_ReadInt16(msgReader) << 16;
/*#ifdef _DEBUG
        Con_Message("Cl_ReadSectorDelta2: Ceiling target %f for sector %i\n", FIX2FLT(target), num);
#endif*/

        //if(!skip)
        {
            P_SetFloatp(sec, DMU_CEILING_OF_SECTOR | DMU_TARGET_HEIGHT, FIX2FLT(target));

          /*  VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Ceiling target=%f\n",
                                num, sec->planes[PLN_CEILING]->target) );*/
        }
    }
    if(df & SDF_CEILING_SPEED)
    {
        byte speed = Reader_ReadByte(msgReader);
        //if(!skip)
        {
            P_SetFloatp(sec, DMU_CEILING_OF_SECTOR | DMU_SPEED,
                        FIX2FLT(speed << (df & SDF_CEILING_SPEED_44 ? 12 : 15)));

            /*VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Ceiling speed=%f\n",
                                num, sec->planes[PLN_CEILING]->speed) );*/
        }
    }
#if 0
    if(df & SDF_CEILING_TEXMOVE)
    {   // Old clients might include these.
        /*fixed_t moveX = */ Msg_ReadShort() /*<< 8*/;
        /*fixed_t moveY = */ Msg_ReadShort() /*<< 8*/;
    }
#endif
    if(df & SDF_COLOR_RED)
        sec->rgb[0] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SDF_COLOR_GREEN)
        sec->rgb[1] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SDF_COLOR_BLUE)
        sec->rgb[2] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SDF_FLOOR_COLOR_RED)
        Surface_SetColorR(&sec->SP_floorsurface, Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_FLOOR_COLOR_GREEN)
        Surface_SetColorG(&sec->SP_floorsurface, Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_FLOOR_COLOR_BLUE)
        Surface_SetColorB(&sec->SP_floorsurface, Reader_ReadByte(msgReader) / 255.f);

    if(df & SDF_CEIL_COLOR_RED)
        Surface_SetColorR(&sec->SP_ceilsurface, Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_CEIL_COLOR_GREEN)
        Surface_SetColorG(&sec->SP_ceilsurface, Reader_ReadByte(msgReader) / 255.f);
    if(df & SDF_CEIL_COLOR_BLUE)
        Surface_SetColorB(&sec->SP_ceilsurface, Reader_ReadByte(msgReader) / 255.f);

    // The whole delta has been read. If we're about to skip, let's do so.
    if(skip)
        return;

    // If the plane heights were changed, we need to update the mobjs in
    // the sector.
    if(wasChanged)
    {
        // Let the game know we made some changes.
        if(gx.SectorHeightChangeNotification)
            gx.SectorHeightChangeNotification(num);
    }

    // Do we need to start any moving planes?
    if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
    {
        Cl_AddMover(num, MVT_FLOOR, sec->planes[PLN_FLOOR]->target,
                    sec->planes[PLN_FLOOR]->speed);
    }
    if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
    {
        Cl_AddMover(num, MVT_CEILING, sec->planes[PLN_CEILING]->target,
                    sec->planes[PLN_CEILING]->speed);
    }
}

/**
 * Reads a side delta from the message buffer and applies it to the world.
 */
void Cl_ReadSideDelta2(int deltaType, boolean skip)
{
    unsigned short      num;

    int                 df, topMat = 0, midMat = 0, botMat = 0;
    int                 blendmode = 0;
    byte                lineFlags = 0, sideFlags = 0;
    float               toprgb[3] = {0,0,0}, midrgba[4] = {0,0,0,0};
    float               bottomrgb[3] = {0,0,0};
    sidedef_t          *sid;

    // First read all the data.
    num = Reader_ReadUInt16(msgReader);

    // Flags.
    /*if(deltaType == DT_SIDE_R6)
    {
        // The R6 protocol reserves a single byte for a side delta.
        df = Reader_ReadByte(msgReader);
    }
    else*/
    {
        df = Reader_ReadPackedUInt32(msgReader);
    }

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
if(num >= numSideDefs)
{
    // This is worrisome.
    Con_Error("Cl_ReadSideDelta2: Side %i out of range.\n", num);
}
#endif

    sid = SIDE_PTR(num);

    if(df & SIDF_TOP_MATERIAL)
    {
        material_t* mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(topMat);
        Surface_SetMaterial(&sid->SW_topsurface, mat);
    }
    if(df & SIDF_MID_MATERIAL)
    {
        material_t* mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(midMat);
        Surface_SetMaterial(&sid->SW_middlesurface, mat);
    }
    if(df & SIDF_BOTTOM_MATERIAL)
    {
        material_t* mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(botMat);
        Surface_SetMaterial(&sid->SW_bottomsurface, mat);
    }

    if(df & SIDF_TOP_COLOR_RED)
        Surface_SetColorR(&sid->SW_topsurface, toprgb[CR]);
    if(df & SIDF_TOP_COLOR_GREEN)
        Surface_SetColorG(&sid->SW_topsurface, toprgb[CG]);
    if(df & SIDF_TOP_COLOR_BLUE)
        Surface_SetColorB(&sid->SW_topsurface, toprgb[CB]);

    if(df & SIDF_MID_COLOR_RED)
        Surface_SetColorR(&sid->SW_middlesurface, midrgba[CR]);
    if(df & SIDF_MID_COLOR_GREEN)
        Surface_SetColorG(&sid->SW_middlesurface, midrgba[CG]);
    if(df & SIDF_MID_COLOR_BLUE)
        Surface_SetColorB(&sid->SW_middlesurface, midrgba[CB]);
    if(df & SIDF_MID_COLOR_ALPHA)
        Surface_SetColorA(&sid->SW_middlesurface, midrgba[CA]);

    if(df & SIDF_BOTTOM_COLOR_RED)
        Surface_SetColorR(&sid->SW_bottomsurface, bottomrgb[CR]);
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        Surface_SetColorG(&sid->SW_bottomsurface, bottomrgb[CG]);
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        Surface_SetColorB(&sid->SW_bottomsurface, bottomrgb[CB]);

    if(df & SIDF_MID_BLENDMODE)
        Surface_SetBlendMode(&sid->SW_middlesurface, blendmode);

    if(df & SIDF_FLAGS)
    {
        // The delta includes the entire lowest byte.
        sid->flags &= ~0xff;
        sid->flags |= sideFlags;
    }

    if(df & SIDF_LINE_FLAGS)
    {
        linedef_t *line = R_GetLineForSide(num);

        if(line)
        {
            // The delta includes the entire lowest byte.
            line->flags &= ~0xff;
            line->flags |= lineFlags;
#if _DEBUG
Con_Printf("Cl_ReadSideDelta2: Lineflag %u: %02x\n", (unsigned int) GET_LINE_IDX(line), lineFlags);
#endif
        }
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
    polyobj_t          *po;
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
    Con_Message("Cl_ReadPolyDelta2: PO %i, angle %f, speed %f\n", num, FIX2FLT(destAngle), FIX2FLT(angleSpeed));
#endif*/

    if(skip)
        return;

#ifdef _DEBUG
if(num >= numPolyObjs)
{
    // This is worrisome.
    Con_Error("Cl_ReadPolyDelta2: PO %i out of range.\n", num);
}
#endif

    po = polyObjs[num];

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
