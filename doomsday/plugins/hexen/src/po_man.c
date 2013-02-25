/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * po_man.c: Polyobject management.
 */

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_map.h"
#include "g_common.h"

static int getPolyobjMirror(uint polyNum);
static void thrustMobj(struct mobj_s* mo, void* linep, void* pop);

Polyobj* P_GetPolyobj(uint num)
{
    // By unique ID?
    if(num & 0x80000000)
    {
        return P_PolyobjByID(num & 0x7fffffff);
    }

    // By tag.
    return P_PolyobjByTag((int)num);
}

void PO_StartSequence(Polyobj* po, int seqBase)
{
    SN_StartSequence((mobj_t*) po, seqBase + po->seqType);
}

void PO_StopSequence(Polyobj* po)
{
    SN_StopSequence((mobj_t*) po);
}

void PO_SetDestination(Polyobj* po, coord_t dist, uint fineAngle, float speed)
{
    assert(fineAngle < FINEANGLES);
    po->dest[VX] = po->origin[VX] + dist * FIX2FLT(finecosine[fineAngle]);
    po->dest[VY] = po->origin[VY] + dist * FIX2FLT(finesine[fineAngle]);
    po->speed    = speed;
}

void PODoor_UpdateDestination(polydoor_t* pd)
{
    Polyobj* po = P_GetPolyobj(pd->polyobj);

    // Only sliding doors need the destination info. (Right? -jk)
    if(pd->type == PODOOR_SLIDE)
    {
        PO_SetDestination(po, FIX2FLT(pd->dist), pd->direction, FIX2FLT(pd->intSpeed));
    }
}

// ===== Polyobj Event Code =====

void T_RotatePoly(void *polyThinker)
{
    polyevent_t* pe = polyThinker;
    unsigned int absSpeed;
    Polyobj* po = P_GetPolyobj(pe->polyobj);

    if(P_PolyobjRotate(po, pe->intSpeed))
    {
        absSpeed = abs(pe->intSpeed);

        if(pe->dist == -1)
        {   // perpetual polyobj.
            return;
        }

        pe->dist -= absSpeed;
        if(pe->dist <= 0)
        {
            if(po->specialData == pe)
                po->specialData = NULL;

            PO_StopSequence(po);
            P_PolyobjFinished(po->tag);
            Thinker_Remove(&pe->thinker);
            po->angleSpeed = 0;
        }

        if(pe->dist < absSpeed)
        {
            pe->intSpeed = pe->dist * (pe->intSpeed < 0 ? -1 : 1);
        }
    }
}

