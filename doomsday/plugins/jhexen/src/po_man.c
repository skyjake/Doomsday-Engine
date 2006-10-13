/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * Po_man.c : Polyobject management.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_mapsetup.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

/* Returns the special data pointer of the polyobj. */
#define PO_SpecialData(polyPtr)     P_GetPtrp((polyPtr), DMU_SPECIAL_DATA)
#define PO_Tag(polyPtr)             P_GetIntp((polyPtr), DMU_TAG)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    PO_Init(int lump);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static polyobj_t *GetPolyobj(int polyNum);
static int GetPolyobjMirror(int poly);
static void ThrustMobj(mobj_t *mobj, seg_t *seg, polyobj_t * po);
static void InitBlockMap(void);
static void IterFindPolySegs(int x, int y, seg_t **segList);
static void SpawnPolyobj(int index, int tag, boolean crush);
static void TranslateToStartSpot(int tag, int originX, int originY);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int  numthings;
extern thing_t *things;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int PolySegCount;
static fixed_t PolyStartX;
static fixed_t PolyStartY;

// CODE --------------------------------------------------------------------

void PO_StartSequence(polyobj_t *poly, int seqBase)
{
    SN_StartSequence(P_GetPtrp(poly, DMU_START_SPOT),
                     seqBase + P_GetIntp(poly, DMU_SEQUENCE_TYPE));
}

void PO_StopSequence(polyobj_t *poly)
{
    SN_StopSequence(P_GetPtrp(poly, DMU_START_SPOT));
}

void PO_SetDestination(polyobj_t *poly, fixed_t dist, angle_t angle,
                       fixed_t speed)
{
    ddvertex_t startSpot;

    P_GetFixedpv(poly, DMU_START_SPOT_XY, startSpot.pos);

    P_SetFixedp(poly, DMU_DESTINATION_X,
                startSpot.pos[VX] + FixedMul(dist, finecosine[angle]));
    P_SetFixedp(poly, DMU_DESTINATION_Y,
                startSpot.pos[VY] + FixedMul(dist, finesine[angle]));
    P_SetFixedp(poly, DMU_SPEED, speed);
}

// ===== Polyobj Event Code =====

//==========================================================================
//
// T_RotatePoly
//
//==========================================================================

void T_RotatePoly(polyevent_t * pe)
{
    unsigned int absSpeed;
    polyobj_t *poly;

    if(PO_RotatePolyobj(pe->polyobj, pe->speed))
    {
        absSpeed = abs(pe->speed);

        if(pe->dist == -1)
        {                       // perpetual polyobj
            return;
        }
        pe->dist -= absSpeed;
        if(pe->dist <= 0)
        {
            poly = GetPolyobj(pe->polyobj);
            if(PO_SpecialData(poly) == pe)
            {
                P_SetPtrp(poly, DMU_SPECIAL_DATA, NULL);
            }
            PO_StopSequence(poly);
            P_PolyobjFinished(PO_Tag(poly));
            P_RemoveThinker(&pe->thinker);
            P_SetAnglep(poly, DMU_ANGLE_SPEED, 0);
        }
        if(pe->dist < absSpeed)
        {
            pe->speed = pe->dist * (pe->speed < 0 ? -1 : 1);
        }
    }
}

//==========================================================================
//
// EV_RotatePoly
//
//==========================================================================

