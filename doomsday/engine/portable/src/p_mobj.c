/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_mobj.c: Map Objects
 *
 * Contains various routines for moving mobjs.
 * Collision and Z checking.
 * Original code from Doom (by id Software).
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

// Max. distance to move in one call to P_XYMovement.
#define MAXRADIUS       (32*FRACUNIT)
#define MAXMOVE         (30*FRACUNIT)

// Shortest possible movement step.
#define MINMOVE         (FRACUNIT >> 7)

#define MIN_STEP(d)     ((d) <= MINMOVE && (d) >= -MINMOVE)

#define STOPSPEED           0x1000

// TYPES -------------------------------------------------------------------

typedef struct {
    mobj_t *thing;
    fixed_t box[4];
    int     flags;
    fixed_t pos[3];
    float   height, floorz, ceilingz, dropoffz;
} checkpos_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t mapgravity;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// If set to true, P_CheckPosition will skip the mobj hit test.
boolean dontHitMobjs = false;

float tmpFloorZ;
float tmpCeilingZ;

// When a mobj is contacted in PIT_CheckThing, this pointer is set.
// It's reset to NULL in the beginning of P_CheckPosition.
mobj_t *blockingMobj;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Slide variables.
static fixed_t bestSlideFrac;
static fixed_t secondSlideFrac;
static line_t *bestSlideLine;
static line_t *secondSlideLine;

static mobj_t *slideMo;

static boolean noFit;

static float tmpDropOffZ;
static fixed_t tmpMove[3];

// CODE --------------------------------------------------------------------

/**
 * 'statenum' must be a valid state (not null!).
 */
void P_SetState(mobj_t *mobj, int statenum)
{
    state_t *st = states + statenum;
    boolean spawning = (mobj->state == 0);
    ded_ptcgen_t *pg;

#if _DEBUG
    if(statenum < 0 || statenum >= defs.count.states.num)
        Con_Error("P_SetState: statenum %i out of bounds.\n", statenum);
#endif

    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame = st->frame;

    // Check for a ptcgen trigger.
    for(pg = st->ptrigger; statenum && pg; pg = pg->state_next)
    {
        if(!(pg->flags & PGF_SPAWN_ONLY) || spawning)
        {
            // We are allowed to spawn the generator.
            P_SpawnParticleGen(pg, mobj);
        }
    }

    if(defs.states[statenum].execute)
        Con_Execute(CMDS_DED, defs.states[statenum].execute, true, false);
}

/**
 * Adjusts tmpFloorZ and tmpCeilingZ as lines are contacted.
 */
static boolean PIT_CheckLine(line_t *ld, void *parm)
{
    int         p;
    checkpos_data_t *tm = parm;
    fixed_t     bbox[4];

    // Setup the bounding box for the line.
    p = (ld->v[0]->pos[VX] < ld->v[1]->pos[VX]);
    bbox[BOXLEFT]   = FLT2FIX(ld->v[p^1]->pos[VX]);
    bbox[BOXRIGHT]  = FLT2FIX(ld->v[p]->pos[VX]);

    p = (ld->v[0]->pos[VY] < ld->v[1]->pos[VY]);
    bbox[BOXBOTTOM] = FLT2FIX(ld->v[p^1]->pos[VY]);
    bbox[BOXTOP]    = FLT2FIX(ld->v[p]->pos[VY]);

    if(tm->box[BOXRIGHT] <= bbox[BOXLEFT] || tm->box[BOXLEFT] >= bbox[BOXRIGHT] ||
       tm->box[BOXTOP] <= bbox[BOXBOTTOM] || tm->box[BOXBOTTOM] >= bbox[BOXTOP])
        return true;

    if(P_BoxOnLineSide(tm->box, ld) != -1)
        return true;

    // A line has been hit.
    tm->thing->wallhit = true;

    if(!ld->L_backsector)
        return false;           // One sided line, can't go through.

    if(!(tm->thing->ddflags & DDMF_MISSILE))
    {
        if(ld->flags & ML_BLOCKING)
            return false;       // explicitly blocking everything
    }

    // set openrange, opentop, openbottom.
    P_LineOpening(ld);

    // adjust floor / ceiling heights.
    if(opentop < tm->ceilingz)
        tm->ceilingz = opentop;
    if(openbottom > tm->floorz)
        tm->floorz = openbottom;
    if(lowfloor < tm->dropoffz)
        tm->dropoffz = lowfloor;

    tm->thing->wallhit = false;
    return true;
}

