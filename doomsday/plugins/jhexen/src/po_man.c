/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Kernen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 *
 * \bug PO_TranslateToStartSpot:  Multiple polyobjs in a single subsector -> https://sourceforge.net/tracker/?func=detail&atid=542099&aid=1530158&group_id=74815
 *
 */

/**
 * po_man.c: Polyobject management.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_mapsetup.h"
#include "p_map.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

/* Returns the special data pointer of the polyobj. */
#define PO_SpecialData(polyPtr)     P_GetPtrp((polyPtr), DMU_SPECIAL_DATA)
#define PO_Tag(polyPtr)             P_GetIntp((polyPtr), DMU_TAG)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static polyobj_t *GetPolyobj(uint polyNum);
static int GetPolyobjMirror(uint polyNum);
static void ThrustMobj(mobj_t *mobj, seg_t *seg, polyobj_t *po);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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

void PO_SetDestination(polyobj_t *poly, float dist, uint an, float speed)
{
    ddvertexf_t startSpot;

    P_GetFloatpv(poly, DMU_START_SPOT_XY, startSpot.pos);

    P_SetFloatp(poly, DMU_DESTINATION_X,
                startSpot.pos[VX] + dist * FIX2FLT(finecosine[an]));
    P_SetFloatp(poly, DMU_DESTINATION_Y,
                startSpot.pos[VY] + dist * FIX2FLT(finesine[an]));
    P_SetFloatp(poly, DMU_SPEED, speed);
}

// ===== Polyobj Event Code =====

void T_RotatePoly(polyevent_t *pe)
{
    unsigned int absSpeed;
    polyobj_t  *poly;

    if(PO_RotatePolyobj(pe->polyobj, pe->intSpeed))
    {
        absSpeed = abs(pe->intSpeed);

        if(pe->dist == -1)
        {   // perpetual polyobj.
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
            pe->intSpeed = pe->dist * (pe->intSpeed < 0 ? -1 : 1);
        }
    }
}