boolean EV_RotatePoly(line_t *line, byte *args, int direction,
                      boolean overRide)
{
    int     mirror;
    int     polyNum;
    polyevent_t *pe;
    polyobj_t *poly;

    polyNum = args[0];
    poly = GetPolyobj(polyNum);
    if(poly)
    {
        if(PO_SpecialData(poly) && !overRide)
        {                       // poly is already moving
            return false;
        }
    }
    else
    {
        Con_Error("EV_RotatePoly:  Invalid polyobj num: %d\n", polyNum);
    }
    pe = Z_Malloc(sizeof(polyevent_t), PU_LEVSPEC, 0);
    P_AddThinker(&pe->thinker);
    pe->thinker.function = T_RotatePoly;
    pe->polyobj = polyNum;
    if(args[2])
    {
        if(args[2] == 255)
        {
            // Perpetual rotation.
            pe->dist = -1;
            P_SetAnglep(poly, DMU_DESTINATION_ANGLE, -1);
        }
        else
        {
            pe->dist = args[2] * (ANGLE_90 / 64);   // Angle
            P_SetAnglep(poly, DMU_DESTINATION_ANGLE,
                        P_GetAnglep(poly, DMU_ANGLE) +
                        pe->dist * direction);
        }
    }
    else
    {
        pe->dist = ANGLE_MAX - 1;
        P_SetAnglep(poly, DMU_DESTINATION_ANGLE,
                        P_GetAnglep(poly, DMU_ANGLE) + pe->dist);
    }
    pe->speed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
    P_SetPtrp(poly, DMU_SPECIAL_DATA, pe);
    P_SetAnglep(poly, DMU_ANGLE_SPEED, pe->speed);
    PO_StartSequence(poly, SEQ_DOOR_STONE);

    while((mirror = GetPolyobjMirror(polyNum)) != 0)
    {
        poly = GetPolyobj(mirror);
        if(poly && PO_SpecialData(poly) && !overRide)
        {                       // mirroring poly is already in motion
            break;
        }
        pe = Z_Malloc(sizeof(polyevent_t), PU_LEVSPEC, 0);
        P_AddThinker(&pe->thinker);
        pe->thinker.function = T_RotatePoly;
        P_SetPtrp(poly, DMU_SPECIAL_DATA, pe);
        pe->polyobj = mirror;
        if(args[2])
        {
            if(args[2] == 255)
            {
                pe->dist = -1;
                P_SetAnglep(poly, DMU_DESTINATION_ANGLE, -1);
            }
            else
            {
                pe->dist = args[2] * (ANGLE_90 / 64);   // Angle
                P_SetAnglep(poly, DMU_DESTINATION_ANGLE,
                            P_GetAnglep(poly, DMU_ANGLE) +
                            pe->dist * -direction);
            }
        }
        else
        {
            pe->dist = ANGLE_MAX - 1;
            P_SetAnglep(poly, DMU_DESTINATION_ANGLE,
                            P_GetAnglep(poly, DMU_ANGLE) +
                            pe->dist);
        }
        direction = -direction;
        pe->speed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
        P_SetAnglep(poly, DMU_ANGLE_SPEED, pe->speed);
        poly = GetPolyobj(polyNum);
        if(poly)
        {
            P_SetPtrp(poly, DMU_SPECIAL_DATA, pe);
        }
        else
        {
            Con_Error("EV_RotatePoly:  Invalid polyobj num: %d\n", polyNum);
        }
        polyNum = mirror;
        PO_StartSequence(poly, SEQ_DOOR_STONE);
    }
    return true;
}

//==========================================================================
//
// T_MovePoly
//
//==========================================================================

void T_MovePoly(polyevent_t * pe)
{
    unsigned int absSpeed;
    polyobj_t *poly;

    if(PO_MovePolyobj(pe->polyobj, pe->xSpeed, pe->ySpeed))
    {
        absSpeed = abs(pe->speed);
        pe->dist -= absSpeed;
        if(pe->dist <= 0)
        {
            poly = GetPolyobj(pe->polyobj);
            if(PO_SpecialData(poly) == pe)
            {
                P_SetPtrp(poly, DMU_SPECIAL_DATA, NULL);
            }
            PO_StopSequence(poly);
            P_PolyobjFinished(PO_Tag(poly));
            P_RemoveThinker(&pe->thinker);
            P_SetIntp(poly, DMU_SPEED, 0);
        }
        if(pe->dist < absSpeed)
        {
            pe->speed = pe->dist * (pe->speed < 0 ? -1 : 1);
            pe->xSpeed = FixedMul(pe->speed, finecosine[pe->angle]);
            pe->ySpeed = FixedMul(pe->speed, finesine[pe->angle]);
        }
    }
}

//==========================================================================
//
// EV_MovePoly
//
//==========================================================================

boolean EV_MovePoly(line_t *line, byte *args, boolean timesEight,
                    boolean overRide)
{
    int     mirror;
    int     polyNum;
    polyevent_t *pe;
    polyobj_t *poly;
    angle_t an;

    polyNum = args[0];
    poly = GetPolyobj(polyNum);
    if(poly)
    {
        if(PO_SpecialData(poly) && !overRide)
        {                       // poly is already moving
            return false;
        }
    }
    else
    {
        Con_Error("EV_MovePoly:  Invalid polyobj num: %d\n", polyNum);
    }
    pe = Z_Malloc(sizeof(polyevent_t), PU_LEVSPEC, 0);
    P_AddThinker(&pe->thinker);
    pe->thinker.function = T_MovePoly;
    pe->polyobj = polyNum;
    if(timesEight)
    {
        pe->dist = args[3] * 8 * FRACUNIT;
    }
    else
    {
        pe->dist = args[3] * FRACUNIT;  // Distance
    }
    pe->speed = args[1] * (FRACUNIT / 8);
    P_SetPtrp(poly, DMU_SPECIAL_DATA, pe);

    an = args[2] * (ANGLE_90 / 64);

    pe->angle = an >> ANGLETOFINESHIFT;
    pe->xSpeed = FixedMul(pe->speed, finecosine[pe->angle]);
    pe->ySpeed = FixedMul(pe->speed, finesine[pe->angle]);
    PO_StartSequence(poly, SEQ_DOOR_STONE);

    PO_SetDestination(poly, pe->dist, pe->angle, pe->speed);

    while((mirror = GetPolyobjMirror(polyNum)) != 0)
    {
        poly = GetPolyobj(mirror);
        if(poly && PO_SpecialData(poly) && !overRide)
        {                       // mirroring poly is already in motion
            break;
        }
        pe = Z_Malloc(sizeof(polyevent_t), PU_LEVSPEC, 0);
        P_AddThinker(&pe->thinker);
        pe->thinker.function = T_MovePoly;
        pe->polyobj = mirror;
        P_SetPtrp(poly, DMU_SPECIAL_DATA, pe);
        if(timesEight)
        {
            pe->dist = args[3] * 8 * FRACUNIT;
        }
        else
        {
            pe->dist = args[3] * FRACUNIT;  // Distance
        }
        pe->speed = args[1] * (FRACUNIT / 8);
        an = an + ANGLE_180;    // reverse the angle
        pe->angle = an >> ANGLETOFINESHIFT;
        pe->xSpeed = FixedMul(pe->speed, finecosine[pe->angle]);
        pe->ySpeed = FixedMul(pe->speed, finesine[pe->angle]);
        polyNum = mirror;
        PO_StartSequence(poly, SEQ_DOOR_STONE);

        PO_SetDestination(poly, pe->dist, pe->angle, pe->speed);
    }
    return true;
}