static boolean PIT_CheckThing(mobj_t *thing, void *parm)
{
    checkpos_data_t *tm = parm;
    fixed_t blockdist;
    boolean overlap = false;

    // don't clip against self
    if(thing == tm->thing)
        return true;
    if(!(thing->ddflags & DDMF_SOLID))
        return true;

    blockdist = thing->radius + tm->thing->radius;

    // Only players can move under or over other things.
    if(tm->pos[VZ] != DDMAXINT && (tm->thing->dplayer /* || thing->dplayer */
                             || thing->ddflags & DDMF_NOGRAVITY))
    {
        if(thing->pos[VZ] > tm->pos[VZ] + FLT2FIX(tm->height))
        {
            // We're under it.
            return true;
        }
        else if(FIX2FLT(thing->pos[VZ]) + thing->height < tm->pos[VZ])
        {
            // We're over it.
            return true;
        }
        overlap = true;
    }

    if(abs(thing->pos[VX] - tm->pos[VX]) >= blockdist ||
       abs(thing->pos[VY] - tm->pos[VY]) >= blockdist)
    {
        // Didn't hit it.
        return true;
    }

    if(overlap)
    {
        // How are we positioned?
        if(tm->pos[VZ] >= thing->pos[VZ] + FLT2FIX(thing->height - 24))
        {
            // Above, allowing stepup.
            tm->thing->onmobj = thing;
            tm->floorz = FIX2FLT(thing->pos[VZ]) + thing->height;
            return true;
        }

        // This was causing problems in Doom E1M1, where a client was able
        // to walk through the barrel obstacle and get the shotgun. -jk
        /*
        // To prevent getting stuck, don't block if moving away from
        // the object.
        if(tm->thing->dplayer &&
           P_ApproxDistance(tm->thing->pos[VX] - thing->pos[VX],
                            tm->thing->pos[VY] - thing->pos[VY]) <
           P_ApproxDistance(tm->pos[VX] - thing->pos[VX], tm->pos[VY] - thing->pos[VY]) &&
           tm->thing->momz > -12 * FRACUNIT)
        {
            // The current distance is smaller than the new one would be.
            // No blocking needs to occur.
            // The Z movement test is done to prevent a 'falling through'
            // case when a thing is moving at a high speed.
            return true;
        }*/

        // We're hitting this mobj.
        blockingMobj = thing;
    }

    return false;
}

/**
 * Returns true if it the thing can be positioned in the coordinates.
 */
