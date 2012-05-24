/**\file p_floors.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * Moving floors.
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_tick.h"
#include "p_floor.h"
#if __JHEXEN__ || __JDOOM64__
#include "p_ceiling.h"
#endif
#include "p_sound.h"

// MACROS ------------------------------------------------------------------

#if __JHERETIC__
# define SFX_FLOORMOVE          (SFX_DORMOV)
#else
# define SFX_FLOORMOVE          (SFX_STNMOV)
#endif

#if __JHEXEN__
#define STAIR_SECTOR_TYPE       26
#define STAIR_QUEUE_SIZE        32
#endif

// TYPES -------------------------------------------------------------------

#if __JHEXEN__
typedef struct stairqueue_s {
    Sector*         sector;
    int             type;
    coord_t         height;
} stairqueue_t;

// Global vars for stair building, in a struct for neatness.
typedef struct stairdata_s {
    coord_t         stepDelta;
    int             direction;
    float           speed;
    material_t*     material;
    int             startDelay;
    int             startDelayDelta;
    int             textureChange;
    coord_t         startHeight;
} stairdata_t;
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

#if __JHEXEN__
static void enqueueStairSector(Sector* sec, int type, coord_t height);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHEXEN__
stairdata_t stairData;
stairqueue_t stairQueue[STAIR_QUEUE_SIZE];
static int stairQueueHead;
static int stairQueueTail;
#endif

// CODE --------------------------------------------------------------------

/**
 * Move a plane (floor or ceiling) and check for crushing.
 */