//==========================================================================
//
// T_PolyDoor
//
//==========================================================================

void T_PolyDoor(polydoor_t * pd)
{
    int     absSpeed;
    polyobj_t *poly;

    if(pd->tics)
    {
        if(!--pd->tics)
        {
            poly = GetPolyobj(pd->polyobj);
            PO_StartSequence(poly, SEQ_DOOR_STONE);

            // Movement is about to begin. Update the destination.
            PO_SetDestination(GetPolyobj(pd->polyobj), pd->dist, pd->direction,
                              pd->speed);
        }
        return;
    }
    switch (pd->type)
    {
    case PODOOR_SLIDE:
        if(PO_MovePolyobj(pd->polyobj, pd->xSpeed, pd->ySpeed))
        {
            absSpeed = abs(pd->speed);
            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
                poly = GetPolyobj(pd->polyobj);
                PO_StopSequence(poly);
                if(!pd->close)
                {
                    pd->dist = pd->totalDist;
                    pd->close = true;
                    pd->tics = pd->waitTics;
                    pd->direction =
                        (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                    pd->xSpeed = -pd->xSpeed;
                    pd->ySpeed = -pd->ySpeed;
                }
                else
                {
                    if(PO_SpecialData(poly) == pd)
                    {
                        P_SetPtrp(poly, DMU_SPECIAL_DATA, NULL);
                    }
                    P_PolyobjFinished(PO_Tag(poly));
                    P_RemoveThinker(&pd->thinker);
                }
            }
        }
        else
        {
            poly = GetPolyobj(pd->polyobj);
            if(P_GetBoolp(poly, DMU_CRUSH) || !pd->close)
            {                   // continue moving if the poly is a crusher, or is opening
                return;
            }
            else
            {                   // open back up
                pd->dist = pd->totalDist - pd->dist;
                pd->direction =
                    (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                pd->xSpeed = -pd->xSpeed;
                pd->ySpeed = -pd->ySpeed;
                // Update destination.
                PO_SetDestination(GetPolyobj(pd->polyobj), pd->dist,
                                  pd->direction, pd->speed);
                pd->close = false;
                PO_StartSequence(poly, SEQ_DOOR_STONE);
            }
        }
        break;
    case PODOOR_SWING:
        if(PO_RotatePolyobj(pd->polyobj, pd->speed))
        {
            absSpeed = abs(pd->speed);
            if(pd->dist == -1)
            {                   // perpetual polyobj
                return;
            }
            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
                poly = GetPolyobj(pd->polyobj);
                PO_StopSequence(poly);
                if(!pd->close)
                {
                    pd->dist = pd->totalDist;
                    pd->close = true;
                    pd->tics = pd->waitTics;
                    pd->speed = -pd->speed;
                }
                else
                {
                    if(PO_SpecialData(poly) == pd)
                    {
                        P_SetPtrp(poly, DMU_SPECIAL_DATA, NULL);
                    }
                    P_PolyobjFinished(PO_Tag(poly));
                    P_RemoveThinker(&pd->thinker);
                }
            }
        }
        else
        {
            poly = GetPolyobj(pd->polyobj);
            if(P_GetBoolp(poly, DMU_CRUSH) || !pd->close)
            {                   // continue moving if the poly is a crusher, or is opening
                return;
            }
            else
            {                   // open back up and rewait
                pd->dist = pd->totalDist - pd->dist;
                pd->speed = -pd->speed;
                pd->close = false;
                PO_StartSequence(poly, SEQ_DOOR_STONE);
            }
        }
        break;
    default:
        break;
    }
}