boolean P_CheckPosXYZ(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z)
{
    int     xl, xh;
    int     yl, yh;
    int     bx, by;
    subsector_t *newsubsec;
    checkpos_data_t data;
    boolean result = true;

    blockingMobj = NULL;
    thing->onmobj = NULL;
    thing->wallhit = false;

    // Prepare the data struct.
    data.thing = thing;
    data.flags = thing->ddflags;
    data.pos[VX] = x;
    data.pos[VY] = y;
    data.pos[VZ] = z;
    data.height = thing->height;
    data.box[BOXTOP]    = y + thing->radius;
    data.box[BOXBOTTOM] = y - thing->radius;
    data.box[BOXRIGHT]  = x + thing->radius;
    data.box[BOXLEFT]   = x - thing->radius;

    newsubsec = R_PointInSubsector(x, y);

    // The base floor / ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    data.floorz = data.dropoffz = newsubsec->sector->SP_floorheight;
    data.ceilingz = newsubsec->sector->SP_ceilheight;

    validcount++;

    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    xl = (data.box[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    xh = (data.box[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    yl = (data.box[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    yh = (data.box[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    if(!dontHitMobjs)
    {
        for(bx = xl; bx <= xh; ++bx)
            for(by = yl; by <= yh; ++by)
                if(!P_BlockThingsIterator(bx, by, PIT_CheckThing, &data))
                {
                    result = false;
                    goto checkpos_done;
                }
    }

    // check lines
    xl = (data.box[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
    xh = (data.box[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
    yl = (data.box[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
    yh = (data.box[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;

    for(bx = xl; bx <= xh; ++bx)
        for(by = yl; by <= yh; ++by)
            if(!P_BlockLinesIterator(bx, by, PIT_CheckLine, &data))
            {
                result = false;
                goto checkpos_done;
            }

  checkpos_done:
    tmpCeilingZ = data.ceilingz;
    tmpFloorZ = data.floorz;
    tmpDropOffZ = data.dropoffz;
    return result;
}

/**
 * Returns true if the thing can be positioned in the coordinates
 * (x,y), assuming traditional 2D Doom item placement rules.
 */
boolean P_CheckPosXY(mobj_t *thing, fixed_t x, fixed_t y)
{
    return P_CheckPosXYZ(thing, x, y, DDMAXINT);
}

/**
 * Attempt to move to a new (x,y,z) position.  Returns true if the
 * move was successful. Both lines and things are checked for
 * collisions.
 */
boolean P_TryMoveXYZ(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z)
{
    int     links = 0;
    boolean goodPos;

    blockingMobj = NULL;

    // Is this a real move?
    if(thing->pos[VX] == x && thing->pos[VY] == y && thing->pos[VZ] == z)
    {
        // No move. Of course it's successful.
        return true;
    }

    goodPos = P_CheckPosXYZ(thing, x, y, z);

    // Is movement clipping in effect?
    if(!thing->dplayer || !(thing->dplayer->flags & DDPF_NOCLIP))
    {
        if(!goodPos)
        {
            if(!thing->onmobj || thing->wallhit)
                return false;   // Solid wall or thing.
        }

        // Does it fit between contacted ceiling and floor?
        if(tmpCeilingZ - tmpFloorZ < thing->height)
            return false;

        if(tmpCeilingZ - FIX2FLT(z) < thing->height)
            return false;       // mobj must lower itself to fit

        if(thing->dplayer)
        {
            // Players are allowed a stepup.
            if(tmpFloorZ - FIX2FLT(z) > 24)
                return false;   // too big a step up
        }
        else
        {
            // Normals mobjs are not...
            if(tmpFloorZ > FIX2FLT(z))
                return false;   // below the floor
        }
    }

    // The move is OK. First unlink.
    if(IS_SECTOR_LINKED(thing))
        links |= DDLINK_SECTOR;
    if(IS_BLOCK_LINKED(thing))
        links |= DDLINK_BLOCKMAP;
    P_UnlinkThing(thing);

    thing->floorz = tmpFloorZ;
    thing->ceilingz = tmpCeilingZ;
    thing->pos[VX] = x;
    thing->pos[VY] = y;
    thing->pos[VZ] = z;

    // Put back to the same links.
    P_LinkThing(thing, links);
    return true;
}

/**
 * Try to do the given move. Returns true if nothing was hit.
 */
boolean P_StepMove(mobj_t *thing, fixed_t dx, fixed_t dy, fixed_t dz)
{
    fixed_t stepX, stepY, stepZ;
    boolean notHit = true;

    while(dx || dy || dz)
    {
        stepX = dx;
        stepY = dy;
        stepZ = dz;

        // Is the step too long?
        while(stepX > MAXMOVE || stepX < -MAXMOVE || stepY > MAXMOVE ||
              stepY < -MAXMOVE || stepZ > MAXMOVE || stepZ < -MAXMOVE)
        {
            // Only half that, then.
            stepX /= 2;
            stepY /= 2;
            stepZ /= 2;
        }

        // If there is no step, we're already there!
        if(!(stepX | stepY | stepZ))
            return notHit;

        // Can we do this step?
        while(!P_TryMoveXYZ
              (thing, thing->pos[VX] + stepX, thing->pos[VY] + stepY, thing->pos[VZ] + stepZ))
        {
            // We hit something!
            notHit = false;

            // This means even the current step is unreachable.
            // Let's make it our intended destination.
            dx = stepX;
            dy = stepY;
            dz = stepZ;

            // Try a smaller step.
            stepX /= 2;
            stepY /= 2;
            stepZ /= 2;

            // If we run out of step, we must give up.
            //if(!(stepX | stepY | stepZ)) return false;
            if(MIN_STEP(stepX) && MIN_STEP(stepY) && MIN_STEP(stepZ))
            {
                return false;
            }
        }

        // Subtract from the 'to go' distance.
        dx -= stepX;
        dy -= stepY;
        dz -= stepZ;
    }
    return notHit;
}

/**
 * Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
 * and possibly thing->z.  This is called for all nearby monsters
 * whenever a sector changes height.  If the thing doesn't fit, the z
 * will be set to the lowest value and false will be returned.
 */
static boolean P_HeightClip(mobj_t *thing)
{
    boolean onfloor;

    // During demo playback the player gets preferential
    // treatment.
    if(thing->dplayer == &players[consoleplayer] && playback)
        return true;

    onfloor = (thing->pos[VZ] <= FLT2FIX(thing->floorz));

    P_CheckPosXYZ(thing, thing->pos[VX], thing->pos[VY], thing->pos[VZ]);
    thing->floorz = tmpFloorZ;
    thing->ceilingz = tmpCeilingZ;

    if(onfloor)
    {
        thing->pos[VZ] = FLT2FIX(thing->floorz);
    }
    else
    {
        // don't adjust a floating monster unless forced to
        if(FIX2FLT(thing->pos[VZ]) + thing->height > thing->ceilingz)
            thing->pos[VZ] = FLT2FIX(thing->ceilingz - thing->height);
    }

    // On clientside, players are represented by two mobjs: the real mobj,
    // created by the Game, is the one that is visible and modified in this
    // function. We'll need to sync the hidden client mobj (that receives
    // all the changes from the server) to match the changes.
    if(isClient && thing->dplayer)
    {
        Cl_UpdatePlayerPos(thing->dplayer);
    }

    if(thing->ceilingz - thing->floorz < thing->height)
        return false;
    return true;
}

/**
 * Do we THINK the given (camera) player is currently in the void.
 * The method used to test this is to compare the position of the mobj
 * each time it is linked into a subsector.
 * Cannot be 100% accurate so best not to use it for anything critical...
 *
 * @param player        The player to test.
 *
 * @return boolean      (TRUE) If the player is thought to be in the void.
 */
boolean P_IsInVoid(ddplayer_t *player)
{
    if(!player)
        return false;

    // Cameras are allowed to move completely freely
    // (so check z height above/below ceiling/floor).
    if((player->flags & DDPF_CAMERA) &&
       (player->invoid ||
         (FIX2FLT(player->mo->pos[VZ]) > player->mo->ceilingz ||
          FIX2FLT(player->mo->pos[VZ]) < player->mo->floorz)))
        return true;

    return false;
}

/**
 * Allows the player to slide along any angled walls.
 * Adjusts the xmove / ymove so that the next move will
 * slide along the wall.
 */
static void P_WallMomSlide(line_t *ld)
{
    int     side;
    angle_t lineangle;
    angle_t moveangle;
    angle_t deltaangle;
    fixed_t movelen;
    fixed_t newlen;

    // First check the simple cases.
    if(ld->slopetype == ST_HORIZONTAL)
    {
        tmpMove[VY] = 0;
        return;
    }
    if(ld->slopetype == ST_VERTICAL)
    {
        tmpMove[VX] = 0;
        return;
    }

    side = P_PointOnLineSide(slideMo->pos[VX], slideMo->pos[VY], ld);
    lineangle = R_PointToAngle2(0, 0, ld->dx, ld->dy);

    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmpMove[VX], tmpMove[VY]);
    deltaangle = moveangle - lineangle;

    if(deltaangle > ANG180)
        deltaangle += ANG180;

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(tmpMove[MX], tmpMove[MY]);
    newlen = FixedMul(movelen, finecosine[deltaangle]);

    tmpMove[VX] = FixedMul(newlen, finecosine[lineangle]);
    tmpMove[VY] = FixedMul(newlen, finesine[lineangle]);
}

static boolean PTR_SlideTraverse(intercept_t *in)
{
    line_t *li;

    if(!in->isaline)
        Con_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;

    if(!(li->flags & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slideMo->pos[VX], slideMo->pos[VY], li))
        {
            // don't hit the back side
            return true;
        }
        goto isblocking;
    }

    // set openrange, opentop, openbottom
    P_LineOpening(li);

    if(openrange < slideMo->height)
        goto isblocking;        // doesn't fit

    if(opentop - FIX2FLT(slideMo->pos[VZ]) < slideMo->height)
        goto isblocking;        // mobj is too high

    if(openbottom - FIX2FLT(slideMo->pos[VZ]) > 24)
        goto isblocking;        // too big a step up

    // this line doesn't block movement
    return true;

    // the line does block movement,
    // see if it is closer than best so far
  isblocking:
    if(in->frac < bestSlideFrac)
    {
        secondSlideFrac = bestSlideFrac;
        secondSlideLine = bestSlideLine;
        bestSlideFrac = in->frac;
        bestSlideLine = li;
    }
    return false;               // stop
}

/**
 * The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * This is a kludgy mess. (No kidding?)
 */
static void P_ThingSlidingMove(mobj_t *mo)
{
    fixed_t leadx;
    fixed_t leady;
    fixed_t trailx;
    fixed_t traily;
    fixed_t newx;
    fixed_t newy;
    int     hitcount;

    slideMo = mo;
    hitcount = 0;

  retry:
    if(++hitcount == 3)
        goto stairstep;         // don't loop forever

    // trace along the three leading corners
    if(mo->mom[MX] > 0)
    {
        leadx  = mo->pos[VX] + mo->radius;
        trailx = mo->pos[VX] - mo->radius;
    }
    else
    {
        leadx  = mo->pos[VX] - mo->radius;
        trailx = mo->pos[VX] + mo->radius;
    }

    if(mo->mom[MY] > 0)
    {
        leady  = mo->pos[VY] + mo->radius;
        traily = mo->pos[VY] - mo->radius;
    }
    else
    {
        leady  = mo->pos[VY] - mo->radius;
        traily = mo->pos[VY] + mo->radius;
    }

    bestSlideFrac = FRACUNIT + 1;

    P_PathTraverse(leadx, leady, leadx + mo->mom[MX], leady + mo->mom[MY],
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(trailx, leady, trailx + mo->mom[MX], leady + mo->mom[MY],
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(leadx, traily, leadx + mo->mom[MX], traily + mo->mom[MY],
                   PT_ADDLINES, PTR_SlideTraverse);

    // Move up to the wall.
    if(bestSlideFrac == FRACUNIT + 1)
    {
        // The move most have hit the middle, so stairstep.
      stairstep:
        if(!P_TryMoveXYZ(mo, mo->pos[VX], mo->pos[VY] + mo->mom[MY], mo->pos[VZ]))
            P_TryMoveXYZ(mo, mo->pos[VX] + mo->mom[MX], mo->pos[VY], mo->pos[VZ]);
        return;
    }

    // Fudge a bit to make sure it doesn't hit.
    bestSlideFrac -= 0x800;
    if(bestSlideFrac > 0)
    {
        newx = FixedMul(mo->mom[MX], bestSlideFrac);
        newy = FixedMul(mo->mom[MY], bestSlideFrac);
        if(!P_TryMoveXYZ(mo, mo->pos[VX] + newx, mo->pos[VY] + newy, mo->pos[VZ]))
            goto stairstep;
    }

    // Now continue along the wall.
    // First calculate remainder.
    bestSlideFrac = FRACUNIT - (bestSlideFrac + 0x800);

    if(bestSlideFrac > FRACUNIT)
        bestSlideFrac = FRACUNIT;

    if(bestSlideFrac <= 0)
        return;

    tmpMove[MX] = FixedMul(mo->mom[MX], bestSlideFrac);
    tmpMove[MY] = FixedMul(mo->mom[MY], bestSlideFrac);

    P_WallMomSlide(bestSlideLine);  // clip the moves

    mo->mom[MX] = tmpMove[MX];
    mo->mom[MY] = tmpMove[MY];

    if(!P_TryMoveXYZ(mo, mo->pos[VX] + tmpMove[MX],
                         mo->pos[VY] + tmpMove[MY], mo->pos[VZ]))
    {
        goto retry;
    }
}

/**
 * After modifying a sectors floor or ceiling height, call this routine
 * to adjust the positions of all things that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 */
static boolean PIT_SectorPlanesChanged(mobj_t *thing, void *data)
{
    // Always keep checking.
    if(P_HeightClip(thing))
        return true;
    noFit = true;
    return true;
}

/**
 * Called whenever a sector's planes are moved.  This will update the
 * things inside the sector and do crushing.
 */
boolean P_SectorPlanesChanged(sector_t *sector)
{
    noFit = false;

    // We'll use validcount to make sure things are only checked once.
    validcount++;
    P_SectorTouchingThingsIterator(sector, PIT_SectorPlanesChanged, 0);

    return noFit;
}

void P_ThingMovement(mobj_t *mo)
{
    P_ThingMovement2(mo, NULL);
}

/**
 * Playmove can be NULL. It's only used with player mobjs.
 */
void P_ThingMovement2(mobj_t *mo, void *pstate)
{
    playerstate_t *playstate = pstate;
    fixed_t     ptry[3];
    fixed_t     move[3];
    ddplayer_t *player;

    if(!mo->mom[MX] && !mo->mom[MY])
        return;                 // This isn't moving anywhere.

    player = mo->dplayer;

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
    move[MX] = mo->mom[MX];
    move[MY] = mo->mom[MY];
    do
    {
        if(move[MX] > MAXMOVE / 2  || move[MY] > MAXMOVE / 2 ||
           move[MX] < -MAXMOVE / 2 || move[MY] < -MAXMOVE / 2)
        {
            ptry[MX] = mo->pos[VX] + move[MX] / 2;
            ptry[MY] = mo->pos[VY] + move[MY] / 2;
            move[MX] >>= 1;
            move[MY] >>= 1;
        }
        else
        {
            ptry[MX] = mo->pos[VX] + move[MX];
            ptry[MY] = mo->pos[VY] + move[MY];
            move[MX] = move[MY] = 0;
        }

        if(!P_TryMoveXYZ(mo, ptry[MX], ptry[MY], mo->pos[VZ]))
        {
            // Blocked move.
            if(player)
            {
                if(blockingMobj)
                {
                    // Slide along the side of the mobj.
                    if(P_TryMoveXYZ(mo, mo->pos[VX], ptry[MY], mo->pos[VZ]))
                    {
                        mo->mom[MX] = 0;
                    }
                    else if(P_TryMoveXYZ(mo, ptry[MX], mo->pos[VY], mo->pos[VZ]))
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
                    P_ThingSlidingMove(mo);
                }
            }
            else
            {
                // Stop moving.
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(move[MX] || move[MY]);

    // Apply friction.
    if(mo->ddflags & DDMF_MISSILE)
        return;                 // No friction for missiles, ever.

    if(mo->pos[VZ] > FLT2FIX(mo->floorz) && !mo->onmobj && !(mo->ddflags & DDMF_FLY))
        return;                 // No friction when airborne.

    if(mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED &&
       (!playstate || (!playstate->forwardMove && !playstate->sideMove)))
    {
        mo->mom[MX] = mo->mom[MY] = 0;
    }
    else
    {
        fixed_t friction = DEFAULT_FRICTION;

        if(playstate)
            friction = playstate->friction;

        mo->mom[MX] = FixedMul(mo->mom[MX], friction);
        mo->mom[MY] = FixedMul(mo->mom[MY], friction);
    }
}

void P_ThingZMovement(mobj_t *mo)
{
    // check for smooth step up
    if(mo->dplayer && FIX2FLT(mo->pos[VZ]) < mo->floorz)
    {
        mo->dplayer->viewheight -= mo->floorz - FIX2FLT(mo->pos[VZ]);
        mo->dplayer->deltaviewheight =
            FIX2FLT(((41 << FRACBITS) - FLT2FIX(mo->dplayer->viewheight)) >> 3);
    }

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];

    // Clip movement. Another thing?
    if(mo->onmobj && mo->pos[VZ] <= mo->onmobj->pos[VZ] + FLT2FIX(mo->onmobj->height))
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->dplayer && mo->mom[MZ] < -mapgravity * 8)
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dplayer->deltaviewheight = FIX2FLT(mo->mom[MZ] >> 3);
            }
            mo->mom[MZ] = 0;
        }
        if(!mo->mom[MZ])
            mo->pos[VZ] = mo->onmobj->pos[VZ] + FLT2FIX(mo->onmobj->height);
    }

    // The floor.
    if(mo->pos[VZ] <= FLT2FIX(mo->floorz))
    {
        // Hit the floor.
        if(mo->mom[MZ] < 0)
        {
            if(mo->dplayer && mo->mom[MZ] < -mapgravity * 8)
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dplayer->deltaviewheight = FIX2FLT(mo->mom[MZ] >> 3);
            }
            mo->mom[MZ] = 0;
        }
        mo->pos[VZ] = FLT2FIX(mo->floorz);
    }
    else if(mo->ddflags & DDMF_LOWGRAVITY)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(mapgravity >> 3) * 2;
        else
            mo->mom[MZ] -= mapgravity >> 3;
    }
    else if(!(mo->ddflags & DDMF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -mapgravity * 2;
        else
            mo->mom[MZ] -= mapgravity;
    }
    if(FIX2FLT(mo->pos[VZ]) + mo->height > mo->ceilingz)
    {
        // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;
        mo->pos[VZ] = FLT2FIX(mo->ceilingz - mo->height);
    }
}
