/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * cl_world.c: Clientside World Management
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"

#include "r_util.h"

// MACROS ------------------------------------------------------------------

#define MAX_MOVERS          128 // Definitely enough!
#define MAX_TRANSLATIONS    16384

#define MVF_CEILING         0x1 // Move ceiling.
#define MVF_SET_FLOORPIC    0x2 // Set floor texture when move done.

// TYPES -------------------------------------------------------------------

typedef struct {
    thinker_t   thinker;
    sector_t   *sector;
    uint        sectornum;
    clmovertype_t type;
    float      *current;
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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mover_t *activemovers[MAX_MOVERS];
static polymover_t *activepolys[MAX_MOVERS];
short *xlat_lump;

// CODE --------------------------------------------------------------------

/**
 * Allocates and inits the lump translation array. Clients use this
 * to make sure lump references are correct (in case the server and the
 * client are using different WAD configurations and the lump index
 * numbers happen to differ).
 *
 * \fixme A bit questionable? Why not allow the clients to download
 * data from the server in ambiguous cases?
 */
void Cl_InitTranslations(void)
{
    int         i;

    xlat_lump = Z_Malloc(sizeof(short) * MAX_TRANSLATIONS, PU_REFRESHTEX, 0);
    memset(xlat_lump, 0, sizeof(short) * MAX_TRANSLATIONS);
    for(i = 0; i < numlumps; ++i)
        xlat_lump[i] = i; // Identity translation.
}

void Cl_SetLumpTranslation(short lumpnum, char *name)
{
    if(lumpnum < 0 || lumpnum >= MAX_TRANSLATIONS)
        return; // Can't do it, sir! We just don't the power!!

    xlat_lump[lumpnum] = W_CheckNumForName(name);
    if(xlat_lump[lumpnum] < 0)
    {
        VERBOSE(Con_Message("Cl_SetLumpTranslation: %s not found.\n", name));
        xlat_lump[lumpnum] = 0;
    }
}

/**
 * This is a fail-safe operation.
 */
short Cl_TranslateLump(short lump)
{
    if(lump < 0 || lump >= MAX_TRANSLATIONS)
        return 0;
    return xlat_lump[lump];
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
    int         i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activemovers[i] == mover)
        {
            P_RemoveThinker(&mover->thinker);
            activemovers[i] = NULL;
            break;
        }
}

/**
 * Removes the given polymover from the active polys array.
 */
void Cl_RemoveActivePoly(polymover_t *mover)
{
    int         i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activepolys[i] == mover)
        {
            P_RemoveThinker(&mover->thinker);
            activepolys[i] = NULL;
            break;
        }
}

/**
 * Plane mover.
 */
void Cl_MoverThinker(mover_t *mover)
{
    float      *current = mover->current, original = *current;
    boolean     remove = false;
    boolean     freeMove;
    float       fspeed;

    if(!Cl_GameReady())
        return;                 // Can we think yet?

    // The move is cancelled if the consoleplayer becomes obstructed.
    freeMove = Cl_IsFreeToMove(consoleplayer);
    fspeed = mover->speed;
    // How's the gap?
    if(fabs(mover->destination - *current) > fabs(fspeed))
    {
        // Do the move.
        *current += fspeed;
    }
    else
    {
        // We have reached the destination.
        *current = mover->destination;

        // This thinker can now be removed.
        remove = true;
    }

    P_SectorPlanesChanged(mover->sector);

    // Make sure the client didn't get stuck as a result of this move.
    if(freeMove != Cl_IsFreeToMove(consoleplayer))
    {
        // Something was blocking the way!
        *current = original;
        P_SectorPlanesChanged(mover->sector);
    }
    else if(remove)             // Can we remove this thinker?
    {
        Cl_RemoveActiveMover(mover);
    }
}

