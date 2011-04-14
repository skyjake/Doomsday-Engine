/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_mobj.c: Map Objects
 *
 * Contains various routines for moving mobjs, collision and Z checking.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_audio.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

// Max. distance to move in one call to P_MobjMoveXY.
#define MAXMOVE         30

// Shortest possible movement step.
#define MINMOVE         (1.0f/128)

#define MIN_STEP(d)     ((d) <= MINMOVE && (d) >= -MINMOVE)

#define STOPSPEED       (0.1f/1.6f)

// TYPES -------------------------------------------------------------------

typedef struct {
    mobj_t     *mo;
    vec2_t      box[2];
    int         flags;
    float       pos[3];
    float       height, floorZ, ceilingZ, dropOffZ;
} checkpos_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// If set to true, P_CheckPosition will skip the mobj hit test.
boolean dontHitMobjs = false;

float tmpFloorZ;
float tmpCeilingZ;

// When a mobj is contacted in PIT_CheckMobj, this pointer is set.
// It's reset to NULL in the beginning of P_CheckPosition.
mobj_t *blockingMobj;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Slide variables.
static float bestSlideFrac;
static float secondSlideFrac;
static linedef_t *bestSlideLine;
static linedef_t *secondSlideLine;

static mobj_t *slideMo;

static boolean noFit;

static float tmpDropOffZ;
static float tmpMom[3];

static mobj_t *unusedMobjs = NULL;

// CODE --------------------------------------------------------------------

/**
 * Called during map loading.
 */
void P_InitUnusedMobjList(void)
{
    // Any zone memory allocated for the mobjs will have already been purged.
    unusedMobjs = NULL;
}

/**
 * All mobjs must be allocated through this routine. Part of the public API.
 */
mobj_t* P_MobjCreate(think_t function, float x, float y, float z,
                     angle_t angle, float radius, float height, int ddflags)
{
    mobj_t*             mo;

    if(!function)
        Con_Error("P_MobjCreate: Think function invalid, cannot create mobj.");

#ifdef _DEBUG
    if(isClient)
    {
        Con_Message("P_MobjCreate: Client creating mobj at %f,%f\n", x, y);
    }
#endif

    // Do we have any unused mobjs we can reuse?
    if(unusedMobjs)
    {
        mo = unusedMobjs;
        unusedMobjs = unusedMobjs->sNext;
        memset(mo, 0, MOBJ_SIZE);
    }
    else
    {   // No, we need to allocate another.
        mo = Z_Calloc(MOBJ_SIZE, PU_MAP, NULL);
    }

    mo->pos[VX] = x;
    mo->pos[VY] = y;
    mo->pos[VZ] = z;
    mo->angle = angle;
    mo->visAngle = mo->angle >> 16; // "angle-servo"; smooth actor turning.
    mo->radius = radius;
    mo->height = height;
    mo->ddFlags = ddflags;
    mo->thinker.function = function;
    if(mo->thinker.function)
        P_ThinkerAdd(&mo->thinker, true); // Make it public.

    return mo;
}

/**
 * All mobjs must be destroyed through this routine. Part of the public API.
 *
 * \note Does not actually destroy the mobj. Instead, mobj is marked as
 * awaiting removal (which occurs when its turn for thinking comes around).
 */
void P_MobjDestroy(mobj_t* mo)
{
    // Unlink from sector and block lists.
    P_MobjUnlink(mo);

    S_StopSound(0, mo);

    P_ThinkerRemove((thinker_t *) mo);
}

/**
 * Called when a mobj is actually removed (when it's thinking turn comes around).
 * The mobj is moved to the unused list to be reused later.
 */
void P_MobjRecycle(mobj_t* mo)
{
    // The sector next link is used as the unused mobj list links.
    mo->sNext = unusedMobjs;
    unusedMobjs = mo;
}

/**
 * 'statenum' must be a valid state (not null!).
 */
