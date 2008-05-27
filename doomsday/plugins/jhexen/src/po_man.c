/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * po_man.c: Polyobject management.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_mapsetup.h"
#include "p_map.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int getPolyobjMirror(uint polyNum);
static void thrustMobj(mobj_t *mobj, seg_t *seg, polyobj_t *po);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void PO_StartSequence(polyobj_t *po, int seqBase)
{
    SN_StartSequence((mobj_t*) &po->startSpot, seqBase + po->seqType);
}

void PO_StopSequence(polyobj_t *po)
{
    SN_StopSequence((mobj_t*) &po->startSpot);
}

void PO_SetDestination(polyobj_t *po, float dist, uint an, float speed)
{
    po->dest[VX] = po->startSpot.pos[VX] + dist * FIX2FLT(finecosine[an]);
    po->dest[VY] = po->startSpot.pos[VY] + dist * FIX2FLT(finesine[an]);
    po->speed = speed;
}

// ===== Polyobj Event Code =====

void T_RotatePoly(polyevent_t *pe)
{
    unsigned int absSpeed;
    polyobj_t  *poly;

    if(P_PolyobjRotate(pe->polyobj, pe->intSpeed))
    {
        absSpeed = abs(pe->intSpeed);

        if(pe->dist == -1)
        {   // perpetual polyobj.
            return;
        }

        pe->dist -= absSpeed;
        if(pe->dist <= 0)
        {
            poly = PO_GetPolyobj(pe->polyobj);

            if(poly->specialData == pe)
                poly->specialData = NULL;

            PO_StopSequence(poly);
            P_PolyobjFinished(poly->tag);
            P_RemoveThinker(&pe->thinker);
            poly->angleSpeed = 0;
        }

        if(pe->dist < absSpeed)
        {
            pe->intSpeed = pe->dist * (pe->intSpeed < 0 ? -1 : 1);
        }
    }
}

boolean EV_RotatePoly(linedef_t *line, byte *args, int direction,
                      boolean overRide)
{
    int                 mirror, polyNum;
    polyevent_t*        pe;
    polyobj_t*          po;

    polyNum = args[0];
    po = PO_GetPolyobj(polyNum);
    if(po)
    {
        if(po->specialData && !overRide)
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
            po->destAngle = -1;
        }
        else
        {
            pe->dist = args[2] * (ANGLE_90 / 64);   // Angle
            po->destAngle = po->angle + pe->dist * direction;
        }
    }
    else
    {
        pe->dist = ANGLE_MAX - 1;
        po->destAngle = po->angle + pe->dist;
    }

    pe->intSpeed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
    po->specialData = pe;
    po->angleSpeed = pe->intSpeed;
    PO_StartSequence(po, SEQ_DOOR_STONE);

    while((mirror = getPolyobjMirror(polyNum)) != 0)
    {
        po = PO_GetPolyobj(mirror);
        if(po && po->specialData && !overRide)
        {   // Mirroring po is already in motion.
            break;
        }

        pe = Z_Malloc(sizeof(polyevent_t), PU_LEVSPEC, 0);
        P_AddThinker(&pe->thinker);
        pe->thinker.function = T_RotatePoly;
        po->specialData = pe;
        pe->polyobj = mirror;
        if(args[2])
        {
            if(args[2] == 255)
            {
                pe->dist = -1;
                po->destAngle = -1;
            }
            else
            {
                pe->dist = args[2] * (ANGLE_90 / 64); // Angle
                po->destAngle = po->angle + pe->dist * -direction;
            }
        }
        else
        {
            pe->dist = ANGLE_MAX - 1;
            po->destAngle =  po->angle + pe->dist;
        }
        direction = -direction;
        pe->intSpeed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
        po->angleSpeed = pe->intSpeed;

        po = PO_GetPolyobj(polyNum);
        if(po)
        {
            po->specialData = pe;
        }
        else
        {
            Con_Error("EV_RotatePoly:  Invalid polyobj num: %d\n", polyNum);
        }

        polyNum = mirror;
        PO_StartSequence(po, SEQ_DOOR_STONE);
    }
    return true;
}