result_e T_MovePlane(Sector* sector, float speed, coord_t dest,
                     int crush, int isCeiling, int direction)
{
    boolean flag;
    coord_t lastpos;
    coord_t floorheight, ceilingheight;
    int ptarget = (isCeiling? DMU_CEILING_TARGET_HEIGHT : DMU_FLOOR_TARGET_HEIGHT);
    int pspeed = (isCeiling? DMU_CEILING_SPEED : DMU_FLOOR_SPEED);

    // Let the engine know about the movement of this plane.
    P_SetDoublep(sector, ptarget, dest);
    P_SetFloatp(sector, pspeed, speed);

    floorheight = P_GetDoublep(sector, DMU_FLOOR_HEIGHT);
    ceilingheight = P_GetDoublep(sector, DMU_CEILING_HEIGHT);

    switch(isCeiling)
    {
    case 0:
        // FLOOR
        switch(direction)
        {
        case -1:
            // DOWN
            if(floorheight - speed < dest)
            {
                // The move is complete.
                lastpos = floorheight;
                P_SetDoublep(sector, DMU_FLOOR_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    // Oh no, the move failed.
                    P_SetDoublep(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetDoublep(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                lastpos = floorheight;
                P_SetDoublep(sector, DMU_FLOOR_HEIGHT, floorheight - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetDoublep(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetDoublep(sector, ptarget, lastpos);
#if __JHEXEN__
                    P_SetFloatp(sector, pspeed, 0);
#endif
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(floorheight + speed > dest)
            {
                // The move is complete.
                lastpos = floorheight;
                P_SetDoublep(sector, DMU_FLOOR_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    // Oh no, the move failed.
                    P_SetDoublep(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetDoublep(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = floorheight;
                P_SetDoublep(sector, DMU_FLOOR_HEIGHT, floorheight + speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
#if !__JHEXEN__
                    if(crush)
                        return crushed;
#endif
                    P_SetDoublep(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetDoublep(sector, ptarget, lastpos);
#if __JHEXEN__
                    P_SetFloatp(sector, pspeed, 0);
#endif
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;

        default:
            break;
        }
        break;

    case 1:
        // CEILING
        switch(direction)
        {
        case -1:
            // DOWN
            if(ceilingheight - speed < dest)
            {
                // The move is complete.
                lastpos = ceilingheight;
                P_SetDoublep(sector, DMU_CEILING_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetDoublep(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetDoublep(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = ceilingheight;
                P_SetDoublep(sector, DMU_CEILING_HEIGHT, ceilingheight - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
#if !__JHEXEN__
                    if(crush)
                        return crushed;
#endif
                    P_SetDoublep(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetDoublep(sector, ptarget, lastpos);
#if __JHEXEN__
                    P_SetFloatp(sector, pspeed, 0);
#endif
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(ceilingheight + speed > dest)
            {
                // The move is complete.
                lastpos = ceilingheight;
                P_SetDoublep(sector, DMU_CEILING_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetDoublep(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetDoublep(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                lastpos = ceilingheight;
                P_SetDoublep(sector, DMU_CEILING_HEIGHT, ceilingheight + speed);
                flag = P_ChangeSector(sector, crush);
            }
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return ok;
}

/**
 * Move a floor to it's destination (up or down).
 */
void T_MoveFloor(floor_t* floor)
{
    result_e            res;

#if __JHEXEN__
    if(floor->resetDelayCount)
    {
        floor->resetDelayCount--;
        if(!floor->resetDelayCount)
        {
            floor->floorDestHeight = floor->resetHeight;
            floor->state = ((floor->state == FS_UP)? FS_DOWN : FS_UP);
            floor->resetDelay = 0;
            floor->delayCount = 0;
            floor->delayTotal = 0;
        }
    }
    if(floor->delayCount)
    {
        floor->delayCount--;
        if(!floor->delayCount && floor->material)
        {
            P_SetPtrp(floor->sector, DMU_FLOOR_MATERIAL, floor->material);
        }
        return;
    }
#endif

    res =
        T_MovePlane(floor->sector, floor->speed, floor->floorDestHeight,
                    floor->crush, 0, floor->state);

#if __JHEXEN__
    if(floor->type == FT_RAISEBUILDSTEP)
    {
        if((floor->state == FS_UP &&
            P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) >= floor->stairsDelayHeight) ||
           (floor->state == FS_DOWN &&
            P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) <= floor->stairsDelayHeight))
        {
            floor->delayCount = floor->delayTotal;
            floor->stairsDelayHeight += floor->stairsDelayHeightDelta;
        }
    }
#endif

#if !__JHEXEN__
    if(!(mapTime & 7))
        S_PlaneSound(P_GetPtrp(floor->sector, DMU_FLOOR_PLANE), SFX_FLOORMOVE);
#endif

    if(res == pastdest)
    {
        xsector_t *xsec = P_ToXSector(floor->sector);
        P_SetFloatp(floor->sector, DMU_FLOOR_SPEED, 0);

#if __JHEXEN__
        SN_StopSequence(P_GetPtrp(floor->sector, DMU_BASE));
#else
#  if __JHERETIC__
        if(floor->type == FT_RAISEBUILDSTEP)
#  endif
            S_PlaneSound(P_GetPtrp(floor->sector, DMU_FLOOR_PLANE), SFX_PSTOP);

#endif
#if __JHEXEN__
        if(floor->delayTotal)
            floor->delayTotal = 0;

        if(floor->resetDelay)
        {
            //          floor->resetDelayCount = floor->resetDelay;
            //          floor->resetDelay = 0;
            return;
        }
#endif
        xsec->specialData = NULL;
#if __JHEXEN__
        if(floor->material)
        {
            P_SetPtrp(floor->sector, DMU_FLOOR_MATERIAL, floor->material);
        }
#else
        if(floor->state == FS_UP)
        {
            switch(floor->type)
            {
            case FT_RAISEDONUT:
                xsec->special = floor->newSpecial;
                P_SetPtrp(floor->sector, DMU_FLOOR_MATERIAL, floor->material);
                break;

            default:
                break;
            }
        }
        else if(floor->state == FS_DOWN)
        {
            switch(floor->type)
            {
            case FT_LOWERANDCHANGE:
                xsec->special = floor->newSpecial;
                P_SetPtrp(floor->sector, DMU_FLOOR_MATERIAL, floor->material);
                break;

            default:
                break;
            }
        }
#endif
#if __JHEXEN__
        P_TagFinished(P_ToXSector(floor->sector)->tag);
#endif
        DD_ThinkerRemove(&floor->thinker);
    }
}

typedef struct findlineinsectorsmallestbottommaterialparams_s {
    Sector             *baseSec;
    int                 minSize;
    LineDef            *foundLine;
} findlineinsectorsmallestbottommaterialparams_t;

int findLineInSectorSmallestBottomMaterial(void *ptr, void *context)
{
    LineDef* li = (LineDef*) ptr;
    findlineinsectorsmallestbottommaterialparams_t* params =
        (findlineinsectorsmallestbottommaterialparams_t*) context;
    Sector* frontSec, *backSec;

    frontSec = P_GetPtrp(li, DMU_FRONT_SECTOR);
    backSec = P_GetPtrp(li, DMU_BACK_SECTOR);

    if(frontSec && backSec)
    {
        SideDef* side;
        material_t* mat;

        side = P_GetPtrp(li, DMU_SIDEDEF0);
        mat = P_GetPtrp(side, DMU_BOTTOM_MATERIAL);

        /**
         * Emulate DOOM.exe behaviour. In the instance where no material is
         * present, the height is taken from the very first texture.
         */
        if(!mat)
            mat = P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, 0));

        if(mat)
        {
            int height = P_GetIntp(mat, DMU_HEIGHT);

            if(height < params->minSize)
            {
                params->minSize = height;
                params->foundLine = li;
            }
        }

        side = P_GetPtrp(li, DMU_SIDEDEF1);
        mat = P_GetPtrp(side, DMU_BOTTOM_MATERIAL);
        if(!mat)
            mat = P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, 0));

        if(mat)
        {
            int height = P_GetIntp(mat, DMU_HEIGHT);

            if(height < params->minSize)
            {
                params->minSize = height;
                params->foundLine = li;
            }
        }
    }

    return false; // Continue iteration.
}

LineDef* P_FindLineInSectorSmallestBottomMaterial(Sector *sec, int *val)
{
    findlineinsectorsmallestbottommaterialparams_t params;

    params.baseSec = sec;
    params.minSize = DDMAXINT;
    params.foundLine = NULL;
    P_Iteratep(sec, DMU_LINEDEF, &params,
               findLineInSectorSmallestBottomMaterial);

    if(val)
        *val = params.minSize;

    return params.foundLine;
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
/**
 * Find the first sector which shares a border to the specified sector
 * and whose floor height matches that specified.
 *
 * @note Behaviour here is dependant upon the order of the sector-linked
 * LineDefs list. This is necessary to emulate the flawed algorithm used in
 * DOOM.exe In addition, this algorithm was further broken in Heretic as the
 * test which compares floor heights was removed.
 *
 * @warning DO NOT USE THIS ANYWHERE ELSE!
 */

typedef struct findfirstneighbouratfloorheightparams_s {
    Sector* baseSec;
    coord_t height;
    Sector* foundSec;
} findfirstneighbouratfloorheightparams_t;

static int findFirstNeighbourAtFloorHeight(void* ptr, void* context)
{
    LineDef* ln = (LineDef*) ptr;
    findfirstneighbouratfloorheightparams_t* params =
        (findfirstneighbouratfloorheightparams_t*) context;
    Sector* other;

    other = P_GetNextSector(ln, params->baseSec);
# if __JDOOM__ || __JDOOM64__
    if(other && FEQUAL(P_GetDoublep(other, DMU_FLOOR_HEIGHT), params->height))
# elif __JHERETIC__
    if(other)
# endif
    {
        params->foundSec = other;
        return true; // Stop iteration.
    }

    return false; // Continue iteration.
}

static Sector* findSectorSurroundingAtFloorHeight(Sector* sec, coord_t height)
{
    findfirstneighbouratfloorheightparams_t params;

    params.baseSec = sec;
    params.foundSec = NULL;
    params.height = height;
    P_Iteratep(sec, DMU_LINEDEF, &params, findFirstNeighbourAtFloorHeight);
    return params.foundSec;
}
#endif

/**
 * Handle moving floors.
 */
#if __JHEXEN__
int EV_DoFloor(LineDef* line, byte* args, floortype_e floortype)
#else
int EV_DoFloor(LineDef* line, floortype_e floortype)
#endif
{
#if !__JHEXEN__
    Sector* frontsector;
#endif
    int rtn = 0;
    xsector_t* xsec;
    Sector* sec = NULL;
    floor_t* floor = NULL;
    iterlist_t* list;
#if __JHEXEN__
    int tag = (int) args[0];
#else
    int tag = P_ToXLine(line)->tag;
#endif

#if __JDOOM64__
    // jd64 > bitmip? wha?
    coord_t bitmipL = 0, bitmipR = 0;
    SideDef *front = P_GetPtrp(line, DMU_SIDEDEF0);
    SideDef *back  = P_GetPtrp(line, DMU_SIDEDEF1);

    bitmipL = P_GetDoublep(front, DMU_MIDDLE_MATERIAL_OFFSET_X);
    if(back)
        bitmipR = P_GetDoublep(back, DMU_MIDDLE_MATERIAL_OFFSET_X);
    // < d64tc
#endif

    list = P_GetSectorIterListForTag(tag, false);
    if(!list)
        return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        xsec = P_ToXSector(sec);
        // If already moving, keep going...
        if(xsec->specialData)
            continue;
        rtn = 1;

        // New floor thinker.
        floor = Z_Calloc(sizeof(*floor), PU_MAP, 0);
        floor->thinker.function = T_MoveFloor;
        DD_ThinkerAdd(&floor->thinker);
        xsec->specialData = floor;

        floor->type = floortype;
        floor->crush = false;
#if __JHEXEN__
        floor->speed = (float) args[1] * (1.0 / 8);
        if(floortype == FT_LOWERMUL8INSTANT ||
           floortype == FT_RAISEMUL8INSTANT)
        {
            floor->speed = 2000;
        }
#endif
        switch(floortype)
        {
        case FT_LOWER:
            floor->state = FS_DOWN;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __JDOOM64__
            floor->speed *= 4;
# endif
#endif
            P_FindSectorSurroundingHighestFloor(sec, -500, &floor->floorDestHeight);
            break;

        case FT_LOWERTOLOWEST:
            floor->state = FS_DOWN;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __JDOOM64__
            floor->speed *= 4;
# endif
#endif
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &floor->floorDestHeight);
            break;
#if __JHEXEN__
        case FT_LOWERBYVALUE:
            floor->state = FS_DOWN;
            floor->sector = sec;
            floor->floorDestHeight =
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT) - (coord_t) args[2];
            break;

        case FT_LOWERMUL8INSTANT:
        case FT_LOWERBYVALUEMUL8:
            floor->state = FS_DOWN;
            floor->sector = sec;
            floor->floorDestHeight =
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT) - (coord_t) args[2] * 8;
            break;
#endif
#if !__JHEXEN__
        case FT_LOWERTURBO:
            floor->state = FS_DOWN;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 4;
            P_FindSectorSurroundingHighestFloor(sec, -500, &floor->floorDestHeight);
# if __JHERETIC__
            floor->floorDestHeight += 8;
# else
            if(!FEQUAL(floor->floorDestHeight, P_GetDoublep(sec, DMU_FLOOR_HEIGHT)))
                floor->floorDestHeight += 8;
# endif
            break;
#endif
#if __JDOOM64__
        case FT_TOHIGHESTPLUS8: // jd64
            floor->state = FS_DOWN;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            P_FindSectorSurroundingHighestFloor(sec, -500, &floor->floorDestHeight);
            if(!FEQUAL(floor->floorDestHeight, P_GetDoublep(sec, DMU_FLOOR_HEIGHT)))
                floor->floorDestHeight += 8;
            break;

        case FT_TOHIGHESTPLUSBITMIP: // jd64
            if(bitmipR > 0)
            {
                floor->state = FS_DOWN;
                floor->sector = sec;
                floor->speed = FLOORSPEED * bitmipL;
                P_FindSectorSurroundingHighestFloor(sec, -500, &floor->floorDestHeight);

                if(!FEQUAL(floor->floorDestHeight, P_GetDoublep(sec, DMU_FLOOR_HEIGHT)))
                    floor->floorDestHeight += bitmipR;
            }
            else
            {
                floor->state = FS_UP;
                floor->sector = sec;
                floor->speed = FLOORSPEED * bitmipL;
                floor->floorDestHeight =
                    P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) - bitmipR;
            }
            break;

        case FT_CUSTOMCHANGESEC: // jd64
            floor->state = FS_UP;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 16;
            floor->floorDestHeight = P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT);

            //// \kludge fake the engine into accepting this special
            P_ToXSector(sec)->special = bitmipR;
            // < KLUDGE
            break;
#endif
        case FT_RAISEFLOORCRUSH:
#if __JHEXEN__
            floor->crush = (int) args[2]; // arg[2] = crushing value
#else
            floor->crush = true;
#endif
            floor->state = FS_UP;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __JDOOM64__
            floor->speed *= 4;
# endif
#endif
#if __JHEXEN__
            floor->floorDestHeight = P_GetDoublep(sec, DMU_CEILING_HEIGHT)-8;
#else
            P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &floor->floorDestHeight);

            if(floor->floorDestHeight > P_GetDoublep(sec, DMU_CEILING_HEIGHT))
                floor->floorDestHeight = P_GetDoublep(sec, DMU_CEILING_HEIGHT);

            floor->floorDestHeight -= 8 * (floortype == FT_RAISEFLOORCRUSH);
#endif
            break;

        case FT_RAISEFLOOR:
            floor->state = FS_UP;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __JDOOM64__
            floor->speed *= 4;
# endif
#endif
            P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &floor->floorDestHeight);

            if(floor->floorDestHeight > P_GetDoublep(sec, DMU_CEILING_HEIGHT))
                floor->floorDestHeight = P_GetDoublep(sec, DMU_CEILING_HEIGHT);

#if !__JHEXEN__
            floor->floorDestHeight -= 8 * (floortype == FT_RAISEFLOORCRUSH);
#endif
            break;

#if __JDOOM__ || __JDOOM64__
        case FT_RAISEFLOORTURBO:
            floor->state = FS_UP;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 4;
# if __JDOOM64__
            floor->speed *= 2;
# endif
            {
            coord_t floorHeight, nextFloor;

            floorHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);
            if(P_FindSectorSurroundingNextHighestFloor(sec, floorHeight, &nextFloor))
                floor->floorDestHeight = nextFloor;
            else
                floor->floorDestHeight = floorHeight;
            }
            break;
#endif

        case FT_RAISEFLOORTONEAREST:
            floor->state = FS_UP;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __JDOOM64__
            floor->speed *= 8;
# endif
#endif
            {
            coord_t floorHeight, nextFloor;

            floorHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);
            if(P_FindSectorSurroundingNextHighestFloor(sec, floorHeight, &nextFloor))
                floor->floorDestHeight = nextFloor;
            else
                floor->floorDestHeight = floorHeight;
            }
            break;

#if __JHEXEN__
        case FT_RAISEFLOORBYVALUE:
            floor->state = FS_UP;
            floor->sector = sec;
            floor->floorDestHeight =
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + (coord_t) args[2];
            break;

        case FT_RAISEMUL8INSTANT:
        case FT_RAISEBYVALUEMUL8:
            floor->state = FS_UP;
            floor->sector = sec;
            floor->floorDestHeight =
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + (coord_t) args[2] * 8;
            break;

        case FT_TOVALUEMUL8:
            floor->sector = sec;
            floor->floorDestHeight = (coord_t) args[2] * 8;
            if(args[3])
                floor->floorDestHeight = -floor->floorDestHeight;

            if(floor->floorDestHeight > P_GetDoublep(sec, DMU_FLOOR_HEIGHT))
                floor->state = FS_UP;
            else if(floor->floorDestHeight < P_GetDoublep(sec, DMU_FLOOR_HEIGHT))
                floor->state = FS_DOWN;
            else
                rtn = 0; // Already at lowest position.

            break;
#endif
#if !__JHEXEN__
        case FT_RAISE24:
            floor->state = FS_UP;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
# if __JDOOM64__
            floor->speed *= 8;
# endif
            floor->floorDestHeight =
                P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) + 24;
            break;
#endif
#if !__JHEXEN__
        case FT_RAISE24ANDCHANGE:
            floor->state = FS_UP;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
# if __JDOOM64__
            floor->speed *= 8;
# endif
            floor->floorDestHeight =
                P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) + 24;

            frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

            P_SetPtrp(sec, DMU_FLOOR_MATERIAL,
                      P_GetPtrp(frontsector, DMU_FLOOR_MATERIAL));

            xsec->special = P_ToXSector(frontsector)->special;
            break;

#if __JDOOM__ || __JDOOM64__
        case FT_RAISE512:
            floor->state = FS_UP;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floorDestHeight =
                P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) + 512;
            break;
#endif

# if __JDOOM64__
        case FT_RAISE32: // jd64
            floor->state = FS_UP;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 8;
            floor->floorDestHeight =
                P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) + 32;
            break;
# endif
        case FT_RAISETOTEXTURE:
            {
            int             minSize = DDMAXINT;

            floor->state = FS_UP;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            P_FindLineInSectorSmallestBottomMaterial(sec, &minSize);
            floor->floorDestHeight =
                P_GetDoublep(floor->sector, DMU_FLOOR_HEIGHT) +
                    (coord_t) minSize;
            }
            break;

        case FT_LOWERANDCHANGE:
            floor->state = FS_DOWN;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &floor->floorDestHeight);
            floor->material = P_GetPtrp(sec, DMU_FLOOR_MATERIAL);

            {
            Sector* otherSec = findSectorSurroundingAtFloorHeight(sec,
                floor->floorDestHeight);

            if(otherSec)
            {
                floor->material = P_GetPtrp(otherSec, DMU_FLOOR_MATERIAL);
                floor->newSpecial = P_ToXSector(otherSec)->special;
            }
            }
            break;
#endif
        default:
#if __JHEXEN__
            rtn = 0;
#endif
            break;
        }
    }

#if __JHEXEN__
    if(rtn && floor)
    {
        SN_StartSequence(P_GetPtrp(floor->sector, DMU_BASE),
                         SEQ_PLATFORM + P_ToXSector(floor->sector)->seqType);
    }
#endif
    return rtn;
}

#if __JHEXEN__
typedef struct {
    int type;
    coord_t height;
} findsectorneighborsforstairbuildparams_t;

static int findSectorNeighborsForStairBuild(void* ptr, void* context)
{
    LineDef* li = (LineDef*) ptr;
    findsectorneighborsforstairbuildparams_t* params =
        (findsectorneighborsforstairbuildparams_t*) context;
    Sector* frontSec, *backSec;
    xsector_t* xsec;

    frontSec = P_GetPtrp(li, DMU_FRONT_SECTOR);
    if(!frontSec)
        return false; // Continue iteration.

    backSec = P_GetPtrp(li, DMU_BACK_SECTOR);
    if(!backSec)
        return false; // Continue iteration.

    xsec = P_ToXSector(frontSec);
    if(xsec->special == params->type + STAIR_SECTOR_TYPE && !xsec->specialData &&
       P_GetPtrp(frontSec, DMU_FLOOR_MATERIAL) == stairData.material &&
       P_GetIntp(frontSec, DMU_VALID_COUNT) != VALIDCOUNT)
    {
        enqueueStairSector(frontSec, params->type ^ 1, params->height);
        P_SetIntp(frontSec, DMU_VALID_COUNT, VALIDCOUNT);
    }

    xsec = P_ToXSector(backSec);
    if(xsec->special == params->type + STAIR_SECTOR_TYPE && !xsec->specialData &&
       P_GetPtrp(backSec, DMU_FLOOR_MATERIAL) == stairData.material &&
       P_GetIntp(backSec, DMU_VALID_COUNT) != VALIDCOUNT)
    {
        enqueueStairSector(backSec, params->type ^ 1, params->height);
        P_SetIntp(backSec, DMU_VALID_COUNT, VALIDCOUNT);
    }

    return false; // Continue iteration.
}
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
/**
 * @note Behaviour here is dependant upon the order of the sector-linked
 * LineDefs list. This is necessary to emulate the flawed algorithm used in
 * DOOM.exe In addition, this algorithm was further broken in Heretic as the
 * test which compares floor heights was removed.
 *
 * @warning DO NOT USE THIS ANYWHERE ELSE!
 */
typedef struct spreadsectorparams_s {
    Sector* baseSec;
    material_t* material;
    Sector* foundSec;
    coord_t height, stairSize;
} spreadsectorparams_t;

int findAdjacentSectorForSpread(void* ptr, void* context)
{
    LineDef* li = (LineDef*) ptr;
    spreadsectorparams_t* params = (spreadsectorparams_t*) context;
    Sector* frontSec, *backSec;
    xsector_t* xsec;

    frontSec = P_GetPtrp(li, DMU_FRONT_SECTOR);
    if(!frontSec)
        return false; // Continue iteration.

    if(params->baseSec != frontSec)
        return false; // Continue iteration.

    backSec = P_GetPtrp(li, DMU_BACK_SECTOR);
    if(!backSec)
        return false; // Continue iteration.

    if(P_GetPtrp(backSec, DMU_FLOOR_MATERIAL) != params->material)
        return false; // Continue iteration.

    /**
     * @note The placement of this step height addition is vital to ensure
     * the exact behaviour of the original DOOM algorithm. Logically this
     * should occur after the test below...
     */
    params->height += params->stairSize;

    xsec = P_ToXSector(backSec);
    if(xsec->specialData)
        return false; // Continue iteration.

    // This looks good.
    params->foundSec = backSec;
    return true; // Stop iteration.
}
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
int EV_BuildStairs(LineDef* line, stair_e type)
{
    int rtn = 0;
    xsector_t* xsec;
    Sector* sec = NULL;
    floor_t* floor;
    coord_t height = 0, stairsize = 0;
    float speed = 0;
    iterlist_t* list;
    spreadsectorparams_t params;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        xsec = P_ToXSector(sec);

        // Already moving? If so, keep going...
        if(xsec->specialData)
            continue;

        // New floor thinker.
        rtn = 1;
        floor = Z_Calloc(sizeof(*floor), PU_MAP, 0);
        floor->thinker.function = T_MoveFloor;
        DD_ThinkerAdd(&floor->thinker);

        xsec->specialData = floor;
        floor->state = FS_UP;
        floor->sector = sec;
        switch(type)
        {
#if __JHERETIC__
        case build8:
            stairsize = 8;
            break;
        case build16:
            stairsize = 16;
            break;
#else
        case build8:
            speed = FLOORSPEED * .25;
            stairsize = 8;
            break;
        case turbo16:
            speed = FLOORSPEED * 4;
            stairsize = 16;
            break;
#endif
        default:
            break;
        }
#if __JHERETIC__
        floor->type = FT_RAISEBUILDSTEP;
        speed = FLOORSPEED;
        floor->speed = speed;
#else
        floor->speed = speed;
#endif
        height = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + stairsize;
        floor->floorDestHeight = height;

        // Find next sector to raise.
        // 1. Find 2-sided line with a front side in the same sector.
        // 2. Other side is the next sector to raise.
        params.baseSec = sec;
        params.material = P_GetPtrp(sec, DMU_FLOOR_MATERIAL);
        params.foundSec = NULL;
        params.height = height;
        params.stairSize = stairsize;

        while(P_Iteratep(params.baseSec, DMU_LINEDEF, &params,
                          findAdjacentSectorForSpread))
        {   // We found another sector to spread to.
            floor = Z_Calloc(sizeof(*floor), PU_MAP, 0);
            floor->thinker.function = T_MoveFloor;
            DD_ThinkerAdd(&floor->thinker);

            P_ToXSector(params.foundSec)->specialData = floor;
#if __JHERETIC__
            floor->type = FT_RAISEBUILDSTEP;
#endif
            floor->state = FS_UP;
            floor->speed = speed;
            floor->sector = params.foundSec;
            floor->floorDestHeight = params.height;

            // Prepare for the next pass.
            params.baseSec = params.foundSec;
            params.foundSec = NULL;
        }
    }

    return rtn;
}
#endif

#if __JHEXEN__
static void enqueueStairSector(Sector* sec, int type, coord_t height)
{
    if((stairQueueTail + 1) % STAIR_QUEUE_SIZE == stairQueueHead)
    {
        Con_Error("BuildStairs:  Too many branches located.\n");
    }
    stairQueue[stairQueueTail].sector = sec;
    stairQueue[stairQueueTail].type = type;
    stairQueue[stairQueueTail].height = height;

    stairQueueTail = (stairQueueTail + 1) % STAIR_QUEUE_SIZE;
}

static Sector* dequeueStairSector(int* type, coord_t* height)
{
    Sector* sec;

    if(stairQueueHead == stairQueueTail)
    {
        // Queue is empty.
        return NULL;
    }

    *type = stairQueue[stairQueueHead].type;
    *height = stairQueue[stairQueueHead].height;
    sec = stairQueue[stairQueueHead].sector;
    stairQueueHead = (stairQueueHead + 1) % STAIR_QUEUE_SIZE;

    return sec;
}

static void processStairSector(Sector* sec, int type, coord_t height,
    stairs_e stairsType, int delay, int resetDelay)
{
    floor_t* floor;
    findsectorneighborsforstairbuildparams_t params;

    height += stairData.stepDelta;

    floor = Z_Calloc(sizeof(*floor), PU_MAP, 0);
    floor->thinker.function = T_MoveFloor;
    DD_ThinkerAdd(&floor->thinker);
    P_ToXSector(sec)->specialData = floor;
    floor->type = FT_RAISEBUILDSTEP;
    floor->state = (stairData.direction == -1? FS_DOWN : FS_UP);
    floor->sector = sec;
    floor->floorDestHeight = height;
    switch(stairsType)
    {
    case STAIRS_NORMAL:
        floor->speed = stairData.speed;
        if(delay)
        {
            floor->delayTotal = delay;
            floor->stairsDelayHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + stairData.stepDelta;
            floor->stairsDelayHeightDelta = stairData.stepDelta;
        }
        floor->resetDelay = resetDelay;
        floor->resetDelayCount = resetDelay;
        floor->resetHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);
        break;

    case STAIRS_SYNC:
        floor->speed =
            stairData.speed * ((height - stairData.startHeight) / stairData.stepDelta);
        floor->resetDelay = delay; //arg4
        floor->resetDelayCount = delay;
        floor->resetHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);
        break;

    default:
        break;
    }

    SN_StartSequence(P_GetPtrp(sec, DMU_BASE),
                     SEQ_PLATFORM + P_ToXSector(sec)->seqType);

    params.type = type;
    params.height = height;

    // Find all neigboring sectors with sector special equal to type and add
    // them to the stairbuild queue.
    P_Iteratep(sec, DMU_LINEDEF, &params, findSectorNeighborsForStairBuild);
}
#endif