void P_MobjSetState(mobj_t* mobj, int statenum)
{
    state_t*            st = states + statenum;
    boolean             spawning = (mobj->state == 0);
    ded_ptcgen_t*       pg;

#if _DEBUG
    if(statenum < 0 || statenum >= defs.count.states.num)
        Con_Error("P_MobjSetState: statenum %i out of bounds.\n", statenum);
#endif

    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame = st->frame;

    // Check for a ptcgen trigger.
    for(pg = statePtcGens[statenum]; pg; pg = pg->stateNext)
    {
        if(!(pg->flags & PGF_SPAWN_ONLY) || spawning)
        {
            // We are allowed to spawn the generator.
            P_SpawnParticleGen(pg, mobj);
        }
    }

    if(!(mobj->ddFlags & DDMF_REMOTE))
    {
        if(defs.states[statenum].execute)
            Con_Execute(CMDS_DED, defs.states[statenum].execute, true, false);
    }
}

/**
 * Adjusts tmpFloorZ and tmpCeilingZ as lines are contacted.
 */
boolean PIT_LineCollide(linedef_t* ld, void* parm)
{
    checkpos_data_t*    tm = parm;

    if(tm->box[1][VX] <= ld->bBox[BOXLEFT] ||
       tm->box[0][VX] >= ld->bBox[BOXRIGHT] ||
       tm->box[1][VY] <= ld->bBox[BOXBOTTOM] ||
       tm->box[0][VY] >= ld->bBox[BOXTOP])
       // Bounding boxes do not overlap.
        return true;

    if(P_BoxOnLineSide2(tm->box[0][VX], tm->box[1][VX],
                        tm->box[0][VY], tm->box[1][VY], ld) != -1)
        return true;

    // A line has been hit.
    tm->mo->wallHit = true;

    if(!ld->L_backside)
        return false; // One sided line, can't go through.

    if(!(tm->mo->ddFlags & DDMF_MISSILE))
    {
        if(ld->flags & DDLF_BLOCKING)
            return false; // explicitly blocking everything
    }

    // set openrange, opentop, openbottom.
    P_LineOpening(ld);

    // adjust floor / ceiling heights.
    if(opentop < tm->ceilingZ)
        tm->ceilingZ = opentop;
    if(openbottom > tm->floorZ)
        tm->floorZ = openbottom;
    if(lowfloor < tm->dropOffZ)
        tm->dropOffZ = lowfloor;

    tm->mo->wallHit = false;
    return true;
}

boolean PIT_MobjCollide(mobj_t* mo, void* parm)
{
    checkpos_data_t*    tm = parm;
    float               blockdist;
    boolean             overlap = false;

    // Don't clip against self.
    if(mo == tm->mo)
        return true;
    if(!(mo->ddFlags & DDMF_SOLID))
        return true;

    blockdist = mo->radius + tm->mo->radius;

    // Only players can move under or over other mobjs.
    if(tm->pos[VZ] != DDMAXFLOAT &&
       (tm->mo->dPlayer || mo->ddFlags & DDMF_NOGRAVITY))
    {
        if(mo->pos[VZ] > tm->pos[VZ] + tm->height)
        {
            // We're under it.
            return true;
        }
        else if(mo->pos[VZ] + mo->height < tm->pos[VZ])
        {
            // We're over it.
            return true;
        }
        overlap = true;
    }

    if(fabs(mo->pos[VX] - tm->pos[VX]) >= blockdist ||
       fabs(mo->pos[VY] - tm->pos[VY]) >= blockdist)
    {
        // Didn't hit it.
        return true;
    }

    if(overlap)
    {
        // How are we positioned?
        if(tm->pos[VZ] >= mo->pos[VZ] + mo->height - 24)
        {
            // Above, allowing stepup.
            tm->mo->onMobj = mo;
            tm->floorZ = mo->pos[VZ] + mo->height;
            return true;
        }

        // This was causing problems in Doom E1M1, where a client was able
        // to walk through the barrel obstacle and get the shotgun. -jk
        /*
        // To prevent getting stuck, don't block if moving away from
        // the object.
        if(tm->mo->dPlayer &&
           P_ApproxDistance(tm->mo->pos[VX] - mo->pos[VX],
                            tm->mo->pos[VY] - mo->pos[VY]) <
           P_ApproxDistance(tm->pos[VX] - mo->pos[VX], tm->pos[VY] - mo->pos[VY]) &&
           tm->mo->momz > -12)
        {
            // The current distance is smaller than the new one would be.
            // No blocking needs to occur.
            // The Z movement test is done to prevent a 'falling through'
            // case when a mo is moving at a high speed.
            return true;
        }*/

        // We're hitting this mobj.
        blockingMobj = mo;
    }

    return false;
}

