/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 * Stairs, pillars and waggle floors.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_map.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

#define STAIR_SECTOR_TYPE       26
#define STAIR_QUEUE_SIZE        32

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

// TYPES -------------------------------------------------------------------

typedef struct stairqueue_s {
    sector_t *sector;
    int     type;
    float   height;
} stairqueue_t;

// Global vars for stair building. DJS - In a struct for neatness.
typedef struct stairdata_s {
    float   stepDelta;
    int     direction;
    float   speed;
    int     texture;
    int     startDelay;
    int     startDelayDelta;
    int     textureChange;
    float   startHeight;
} stairdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t FloatBobOffsets[64];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

stairdata_t stairData;
stairqueue_t StairQueue[STAIR_QUEUE_SIZE];
static int QueueHead;
static int QueueTail;

// CODE --------------------------------------------------------------------

static void QueueStairSector(sector_t *sec, int type, float height)
{
    if((QueueTail + 1) % STAIR_QUEUE_SIZE == QueueHead)
    {
        Con_Error("BuildStairs:  Too many branches located.\n");
    }
    StairQueue[QueueTail].sector = sec;
    StairQueue[QueueTail].type = type;
    StairQueue[QueueTail].height = height;

    QueueTail = (QueueTail + 1) % STAIR_QUEUE_SIZE;
}

static sector_t *DequeueStairSector(int *type, float *height)
{
    sector_t   *sec;

    if(QueueHead == QueueTail)
    {                           // queue is empty
        return NULL;
    }
    *type = StairQueue[QueueHead].type;
    *height = StairQueue[QueueHead].height;
    sec = StairQueue[QueueHead].sector;
    QueueHead = (QueueHead + 1) % STAIR_QUEUE_SIZE;

    return sec;
}

static void ProcessStairSector(sector_t *sec, int type, float height,
                               stairs_e stairsType, int delay, int resetDelay)
{
    int         i;
    sector_t   *tsec;
    xsector_t  *xtsec;
    floormove_t *floor;

    //
    // new floor thinker
    //
    height += stairData.stepDelta;
    floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
    memset(floor, 0, sizeof(*floor));
    P_AddThinker(&floor->thinker);
    P_XSector(sec)->specialdata = floor;
    floor->thinker.function = T_MoveFloor;
    floor->type = FLEV_RAISEBUILDSTEP;
    floor->direction = stairData.direction;
    floor->sector = sec;
    floor->floordestheight = height;
    switch(stairsType)
    {
    case STAIRS_NORMAL:
        floor->speed = stairData.speed;
        if(delay)
        {
            floor->delayTotal = delay;
            floor->stairsDelayHeight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + stairData.stepDelta;
            floor->stairsDelayHeightDelta = stairData.stepDelta;
        }
        floor->resetDelay = resetDelay;
        floor->resetDelayCount = resetDelay;
        floor->resetHeight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        break;
    case STAIRS_SYNC:
        floor->speed =
            stairData.speed * ((height - stairData.startHeight) / stairData.stepDelta);
        floor->resetDelay = delay;  //arg4
        floor->resetDelayCount = delay;
        floor->resetHeight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        break;
        /*
           case STAIRS_PHASED:
           floor->floordestheight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + stairData.stepDelta;
           floor->speed = stairData.speed;
           floor->delayCount = stairData.startDelay;
           StartDelay += stairData.startDelayDelta;
           floor->textureChange = stairData.textureChange;
           floor->resetDelayCount = stairData.startDelay;
           break;
         */
    default:
        break;
    }
    SN_StartSequence(P_GetPtrp(sec, DMU_SOUND_ORIGIN),
                     SEQ_PLATFORM + P_XSector(sec)->seqType);
    //
    // Find next sector to raise
    // Find nearby sector with sector special equal to type
    //
    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); ++i)
    {
        line_t *line = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        if(!(P_GetIntp(line, DMU_FLAGS) & ML_TWOSIDED))
        {
            continue;
        }
        tsec = P_GetPtrp(line, DMU_FRONT_SECTOR);
        xtsec = P_XSector(tsec);
        if(xtsec->special == type + STAIR_SECTOR_TYPE && !xtsec->specialdata &&
           P_GetIntp(tsec, DMU_FLOOR_TEXTURE) == stairData.texture &&
           P_GetIntp(tsec, DMU_VALID_COUNT) != validCount)
        {
            QueueStairSector(tsec, type ^ 1, height);
            P_SetIntp(tsec, DMU_VALID_COUNT, validCount);
        }
        tsec = P_GetPtrp(line, DMU_BACK_SECTOR);
        xtsec = P_XSector(tsec);
        if(xtsec->special == type + STAIR_SECTOR_TYPE && !xtsec->specialdata &&
           P_GetIntp(tsec, DMU_FLOOR_TEXTURE) == stairData.texture &&
           P_GetIntp(tsec, DMU_VALID_COUNT) != validCount)
        {
            QueueStairSector(tsec, type ^ 1, height);
            P_SetIntp(tsec, DMU_VALID_COUNT, validCount);
        }
    }
}