void T_MovePoly(polyevent_t* pe)
{
    unsigned int        absSpeed;
    polyobj_t*          po;

    if(P_PolyobjMove(pe->polyobj, pe->speed[MX], pe->speed[MY]))
    {
        absSpeed = abs(pe->intSpeed);
        pe->dist -= absSpeed;
        if(pe->dist <= 0)
        {
            po = PO_GetPolyobj(pe->polyobj);

            if(po->specialData == pe)
                po->specialData = NULL;

            PO_StopSequence(po);
            P_PolyobjFinished(po->tag);
            P_RemoveThinker(&pe->thinker);
            po->speed = 0;
        }

        if(pe->dist < absSpeed)
        {
            pe->intSpeed = pe->dist * (pe->intSpeed < 0 ? -1 : 1);
            pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
            pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
        }
    }
}

boolean EV_MovePoly(linedef_t* line, byte* args, boolean timesEight,
                    boolean overRide)
{
    int                 mirror, polyNum;
    polyevent_t*        pe;
    polyobj_t*          po;
    angle_t             angle;

    polyNum = args[0];
    po = PO_GetPolyobj(polyNum);
    if(po)
    {
        if(po->specialData && !overRide)
        {   // Is already moving.
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
    po->specialData = pe;

    angle = args[2] * (ANGLE_90 / 64);

    pe->fangle = angle >> ANGLETOFINESHIFT;
    pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
    pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
    PO_StartSequence(po, SEQ_DOOR_STONE);

    PO_SetDestination(po, FIX2FLT(pe->dist), pe->fangle, FIX2FLT(pe->intSpeed));

    while((mirror = getPolyobjMirror(polyNum)) != 0)
    {
        po = PO_GetPolyobj(mirror);
        if(po && po->specialData && !overRide)
        {                       // mirroring po is already in motion
            break;
        }

        pe = Z_Malloc(sizeof(polyevent_t), PU_LEVSPEC, 0);
        P_AddThinker(&pe->thinker);
        pe->thinker.function = T_MovePoly;
        pe->polyobj = mirror;
        po->specialData = pe;
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
        PO_StartSequence(po, SEQ_DOOR_STONE);

        PO_SetDestination(po, FIX2FLT(pe->dist), pe->fangle, FIX2FLT(pe->intSpeed));
    }
    return true;
}

void T_PolyDoor(polydoor_t* pd)
{
    int             absSpeed;
    polyobj_t*      po;

    if(pd->tics)
    {
        if(!--pd->tics)
        {
            po = PO_GetPolyobj(pd->polyobj);
            PO_StartSequence(po, SEQ_DOOR_STONE);

            // Movement is about to begin. Update the destination.
            PO_SetDestination(PO_GetPolyobj(pd->polyobj), FIX2FLT(pd->dist),
                              pd->direction, FIX2FLT(pd->intSpeed));
        }
        return;
    }

    switch(pd->type)
    {
    case PODOOR_SLIDE:
        if(P_PolyobjMove(pd->polyobj, pd->speed[MX], pd->speed[MY]))
        {
            absSpeed = abs(pd->intSpeed);
            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
                po = PO_GetPolyobj(pd->polyobj);
                PO_StopSequence(po);
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
                    if(po->specialData == pd)
                        po->specialData = NULL;

                    P_PolyobjFinished(po->tag);
                    P_RemoveThinker(&pd->thinker);
                }
            }
        }
        else
        {
            po = PO_GetPolyobj(pd->polyobj);
            if(po->crush || !pd->close)
            {   // Continue moving if the po is a crusher, or is opening.
                return;
            }
            else
            {   // Open back up.
                pd->dist = pd->totalDist - pd->dist;
                pd->direction =
                    (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                pd->speed[MX] = -pd->speed[MX];
                pd->speed[MY] = -pd->speed[MY];
                // Update destination.
                PO_SetDestination(PO_GetPolyobj(pd->polyobj), FIX2FLT(pd->dist),
                                  pd->direction, FIX2FLT(pd->intSpeed));
                pd->close = false;
                PO_StartSequence(po, SEQ_DOOR_STONE);
            }
        }
        break;

    case PODOOR_SWING:
        if(P_PolyobjRotate(pd->polyobj, pd->intSpeed))
        {
            absSpeed = abs(pd->intSpeed);
            if(pd->dist == -1)
            {   // Perpetual polyobj.
                return;
            }

            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
                po = PO_GetPolyobj(pd->polyobj);
                PO_StopSequence(po);
                if(!pd->close)
                {
                    pd->dist = pd->totalDist;
                    pd->close = true;
                    pd->tics = pd->waitTics;
                    pd->intSpeed = -pd->intSpeed;
                }
                else
                {
                    if(po->specialData == pd)
                        po->specialData = NULL;

                    P_PolyobjFinished(po->tag);
                    P_RemoveThinker(&pd->thinker);
                }
            }
        }
        else
        {
            po = PO_GetPolyobj(pd->polyobj);
            if(po->crush || !pd->close)
            {   // Continue moving if the po is a crusher, or is opening.
                return;
            }
            else
            {   // Open back up and rewait.
                pd->dist = pd->totalDist - pd->dist;
                pd->intSpeed = -pd->intSpeed;
                pd->close = false;
                PO_StartSequence(po, SEQ_DOOR_STONE);
            }
        }
        break;

    default:
        break;
    }
}