/**
 * @return              @c true, if the mobj can be positioned at the
 *                      specified coordinates.
 */
boolean P_CheckPosXYZ(mobj_t *mo, float x, float y, float z)
{
    subsector_t        *newsubsec;
    checkpos_data_t     data;
    vec2_t              point;
    boolean             result = true;

    blockingMobj = NULL;
    mo->onMobj = NULL;
    mo->wallHit = false;

    // Prepare the data struct.
    data.mo = mo;
    data.flags = mo->ddFlags;
    data.pos[VX] = x;
    data.pos[VY] = y;
    data.pos[VZ] = z;
    data.height = mo->height;

    // The bounding box is extended by DDMOBJ_RADIUS_MAX because mobj_ts are
    // grouped into mapblocks based on their origin point, and can overlap
    // into adjacent blocks by up to DDMOBJ_RADIUS_MAX units.
    V2_Set(point, x - mo->radius - DDMOBJ_RADIUS_MAX, y - mo->radius - DDMOBJ_RADIUS_MAX);
    V2_InitBox(data.box, point);
    V2_Set(point, x + mo->radius + DDMOBJ_RADIUS_MAX, y + mo->radius + DDMOBJ_RADIUS_MAX);
    V2_AddToBox(data.box, point);

    newsubsec = R_PointInSubsector(x, y);

    // The base floor / ceiling is from the subsector that contains the
    // point. Any contacted lines the step closer together will adjust them.
    data.floorZ = data.dropOffZ = newsubsec->sector->SP_floorheight;
    data.ceilingZ = newsubsec->sector->SP_ceilheight;

    validCount++;

    // Check mobjs first, possibly picking stuff up.
    if(!dontHitMobjs)
    {
        if(!P_MobjsBoxIteratorv(data.box, PIT_MobjCollide, &data))
        {
            result = false;
        }
    }

    // Hit something yet?
    if(result)
    {   // Nope.
        // Try polyobj->lineDefs and lines.
        if(!P_AllLinesBoxIteratorv(data.box, PIT_LineCollide, &data))
        {
            result = false;
        }
    }

    tmpCeilingZ = data.ceilingZ;
    tmpFloorZ = data.floorZ;
    tmpDropOffZ = data.dropOffZ;
    return result;
}

/**
 * @return              @c true, if the mobj can be positioned at the
 *                      specified coordinates, assuming traditional 2D DOOM
 *                      mobj placement rules.
 */
boolean P_CheckPosXY(mobj_t *mo, float x, float y)
{
    return P_CheckPosXYZ(mo, x, y, DDMAXFLOAT);
}

/**
 * Attempt to move to a mobj to a new position.
 *
 * @return              @c true, if the move was successful. Both lines and
 *                      and other mobjs are checked for collisions.
 */