//==========================================================================
//
// EV_OpenPolyDoor
//
//==========================================================================

boolean EV_OpenPolyDoor(line_t *line, byte *args, podoortype_t type)
{
    int     mirror;
    int     polyNum;
    polydoor_t *pd;
    polyobj_t *poly;
    angle_t an = 0;

    polyNum = args[0];
    poly = GetPolyobj(polyNum);
    if(poly)
    {
        if(PO_SpecialData(poly))
        {                       // poly is already moving
            return false;
        }
    }
    else
    {
        Con_Error("EV_OpenPolyDoor:  Invalid polyobj num: %d\n", polyNum);
    }
    pd = Z_Malloc(sizeof(polydoor_t), PU_LEVSPEC, 0);
    memset(pd, 0, sizeof(polydoor_t));
    P_AddThinker(&pd->thinker);
    pd->thinker.function = T_PolyDoor;
    pd->type = type;
    pd->polyobj = polyNum;
    if(type == PODOOR_SLIDE)
    {
        pd->waitTics = args[4];
        pd->speed = args[1] * (FRACUNIT / 8);
        pd->totalDist = args[3] * FRACUNIT; // Distance
        pd->dist = pd->totalDist;
        an = args[2] * (ANGLE_90 / 64);
        pd->direction = an >> ANGLETOFINESHIFT;
        pd->xSpeed = FixedMul(pd->speed, finecosine[pd->direction]);
        pd->ySpeed = FixedMul(pd->speed, finesine[pd->direction]);
        PO_StartSequence(poly, SEQ_DOOR_STONE);
    }
    else if(type == PODOOR_SWING)
    {
        pd->waitTics = args[3];
        pd->direction = 1;      // ADD:  PODOOR_SWINGL, PODOOR_SWINGR
        pd->speed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
        pd->totalDist = args[2] * (ANGLE_90 / 64);
        pd->dist = pd->totalDist;
        PO_StartSequence(poly, SEQ_DOOR_STONE);
    }

    P_SetPtrp(poly, DMU_SPECIAL_DATA, pd);
    PO_SetDestination(poly, pd->dist, pd->direction, pd->speed);

    while((mirror = GetPolyobjMirror(polyNum)) != 0)
    {
        poly = GetPolyobj(mirror);
        if(poly && PO_SpecialData(poly))
        {                       // mirroring poly is already in motion
            break;
        }
        pd = Z_Malloc(sizeof(polydoor_t), PU_LEVSPEC, 0);
        memset(pd, 0, sizeof(polydoor_t));
        P_AddThinker(&pd->thinker);
        pd->thinker.function = T_PolyDoor;
        pd->polyobj = mirror;
        pd->type = type;
        P_SetPtrp(poly, DMU_SPECIAL_DATA, pd);
        if(type == PODOOR_SLIDE)
        {
            pd->waitTics = args[4];
            pd->speed = args[1] * (FRACUNIT / 8);
            pd->totalDist = args[3] * FRACUNIT; // Distance
            pd->dist = pd->totalDist;
            an = an + ANGLE_180;    // reverse the angle
            pd->direction = an >> ANGLETOFINESHIFT;
            pd->xSpeed = FixedMul(pd->speed, finecosine[pd->direction]);
            pd->ySpeed = FixedMul(pd->speed, finesine[pd->direction]);
            PO_StartSequence(poly, SEQ_DOOR_STONE);
        }
        else if(type == PODOOR_SWING)
        {
            pd->waitTics = args[3];
            pd->direction = -1; // ADD:  same as above
            pd->speed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
            pd->totalDist = args[2] * (ANGLE_90 / 64);
            pd->dist = pd->totalDist;
            PO_StartSequence(poly, SEQ_DOOR_STONE);
        }
        polyNum = mirror;
        PO_SetDestination(poly, pd->dist, pd->direction, pd->speed);
    }
    return true;
}

// ===== Higher Level Poly Interface code =====

//==========================================================================
//
// GetPolyobj
//
//==========================================================================

static polyobj_t *GetPolyobj(int polyNum)
{
    int     i;
    int     numPolyObjs = DD_GetInteger(DD_POLYOBJ_COUNT);

    for(i = 0; i < numPolyObjs; i++)
    {
        if(P_GetInt(DMU_POLYOBJ, i, DMU_TAG) == polyNum)
        {
            return P_ToPtr(DMU_POLYOBJ, i);
        }
    }
    return NULL;
}

//==========================================================================
//
// GetPolyobjMirror
//
//==========================================================================

static int GetPolyobjMirror(int poly)
{
    int     i;
    int     numPolyObjs = DD_GetInteger(DD_POLYOBJ_COUNT);

    for(i = 0; i < numPolyObjs; i++)
    {
        if(P_GetInt(DMU_POLYOBJ, i, DMU_TAG) == poly)
        {
            //return ((*polyobjs[i].Segs)->linedef->arg2);
            seg_t* seg = P_GetPtrp(P_ToPtr(DMU_POLYOBJ, i), DMU_SEG_OF_POLYOBJ | 0);
            line_t* linedef = P_GetPtrp(seg, DMU_LINE);
            return P_XLine(linedef)->arg2;
        }
    }
    return 0;
}