boolean EV_OpenPolyDoor(linedef_t* line, byte* args, podoortype_t type)
{
    int                 mirror, polyNum;
    polydoor_t*         pd;
    polyobj_t*          po;
    angle_t             angle = 0;

    polyNum = args[0];
    po = PO_GetPolyobj(polyNum);
    if(po)
    {
        if(po->specialData)
        {   // Is already moving.
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
        pd->totalDist = args[3] * FRACUNIT; // Distance.
        pd->dist = pd->totalDist;
        angle = args[2] * (ANGLE_90 / 64);
        pd->direction = angle >> ANGLETOFINESHIFT;
        pd->speed[MX] = FIX2FLT(FixedMul(pd->intSpeed, finecosine[pd->direction]));
        pd->speed[MY] = FIX2FLT(FixedMul(pd->intSpeed, finesine[pd->direction]));
        PO_StartSequence(po, SEQ_DOOR_STONE);
    }
    else if(type == PODOOR_SWING)
    {
        pd->waitTics = args[3];
        pd->direction = 1;
        pd->intSpeed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
        pd->totalDist = args[2] * (ANGLE_90 / 64);
        pd->dist = pd->totalDist;
        PO_StartSequence(po, SEQ_DOOR_STONE);
    }

    po->specialData = pd;
    PO_SetDestination(po, FIX2FLT(pd->dist), pd->direction, FIX2FLT(pd->intSpeed));

    while((mirror = getPolyobjMirror(polyNum)) != 0)
    {
        po = PO_GetPolyobj(mirror);
        if(po && po->specialData)
        {   // Mirroring po is already in motion.
            break;
        }

        pd = Z_Malloc(sizeof(polydoor_t), PU_LEVSPEC, 0);
        memset(pd, 0, sizeof(polydoor_t));
        P_AddThinker(&pd->thinker);
        pd->thinker.function = T_PolyDoor;
        pd->polyobj = mirror;
        pd->type = type;
        po->specialData = pd;
        if(type == PODOOR_SLIDE)
        {
            pd->waitTics = args[4];
            pd->intSpeed = args[1] * (FRACUNIT / 8);
            pd->totalDist = args[3] * FRACUNIT; // Distance.
            pd->dist = pd->totalDist;
            angle = angle + ANGLE_180; // Reverse the angle.
            pd->direction = angle >> ANGLETOFINESHIFT;
            pd->speed[MX] = FIX2FLT(FixedMul(pd->intSpeed, finecosine[pd->direction]));
            pd->speed[MY] = FIX2FLT(FixedMul(pd->intSpeed, finesine[pd->direction]));
            PO_StartSequence(po, SEQ_DOOR_STONE);
        }
        else if(type == PODOOR_SWING)
        {
            pd->waitTics = args[3];
            pd->direction = -1;
            pd->intSpeed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
            pd->totalDist = args[2] * (ANGLE_90 / 64);
            pd->dist = pd->totalDist;
            PO_StartSequence(po, SEQ_DOOR_STONE);
        }
        polyNum = mirror;
        PO_SetDestination(po, FIX2FLT(pd->dist), pd->direction, FIX2FLT(pd->intSpeed));
    }

    return true;
}

// ===== Higher Level Poly Interface code =====

static int getPolyobjMirror(uint poly)
{
    uint                i;

    for(i = 0; i < numpolyobjs; ++i)
    {
        polyobj_t*          po = PO_GetPolyobjIdx(i);

        if(po->tag == poly)
        {
            seg_t*              seg = po->segs[0];
            linedef_t*          linedef = P_GetPtrp(seg, DMU_LINEDEF);

            return P_ToXLine(linedef)->arg2;
        }
    }

    return 0;
}

static void thrustMobj(mobj_t* mobj, seg_t* seg, polyobj_t* po)
{
    uint                thrustAn;
    float               thrustX, thrustY, force;
    polyevent_t*        pe;

    // Clients do no polyobj <-> mobj interaction.
    if(IS_CLIENT)
        return;

    if(P_IsCamera(mobj)) // Cameras don't interact with polyobjs.
        return;

    if(!(mobj->flags & MF_SHOOTABLE) && !mobj->player)
        return;

    thrustAn =
        (P_GetAnglep(seg, DMU_ANGLE) - ANGLE_90) >> ANGLETOFINESHIFT;

    pe = (polyevent_t*) po->specialData;
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

    if(po->crush)
    {
        if(!P_CheckPosition2f(mobj, mobj->pos[VX] + thrustX,
                              mobj->pos[VY] + thrustY))
        {
            P_DamageMobj(mobj, NULL, NULL, 3);
        }
    }
}

/**
 * Initialize all polyobjects in the current map.
 */
void PO_InitForMap(void)
{
    uint                i;

    Con_Message("PO_Init: Initializing polyobjects.\n");

    // thrustMobj will handle polyobj <-> mobj interaction.
    PO_SetCallback(thrustMobj);
    for(i = 0; i < numpolyobjs; ++i)
    {
        uint                j;
        spawnspot_t*        mt;
        polyobj_t*          po;

        po = PO_GetPolyobjIdx(i);

        // Init game-specific properties.
        po->specialData = NULL;

        // Find the spawnspot associated with this polyobj.
        j = 0;
        mt = NULL;
        while(j < numthings && !mt)
        {
            if((things[j].type == PO_SPAWN_TYPE ||
                things[j].type == PO_SPAWNCRUSH_TYPE) &&
               things[j].angle == po->tag)
            {   // Polyobj spawnspot.
                mt = &things[j];
            }
            else
            {
                j++;
            }
        }

        if(mt)
        {
            po->crush = ((mt->type == PO_SPAWNCRUSH_TYPE)? 1 : 0);
            P_PolyobjMove(i | 0x80000000,
                          -po->startSpot.pos[VX] + mt->pos[VX],
                          -po->startSpot.pos[VY] + mt->pos[VY]);
        }
        else
        {
            Con_Message("PO_Init: Warning, missing spawnspot for poly %i.", i);
        }
    }
}

boolean PO_Busy(int polyobj)
{
    polyobj_t*          po = PO_GetPolyobj(polyobj);

    if(po && po->specialData != NULL)
        return true;

    return false;
}
