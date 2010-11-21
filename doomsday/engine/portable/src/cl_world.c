/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

static mover_t* activemovers[MAX_MOVERS];
static polymover_t* activepolys[MAX_MOVERS];
static short* xlat_lump;

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
    xlat_lump = Z_Malloc(sizeof(short) * MAX_TRANSLATIONS, PU_REFRESHTEX, 0);
    memset(xlat_lump, 0, sizeof(short) * MAX_TRANSLATIONS);
    { int i, numLumps = W_NumLumps();
    for(i = 0; i < numLumps; ++i)
        xlat_lump[i] = i; // Identity translation.
    }
}

void Cl_SetLumpTranslation(lumpnum_t lump, char *name)
{
    if(lump < 0 || lump >= MAX_TRANSLATIONS)
        return; // Can't do it, sir! We just don't the power!!

    xlat_lump[lump] = W_CheckNumForName(name);
    if(xlat_lump[lump] < 0)
    {
        VERBOSE(Con_Message("Cl_SetLumpTranslation: %s not found.\n", name));
        xlat_lump[lump] = 0;
    }
}

/**
 * This is a fail-safe operation.
 */
lumpnum_t Cl_TranslateLump(lumpnum_t lump)
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
    int                 i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activemovers[i] == mover)
        {
            P_ThinkerRemove(&mover->thinker);
            activemovers[i] = NULL;
            break;
        }
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
            activepolys[i] = NULL;
            break;
        }
}

/**
 * Plane mover.
 */
void Cl_MoverThinker(mover_t *mover)
{
    float              *current = mover->current, original = *current;
    boolean             remove = false;
    boolean             freeMove;
    float               fspeed;

    if(!Cl_GameReady())
        return; // Can we think yet?

    // The move is cancelled if the consolePlayer becomes obstructed.
    freeMove = Cl_IsFreeToMove(consolePlayer);
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
    if(freeMove != Cl_IsFreeToMove(consolePlayer))
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
    sector_t           *sector;
    int                 i;
    mover_t            *mov;

    VERBOSE( Con_Printf("Cl_AddMover: Sector=%i, type=%s, dest=%f, speed=%f\n",
                        sectornum, type==MVT_FLOOR? "floor" : "ceiling",
                        dest, speed) );

    if(speed == 0)
        return;

    if(sectornum >= numSectors)
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
            mov = activemovers[i] = Z_Malloc(sizeof(mover_t), PU_MAP, 0);
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

            // \fixme Do these need to be public?
            P_ThinkerAdd(&mov->thinker, true);
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
        P_PolyobjMove(P_GetPolyobj(mover->number | 0x80000000), dx, dy);
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

        P_PolyobjRotate(P_GetPolyobj(mover->number | 0x80000000), FLT2FIX(dist));
    }

    // Can we get rid of this mover?
    if(!mover->move && !mover->rotate)
        Cl_RemoveActivePoly(mover);
}

polymover_t* Cl_FindActivePoly(uint number)
{
    uint                i;

    for(i = 0; i < MAX_MOVERS; ++i)
        if(activepolys[i] && activepolys[i]->number == number)
            return activepolys[i];
    return NULL;
}

polymover_t* Cl_NewPolyMover(uint number)
{
    polymover_t*        mover;
    polyobj_t*          poly = polyObjs[number];

    mover = Z_Malloc(sizeof(polymover_t), PU_MAP, 0);
    memset(mover, 0, sizeof(*mover));
    mover->thinker.function = Cl_PolyMoverThinker;
    mover->poly = poly;
    mover->number = number;
    // \fixme Do these need to be public?
    P_ThinkerAdd(&mover->thinker, true);
    return mover;
}

void Cl_SetPolyMover(uint number, int move, int rotate)
{
    polymover_t*        mover;

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
    int                 i;

    for(i = 0; i < MAX_MOVERS; ++i)
    {
        if(activemovers[i])
        {
            P_ThinkerRemove(&activemovers[i]->thinker);
            activemovers[i] = NULL;
        }
        if(activepolys[i])
        {
            P_ThinkerRemove(&activepolys[i]->thinker);
            activepolys[i] = NULL;
        }
    }
}