//==========================================================================
//
// ThrustMobj
//
//==========================================================================

static void ThrustMobj(mobj_t *mobj, seg_t *seg, polyobj_t * po)
{
    int     thrustAngle;
    int     thrustX;
    int     thrustY;
    polyevent_t *pe;

    int     force;

    // Clients do no polyobj <-> mobj interaction.
    if(IS_CLIENT)
        return;

    if(!(mobj->flags & MF_SHOOTABLE) && !mobj->player)
    {
        return;
    }
    thrustAngle = (P_GetAnglep(seg, DMU_ANGLE) - ANGLE_90) >>
                  ANGLETOFINESHIFT;

    pe = PO_SpecialData(po);
    if(pe)
    {
        if(pe->thinker.function == T_RotatePoly)
        {
            force = pe->speed >> 8;
        }
        else
        {
            force = pe->speed >> 3;
        }
        if(force < FRACUNIT)
        {
            force = FRACUNIT;
        }
        else if(force > 4 * FRACUNIT)
        {
            force = 4 * FRACUNIT;
        }
    }
    else
    {
        force = FRACUNIT;
    }

    thrustX = FixedMul(force, finecosine[thrustAngle]);
    thrustY = FixedMul(force, finesine[thrustAngle]);
    mobj->momx += thrustX;
    mobj->momy += thrustY;

    if(P_GetBoolp(po, DMU_CRUSH))
    {
        if(!P_CheckPosition(mobj, mobj->pos[VX] + thrustX, mobj->pos[VY] + thrustY))
        {
            P_DamageMobj(mobj, NULL, NULL, 3);
        }
    }
}


//==========================================================================
//
// InitBlockMap
//
//==========================================================================

static void InitBlockMap(void)
{
    int     i;
    int     j;
    seg_t **segList;
    int     area;
    int     leftX, rightX;
    int     topY, bottomY;
    int     numPolyObjs = DD_GetInteger(DD_POLYOBJ_COUNT);

    for(i = 0; i < numPolyObjs; ++i)
    {
        PO_LinkPolyobj(P_ToPtr(DMU_POLYOBJ, i));

        // calculate a rough area
        // right now, working like shit...gotta fix this...
        segList = P_GetPtr(DMU_POLYOBJ, i, DMU_SEG_LIST);
        leftX = rightX = P_GetFixedp(*segList, DMU_VERTEX1_X);
        topY = bottomY = P_GetFixedp(*segList, DMU_VERTEX1_Y);
        for(j = 0; j < P_GetInt(DMU_POLYOBJ, i, DMU_SEG_COUNT); ++j, segList++)
        {
            ddvertex_t v1;
            P_GetFixedpv(*segList, DMU_VERTEX1_XY, v1.pos);
            if(v1.pos[VX] < leftX)
            {
                leftX = v1.pos[VX];
            }
            if(v1.pos[VX] > rightX)
            {
                rightX = v1.pos[VX];
            }
            if(v1.pos[VY] < bottomY)
            {
                bottomY = v1.pos[VY];
            }
            if(v1.pos[VY] > topY)
            {
                topY = v1.pos[VY];
            }
        }
        area = ((rightX >> FRACBITS) -
                (leftX >> FRACBITS)) * ((topY >> FRACBITS) -
                                        (bottomY >> FRACBITS));
    }
}

//==========================================================================
//
// IterFindPolySegs
//
//              Passing NULL for segList will cause IterFindPolySegs to
//      count the number of segs in the polyobj
//==========================================================================

static void IterFindPolySegs(int x, int y, seg_t **segList)
{
    int     i;
    int     segCount = DD_GetInteger(DD_SEG_COUNT);

    if(x == PolyStartX && y == PolyStartY)
    {
        return;
    }
    for(i = 0; i < segCount; i++)
    {
        if(!P_GetPtr(DMU_SEG, i, DMU_LINE))
            continue;
        if(P_GetFixed(DMU_SEG, i, DMU_VERTEX1_X) == x &&
           P_GetFixed(DMU_SEG, i, DMU_VERTEX1_Y) == y)
        {
            if(!segList)
            {
                PolySegCount++;
            }
            else
            {
                *segList++ = P_ToPtr(DMU_SEG, i);
            }
            IterFindPolySegs(P_GetFixed(DMU_SEG, i, DMU_VERTEX2_X),
                             P_GetFixed(DMU_SEG, i, DMU_VERTEX2_Y), segList);
            return;
        }
    }
    Con_Error("IterFindPolySegs:  Non-closed Polyobj located.\n");
}