boolean P_TryMoveXYZ(mobj_t *mo, float x, float y, float z)
{
    int                 links = 0;
    boolean             goodPos;

    blockingMobj = NULL;

    // Is this a real move?
    if(mo->pos[VX] == x && mo->pos[VY] == y && mo->pos[VZ] == z)
    {   // No move. Of course it's successful.
        return true;
    }

    goodPos = P_CheckPosXYZ(mo, x, y, z);

    // Is movement clipping in effect?
    if(!mo->dPlayer || !(mo->dPlayer->flags & DDPF_NOCLIP))
    {
        if(!goodPos)
        {
            if(!mo->onMobj || mo->wallHit)
                return false; // Solid wall or mo.
        }

        // Does it fit between contacted ceiling and floor?
        if(tmpCeilingZ - tmpFloorZ < mo->height)
            return false;

        if(tmpCeilingZ - z < mo->height)
            return false; // mobj must lower itself to fit.

        if(mo->dPlayer)
        {
            // Players are allowed a stepup.
            if(tmpFloorZ - z > 24)
                return false; // Too big a step up.
        }
        else
        {
            // Normals mobjs are not...
            if(tmpFloorZ > z)
                return false; // Below the floor.
        }
    }

    // The move is OK. First unlink.
    links = P_MobjUnlink(mo);

    mo->floorZ = tmpFloorZ;
    mo->ceilingZ = tmpCeilingZ;
    mo->pos[VX] = x;
    mo->pos[VY] = y;
    mo->pos[VZ] = z;

    // Put back to the same links.
    P_MobjLink(mo, links);
    return true;
}

/**
 * Try to do the given move. Returns true if nothing was hit.
 */
boolean P_StepMove(mobj_t *mo, float dx, float dy, float dz)
{
    float               delta[3];
    float               step[3];
    boolean             notHit = true;

    delta[VX] = dx;
    delta[VY] = dy;
    delta[VZ] = dz;

    while(delta[VX] || delta[VY] || delta[VZ])
    {
        step[VX] = delta[VX];
        step[VY] = delta[VY];
        step[VZ] = delta[VZ];

        // Is the step too long?
        while(step[VX] >  MAXMOVE || step[VX] < -MAXMOVE || step[VY] > MAXMOVE ||
              step[VY] < -MAXMOVE || step[VZ] > MAXMOVE  || step[VZ] < -MAXMOVE)
        {
            // Only half that, then.
            step[VX] /= 2;
            step[VY] /= 2;
            step[VZ] /= 2;
        }

        // If there is no step, we're already there!
        if(!(step[VX] == 0 && step[VY] == 0 && step[VZ] == 0))
            return notHit;

        // Can we do this step?
        while(!P_TryMoveXYZ(mo, mo->pos[VX] + step[VX],
                            mo->pos[VY] + step[VY],
                            mo->pos[VZ] + step[VZ]))
        {
            // We hit something!
            notHit = false;

            // This means even the current step is unreachable.
            // Let's make it our intended destination.
            delta[VX] = step[VX];
            delta[VY] = step[VY];
            delta[VZ] = step[VZ];

            // Try a smaller step.
            step[VX] /= 2;
            step[VY] /= 2;
            step[VZ] /= 2;

            // If we run out of step, we must give up.
            if(MIN_STEP(step[VX]) && MIN_STEP(step[VY]) && MIN_STEP(step[VZ]))
            {
                return false;
            }
        }

        // Subtract from the 'to go' distance.
        delta[VX] -= step[VX];
        delta[VY] -= step[VY];
        delta[VZ] -= step[VZ];
    }

    return notHit;
}

/**
 * Takes a valid mobj and adjusts the mobj->floorZ, mobj->ceilingZ, and
 * possibly mobj->z. This is called for all nearby mobjs whenever a sector
 * changes height. If the mobj doesn't fit, the z will be set to the lowest
 * value and false will be returned.
 */
static boolean heightClip(mobj_t *mo)
{
    boolean             onfloor;

    // During demo playback the player gets preferential treatment.
    if(mo->dPlayer == &ddPlayers[consolePlayer].shared && playback)
        return true;

    onfloor = (mo->pos[VZ] <= mo->floorZ);

    P_CheckPosXYZ(mo, mo->pos[VX], mo->pos[VY], mo->pos[VZ]);
    mo->floorZ = tmpFloorZ;
    mo->ceilingZ = tmpCeilingZ;

    if(onfloor)
    {
        mo->pos[VZ] = mo->floorZ;
    }
    else
    {
        // Don't adjust a floating mobj unless forced to.
        if(mo->pos[VZ] + mo->height > mo->ceilingZ)
            mo->pos[VZ] = mo->ceilingZ - mo->height;
    }

    /*
    // On clientside, players are represented by two mobjs: the real mobj,
    // created by the Game, is the one that is visible and modified in this
    // function. We'll need to sync the hidden client mobj (that receives
    // all the changes from the server) to match the changes.
    if(isClient && mo->dPlayer)
    {
        Cl_UpdatePlayerPos(P_GetDDPlayerIdx(mo->dPlayer));
    }
    */

    if(mo->ceilingZ - mo->floorZ < mo->height)
        return false;
    return true;
}