mover_t *Cl_GetActiveMover(uint sectornum, clmovertype_t type)
{
    int                 i;

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
    lumpnum_t           num = (lumpnum_t) Msg_ReadPackedShort();
    char                name[9];

    if(!num)
        return false; // No more.

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
        material_t*         mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(Msg_ReadPackedShort());
        Surface_SetMaterial(&sec->SP_floorsurface, mat);
    }
    if(df & SDF_CEILING_MATERIAL)
    {
        material_t*         mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(Msg_ReadPackedShort());
        Surface_SetMaterial(&sec->SP_ceilsurface, mat);
    }

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
        Surface_SetColorR(&sec->SP_floorsurface, Msg_ReadByte() / 255.f);
    if(df & SDF_FLOOR_COLOR_GREEN)
        Surface_SetColorG(&sec->SP_floorsurface, Msg_ReadByte() / 255.f);
    if(df & SDF_FLOOR_COLOR_BLUE)
        Surface_SetColorB(&sec->SP_floorsurface, Msg_ReadByte() / 255.f);

    if(df & SDF_CEIL_COLOR_RED)
        Surface_SetColorR(&sec->SP_ceilsurface, Msg_ReadByte() / 255.f);
    if(df & SDF_CEIL_COLOR_GREEN)
        Surface_SetColorG(&sec->SP_ceilsurface, Msg_ReadByte() / 255.f);
    if(df & SDF_CEIL_COLOR_BLUE)
        Surface_SetColorB(&sec->SP_ceilsurface, Msg_ReadByte() / 255.f);

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
    unsigned short      num;

    int                 df, topMat = 0, midMat = 0, botMat = 0;
    int                 blendmode = 0;
    byte                lineFlags = 0, sideFlags = 0;
    float               toprgb[3] = {0,0,0}, midrgba[4] = {0,0,0,0};
    float               bottomrgb[3] = {0,0,0};
    sidedef_t          *sid;

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

    if(df & SIDF_TOP_MATERIAL)
        topMat = Msg_ReadPackedShort();
    if(df & SIDF_MID_MATERIAL)
        midMat = Msg_ReadPackedShort();
    if(df & SIDF_BOTTOM_MATERIAL)
        botMat = Msg_ReadPackedShort();
    if(df & SIDF_LINE_FLAGS)
        lineFlags = Msg_ReadByte();

    if(df & SIDF_TOP_COLOR_RED)
        toprgb[CR] = Msg_ReadByte() / 255.f;
    if(df & SIDF_TOP_COLOR_GREEN)
        toprgb[CG] = Msg_ReadByte() / 255.f;
    if(df & SIDF_TOP_COLOR_BLUE)
        toprgb[CB] = Msg_ReadByte() / 255.f;

    if(df & SIDF_MID_COLOR_RED)
        midrgba[CR] = Msg_ReadByte() / 255.f;
    if(df & SIDF_MID_COLOR_GREEN)
        midrgba[CG] = Msg_ReadByte() / 255.f;
    if(df & SIDF_MID_COLOR_BLUE)
        midrgba[CB] = Msg_ReadByte() / 255.f;
    if(df & SIDF_MID_COLOR_ALPHA)
        midrgba[CA] = Msg_ReadByte() / 255.f;

    if(df & SIDF_BOTTOM_COLOR_RED)
        bottomrgb[CR] = Msg_ReadByte() / 255.f;
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        bottomrgb[CG] = Msg_ReadByte() / 255.f;
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        bottomrgb[CB] = Msg_ReadByte() / 255.f;

    if(df & SIDF_MID_BLENDMODE)
        blendmode = Msg_ReadShort() << 16;

    if(df & SIDF_FLAGS)
        sideFlags = Msg_ReadByte();

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
        material_t*         mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(topMat);
        Surface_SetMaterial(&sid->SW_topsurface, mat);
    }
    if(df & SIDF_MID_MATERIAL)
    {
        material_t*         mat;
        /**
         * The delta is a server-side materialnum.
         * \fixme What if client and server materialnums differ?
         */
        mat = Materials_ToMaterial(midMat);
        Surface_SetMaterial(&sid->SW_middlesurface, mat);
    }
    if(df & SIDF_BOTTOM_MATERIAL)
    {
        material_t*         mat;
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