void Cl_AddMover(uint sectornum, clmovertype_t type, float dest, float speed)
{
    sector_t   *sector;
    int         i;
    mover_t    *mov;

    VERBOSE( Con_Printf("Cl_AddMover: Sector=%i, type=%s, dest=%f, speed=%f\n",
                        sectornum, type==MVT_FLOOR? "floor" : "ceiling",
                        dest, speed) );

    if(speed == 0)
        return;

    if(sectornum >= numsectors)
        return;
    sector = SECTOR_PTR(sectornum);

    // Remove any existing movers for the same plane.
    for(i = 0; i < MAX_MOVERS; ++i)
        if(activemovers[i] && activemovers[i]->sector == sector &&
           activemovers[i]->type == type)
        {
            Cl_RemoveActiveMover(activemovers[i]);
        }

    /*if(!speed) // Lightspeed?
       {
       // Change immediately.
       if(type == MVT_CEILING)
       sector->planes[PLN_CEILING].height = dest;
       else
       sector->planes[PLN_FLOOR].height = dest;
       return;
       } */

    // Add a new mover.
    for(i = 0; i < MAX_MOVERS; ++i)
        if(activemovers[i] == NULL)
        {
            // Allocate a new mover_t thinker.
            mov = activemovers[i] = Z_Malloc(sizeof(mover_t), PU_LEVEL, 0);
            memset(mov, 0, sizeof(mover_t));
            mov->thinker.function = Cl_MoverThinker;
            mov->type = type;
            mov->sectornum = sectornum;
            mov->sector = SECTOR_PTR(sectornum);
            mov->destination = dest;
            mov->speed = speed;
            mov->current =
                (type ==
                 MVT_FLOOR ? &mov->sector->planes[PLN_FLOOR]->height : &mov->sector->
                 planes[PLN_CEILING]->height);
            // Set the right sign for speed.
            if(mov->destination < *mov->current)
                mov->speed = -mov->speed;

            P_AddThinker(&mov->thinker);
            break;
        }
}

void Cl_PolyMoverThinker(polymover_t *mover)
{
    polyobj_t  *poly = mover->poly;
    float       dx, dy;
    float       dist;

    if(mover->move)
    {
        // How much to go?
        dx = poly->dest.pos[VX] - poly->startSpot.pos[VX];
        dy = poly->dest.pos[VY] - poly->startSpot.pos[VY];
        dist = P_ApproxDistance(dx, dy);
        if(dist <= poly->speed || poly->speed == 0)
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
        P_PolyobjMove(mover->number | 0x80000000, dx, dy);
    }

    if(mover->rotate)
    {
        // How much to go?
        dist = FIX2FLT(poly->destAngle - poly->angle);
        if((abs(FLT2FIX(dist) >> 4) <= abs(((signed) poly->angleSpeed) >> 4)
            /* && poly->destAngle != -1*/) || !poly->angleSpeed)
        {
            // We'll arrive at the destination.
            mover->rotate = false;
        }
        else
        {
            // Adjust to speed.
            dist = FIX2FLT(poly->angleSpeed);
        }
        P_PolyobjRotate(mover->number | 0x80000000, FLT2FIX(dist));
    }

    // Can we get rid of this mover?
    if(!mover->move && !mover->rotate)
        Cl_RemoveActivePoly(mover);
}

polymover_t *Cl_FindActivePoly(uint number)
{
    uint                i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activepolys[i] && activepolys[i]->number == number)
            return activepolys[i];
    return NULL;
}

polymover_t *Cl_NewPolyMover(uint number)
{
    polymover_t        *mover;
    polyobj_t          *poly = polyobjs[number];

    mover = Z_Malloc(sizeof(polymover_t), PU_LEVEL, 0);
    memset(mover, 0, sizeof(*mover));
    mover->thinker.function = Cl_PolyMoverThinker;
    mover->poly = poly;
    mover->number = number;
    P_AddThinker(&mover->thinker);
    return mover;
}

void Cl_SetPolyMover(uint number, int move, int rotate)
{
    polymover_t *mover;

    // Try to find an existing mover.
    mover = Cl_FindActivePoly(number);
    if(!mover)
        mover = Cl_NewPolyMover(number);
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
    int         i;

    for(i = 0; i < MAX_MOVERS; ++i)
    {
        if(activemovers[i])
        {
            P_RemoveThinker(&activemovers[i]->thinker);
            activemovers[i] = NULL;
        }
        if(activepolys[i])
        {
            P_RemoveThinker(&activepolys[i]->thinker);
            activepolys[i] = NULL;
        }
    }
}