/**
 * @param direction     Positive = up. Negative = down.
 */
int EV_BuildStairs(line_t *line, byte *args, int direction,
                   stairs_e stairsType)
{
    float       height;
    int         delay;
    int         type;
    int         resetDelay;
    sector_t   *sec = NULL, *qSec;
    iterlist_t *list;

    // Set global stairs variables
    stairData.textureChange = 0;
    stairData.direction = direction;
    stairData.stepDelta = stairData.direction * (float) args[2];
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

    validCount++;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        stairData.texture = P_GetIntp(sec, DMU_FLOOR_TEXTURE);
        stairData.startHeight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(P_XSector(sec)->specialdata)
            continue;

        QueueStairSector(sec, 0, P_GetFloatp(sec, DMU_FLOOR_HEIGHT));
        P_XSector(sec)->special = 0;
    }
    while((qSec = DequeueStairSector(&type, &height)) != NULL)
    {
        ProcessStairSector(qSec, type, height, stairsType, delay, resetDelay);
    }
    return (1);
}

void T_BuildPillar(pillar_t *pillar)
{
    result_e res1;
    result_e res2;

    // First, raise the floor
    res1 = T_MovePlane(pillar->sector, pillar->floorSpeed, pillar->floordest, pillar->crush, 0, pillar->direction); // floorOrCeiling, direction
    // Then, lower the ceiling
    res2 =
        T_MovePlane(pillar->sector, pillar->ceilingSpeed, pillar->ceilingdest,
                    pillar->crush, 1, -pillar->direction);
    if(res1 == pastdest && res2 == pastdest)
    {
        P_XSector(pillar->sector)->specialdata = NULL;
        SN_StopSequence(P_GetPtrp(pillar->sector, DMU_SOUND_ORIGIN));
        P_TagFinished(P_XSector(pillar->sector)->tag);
        P_RemoveThinker(&pillar->thinker);
    }
}

int EV_BuildPillar(line_t *line, byte *args, boolean crush)
{
    int         rtn = 0;
    float       newHeight;
    sector_t   *sec = NULL;
    pillar_t   *pillar;
    iterlist_t *list;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue; // already moving

        if(P_GetFloatp(sec, DMU_FLOOR_HEIGHT) ==
           P_GetFloatp(sec, DMU_CEILING_HEIGHT))
            continue; // pillar is already closed

        rtn = 1;
        if(!args[2])
        {
            newHeight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) +
                ((P_GetFloatp(sec, DMU_CEILING_HEIGHT) -
                  P_GetFloatp(sec, DMU_FLOOR_HEIGHT)) * .5);
        }
        else
        {
            newHeight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + (float) args[2];
        }

        pillar = Z_Malloc(sizeof(*pillar), PU_LEVSPEC, 0);
        P_XSector(sec)->specialdata = pillar;
        P_AddThinker(&pillar->thinker);
        pillar->thinker.function = T_BuildPillar;
        pillar->sector = sec;
        if(!args[2])
        {
            pillar->ceilingSpeed = pillar->floorSpeed =
                (float) args[1] * (1.0 / 8);
        }
        else if(newHeight - P_GetFloatp(sec, DMU_FLOOR_HEIGHT) >
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newHeight)
        {
            pillar->floorSpeed = (float) args[1] * (1.0 / 8);
            pillar->ceilingSpeed =
                (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newHeight) *
                      (pillar->floorSpeed / (newHeight - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0 / 8);
            pillar->floorSpeed =
                (newHeight - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed / 
                                  (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newHeight));
        }
        pillar->floordest = newHeight;
        pillar->ceilingdest = newHeight;
        pillar->direction = 1;
        pillar->crush = crush * (int) args[3];
        SN_StartSequence(P_GetPtrp(pillar->sector, DMU_SOUND_ORIGIN),
                         SEQ_PLATFORM + P_XSector(pillar->sector)->seqType);
    }
    return rtn;
}