/**
 * @param direction     Positive = up. Negative = down.
 */
#if __JHEXEN__
int EV_BuildStairs(LineDef* line, byte* args, int direction,
                   stairs_e stairsType)
{
    coord_t height;
    int delay;
    int type;
    int resetDelay;
    Sector* sec = NULL, *qSec;
    iterlist_t* list;

    // Set global stairs variables
    stairData.textureChange = 0;
    stairData.direction = direction;
    stairData.stepDelta = stairData.direction * (coord_t) args[2];
    stairData.speed = (float) args[1] * (1.0 / 8);
    resetDelay = (int) args[4];
    delay = (int) args[3];
    if(stairsType == STAIRS_PHASED)
    {
        stairData.startDelay =
            stairData.startDelayDelta = (int) args[3];
        resetDelay = stairData.startDelayDelta;
        delay = 0;
        stairData.textureChange = (int) args[4];
    }

    VALIDCOUNT++;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return 0;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        stairData.material = P_GetPtrp(sec, DMU_FLOOR_MATERIAL);
        stairData.startHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going...

        enqueueStairSector(sec, 0, P_GetDoublep(sec, DMU_FLOOR_HEIGHT));
        P_ToXSector(sec)->special = 0;
    }

    while((qSec = dequeueStairSector(&type, &height)) != NULL)
    {
        processStairSector(qSec, type, height, stairsType, delay, resetDelay);
    }

    return 1;
}
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
typedef struct {
    Sector*         sector;
    LineDef*        foundLineDef;
} findfirsttwosidedparams_t;