boolean EV_RotatePoly(line_t *line, byte *args, int direction,
                      boolean overRide)
{
    int         mirror, polyNum;
    polyevent_t *pe;
    polyobj_t  *poly;

    polyNum = args[0];
    poly = GetPolyobj(polyNum);
    if(poly)
    {
        if(PO_SpecialData(poly) && !overRide)
        {   // Poly is already moving, so keep going...
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

    pe->intSpeed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
    P_SetPtrp(poly, DMU_SPECIAL_DATA, pe);
    P_SetAnglep(poly, DMU_ANGLE_SPEED, pe->intSpeed);
    PO_StartSequence(poly, SEQ_DOOR_STONE);

    while((mirror = GetPolyobjMirror(polyNum)) != 0)
    {
        poly = GetPolyobj(mirror);
        if(poly && PO_SpecialData(poly) && !overRide)
        {   // Mirroring poly is already in motion.
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
        pe->intSpeed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
        P_SetAnglep(poly, DMU_ANGLE_SPEED, pe->intSpeed);

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

void T_MovePoly(polyevent_t *pe)
{
    unsigned int absSpeed;
    polyobj_t  *poly;

    if(PO_MovePolyobj(pe->polyobj, pe->speed[MX], pe->speed[MY]))
    {
        absSpeed = abs(pe->intSpeed);
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
            P_SetFloatp(poly, DMU_SPEED, 0);
        }

        if(pe->dist < absSpeed)
        {
            pe->intSpeed = pe->dist * (pe->intSpeed < 0 ? -1 : 1);
            pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
            pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
        }
    }
}

boolean EV_MovePoly(line_t *line, byte *args, boolean timesEight,
                    boolean overRide)
{
    int         mirror, polyNum;
    polyevent_t *pe;
    polyobj_t  *poly;
    angle_t     angle;

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
    pe->intSpeed = args[1] * (FRACUNIT / 8);
    P_SetPtrp(poly, DMU_SPECIAL_DATA, pe);

    angle = args[2] * (ANGLE_90 / 64);

    pe->fangle = angle >> ANGLETOFINESHIFT;
    pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
    pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
    PO_StartSequence(poly, SEQ_DOOR_STONE);

    PO_SetDestination(poly, FIX2FLT(pe->dist), pe->fangle, FIX2FLT(pe->intSpeed));

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
        pe->intSpeed = args[1] * (FRACUNIT / 8);
        angle = angle + ANGLE_180;    // reverse the angle
        pe->fangle = angle >> ANGLETOFINESHIFT;
        pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
        pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
        polyNum = mirror;
        PO_StartSequence(poly, SEQ_DOOR_STONE);

        PO_SetDestination(poly, FIX2FLT(pe->dist), pe->fangle, FIX2FLT(pe->intSpeed));
    }
    return true;
}

void T_PolyDoor(polydoor_t *pd)
{
    int         absSpeed;
    polyobj_t  *poly;

    if(pd->tics)
    {
        if(!--pd->tics)
        {
            poly = GetPolyobj(pd->polyobj);
            PO_StartSequence(poly, SEQ_DOOR_STONE);

            // Movement is about to begin. Update the destination.
            PO_SetDestination(GetPolyobj(pd->polyobj), FIX2FLT(pd->dist),
                              pd->direction, FIX2FLT(pd->intSpeed));
        }
        return;
    }

    switch(pd->type)
    {
    case PODOOR_SLIDE:
        if(PO_MovePolyobj(pd->polyobj, pd->speed[MX], pd->speed[MY]))
        {
            absSpeed = abs(pd->intSpeed);
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
                    pd->speed[MX] = -pd->speed[MX];
                    pd->speed[MY] = -pd->speed[MY];
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
            {   // continue moving if the poly is a crusher, or is opening
                return;
            }
            else
            {   // open back up
                pd->dist = pd->totalDist - pd->dist;
                pd->direction =
                    (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                pd->speed[MX] = -pd->speed[MX];
                pd->speed[MY] = -pd->speed[MY];
                // Update destination.
                PO_SetDestination(GetPolyobj(pd->polyobj), FIX2FLT(pd->dist),
                                  pd->direction, FIX2FLT(pd->intSpeed));
                pd->close = false;
                PO_StartSequence(poly, SEQ_DOOR_STONE);
            }
        }
        break;

    case PODOOR_SWING:
        if(PO_RotatePolyobj(pd->polyobj, pd->intSpeed))
        {
            absSpeed = abs(pd->intSpeed);
            if(pd->dist == -1)
            {   // perpetual polyobj
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
                    pd->intSpeed = -pd->intSpeed;
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
            {   // continue moving if the poly is a crusher, or is opening
                return;
            }
            else
            {   // open back up and rewait
                pd->dist = pd->totalDist - pd->dist;
                pd->intSpeed = -pd->intSpeed;
                pd->close = false;
                PO_StartSequence(poly, SEQ_DOOR_STONE);
            }
        }
        break;

    default:
        break;
    }
}

boolean EV_OpenPolyDoor(line_t *line, byte *args, podoortype_t type)
{
    int         mirror, polyNum;
    polydoor_t *pd;
    polyobj_t  *poly;
    angle_t     angle = 0;

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
        pd->intSpeed = args[1] * (FRACUNIT / 8);
        pd->totalDist = args[3] * FRACUNIT; // Distance
        pd->dist = pd->totalDist;
        angle = args[2] * (ANGLE_90 / 64);
        pd->direction = angle >> ANGLETOFINESHIFT;
        pd->speed[MX] = FIX2FLT(FixedMul(pd->intSpeed, finecosine[pd->direction]));
        pd->speed[MY] = FIX2FLT(FixedMul(pd->intSpeed, finesine[pd->direction]));
        PO_StartSequence(poly, SEQ_DOOR_STONE);
    }
    else if(type == PODOOR_SWING)
    {
        pd->waitTics = args[3];
        pd->direction = 1;      // ADD:  PODOOR_SWINGL, PODOOR_SWINGR
        pd->intSpeed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
        pd->totalDist = args[2] * (ANGLE_90 / 64);
        pd->dist = pd->totalDist;
        PO_StartSequence(poly, SEQ_DOOR_STONE);
    }

    P_SetPtrp(poly, DMU_SPECIAL_DATA, pd);
    PO_SetDestination(poly, FIX2FLT(pd->dist), pd->direction, FIX2FLT(pd->intSpeed));

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
            pd->intSpeed = args[1] * (FRACUNIT / 8);
            pd->totalDist = args[3] * FRACUNIT; // Distance
            pd->dist = pd->totalDist;
            angle = angle + ANGLE_180;    // reverse the angle
            pd->direction = angle >> ANGLETOFINESHIFT;
            pd->speed[MX] = FIX2FLT(FixedMul(pd->intSpeed, finecosine[pd->direction]));
            pd->speed[MY] = FIX2FLT(FixedMul(pd->intSpeed, finesine[pd->direction]));
            PO_StartSequence(poly, SEQ_DOOR_STONE);
        }
        else if(type == PODOOR_SWING)
        {
            pd->waitTics = args[3];
            pd->direction = -1; // ADD:  same as above
            pd->intSpeed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
            pd->totalDist = args[2] * (ANGLE_90 / 64);
            pd->dist = pd->totalDist;
            PO_StartSequence(poly, SEQ_DOOR_STONE);
        }
        polyNum = mirror;
        PO_SetDestination(poly, FIX2FLT(pd->dist), pd->direction, FIX2FLT(pd->intSpeed));
    }

    return true;
}

// ===== Higher Level Poly Interface code =====

static polyobj_t *GetPolyobj(uint polyNum)
{
    uint        i;

    for(i = 0; i < numpolyobjs; ++i)
    {
        if(P_GetInt(DMU_POLYOBJ, i, DMU_TAG) == polyNum)
        {
            return P_ToPtr(DMU_POLYOBJ, i);
        }
    }
    return NULL;
}

static int GetPolyobjMirror(uint poly)
{
    uint        i;

    for(i = 0; i < numpolyobjs; ++i)
    {
        if(P_GetInt(DMU_POLYOBJ, i, DMU_TAG) == poly)
        {
            seg_t* seg = P_GetPtrp(P_ToPtr(DMU_POLYOBJ, i), DMU_SEG_OF_POLYOBJ | 0);
            line_t* linedef = P_GetPtrp(seg, DMU_LINE);
            return P_ToXLine(linedef)->arg2;
        }
    }

    return 0;
}

static void ThrustMobj(mobj_t *mobj, seg_t *seg, polyobj_t * po)
{
    uint        thrustAn;
    float       thrustX, thrustY, force;
    polyevent_t *pe;

    // Clients do no polyobj <-> mobj interaction.
    if(IS_CLIENT)
        return;

    if(P_IsCamera(mobj)) // Cameras don't interact with polyobjs.
        return;

    if(!(mobj->flags & MF_SHOOTABLE) && !mobj->player)
        return;

    thrustAn =
        (P_GetAnglep(seg, DMU_ANGLE) - ANGLE_90) >> ANGLETOFINESHIFT;

    pe = PO_SpecialData(po);
    if(pe)
    {
        if(pe->thinker.function == T_RotatePoly)
        {
            force = FIX2FLT(pe->intSpeed >> 8);
        }
        else
        {
            force = FIX2FLT(pe->intSpeed >> 3);
        }

        if(force < 1)
        {
            force = 1;
        }
        else if(force > 4)
        {
            force = 4;
        }
    }
    else
    {
        force = 1;
    }

    thrustX = force * FIX2FLT(finecosine[thrustAn]);
    thrustY = force * FIX2FLT(finesine[thrustAn]);
    mobj->mom[MX] += thrustX;
    mobj->mom[MY] += thrustY;

    if(P_GetBoolp(po, DMU_CRUSH))
    {
        if(!P_CheckPosition2f(mobj, mobj->pos[VX] + thrustX,
                              mobj->pos[VY] + thrustY))
        {
            P_DamageMobj(mobj, NULL, NULL, 3);
        }
    }
}

static void translateToStartSpot(polyobj_t *po, float originX, float originY)
{
    uint            i;
    float           delta[2];
    ddvertexf_t     avg; // Used to find a polyobj's center, and hence subsector.
    seg_t         **tempSeg, **veryTempSeg;
    ddvertexf_t    *tempPt;
    subsector_t    *sub;
    int             poNumSegs = 0;

    if(P_GetPtrp(po, DMU_SEG_LIST) == NULL)
    {
        Con_Error
            ("translateToStartSpot:  Anchor point located without a "
             "StartSpot point: %d\n", P_ToIndex(po));
    }
    poNumSegs = P_GetIntp(po, DMU_SEG_COUNT);

    P_SetPtrp(po, DMU_ORIGINAL_POINTS,
              Z_Malloc(poNumSegs * sizeof(ddvertex_t), PU_LEVEL, 0));
    P_SetPtrp(po, DMU_PREVIOUS_POINTS,
              Z_Malloc(poNumSegs * sizeof(ddvertex_t), PU_LEVEL, 0));
    delta[VX] = originX - P_GetFloatp(po, DMU_START_SPOT_X);
    delta[VY] = originY - P_GetFloatp(po, DMU_START_SPOT_Y);

    tempSeg = P_GetPtrp(po, DMU_SEG_LIST);
    tempPt = P_GetPtrp(po, DMU_ORIGINAL_POINTS);
    avg.pos[VX] = 0;
    avg.pos[VY] = 0;

    VALIDCOUNT++;
    for(i = 0; i < (uint) poNumSegs; ++i, tempSeg++, tempPt++)
    {
        line_t* linedef = P_GetPtrp(*tempSeg, DMU_LINE);
        if(P_GetIntp(linedef, DMU_VALID_COUNT) != VALIDCOUNT)
        {
            float          *bbox = P_GetPtrp(linedef, DMU_BOUNDING_BOX);
            bbox[BOXTOP]    -= delta[VY];
            bbox[BOXBOTTOM] -= delta[VY];
            bbox[BOXLEFT]   -= delta[VX];
            bbox[BOXRIGHT]  -= delta[VX];
            P_SetIntp(linedef, DMU_VALID_COUNT, VALIDCOUNT);
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
        {   // The point hasn't been translated, yet.
            float           v1[2];

            P_GetFloatpv(*tempSeg, DMU_VERTEX1_XY, v1);
            v1[0] -= delta[VX];
            v1[1] -= delta[VY];
            P_SetFloatpv(*tempSeg, DMU_VERTEX1_XY, v1);
        }

        avg.pos[VX] += P_GetFloatp(*tempSeg, DMU_VERTEX1_X);
        avg.pos[VY] += P_GetFloatp(*tempSeg, DMU_VERTEX1_Y);
        // The original Pts are based off the startSpot Pt, and are unique
        // to each seg, not each linedef.
        tempPt->pos[VX] = P_GetFloatp(*tempSeg, DMU_VERTEX1_X) -
                            P_GetFloatp(po, DMU_START_SPOT_X);
        tempPt->pos[VY] = P_GetFloatp(*tempSeg, DMU_VERTEX1_Y) -
                            P_GetFloatp(po, DMU_START_SPOT_Y);
    }

    avg.pos[VX] /= poNumSegs;
    avg.pos[VY] /= poNumSegs;
    sub = R_PointInSubsector(avg.pos[VX], avg.pos[VY]);
    if(P_GetPtrp(sub, DMU_POLYOBJ) != NULL)
    {
        Con_Message("translateToStartSpot: Warning: Multiple polyobjs in a single subsector\n"
                    "  (ssec %i, sector %i). Previous polyobj overridden.\n",
                    P_ToIndex(sub), P_GetIntp(sub, DMU_SECTOR));
    }
    P_SetPtrp(sub, DMU_POLYOBJ, po);
}

/**
 * Initialize all polyobjects in the current map.
 */
void PO_InitForMap(void)
{
    uint            i;

    Con_Message("PO_Init: Initializing polyobjects.\n");

    // ThrustMobj will handle polyobj <-> mobj interaction.
    PO_SetCallback(ThrustMobj);

    // Move all polyobjects to their start positions.
    for(i = 0; i < numpolyobjs; ++i)
    {
        int                 j, tag;
        spawnspot_t        *mt;

        tag = P_GetInt(DMU_POLYOBJ, i, DMU_TAG);

        // Find the anchor associated with this polyobj and move there.
        j = 0;
        mt = NULL;
        while(j < numthings && !mt)
        {
            if(things[j].type == PO_ANCHOR_TYPE && things[j].angle == tag)
            {   // Polyobj Anchor Pt.
                mt = &things[j];
            }
            else
            {
                j++;
            }
        }

        if(mt)
        {
            translateToStartSpot(P_ToPtr(DMU_POLYOBJ, i),
                                 mt->pos[VX], mt->pos[VY]);
        }
        else
        {
            Con_Message("PO_Init: Warning, missing anchor for poly %i.", i);
        }
    }
}

boolean PO_Busy(int polyobj)
{
    return P_GetPtrp(GetPolyobj(polyobj), DMU_SPECIAL_DATA) != NULL;
}