boolean EV_RotatePoly(LineDef *line, byte *args, int direction,
                      boolean overRide)
{
    int                 mirror, polyNum;
    polyevent_t*        pe;
    Polyobj*            po;

    polyNum = args[0];
    po = P_GetPolyobj(polyNum);
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

    pe = Z_Calloc(sizeof(*pe), PU_MAP, 0);
    pe->thinker.function = T_RotatePoly;
    Thinker_Add(&pe->thinker);

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
        po = P_GetPolyobj(mirror);
        if(po && po->specialData && !overRide)
        {   // Mirroring po is already in motion.
            break;
        }

        pe = Z_Calloc(sizeof(*pe), PU_MAP, 0);
        pe->thinker.function = T_RotatePoly;
        Thinker_Add(&pe->thinker);

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

        po = P_GetPolyobj(polyNum);
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

void T_MovePoly(void *polyThinker)
{
    polyevent_t *pe = polyThinker;
    unsigned int        absSpeed;
    Polyobj*            po = P_GetPolyobj(pe->polyobj);

    if(P_PolyobjMoveXY(po, pe->speed[MX], pe->speed[MY]))
    {
        absSpeed = abs(pe->intSpeed);
        pe->dist -= absSpeed;
        if(pe->dist <= 0)
        {
            if(po->specialData == pe)
                po->specialData = NULL;

            PO_StopSequence(po);
            P_PolyobjFinished(po->tag);
            Thinker_Remove(&pe->thinker);
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

boolean EV_MovePoly(LineDef* line, byte* args, boolean timesEight,
                    boolean overRide)
{
    int                 mirror, polyNum;
    polyevent_t*        pe;
    Polyobj*            po;
    angle_t             angle;

    polyNum = args[0];
    po = P_GetPolyobj(polyNum);
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

    pe = Z_Calloc(sizeof(*pe), PU_MAP, 0);
    pe->thinker.function = T_MovePoly;
    Thinker_Add(&pe->thinker);

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
        po = P_GetPolyobj(mirror);
        if(po && po->specialData && !overRide)
        {                       // mirroring po is already in motion
            break;
        }

        pe = Z_Calloc(sizeof(*pe), PU_MAP, 0);
        pe->thinker.function = T_MovePoly;
        Thinker_Add(&pe->thinker);

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

void T_PolyDoor(void *polyDoorThinker)
{
    polydoor_t* pd = polyDoorThinker;
    int                 absSpeed;
    Polyobj*            po = P_GetPolyobj(pd->polyobj);

    if(pd->tics)
    {
        if(!--pd->tics)
        {
            PO_StartSequence(po, SEQ_DOOR_STONE);

            // Movement is about to begin. Update the destination.
            PODoor_UpdateDestination(pd);
        }
        return;
    }

    switch(pd->type)
    {
    case PODOOR_SLIDE:
        if(P_PolyobjMoveXY(po, pd->speed[MX], pd->speed[MY]))
        {
            absSpeed = abs(pd->intSpeed);
            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
                PO_StopSequence(po);
                if(!pd->close)
                {
                    pd->dist = pd->totalDist;
                    pd->close = true;
                    pd->tics = pd->waitTics;
                    pd->direction = (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                    pd->speed[MX] = -pd->speed[MX];
                    pd->speed[MY] = -pd->speed[MY];
                }
                else
                {
                    if(po->specialData == pd)
                        po->specialData = NULL;

                    P_PolyobjFinished(po->tag);
                    Thinker_Remove(&pd->thinker);
                }
            }
        }
        else
        {
            if(po->crush || !pd->close)
            {   // Continue moving if the po is a crusher, or is opening.
                return;
            }
            else
            {   // Open back up.
                pd->dist = pd->totalDist - pd->dist;
                pd->direction = (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                pd->speed[MX] = -pd->speed[MX];
                pd->speed[MY] = -pd->speed[MY];
                // Update destination.
                PODoor_UpdateDestination(pd);
                pd->close = false;
                PO_StartSequence(po, SEQ_DOOR_STONE);
            }
        }
        break;

    case PODOOR_SWING:
        if(P_PolyobjRotate(po, pd->intSpeed))
        {
            absSpeed = abs(pd->intSpeed);
            if(pd->dist == -1)
            {   // Perpetual polyobj.
                return;
            }

            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
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
                    Thinker_Remove(&pd->thinker);
                }
            }
        }
        else
        {
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

boolean EV_OpenPolyDoor(LineDef* line, byte* args, podoortype_t type)
{
    int                 mirror, polyNum;
    polydoor_t*         pd;
    Polyobj*            po;
    angle_t             angle = 0;

    polyNum = args[0];
    po = P_GetPolyobj(polyNum);
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

    pd = Z_Calloc(sizeof(*pd), PU_MAP, 0);
    pd->thinker.function = T_PolyDoor;
    Thinker_Add(&pd->thinker);

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
    PODoor_UpdateDestination(pd);

    while((mirror = getPolyobjMirror(polyNum)) != 0)
    {
        po = P_GetPolyobj(mirror);
        if(po && po->specialData)
        {   // Mirroring po is already in motion.
            break;
        }

        pd = Z_Calloc(sizeof(*pd), PU_MAP, 0);
        pd->thinker.function = T_PolyDoor;
        Thinker_Add(&pd->thinker);

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
        PODoor_UpdateDestination(pd);
    }

    return true;
}

// ===== Higher Level Poly Interface code =====

static int getPolyobjMirror(uint poly)
{
    uint i;

    for(i = 0; i < numpolyobjs; ++i)
    {
        Polyobj* po = P_GetPolyobj(i | 0x80000000);

        if(po->tag == poly)
        {
            LineDef* line = po->lines[0];
            return P_ToXLine(line)->arg2;
        }
    }

    return 0;
}

static void thrustMobj(struct mobj_s* mo, void* linep, void* pop)
{
    LineDef* line = (LineDef*) linep;
    Polyobj* po = (Polyobj*) pop;
    coord_t thrust[2], force;
    polyevent_t* pe;
    uint thrustAn;

    // Clients do no polyobj <-> mobj interaction.
    if(IS_CLIENT) return;

    // Cameras don't interact with polyobjs.
    if(P_MobjIsCamera(mo)) return;

    if(!(mo->flags & MF_SHOOTABLE) && !mo->player) return;

    thrustAn = (P_GetAnglep(line, DMU_ANGLE) - ANGLE_90) >> ANGLETOFINESHIFT;

    pe = (polyevent_t*) po->specialData;
    if(pe)
    {
        if(pe->thinker.function == (thinkfunc_t) T_RotatePoly)
        {
            force = FIX2FLT(pe->intSpeed >> 8);
        }
        else
        {
            force = FIX2FLT(pe->intSpeed >> 3);
        }

        force = MINMAX_OF(1, force, 4);
    }
    else
    {
        force = 1;
    }

    thrust[VX] = force * FIX2FLT(finecosine[thrustAn]);
    thrust[VY] = force * FIX2FLT(finesine[thrustAn]);
    mo->mom[MX] += thrust[VX];
    mo->mom[MY] += thrust[VY];

    if(po->crush)
    {
        if(!P_CheckPositionXY(mo, mo->origin[VX] + thrust[VX], mo->origin[VY] + thrust[VY]))
        {
            P_DamageMobj(mo, NULL, NULL, 3, false);
        }
    }
}

/**
 * Initialize all polyobjects in the current map.
 */
void PO_InitForMap(void)
{
    uint                i;

    Con_Message("PO_InitForMap: Initializing polyobjects.");

    // thrustMobj will handle polyobj <-> mobj interaction.
    P_SetPolyobjCallback(thrustMobj);
    for(i = 0; i < numpolyobjs; ++i)
    {
        uint j;
        const mapspot_t* spot;
        Polyobj* po;

        po = P_GetPolyobj(i | 0x80000000);

        // Init game-specific properties.
        po->specialData = NULL;

        // Find the mapspot associated with this polyobj.
        j = 0;
        spot = NULL;
        while(j < numMapSpots && !spot)
        {
            if((mapSpots[j].doomEdNum == PO_SPAWN_DOOMEDNUM ||
                mapSpots[j].doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM) &&
               mapSpots[j].angle == po->tag)
            {
                // Polyobj mapspot.
                spot = &mapSpots[j];
            }
            else
            {
                j++;
            }
        }

        if(spot)
        {
            po->crush = (spot->doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM? 1 : 0);
            P_PolyobjMoveXY(po, -po->origin[VX] + spot->origin[VX],
                                -po->origin[VY] + spot->origin[VY]);
        }
        else
        {
            Con_Message("Warning: Missing spawn spot for PolyObj #%i, ignoring.", i);
        }
    }
}

boolean PO_Busy(int polyobj)
{
    Polyobj* po = P_GetPolyobj(polyobj);

    if(po && po->specialData != NULL)
        return true;

    return false;
}