/*
 * Might be best to move this over to the engine.
 */
static void SpawnPolyobj(int index, int tag, boolean crush)
{
    int     i;
    int     j;
    int     seqType;
    int     psIndex;
    int     psIndexOld;
    seg_t  *polySegList[PO_MAXPOLYSEGS];
    int     segCount = DD_GetInteger(DD_SEG_COUNT);
    line_t *ldef;

    for(i = 0; i < segCount; i++)
    {
        line_t *linedef = P_GetPtr(DMU_SEG, i, DMU_LINE);
        if(!linedef)
            continue;

        if(P_XLine(linedef)->special == PO_LINE_START &&
           P_XLine(linedef)->arg1 == tag)
        {
            if(P_GetPtr(DMU_POLYOBJ, index, DMU_SEG_LIST))
            {
                Con_Error("SpawnPolyobj:  Polyobj %d already spawned.\n", tag);
            }
            P_XLine(linedef)->special = 0;
            P_XLine(linedef)->arg1 = 0;
            PolySegCount = 1;
            PolyStartX = P_GetFixed(DMU_SEG, i, DMU_VERTEX1_X);
            PolyStartY = P_GetFixed(DMU_SEG, i, DMU_VERTEX1_Y);
            IterFindPolySegs(P_GetFixed(DMU_SEG, i, DMU_VERTEX2_X),
                             P_GetFixed(DMU_SEG, i, DMU_VERTEX2_Y), NULL);
            P_SetInt(DMU_POLYOBJ, index, DMU_SEG_COUNT, PolySegCount);
            P_SetPtr(DMU_POLYOBJ, index, DMU_SEG_LIST,
                Z_Malloc(PolySegCount * sizeof(seg_t*), PU_LEVEL, 0));
            P_SetPtrp(P_ToPtr(DMU_POLYOBJ, index), DMU_SEG_OF_POLYOBJ | 0,
                      P_ToPtr(DMU_SEG, i)); // insert the first seg
            IterFindPolySegs(P_GetFixed(DMU_SEG, i, DMU_VERTEX2_X),
                             P_GetFixed(DMU_SEG, i, DMU_VERTEX2_Y),
                             /*P_GetPtrp(P_ToPtr(DMU_POLYOBJ, index),
                                       DMU_SEG_OF_POLYOBJ | 1)*/
                             ((seg_t**)P_GetPtr(DMU_POLYOBJ, index, DMU_SEG_LIST)) + 1);
            P_SetBool(DMU_POLYOBJ, index, DMU_CRUSH, crush);
            P_SetInt(DMU_POLYOBJ, index, DMU_TAG, tag);

            seqType = P_XLine(linedef)->arg3;
            if(seqType < 0 || seqType >= SEQTYPE_NUMSEQ)
            {
                seqType = 0;
            }
            P_SetInt(DMU_POLYOBJ, index, DMU_SEQUENCE_TYPE, seqType);
            break;
        }
    }
    if(!P_GetPtr(DMU_POLYOBJ, index, DMU_SEG_LIST))
    {                           // didn't find a polyobj through PO_LINE_START
        psIndex = 0;
        P_SetInt(DMU_POLYOBJ, index, DMU_SEG_COUNT, 0);
        for(j = 1; j < PO_MAXPOLYSEGS; j++)
        {
            psIndexOld = psIndex;
            for(i = 0; i < segCount; i++)
            {
                line_t *linedef = P_GetPtr(DMU_SEG, i, DMU_LINE);
                if(!linedef)
                    continue;
                if(P_XLine(linedef)->special == PO_LINE_EXPLICIT &&
                   P_XLine(linedef)->arg1 == tag)
                {
                    if(!P_XLine(linedef)->arg2)
                    {
                        Con_Error
                            ("SpawnPolyobj:  Explicit line missing order number (probably %d) in poly %d.\n",
                             j + 1, tag);
                    }
                    if(P_XLine(linedef)->arg2 == j)
                    {
                        polySegList[psIndex] = P_ToPtr(DMU_SEG, i);
                        P_SetInt(DMU_POLYOBJ, index, DMU_SEG_COUNT,
                                 P_GetInt(DMU_POLYOBJ, index, DMU_SEG_COUNT) + 1);
                        psIndex++;
                        if(psIndex > PO_MAXPOLYSEGS)
                        {
                            Con_Error
                                ("SpawnPolyobj:  psIndex > PO_MAXPOLYSEGS\n");
                        }
                    }
                }
            }
            // Clear out any specials for these segs...we cannot clear them out
            //  in the above loop, since we aren't guaranteed one seg per
            //      linedef.
            for(i = 0; i < numsegs; i++)
            {
                line_t *linedef = P_GetPtr(DMU_SEG, i, DMU_LINE);
                if(!linedef)
                    continue;
                if(P_XLine(linedef)->special == PO_LINE_EXPLICIT &&
                   P_XLine(linedef)->arg1 == tag &&
                   P_XLine(linedef)->arg2 == j)
                {
                    P_XLine(linedef)->special = 0;
                    P_XLine(linedef)->arg1 = 0;
                }
            }
            if(psIndex == psIndexOld)
            {                   // Check if an explicit line order has been skipped
                // A line has been skipped if there are any more explicit
                // lines with the current tag value
                for(i = 0; i < numsegs; i++)
                {
                    line_t *linedef = P_GetPtr(DMU_SEG, i, DMU_LINE);
                    if(!linedef)
                        continue;
                    if(P_XLine(linedef)->special == PO_LINE_EXPLICIT &&
                       P_XLine(linedef)->arg1 == tag)
                    {
                        Con_Error
                            ("SpawnPolyobj:  Missing explicit line %d for poly %d\n",
                             j, tag);
                    }
                }
            }
        }
        if(P_GetInt(DMU_POLYOBJ, index, DMU_SEG_COUNT))
        {
            polyobj_t *po = P_ToPtr(DMU_POLYOBJ, index);
            PolySegCount = P_GetInt(DMU_POLYOBJ, index, DMU_SEG_COUNT); // PolySegCount used globally
            P_SetBool(DMU_POLYOBJ, index, DMU_CRUSH, crush);
            P_SetInt(DMU_POLYOBJ, index, DMU_TAG, tag);
            P_SetPtr(DMU_POLYOBJ, index, DMU_SEG_LIST,
                Z_Malloc(PolySegCount * sizeof(seg_t *), PU_LEVEL, 0));
            for(i = 0; i < PolySegCount; i++)
            {
                P_SetPtrp(po, DMU_SEG_OF_POLYOBJ | i, polySegList[i]);
            }
            P_SetInt(DMU_POLYOBJ, index, DMU_SEQUENCE_TYPE,
                P_XLine(P_GetPtrp(P_GetPtrp(po, DMU_SEG_OF_POLYOBJ | 0),
                                  DMU_LINE))->arg4);
        }
        // Next, change the polyobjs first line to point to a mirror
        //      if it exists
        ldef = P_GetPtrp( P_GetPtrp(P_ToPtr(DMU_POLYOBJ, index),
                                    DMU_SEG_OF_POLYOBJ | 0),
                         DMU_LINE );
        P_XLine(ldef)->arg2 = P_XLine(ldef)->arg3;
        //(*polyobjs[index].Segs)->linedef->arg2 =
        //  (*polyobjs[index].Segs)->linedef->arg3;
    }
}