int EV_OpenPillar(line_t *line, byte *args)
{
    int         rtn = 0;
    sector_t   *sec = NULL;
    pillar_t   *pillar;
    iterlist_t *list;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue; // already moving

        if(P_GetFloatp(sec, DMU_FLOOR_HEIGHT) !=
           P_GetFloatp(sec, DMU_CEILING_HEIGHT))
            continue; // pillar isn't closed

        rtn = 1;
        pillar = Z_Malloc(sizeof(*pillar), PU_LEVSPEC, 0);
        P_XSector(sec)->specialdata = pillar;
        P_AddThinker(&pillar->thinker);
        pillar->thinker.function = T_BuildPillar;
        pillar->sector = sec;
        if(!args[2])
        {
            pillar->floordest = P_FindLowestFloorSurrounding(sec);
        }
        else
        {
            pillar->floordest =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) - (float) args[2];
        }
        if(!args[3])
        {
            pillar->ceilingdest = P_FindHighestCeilingSurrounding(sec);
        }
        else
        {
            pillar->ceilingdest =
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) + (float) args[3];
        }
        if(P_GetFloatp(sec, DMU_FLOOR_HEIGHT) - pillar->floordest >=
           pillar->ceilingdest - P_GetFloatp(sec, DMU_CEILING_HEIGHT))
        {
            pillar->floorSpeed = (float) args[1] * (1.0 / 8);
            pillar->ceilingSpeed =
                (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - pillar->ceilingdest) *
                    (pillar->floorSpeed /
                        (pillar->floordest - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0 / 8);
            pillar->floorSpeed =
                (pillar->floordest - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed /
                        (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - pillar->ceilingdest));
        }
        pillar->direction = -1; // open the pillar
        SN_StartSequence(P_GetPtrp(pillar->sector, DMU_SOUND_ORIGIN),
                         SEQ_PLATFORM + P_XSector(pillar->sector)->seqType);
    }
    return rtn;
}

void T_FloorWaggle(floorWaggle_t *waggle)
{
    float       fh;

    switch(waggle->state)
    {
    case WGLSTATE_EXPAND:
        if((waggle->scale += waggle->scaleDelta) >= waggle->targetScale)
        {
            waggle->scale = waggle->targetScale;
            waggle->state = WGLSTATE_STABLE;
        }
        break;
    case WGLSTATE_REDUCE:
        if((waggle->scale -= waggle->scaleDelta) <= 0)
        {                       // Remove
            P_SetFloatp(waggle->sector, DMU_FLOOR_HEIGHT,
                        waggle->originalHeight);
            P_ChangeSector(waggle->sector, true);
            P_XSector(waggle->sector)->specialdata = NULL;
            P_TagFinished(P_XSector(waggle->sector)->tag);
            P_RemoveThinker(&waggle->thinker);
            return;
        }
        break;
    case WGLSTATE_STABLE:
        if(waggle->ticker != -1)
        {
            if(!--waggle->ticker)
            {
                waggle->state = WGLSTATE_REDUCE;
            }
        }
        break;
    }
    waggle->accumulator += waggle->accDelta;
    fh = waggle->originalHeight +
        FIX2FLT(FloatBobOffsets[((int) waggle->accumulator) & 63]) *
                 waggle->scale;
    P_SetFloatp(waggle->sector, DMU_FLOOR_HEIGHT, fh);
    P_SetFloatp(waggle->sector, DMU_FLOOR_TARGET, fh);
    P_SetFloatp(waggle->sector, DMU_FLOOR_SPEED, 0);
    P_ChangeSector(waggle->sector, true);
}

boolean EV_StartFloorWaggle(int tag, int height, int speed, int offset,
                            int timer)
{
    boolean     retCode = false;
    sector_t   *sec = NULL;
    floorWaggle_t *waggle;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(tag, false);
    if(!list)
        return retCode;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue; // already moving

        retCode = true;
        waggle = Z_Malloc(sizeof(*waggle), PU_LEVSPEC, 0);
        P_XSector(sec)->specialdata = waggle;
        waggle->thinker.function = T_FloorWaggle;
        waggle->sector = sec;
        waggle->originalHeight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        waggle->accumulator = offset;
        waggle->accDelta = FIX2FLT(speed << 10);
        waggle->scale = 0;
        waggle->targetScale = FIX2FLT(height << 10);
        waggle->scaleDelta =
            FIX2FLT(FLT2FIX(waggle->targetScale) / (35 + ((3 * 35) * height) / 255));
        waggle->ticker = timer ? timer * 35 : -1;
        waggle->state = WGLSTATE_EXPAND;
        P_AddThinker(&waggle->thinker);
    }
    return retCode;
}