/**
 * Allows the player to slide along any angled walls. Adjusts the player's
 * momentum vector so that the next move slides along the linedef.
 */
static void wallMomSlide(linedef_t *ld)
{
    int                 side;
    uint                an;
    float               movelen, newlen;
    angle_t             moveangle, lineangle, deltaangle;

    // First check the simple cases.
    if(ld->slopeType == ST_HORIZONTAL)
    {
        tmpMom[VY] = 0;
        return;
    }
    if(ld->slopeType == ST_VERTICAL)
    {
        tmpMom[VX] = 0;
        return;
    }

    side = P_PointOnLinedefSide(slideMo->pos[VX], slideMo->pos[VY], ld);
    lineangle = R_PointToAngle2(0, 0, ld->dX, ld->dY);

    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmpMom[VX], tmpMom[VY]);
    deltaangle = moveangle - lineangle;

    if(deltaangle > ANG180)
        deltaangle += ANG180;

    an = deltaangle >> ANGLETOFINESHIFT;
    movelen = P_ApproxDistance(tmpMom[MX], tmpMom[MY]);
    newlen = movelen * FIX2FLT(fineCosine[an]);

    an = lineangle >> ANGLETOFINESHIFT;
    tmpMom[VX] = newlen * FIX2FLT(fineCosine[an]);
    tmpMom[VY] = newlen * FIX2FLT(finesine[an]);
}

static boolean slideTraverse(intercept_t *in)
{
    linedef_t          *li;

    if(in->type != ICPT_LINE)
        Con_Error("PTR_SlideTraverse: not a line?");

    li = in->d.lineDef;

    if(!li->L_frontside || !li->L_backside)
    {
        if(P_PointOnLinedefSide(slideMo->pos[VX],
                             slideMo->pos[VY], li))
        {   // The back side.
            return true; // Continue iteration.
        }

        goto isblocking;
    }

    // Set openrange, opentop, openbottom.
    P_LineOpening(li);

    if(openrange < slideMo->height)
        goto isblocking; // Doesn't fit.

    if(opentop - slideMo->pos[VZ] < slideMo->height)
        goto isblocking; // Mobj is too high.

    if(openbottom - slideMo->pos[VZ] > 24)
        goto isblocking; // Too big a step up.

    // This line doesn't block movement.
    return true; // Continue iteration.

    // The line does block movement, see if it is closer than best so far.
  isblocking:
    if(in->frac < bestSlideFrac)
    {
        secondSlideFrac = bestSlideFrac;
        secondSlideLine = bestSlideLine;
        bestSlideFrac = in->frac;
        bestSlideLine = li;
    }

    return false; // Stop iteration.
}

/**
 * The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * This is a kludgy mess. (No kidding?)
 */