//==========================================================================
//
// TranslateToStartSpot
//
//==========================================================================

static void TranslateToStartSpot(int tag, int originX, int originY)
{
    seg_t **tempSeg;
    seg_t **veryTempSeg;
    ddvertex_t *tempPt;
    subsector_t *sub;
    polyobj_t *po;
    int     deltaX;
    int     deltaY;
    ddvertex_t avg;             // used to find a polyobj's center, and hence subsector
    int     i;
    int     poNumSegs;

    po = GetPolyobj(tag);
    if(!po)
    {                           // didn't match the tag with a polyobj tag
        Con_Error("TranslateToStartSpot:  Unable to match polyobj tag: %d\n",
                  tag);
    }
    if(P_GetPtrp(po, DMU_SEG_LIST) == NULL)
    {
        Con_Error
            ("TranslateToStartSpot:  Anchor point located without a StartSpot point: %d\n",
             tag);
    }

    poNumSegs = P_GetIntp(po, DMU_SEG_COUNT);

    P_SetPtrp(po, DMU_ORIGINAL_POINTS,
              Z_Malloc(poNumSegs * sizeof(ddvertex_t), PU_LEVEL, 0));
    P_SetPtrp(po, DMU_PREVIOUS_POINTS,
              Z_Malloc(poNumSegs * sizeof(ddvertex_t), PU_LEVEL, 0));
    deltaX = originX - P_GetFixedp(po, DMU_START_SPOT_X);
    deltaY = originY - P_GetFixedp(po, DMU_START_SPOT_Y);

    tempSeg = P_GetPtrp(po, DMU_SEG_LIST);
    tempPt = P_GetPtrp(po, DMU_ORIGINAL_POINTS);
    avg.pos[VX] = 0;
    avg.pos[VY] = 0;

    validCount++;
    for(i = 0; i < poNumSegs; ++i, tempSeg++, tempPt++)
    {
        line_t* linedef = P_GetPtrp(*tempSeg, DMU_LINE);
        if(P_GetIntp(linedef, DMU_VALID_COUNT) != validCount)
        {
            fixed_t *bbox = P_GetPtrp(linedef, DMU_BOUNDING_BOX);
            bbox[BOXTOP]    -= deltaY;
            bbox[BOXBOTTOM] -= deltaY;
            bbox[BOXLEFT]   -= deltaX;
            bbox[BOXRIGHT]  -= deltaX;
            P_SetIntp(linedef, DMU_VALID_COUNT, validCount);
        }
        for(veryTempSeg = P_GetPtrp(po, DMU_SEG_LIST);
            veryTempSeg != tempSeg; veryTempSeg++)
        {
            if(P_GetPtrp(*veryTempSeg, DMU_VERTEX1) ==
               P_GetPtrp(*tempSeg, DMU_VERTEX1))
            {
                break;
            }
        }
        if(veryTempSeg == tempSeg)
        {                       // the point hasn't been translated, yet
            fixed_t v1[2];
            P_GetFixedpv(*tempSeg, DMU_VERTEX1_XY, v1);
            v1[0] -= deltaX;
            v1[1] -= deltaY;
            P_SetFixedpv(*tempSeg, DMU_VERTEX1_XY, v1);
        }
        avg.pos[VX] += P_GetIntp(*tempSeg, DMU_VERTEX1_X);
        avg.pos[VY] += P_GetIntp(*tempSeg, DMU_VERTEX1_Y);
        // the original Pts are based off the startSpot Pt, and are
        // unique to each seg, not each linedef
        tempPt->pos[VX] = P_GetFixedp(*tempSeg, DMU_VERTEX1_X) -
                            P_GetFixedp(po, DMU_START_SPOT_X);
        tempPt->pos[VY] = P_GetFixedp(*tempSeg, DMU_VERTEX1_Y) -
                            P_GetFixedp(po, DMU_START_SPOT_Y);
    }
    avg.pos[VX] /= poNumSegs;
    avg.pos[VY] /= poNumSegs;
    sub = R_PointInSubsector(avg.pos[VX] << FRACBITS, avg.pos[VY] << FRACBITS);
    if(P_GetPtrp(sub, DMU_POLYOBJ) != NULL)
    {
        Con_Message("PO_TranslateToStartSpot: Warning: Multiple polyobjs in a single subsector\n"
                    "  (ssec %i, sector %i). Previous polyobj overridden.\n",
                    P_ToIndex(sub), P_GetIntp(sub, DMU_SECTOR));
    }
    P_SetPtrp(sub, DMU_POLYOBJ, po);
}