int findFirstTwosided(void *ptr, void *context)
{
    LineDef* li = (LineDef*) ptr;
    findfirsttwosidedparams_t* params = (findfirsttwosidedparams_t*) context;
    Sector* backSec = P_GetPtrp(li, DMU_BACK_SECTOR);

    if(backSec && !(params->sector && backSec == params->sector))
    {
        params->foundLineDef = li;
        return true; // Stop iteration, this will do.
    }

    return false; // Continue iteration.
}
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
int EV_DoDonut(LineDef* line)
{
    int rtn = 0;
    Sector* sec, *outer, *ring;
    iterlist_t* list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        findfirsttwosidedparams_t params;

        // Already moving? If so, keep going...
        if(P_ToXSector(sec)->specialData)
            continue;

        rtn = 1;
        outer = ring = NULL;

        params.sector = NULL;
        params.foundLineDef = NULL;
        if(P_Iteratep(sec, DMU_LINEDEF, &params, findFirstTwosided))
        {
            ring = P_GetPtrp(params.foundLineDef, DMU_BACK_SECTOR);
            if(ring == sec)
                ring = P_GetPtrp(params.foundLineDef, DMU_FRONT_SECTOR);

            params.sector = sec;
            params.foundLineDef = NULL;
            if(P_Iteratep(ring, DMU_LINEDEF, &params, findFirstTwosided))
                outer = P_GetPtrp(params.foundLineDef, DMU_BACK_SECTOR);
        }

        if(outer && ring)
        {   // Found both parts of the donut.
            floor_t* floor;
            coord_t destHeight = P_GetDoublep(outer, DMU_FLOOR_HEIGHT);

            // Spawn rising slime.
            floor = Z_Calloc(sizeof(*floor), PU_MAP, 0);
            floor->thinker.function = T_MoveFloor;
            DD_ThinkerAdd(&floor->thinker);

            P_ToXSector(ring)->specialData = floor;

            floor->type = FT_RAISEDONUT;
            floor->crush = false;
            floor->state = FS_UP;
            floor->sector = ring;
            floor->speed = FLOORSPEED * .5;
            floor->material = P_GetPtrp(outer, DMU_FLOOR_MATERIAL);
            floor->newSpecial = 0;
            floor->floorDestHeight = destHeight;

            // Spawn lowering donut-hole.
            floor = Z_Calloc(sizeof(*floor), PU_MAP, 0);
            floor->thinker.function = T_MoveFloor;
            DD_ThinkerAdd(&floor->thinker);

            P_ToXSector(sec)->specialData = floor;
            floor->type = FT_LOWER;
            floor->crush = false;
            floor->state = FS_DOWN;
            floor->sector = sec;
            floor->speed = FLOORSPEED * .5;
            floor->floorDestHeight = destHeight;
        }
    }

    return rtn;
}
#endif