static void mobjSlideMove(mobj_t *mo)
{
    float               leadPos[2];
    float               trailPos[2];
    float               delta[2];
    int                 hitcount;

    slideMo = mo;
    hitcount = 0;

  retry:
    if(++hitcount == 3)
        goto stairstep; // Don't loop forever.

    // Trace along the three leading corners.
    if(mo->mom[MX] > 0)
    {
        leadPos[VX]  = mo->pos[VX] + mo->radius;
        trailPos[VX] = mo->pos[VX] - mo->radius;
    }
    else
    {
        leadPos[VX]  = mo->pos[VX] - mo->radius;
        trailPos[VX] = mo->pos[VX] + mo->radius;
    }

    if(mo->mom[MY] > 0)
    {
        leadPos[VY]  = mo->pos[VY] + mo->radius;
        trailPos[VY] = mo->pos[VY] - mo->radius;
    }
    else
    {
        leadPos[VY]  = mo->pos[VY] - mo->radius;
        trailPos[VY] = mo->pos[VY] + mo->radius;
    }

    bestSlideFrac = FIX2FLT(FRACUNIT + 1);

    P_PathTraverse(leadPos[VX], leadPos[VY],
                   leadPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                   PT_ADDLINES, slideTraverse);
    P_PathTraverse(trailPos[VX], leadPos[VY],
                   trailPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                   PT_ADDLINES, slideTraverse);
    P_PathTraverse(leadPos[VX], trailPos[VY],
                   leadPos[VX] + mo->mom[MX], trailPos[VY] + mo->mom[MY],
                   PT_ADDLINES, slideTraverse);

    // Move up to the wall.
    if(bestSlideFrac == FIX2FLT(FRACUNIT + 1))
    {
        // The move most have hit the middle, so stairstep.
      stairstep:
        if(!P_TryMoveXYZ(mo, mo->pos[VX], mo->pos[VY] + mo->mom[MY], mo->pos[VZ]))
            P_TryMoveXYZ(mo, mo->pos[VX] + mo->mom[MX], mo->pos[VY], mo->pos[VZ]);
        return;
    }

    // Fudge a bit to make sure it doesn't hit.
    bestSlideFrac -= FIX2FLT(0x800);
    if(bestSlideFrac > 0)
    {
        delta[VX] = mo->mom[MX] * bestSlideFrac;
        delta[VY] = mo->mom[MY] * bestSlideFrac;
        if(!P_TryMoveXYZ(mo, mo->pos[VX] + delta[VX], mo->pos[VY] + delta[VY], mo->pos[VZ]))
            goto stairstep;
    }

    // Now continue along the wall, first calculate remainder.
    bestSlideFrac = 1 - (bestSlideFrac + FIX2FLT(0x800));

    if(bestSlideFrac > 1)
        bestSlideFrac = 1;

    if(bestSlideFrac <= 0)
        return;

    tmpMom[MX] = mo->mom[MX] * bestSlideFrac;
    tmpMom[MY] = mo->mom[MY] * bestSlideFrac;

    wallMomSlide(bestSlideLine); // Clip the move.

    mo->mom[MX] = tmpMom[MX];
    mo->mom[MY] = tmpMom[MY];

    if(!P_TryMoveXYZ(mo, mo->pos[VX] + tmpMom[MX], mo->pos[VY] + tmpMom[MY],
                     mo->pos[VZ]))
    {
        goto retry;
    }
}

/**
 * After modifying a sectors floor or ceiling height, call this routine
 * to adjust the positions of all mobjs that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 */
boolean PIT_SectorPlanesChanged(mobj_t *mo, void *data)
{
    // Always keep checking.
    if(heightClip(mo))
        return true;

    noFit = true;
    return true;
}

/**
 * Called whenever a sector's planes are moved. This will update the mobjs
 * inside the sector and do crushing.
 *
 * @param sector  Sector number.
 */
boolean P_SectorPlanesChanged(uint sector)
{
    noFit = false;

    // We'll use validCount to make sure mobjs are only checked once.
    validCount++;
    P_SectorTouchingMobjsIterator(SECTOR_PTR(sector), PIT_SectorPlanesChanged, 0);

    return noFit;
}

void P_MobjMovement(mobj_t *mo)
{
    P_MobjMovement2(mo, NULL);
}

/**
 * Playmove can be NULL. It's only used with player mobjs.
 */