//==========================================================================
//
// PO_Init
// FIXME: DJS - We shouldn't need to go twice round the thing list here too
//==========================================================================

void PO_Init(int lump)
{
    int     i;
    thing_t *mt;
    int     polyIndex;
    fixed_t x, y;
    short   angle, type;

    // ThrustMobj will handle polyobj <-> mobj interaction.
    PO_SetCallback(ThrustMobj);

    // Engine maintains the array of polyobjs and knows how large polyobj_t
    // actually is, so we let the engine allocate memory for them.
    PO_Allocate();

    mt = things;
    polyIndex = 0;              // index polyobj number

    //Con_Message( "Find startSpot points.\n");

    // Find the startSpot points, and spawn each polyobj
    for(i = 0; i < numthings; ++i, mt++)
    {
        x = mt->x;
        y = mt->y;
        angle = mt->angle;
        type = mt->type;

        // 3001 = no crush, 3002 = crushing
        if(type == PO_SPAWN_TYPE ||
           type == PO_SPAWNCRUSH_TYPE)
        {                       // Polyobj StartSpot Pt.
            P_SetInt(DMU_POLYOBJ, polyIndex, DMU_START_SPOT_X, x);
            P_SetInt(DMU_POLYOBJ, polyIndex, DMU_START_SPOT_Y, y);
            SpawnPolyobj(polyIndex, angle,
                         (type == PO_SPAWNCRUSH_TYPE));
            polyIndex++;
        }
    }

    mt = things;
    for(i = 0; i < numthings; ++i, mt++)
    {
        x = mt->x;
        y = mt->y;
        angle = mt->angle;
        type = mt->type;

        if(type == PO_ANCHOR_TYPE)
        {                       // Polyobj Anchor Pt.
            TranslateToStartSpot(angle, x << FRACBITS, y << FRACBITS);
        }
    }

    // check for a startspot without an anchor point
    for(i = 0; i < DD_GetInteger(DD_POLYOBJ_COUNT); ++i)
    {
        if(!P_GetPtr(DMU_POLYOBJ, i, DMU_ORIGINAL_POINTS))
        {
            Con_Error
                ("PO_Init: StartSpot located without an Anchor point: %d\n",
                 P_GetInt(DMU_POLYOBJ, i, DMU_TAG));
        }
    }
    InitBlockMap();
}

//==========================================================================
//
// PO_Busy
//
//==========================================================================

boolean PO_Busy(int polyobj)
{
    return P_GetPtrp(GetPolyobj(polyobj), DMU_SPECIAL_DATA) != NULL;
}