#if __JHEXEN__
static int stopFloorCrush(thinker_t* th, void* context)
{
    boolean*            found = (boolean*) context;
    floor_t*        floor = (floor_t *) th;

    if(floor->type == FT_RAISEFLOORCRUSH)
    {
        // Completely remove the crushing floor
        SN_StopSequence(P_GetPtrp(floor->sector, DMU_BASE));
        P_ToXSector(floor->sector)->specialData = NULL;
        P_TagFinished(P_ToXSector(floor->sector)->tag);
        DD_ThinkerRemove(&floor->thinker);
        (*found) = true;
    }

    return false; // Continue iteration.
}

int EV_FloorCrushStop(LineDef* line, byte* args)
{
    boolean             found = false;

    DD_IterateThinkers(T_MoveFloor, stopFloorCrush, &found);

    return (found? 1 : 0);
}
#endif

#if __JHEXEN__ || __JDOOM64__
# if __JHEXEN__
int EV_DoFloorAndCeiling(LineDef *line, byte *args, int ftype, int ctype)
# else
int EV_DoFloorAndCeiling(LineDef* line, int ftype, int ctype)
# endif
{
# if __JHEXEN__
    int                 tag = (int) args[0];
# else
    int                 tag = P_ToXLine(line)->tag;
# endif
    boolean             floor, ceiling;
    Sector*             sec = NULL;
    iterlist_t*         list;

    list = P_GetSectorIterListForTag(tag, false);
    if(!list)
        return 0;

    /** \kludge
     * Due to the fact that sectors can only have one special thinker
     * linked at a time, this routine manually removes the link before
     * then creating a second thinker for the sector.
     * In order to commonize this we should maintain seperate links in
     * xsector_t for each type of special (not thinker type) i.e:
     *
     *   floor, ceiling, lightlevel
     *
     * Note: floor and ceiling are capable of moving at different speeds
     * and with different target heights, we must remain compatible.
     */

# if __JHEXEN__
    floor = EV_DoFloor(line, args, ftype);
# else
    floor = EV_DoFloor(line, ftype);
# endif
    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        P_ToXSector(sec)->specialData = NULL;
    }
# if __JHEXEN__
    ceiling = EV_DoCeiling(line, args, ctype);
# else
    ceiling = EV_DoCeiling(line, ctype);
# endif
    // < KLUDGE

    return (floor | ceiling);
}
#endif