void P_MobjMovement2(mobj_t *mo, void *pstate)
{
    clplayerstate_t    *playstate = pstate;
    float               tryPos[3];
    float               delta[3];
    ddplayer_t         *player;

    if(mo->mom[MX] == 0 && mo->mom[MY] == 0)
        return; // This isn't moving anywhere.

    player = mo->dPlayer;

    // Make sure we're not trying to move too much.
    if(mo->mom[MX] > MAXMOVE)
        mo->mom[MX] = MAXMOVE;
    else if(mo->mom[MX] < -MAXMOVE)
        mo->mom[MX] = -MAXMOVE;

    if(mo->mom[MY] > MAXMOVE)
        mo->mom[MY] = MAXMOVE;
    else if(mo->mom[MY] < -MAXMOVE)
        mo->mom[MY] = -MAXMOVE;

    // Do the move in progressive steps.
    delta[MX] = mo->mom[MX];
    delta[MY] = mo->mom[MY];
    do
    {
        if(delta[MX] > MAXMOVE / 2  || delta[MY] > MAXMOVE / 2 ||
           delta[MX] < -MAXMOVE / 2 || delta[MY] < -MAXMOVE / 2)
        {
            tryPos[VX] = mo->pos[VX] + delta[MX] / 2;
            tryPos[VY] = mo->pos[VY] + delta[MY] / 2;
            delta[MX] /= 2;
            delta[MY] /= 2;
        }
        else
        {
            tryPos[VX] = mo->pos[VX] + delta[MX];
            tryPos[VY] = mo->pos[VY] + delta[MY];
            delta[MX] = delta[MY] = 0;
        }

        if(!P_TryMoveXYZ(mo, tryPos[VX], tryPos[VY], mo->pos[VZ]))
        {
            // Blocked move.
            if(player)
            {
                if(blockingMobj)
                {
                    // Slide along the side of the mobj.
                    if(P_TryMoveXYZ(mo, mo->pos[VX], tryPos[VY], mo->pos[VZ]))
                    {
                        mo->mom[MX] = 0;
                    }
                    else if(P_TryMoveXYZ(mo, tryPos[VX], mo->pos[VY], mo->pos[VZ]))
                    {
                        mo->mom[MY] = 0;
                    }
                    else
                    {
                        // All movement stops here.
                        mo->mom[MX] = mo->mom[MY] = 0;
                    }
                }
                else
                {
                    // Try to slide along it.
                    mobjSlideMove(mo);
                }
            }
            else
            {
                // Stop moving.
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(delta[MX] != 0 || delta[MY] != 0);

    // Apply friction.
    if(mo->ddFlags & DDMF_MISSILE)
        return;                 // No friction for missiles, ever.

    if(mo->pos[VZ] > mo->floorZ && !mo->onMobj && !(mo->ddFlags & DDMF_FLY))
        return;                 // No friction when airborne.

    if(mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED &&
       (!playstate || (!playstate->forwardMove && !playstate->sideMove)))
    {
        mo->mom[MX] = mo->mom[MY] = 0;
    }
    else
    {
        float       friction;

        if(playstate)
            friction = FIX2FLT(playstate->friction);
        else
            friction = DEFAULT_FRICTION;

        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
}

void P_MobjZMovement(mobj_t *mo)
{
    float               gravity = FIX2FLT(mapGravity);

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];

    // Clip movement. Another mobj?
    if(mo->onMobj && mo->pos[VZ] <= mo->onMobj->pos[VZ] + mo->onMobj->height)
    {
        if(mo->mom[MZ] < 0)
        {
            mo->mom[MZ] = 0;
        }

        if(mo->mom[MZ] == 0)
            mo->pos[VZ] = mo->onMobj->pos[VZ] + mo->onMobj->height;
    }

    // The floor.
    if(mo->pos[VZ] <= mo->floorZ)
    {
        // Hit the floor.
        if(mo->mom[MZ] < 0)
        {
            mo->mom[MZ] = 0;
        }

        mo->pos[VZ] = mo->floorZ;
    }
    else if(mo->ddFlags & DDMF_LOWGRAVITY)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(gravity / 8) * 2;
        else
            mo->mom[MZ] -= gravity / 8;
    }
    else if(!(mo->ddFlags & DDMF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingZ)
    {
        // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = mo->ceilingZ - mo->height;
    }
}