mover_t *Cl_GetActiveMover(uint sectornum, clmovertype_t type)
{
    int         i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activemovers[i] && activemovers[i]->sectornum == sectornum &&
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
    int         num = Msg_ReadPackedShort();
    char        name[9];

    if(!num)
        return false;           // No more.

    // Read the name of the lump.
    memset(name, 0, sizeof(name));
    Msg_Read(name, 8);

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
    unsigned short num;
    sector_t   *sec;
    int         df;
    boolean     wasChanged = false;
    static sector_t dummy;      // Used when skipping.
    static plane_t* dummyPlaneArray[2];
    static plane_t dummyPlanes[2];

    // Set up the dummy.
    dummyPlaneArray[0] = &dummyPlanes[0];
    dummyPlaneArray[1] = &dummyPlanes[1];
    dummy.planes = dummyPlaneArray;

    // Sector index number.
    num = Msg_ReadShort();

    // Flags.
    if(deltaType == DT_SECTOR_R6)
    {
        // The R6 protocol reserves two bytes for the flags.
        df = Msg_ReadShort();
    }
    else
    {
        df = Msg_ReadPackedLong();
    }

    if(!skip)
    {
#ifdef _DEBUG
if(num >= numsectors)
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

    if(df & SDF_FLOORPIC)
        sec->SP_floormaterial =
            R_GetMaterial(Msg_ReadPackedShort(), MAT_FLAT);
    if(df & SDF_CEILINGPIC)
        sec->SP_ceilmaterial =
            R_GetMaterial(Msg_ReadPackedShort(), MAT_FLAT);
    if(df & SDF_LIGHT)
        sec->lightLevel = Msg_ReadByte() / 255.0f;
    if(df & SDF_FLOOR_HEIGHT)
    {
        sec->planes[PLN_FLOOR]->height = FIX2FLT(Msg_ReadShort() << 16);
        wasChanged = true;

        if(!skip)
        {
            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Absolute floor height=%f\n",
                                num, sec->SP_floorheight) );
        }
    }
    if(df & SDF_CEILING_HEIGHT)
    {
        fixed_t height = Msg_ReadShort() << 16;
        if(!skip)
        {
            sec->planes[PLN_CEILING]->height = FIX2FLT(height);
            wasChanged = true;

            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Absolute ceiling height=%f%s\n",
                                num, sec->SP_ceilheight, skip? " --SKIPPED!--" : "") );
        }
    }
    if(df & SDF_FLOOR_TARGET)
    {
        fixed_t height = Msg_ReadShort() << 16;
        if(!skip)
        {
            sec->planes[PLN_FLOOR]->target = FIX2FLT(height);

            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Floor target=%f\n",
                                num, sec->planes[PLN_FLOOR]->target) );
        }
    }
    if(df & SDF_FLOOR_SPEED)
    {
        fixed_t speed = Msg_ReadByte();
        if(!skip)
        {
            sec->planes[PLN_FLOOR]->speed =
                FIX2FLT(speed << (df & SDF_FLOOR_SPEED_44 ? 12 : 15));

            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Floor speed=%f\n",
                                num, sec->planes[PLN_FLOOR]->speed) );
        }
    }
    if(df & SDF_FLOOR_TEXMOVE)
    {   // Old clients might include these.
        fixed_t moveX = Msg_ReadShort() << 8;
        fixed_t moveY = Msg_ReadShort() << 8;
    }
    if(df & SDF_CEILING_TARGET)
    {
        fixed_t target = Msg_ReadShort() << 16;
        if(!skip)
        {
            sec->planes[PLN_CEILING]->target = FIX2FLT(target);

            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Ceiling target=%f\n",
                                num, sec->planes[PLN_CEILING]->target) );
        }
    }
    if(df & SDF_CEILING_SPEED)
    {
        byte speed = Msg_ReadByte();
        if(!skip)
        {
            sec->planes[PLN_CEILING]->speed =
                FIX2FLT(speed << (df & SDF_CEILING_SPEED_44 ? 12 : 15));

            VERBOSE( Con_Printf("Cl_ReadSectorDelta2: (%i) Ceiling speed=%f\n",
                                num, sec->planes[PLN_CEILING]->speed) );
        }
    }
    if(df & SDF_CEILING_TEXMOVE)
    {   // Old clients might include these.
        fixed_t moveX = Msg_ReadShort() << 8;
        fixed_t moveY = Msg_ReadShort() << 8;
    }
    if(df & SDF_COLOR_RED)
        sec->rgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_COLOR_GREEN)
        sec->rgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_COLOR_BLUE)
        sec->rgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_FLOOR_COLOR_RED)
        sec->SP_floorrgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_COLOR_GREEN)
        sec->SP_floorrgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_COLOR_BLUE)
        sec->SP_floorrgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_CEIL_COLOR_RED)
        sec->SP_ceilrgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_COLOR_GREEN)
        sec->SP_ceilrgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_COLOR_BLUE)
        sec->SP_ceilrgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_FLOOR_GLOW_RED)
        sec->planes[PLN_FLOOR]->glowRGB[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_GLOW_GREEN)
        sec->planes[PLN_FLOOR]->glowRGB[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_GLOW_BLUE)
        sec->planes[PLN_FLOOR]->glowRGB[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_CEIL_GLOW_RED)
        sec->planes[PLN_CEILING]->glowRGB[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_GLOW_GREEN)
        sec->planes[PLN_CEILING]->glowRGB[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_GLOW_BLUE)
        sec->planes[PLN_CEILING]->glowRGB[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_FLOOR_GLOW)
        sec->planes[PLN_FLOOR]->glow = (float) Msg_ReadShort() / DDMAXSHORT;
    if(df & SDF_CEIL_GLOW)
        sec->planes[PLN_CEILING]->glow = (float) Msg_ReadShort() / DDMAXSHORT;

    // The whole delta has been read. If we're about to skip, let's do so.
    if(skip)
        return;

    // If the plane heights were changed, we need to update the mobjs in
    // the sector.
    if(wasChanged)
    {
        P_SectorPlanesChanged(sec);
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
    unsigned short num;
    int         df, toptexture = 0, midtexture = 0, bottomtexture = 0;
    int         blendmode = 0;
    byte        lineFlags = 0, sideFlags = 0;
    float       toprgb[3] = {0,0,0}, midrgba[4] = {0,0,0,0};
    float       bottomrgb[3] = {0,0,0};
    side_t     *sid;

    // First read all the data.
    num = Msg_ReadShort();

    // Flags.
    if(deltaType == DT_SIDE_R6)
    {
        // The R6 protocol reserves a single byte for a side delta.
        df = Msg_ReadByte();
    }
    else
    {
        df = Msg_ReadPackedLong();
    }

    if(df & SIDF_TOPTEX)
        toptexture = Msg_ReadPackedShort();
    if(df & SIDF_MIDTEX)
        midtexture = Msg_ReadPackedShort();
    if(df & SIDF_BOTTOMTEX)
        bottomtexture = Msg_ReadPackedShort();
    if(df & SIDF_LINE_FLAGS)
        lineFlags = Msg_ReadByte();

    if(df & SIDF_TOP_COLOR_RED)
        toprgb[0] = Msg_ReadByte();
    if(df & SIDF_TOP_COLOR_GREEN)
        toprgb[1] = Msg_ReadByte();
    if(df & SIDF_TOP_COLOR_BLUE)
        toprgb[2] = Msg_ReadByte();

    if(df & SIDF_MID_COLOR_RED)
        midrgba[0] = Msg_ReadByte();
    if(df & SIDF_MID_COLOR_GREEN)
        midrgba[1] = Msg_ReadByte();
    if(df & SIDF_MID_COLOR_BLUE)
        midrgba[2] = Msg_ReadByte();
    if(df & SIDF_MID_COLOR_ALPHA)
        midrgba[3] = Msg_ReadByte();

    if(df & SIDF_BOTTOM_COLOR_RED)
        bottomrgb[0] = Msg_ReadByte();
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        bottomrgb[1] = Msg_ReadByte();
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        bottomrgb[2] = Msg_ReadByte();

    if(df & SIDF_MID_BLENDMODE)
        blendmode = Msg_ReadShort() << 16;

    if(df & SIDF_FLAGS)
        sideFlags = Msg_ReadByte();

    // Must we skip this?
    if(skip)
        return;

#ifdef _DEBUG
if(num >= numsides)
{
    // This is worrisome.
    Con_Error("Cl_ReadSideDelta2: Side %i out of range.\n", num);
}
#endif

    sid = SIDE_PTR(num);

    if(df & SIDF_TOPTEX)
        sid->SW_topmaterial = R_GetMaterial(toptexture, MAT_TEXTURE);
    if(df & SIDF_MIDTEX)
        sid->SW_middlematerial = R_GetMaterial(midtexture, MAT_TEXTURE);
    if(df & SIDF_BOTTOMTEX)
        sid->SW_bottommaterial = R_GetMaterial(bottomtexture, MAT_TEXTURE);

    if(df & SIDF_TOP_COLOR_RED)
        sid->SW_toprgba[0] = toprgb[0];
    if(df & SIDF_TOP_COLOR_GREEN)
        sid->SW_toprgba[1] = toprgb[1];
    if(df & SIDF_TOP_COLOR_BLUE)
        sid->SW_toprgba[2] = toprgb[2];

    if(df & SIDF_MID_COLOR_RED)
        sid->SW_middlergba[0] = midrgba[0];
    if(df & SIDF_MID_COLOR_GREEN)
        sid->SW_middlergba[1] = midrgba[1];
    if(df & SIDF_MID_COLOR_BLUE)
        sid->SW_middlergba[2] = midrgba[2];
    if(df & SIDF_MID_COLOR_ALPHA)
        sid->SW_middlergba[3] = midrgba[3];

    if(df & SIDF_BOTTOM_COLOR_RED)
        sid->SW_bottomrgba[0] = bottomrgb[0];
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        sid->SW_bottomrgba[1] = bottomrgb[1];
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        sid->SW_bottomrgba[2] = bottomrgb[2];

    if(df & SIDF_MID_BLENDMODE)
        sid->SW_middleblendmode = blendmode;

    if(df & SIDF_FLAGS)
    {
        // The delta includes the entire lowest byte.
        sid->flags &= ~0xff;
        sid->flags |= sideFlags;
    }

    if(df & SIDF_LINE_FLAGS)
    {
        line_t *line = R_GetLineForSide(num);

        if(line)
        {
            // The delta includes the entire lowest byte.
            line->mapFlags &= ~0xff;
            line->mapFlags |= lineFlags;
#if _DEBUG
Con_Printf("Cl_ReadSideDelta2: Lineflag %i: %02x\n",
           GET_LINE_IDX(line), lineFlags);
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
    int         df;
    unsigned short num;
    polyobj_t  *po;
    float       destX = 0, destY = 0;
    float       speed = 0;
    int         destAngle = 0, angleSpeed = 0;

    num = Msg_ReadPackedShort();

    // Flags.
    df = Msg_ReadByte();

    if(df & PODF_DEST_X)
        destX = FIX2FLT((Msg_ReadShort() << 16) + ((char) Msg_ReadByte() << 8));
    if(df & PODF_DEST_Y)
        destY = FIX2FLT((Msg_ReadShort() << 16) + ((char) Msg_ReadByte() << 8));
    if(df & PODF_SPEED)
        speed = FIX2FLT(Msg_ReadShort() << 8);
    if(df & PODF_DEST_ANGLE)
        destAngle = Msg_ReadShort() << 16;
    if(df & PODF_ANGSPEED)
        angleSpeed = Msg_ReadShort() << 16;

    if(skip)
        return;

#ifdef _DEBUG
if(num >= numpolyobjs)
{
    // This is worrisome.
    Con_Error("Cl_ReadPolyDelta2: PO %i out of range.\n", num);
}
#endif

    po = polyobjs[num];

    if(df & PODF_DEST_X)
        po->dest.pos[VX] = destX;
    if(df & PODF_DEST_Y)
        po->dest.pos[VY] = destY;
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
