/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Moving object handling. Spawn functions.
 */

#ifdef WIN32
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jdoom.h"

#include "hu_stuff.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS  (2*TICSPERSEC)

#define MAX_BOB_OFFSET  0x80000

#define FRICTION_NORMAL 0xe800
#define FRICTION_FLY    0xeb00
#define STOPSPEED       0x1000
#define STANDSPEED      0x8000

// TYPES -------------------------------------------------------------------

typedef struct spawnobj_s {
    fixed_t pos[3];
    int     angle;
    int     type;
    int     thingflags;
} spawnobj_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_PlayerReborn(int player);
void    P_ApplyTorque(mobj_t *mo);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    P_SpawnMapThing(thing_t * mthing);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t attackrange;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

spawnobj_t itemrespawnque[ITEMQUESIZE];
int     itemrespawntime[ITEMQUESIZE];
int     iquehead;
int     iquetail;

// CODE --------------------------------------------------------------------

/*
 * Returns true if the mobj is still present.
 */
boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
    state_t *st;

    do
    {
        if(state == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_RemoveMobj(mobj);
            return false;
        }

        P_SetState(mobj, state);
        st = &states[state];

        mobj->turntime = false; // $visangle-facetarget

        // Modified handling.
        // Call action functions when the state is set
        if(st->action)
            st->action(mobj);

        state = st->nextstate;
    } while(!mobj->tics);

    return true;
}

void P_ExplodeMissile(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        // Clients won't explode missiles.
        P_SetMobjState(mo, S_NULL);
        return;
    }

    mo->momx = mo->momy = mo->momz = 0;

    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);

    mo->tics -= P_Random() & 3;

    if(mo->tics < 1)
        mo->tics = 1;

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        // Remove the brightshadow flag.
        if(mo->flags & MF_BRIGHTSHADOW)
            mo->flags &= ~MF_BRIGHTSHADOW;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    if(mo->info->deathsound)
        S_StartSound(mo->info->deathsound, mo);
}

void P_FloorBounceMissile(mobj_t *mo)
{
    mo->momz = -mo->momz;
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
}

// Returns the ground friction factor for the mobj.
fixed_t P_GetMobjFriction(mobj_t *mo)
{
    if(mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= mo->floorz) &&
       !mo->onmobj)
    {
        return FRICTION_FLY;
    }
    return XS_Friction(P_GetPtrp(mo->subsector, DMU_SECTOR));
}

void P_XYMovement(mobj_t *mo)
{
    sector_t *backsector;
    fixed_t ptryx;
    fixed_t ptryy;
    player_t *player;
    fixed_t xmove;
    fixed_t ymove;
    boolean largeNegative;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    xmove = mo->momx = CLAMP(mo->momx, -MAXMOVE, MAXMOVE);
    ymove = mo->momy = CLAMP(mo->momy, -MAXMOVE, MAXMOVE);

    if((xmove | ymove) == 0)
    {
        if(mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->momx = mo->momy = mo->momz = 0;

            P_SetMobjState(mo, mo->info->spawnstate);
        }
        return;
    }

    player = mo->player;

    do
    {
        // killough 8/9/98: fix bug in original Doom source:
        // Large negative displacements were never considered.
        // This explains the tendency for Mancubus fireballs
        // to pass through walls.
        largeNegative = false;
        if(!cfg.moveBlock && (xmove < -MAXMOVE / 2 || ymove < -MAXMOVE / 2))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallrun))
                largeNegative = true;
        }

        if(xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2 || largeNegative)
        {
            ptryx = mo->pos[VX] + (xmove >>= 1);
            ptryy = mo->pos[VY] + (ymove >>= 1);
        }
        else
        {
            ptryx = mo->pos[VX] + xmove;
            ptryy = mo->pos[VY] + ymove;
            xmove = ymove = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallrun)
            mo->wallrun = false;

        // killough $dropoff_fix
        if(!P_TryMove(mo, ptryx, ptryy, true, false))
        {
            // blocked move
            if(mo->flags2 & MF2_SLIDE)
            {                   // try to slide along it
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {
                if(ceilingline)
                {
                    backsector = P_GetPtrp(ceilingline, DMU_BACK_SECTOR);
                    if(backsector)
                    {
                        // explode a missile?
                        if(ceilingline &&
                           P_GetIntp(backsector,
                                     DMU_CEILING_TEXTURE) == skyflatnum)
                        {
                            // Hack to prevent missiles exploding
                            // against the sky.
                            // Does not handle sky floors.
                            P_RemoveMobj(mo);
                            return;
                        }
                    }
                }
                P_ExplodeMissile(mo);
            }
            else
                mo->momx = mo->momy = 0;
        }
    } while(xmove | ymove);

    // slow down
    if(player && player->cheats & CF_NOMOMENTUM)
    {
        // debug option for no sliding at all
        mo->momx = mo->momy = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return;                 // no friction for missiles ever

    if(mo->pos[VZ] > mo->floorz && !mo->onmobj && !(mo->flags2 & MF2_FLY))
        return;                 // no friction when falling

    if(cfg.slidingCorpses)
    {
        // killough $dropoff_fix: add objects falling off ledges
        // Does not apply to players! -jk
        if((mo->flags & MF_CORPSE || mo->intflags & MIF_FALLING) &&
           !mo->player)
        {
            // do not stop sliding
            //  if halfway off a step with some momentum
            if(mo->momx > FRACUNIT / 4 || mo->momx < -FRACUNIT / 4 ||
               mo->momy > FRACUNIT / 4 || mo->momy < -FRACUNIT / 4)
            {
                if(mo->floorz !=
                   P_GetFixedp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }

    // Stop player walking animation.
    if(player && mo->momx > -STANDSPEED && mo->momx < STANDSPEED &&
       mo->momy > -STANDSPEED && mo->momy < STANDSPEED &&
       !player->cmd.forwardMove && !player->cmd.sideMove)
    {
        // if in a walking frame, stop moving
        if((unsigned) ((player->plr->mo->state - states) - PCLASS_INFO(player->class)->runstate) < 4)
            P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->normalstate);
    }

    if(mo->momx > -STOPSPEED && mo->momx < STOPSPEED && mo->momy > -STOPSPEED
       && mo->momy < STOPSPEED && (!player ||
                                   (player->cmd.forwardMove == 0 &&
                                    player->cmd.sideMove == 0)))
    {
        mo->momx = 0;
        mo->momy = 0;
    }
    else
    {
        if(mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= mo->floorz) &&
           !mo->onmobj)
        {
            mo->momx = FixedMul(mo->momx, FRICTION_FLY);
            mo->momy = FixedMul(mo->momy, FRICTION_FLY);
        }
#if __JHERETIC__
        else if(P_XSector(P_GetPtrp(mo->subsector,
                                    DMU_SECTOR))->special == 15)
        {
            // Friction_Low
            mo->momx = FixedMul(mo->momx, FRICTION_LOW);
            mo->momy = FixedMul(mo->momy, FRICTION_LOW);
        }
#endif
        else
        {
            fixed_t fric = P_GetMobjFriction(mo);

            mo->momx = FixedMul(mo->momx, fric);
            mo->momy = FixedMul(mo->momy, fric);
        }
    }
}

/*
static boolean PIT_Splash(sector_t *sector, void *data)
{
    mobj_t *mo = data;
    fixed_t floorheight;

    floorheight = P_GetFixedp(sector, DMU_FLOOR_HEIGHT);

    // Is the mobj touching the floor of this sector?
    if(mo->pos[VZ] < floorheight &&
       mo->pos[VZ] + mo->height / 2 > floorheight)
    {
        // TODO: Play a sound, spawn a generator, etc.
    }

    // Continue checking.
    return true;
}
*/

void P_RipperBlood(mobj_t *mo)
{
    mobj_t *th;
    fixed_t pos[3];

    memcpy(pos, mo->pos, sizeof(pos));
    pos[VX] += ((P_Random() - P_Random()) << 12);
    pos[VY] += ((P_Random() - P_Random()) << 12);
    pos[VZ] += ((P_Random() - P_Random()) << 12);
    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_BLOOD);
    th->flags |= MF_NOGRAVITY;
    th->momx = mo->momx >> 1;
    th->momy = mo->momy >> 1;
    th->tics += P_Random() & 3;
}

int P_GetThingFloorType(mobj_t *thing)
{
    return P_GetTerrainType(P_GetPtrp(thing->subsector, DMU_SECTOR), PLN_FLOOR);
}

void P_HitFloor(mobj_t *mo)
{
    //P_ThingSectorsIterator(mo, PIT_Splash, mo);
}

void P_ZMovement(mobj_t *mo)
{
    fixed_t gravity;
    fixed_t dist;
    fixed_t delta;

    gravity = XS_Gravity(P_GetPtrp(mo->subsector, DMU_SECTOR));

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    // check for smooth step up
    if(mo->player && mo->pos[VZ] < mo->floorz)
    {
        mo->dplayer->viewheight -= mo->floorz - mo->pos[VZ];

        mo->dplayer->deltaviewheight =
            ((cfg.plrViewHeight << FRACBITS) - mo->dplayer->viewheight) >> 3;
    }

    // adjust height
    mo->pos[VZ] += mo->momz;
    if((mo->flags2 & MF2_FLY) &&
       mo->onmobj && mo->pos[VZ] > mo->onmobj->pos[VZ] + mo->onmobj->height)
        mo->onmobj = NULL; // We were on a mobj, we are NOT now.

    if(mo->flags & MF_FLOAT && mo->target)
    {
        // float down towards target if too close
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);

            //delta = (mo->target->pos[VZ] + (mo->height>>1)) - mo->z;
            delta = (mo->target->pos[VZ] + mo->target->height / 2) -
                    (mo->pos[VZ] + mo->height / 2);

#define ABS(x)  ((x)<0?-(x):(x))
            if(dist < mo->radius + mo->target->radius &&
               ABS(delta) < mo->height + mo->target->height)
            {
                // Don't go INTO the target.
                delta = 0;
            }

            if(delta < 0 && dist < -(delta * 3))
            {
                mo->pos[VZ] -= FLOATSPEED;
                P_SetThingSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->pos[VZ] += FLOATSPEED;
                P_SetThingSRVOZ(mo, FLOATSPEED);
            }
        }
    }
    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->pos[VZ] > mo->floorz &&
       !mo->onmobj && (leveltime & 2))
    {
        mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
    }

    // Clip movement. Another thing?
    if(mo->onmobj && mo->pos[VZ] <= mo->onmobj->pos[VZ] + mo->onmobj->height)
    {
        if(mo->momz < 0)
        {
            if(mo->player && mo->momz < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dplayer->deltaviewheight = mo->momz >> 3;

                if(mo->player->health > 0)
                    S_StartSound(sfx_oof, mo);
            }
            mo->momz = 0;
        }
        if(!mo->momz)
            mo->pos[VZ] = mo->onmobj->pos[VZ] + mo->onmobj->height;

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // The floor.
    if(mo->pos[VZ] <= mo->floorz)
    {
        // hit the floor

        // Note (id):
        //  somebody left this after the setting momz to 0,
        //  kinda useless there.
        //
        // cph - This was the a bug in the linuxdoom-1.10 source which
        //  caused it not to sync Doom 2 v1.9 demos. Someone
        //  added the above comment and moved up the following code. So
        //  demos would desync in close lost soul fights.
        // Note that this only applies to original Doom 1 or Doom2 demos - not
        //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
        //  gamemission. (Note we assume that Doom1 is always Ult Doom, which
        //  seems to hold for most published demos.)
        //
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
        //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
        //
        // So we need to check that this is either retail or commercial
        // (but not doom2)

        int correct_lost_soul_bounce
                = (gamemode == retail || gamemode == commercial)
                  && gamemission != doom2;

        if(correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->momz = -mo->momz;
        }

        if(mo->momz < 0)
        {
            if(mo->player && mo->momz < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dplayer->deltaviewheight = mo->momz >> 3;

                // Fix DOOM bug - dead players grunting when hitting the ground
                // (e.g., after an archvile attack)
                if(mo->player->health > 0)
                    S_StartSound(sfx_oof, mo);
            }
            P_HitFloor(mo);
            mo->momz = 0;
        }
        mo->pos[VZ] = mo->floorz;

        // cph 2001/05/26 -
        // See lost soul bouncing comment above. We need this here for bug
        // compatibility with original Doom2 v1.9 - if a soul is charging and
        // hit by a raising floor this incorrectly reverses its Y momentum.
        //

        if(!correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
            mo->momz = -mo->momz;

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else
            {
                P_ExplodeMissile(mo);
                return;
            }
        }
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->momz == 0)
            mo->momz = -(gravity >> 3) * 2;
        else
            mo->momz -= gravity >> 3;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->momz == 0)
            mo->momz = -gravity * 2;
        else
            mo->momz -= gravity;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingz)
    {
        // hit the ceiling
        if(mo->momz > 0)
            mo->momz = 0;

        mo->pos[VZ] = mo->ceilingz - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->momz = -mo->momz;
        }

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(P_GetIntp(mo->subsector, DMU_CEILING_TEXTURE) == skyflatnum)
            {
                // Don't explode against sky.
                {
                    P_RemoveMobj(mo);
                }
                return;
            }
            P_ExplodeMissile(mo);
            return;
        }
    }
}

void P_NightmareRespawn(mobj_t *mobj)
{
    fixed_t pos[3];
    subsector_t *ss;
    mobj_t *mo;

    memcpy(pos, mobj->spawninfo.pos, sizeof(pos));

    // somthing is occupying it's position?
    if(!P_CheckPosition(mobj, pos[VX], pos[VY]))
        return;                 // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?
    mo = P_SpawnMobj(mobj->pos[VX], mobj->pos[VY],
                     P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT),
                     MT_TFOG);
    // initiate teleport sound
    S_StartSound(sfx_telept, mo);

    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    mo = P_SpawnMobj(pos[VX], pos[VY],
                     P_GetFixedp(ss, DMU_FLOOR_HEIGHT),
                     MT_TFOG);

    S_StartSound(sfx_telept, mo);

    // spawn it
    if(mobj->info->flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    // inherit attributes from deceased one
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], mobj->type);
    memcpy(mo->spawninfo.pos, mobj->spawninfo.pos, sizeof(mo->spawninfo.pos));
    mo->spawninfo.angle = mobj->spawninfo.angle;
    mo->spawninfo.type = mobj->spawninfo.type;
    mo->spawninfo.options = mobj->spawninfo.options;
    mo->angle = mobj->spawninfo.angle;

    if(mobj->spawninfo.options & MTF_DEAF)
        mo->flags |= MTF_DEAF;

    mo->reactiontime = 18;

    // remove the old monster.
    P_RemoveMobj(mobj);
}

void P_MobjThinker(mobj_t *mobj)
{
    if(mobj->ddflags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.

    // Spectres get selector = 1.
    if(mobj->type == MT_SHADOWS)
        mobj->selector = (mobj->selector & ~DDMOBJ_SELECTOR_MASK) | 1;


    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

#if __JHERETIC__
    // Lightsources must stay where they're hooked.
    if(mobj->type == MT_LIGHTSOURCE)
    {
        if(mobj->movedir > 0)
            mobj->pos[VZ] = P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT) + mobj->movedir;
        else
            mobj->pos[VZ] = P_GetFixedp(mobj->subsector, DMU_CEILING_HEIGHT) + mobj->movedir;
        return;
    }
#endif

    // Handle X and Y momentums
    if(mobj->momx || mobj->momy || (mobj->flags & MF_SKULLFLY))
    {
        P_XYMovement(mobj);

        // FIXME: decent NOP/NULL/Nil function pointer please.
        if(mobj->thinker.function == NOPFUNC)
            return;             // mobj was removed
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {                           // Floating item bobbing motion
        //mobj->pos[VZ] = mobj->floorz+FloatBobOffsets[(mobj->health++)&63];

        // Keep it on the floor.
        mobj->pos[VZ] = mobj->floorz;
#if __JHERETIC__
        // Negative floorclip raises the mobj off the floor.
        mobj->floorclip = -mobj->special1;
#elif __JDOOM__
        mobj->floorclip = 0;
#endif
        if(mobj->floorclip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorclip = -MAX_BOB_OFFSET;
        }

        // Old floatbob used health as index, let's still increase it
        // as before (in case somebody wants to use it).
        mobj->health++;
    }
    else if((mobj->pos[VZ] != mobj->floorz) || mobj->momz) // GMJ 02/02/02
    {
        P_ZMovement(mobj);
        if(mobj->thinker.function != P_MobjThinker) // cph - Must've been removed
            return;             // killough - mobj was removed
    }
    // non-sentient objects at rest
    else if(!(mobj->momx | mobj->momy) && !sentient(mobj) && !(mobj->player) &&
            !((mobj->flags & MF_CORPSE) && cfg.slidingCorpses))
    {
        // killough 9/12/98: objects fall off ledges if they are hanging off
        // slightly push off of ledge if hanging more than halfway off

        if(mobj->pos[VZ] > mobj->dropoffz && // Only objects contacting dropoff
           !(mobj->flags & MF_NOGRAVITY) && cfg.fallOff)
        {
            P_ApplyTorque(mobj);
        }
        else
        {
            mobj->intflags &= ~MIF_FALLING;
            mobj->gear = 0;     // Reset torque
        }
    }

    if(cfg.slidingCorpses)
    {
        if((mobj->flags & MF_CORPSE ? mobj->pos[VZ] > mobj->dropoffz :
                                      mobj->pos[VZ] - mobj->dropoffz > FRACUNIT * 24) && // Only objects contacting drop off
           !(mobj->flags & MF_NOGRAVITY))    // Only objects which fall
        {
            P_ApplyTorque(mobj);    // Apply torque
        }
        else
        {
            mobj->intflags &= ~MIF_FALLING;
            mobj->gear = 0;     // Reset torque
        }
    }

    // $vanish: dead monsters disappear after some time
    if(cfg.corpseTime && (mobj->flags & MF_CORPSE) && mobj->corpsetics != -1)
    {
        if(++mobj->corpsetics < cfg.corpseTime * TICSPERSEC)
        {
            mobj->translucency = 0; // Opaque.
        }
        else if(mobj->corpsetics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mobj->translucency =
                ((mobj->corpsetics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mobj->corpsetics = -1;
            return;
        }
    }

    // cycle through states,
    // calling action functions at transitions
    if(mobj->tics != -1)
    {
        mobj->tics--;

        P_SRVOAngleTicker(mobj);    // "angle-servo"; smooth actor turning

        // you can cycle through multiple states in a tic
        if(!mobj->tics)
        {
            P_ClearThingSRVO(mobj);
            if(!P_SetMobjState(mobj, mobj->state->nextstate))
                return;         // freed itself
        }
    }
    else if(!IS_CLIENT)
    {
        // check for nightmare respawn
        if(!(mobj->flags & MF_COUNTKILL))
            return;

        if(!respawnmonsters)
            return;

        mobj->movecount++;

        if(mobj->movecount < 12 * 35)
            return;

        if(leveltime & 31)
            return;

        if(P_Random() > 4)
            return;

        P_NightmareRespawn(mobj);
    }
}

/*
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    mobj_t *mobj;
    mobjinfo_t *info;
    fixed_t space;

    mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
    memset(mobj, 0, sizeof(*mobj));
    info = &mobjinfo[type];

    mobj->type = type;
    mobj->info = info;
    mobj->pos[VX] = x;
    mobj->pos[VY] = y;
    mobj->radius = info->radius;
    mobj->height = info->height;
    mobj->flags = info->flags;
    mobj->flags2 = info->flags2;

    mobj->damage = info->damage;

    mobj->health = info->spawnhealth
                   * (IS_NETGAME ? cfg.netMobHealthModifier : 1);

    // Let the engine know about solid objects.
    //if(mobj->flags & MF_SOLID) mobj->ddflags |= DDMF_SOLID;
    P_SetDoomsdayFlags(mobj);

    if(gameskill != sk_nightmare)
        mobj->reactiontime = info->reactiontime;

    mobj->lastlook = P_Random() % MAXPLAYERS;
    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet

    // Must link before setting state (ID assigned for the mobj).
    mobj->thinker.function = P_MobjThinker;
    P_AddThinker(&mobj->thinker);

    /*st = &states[info->spawnstate];
       mobj->state = st;
       mobj->tics = st->tics;
       mobj->sprite = st->sprite;
       mobj->frame = st->frame; */
    P_SetState(mobj, info->spawnstate);

    // set subsector and/or block links
    P_SetThingPosition(mobj);

    mobj->dropoffz =            //killough $dropoff_fix
        mobj->floorz =
            P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT);

    mobj->ceilingz =
        P_GetFixedp(mobj->subsector, DMU_CEILING_HEIGHT);

    if(z == ONFLOORZ)
        mobj->pos[VZ] = mobj->floorz;
    else if(z == ONCEILINGZ)
        mobj->pos[VZ] = mobj->ceilingz - mobj->info->height;
    else if(z == FLOATRANDZ)
    {
        space = ((mobj->ceilingz) - (mobj->info->height)) - mobj->floorz;
        if(space > 48 * FRACUNIT)
        {
            space -= 40 * FRACUNIT;
            mobj->pos[VZ] =
                ((space * P_Random()) >> 8) + mobj->floorz + 40 * FRACUNIT;
        }
        else
        {
            mobj->pos[VZ] = mobj->floorz;
        }
    }
    else
        mobj->pos[VZ] = z;

    if(mobj->flags2 & MF2_FLOORCLIP &&
       P_GetThingFloorType(mobj) >= FLOOR_LIQUID &&
       mobj->pos[VZ] == P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT))
    {
        mobj->floorclip = 10 * FRACUNIT;
    }
    else
    {
        mobj->floorclip = 0;
    }
    return mobj;
}

void P_RemoveMobj(mobj_t *mobj)
{
    if((mobj->flags & MF_SPECIAL) && !(mobj->flags & MF_DROPPED) &&
       (mobj->type != MT_INV) && (mobj->type != MT_INS))
    {
        // Copy the mobj's info to the respawn que
        spawnobj_t *spawnobj = &itemrespawnque[iquehead];

        memcpy(spawnobj->pos, mobj->spawninfo.pos, sizeof(spawnobj->pos));
        spawnobj->angle = mobj->spawninfo.angle;
        spawnobj->type = mobj->spawninfo.type;
        spawnobj->thingflags = mobj->spawninfo.options;

        itemrespawntime[iquehead] = leveltime;
        iquehead = (iquehead + 1) & (ITEMQUESIZE - 1);

        // lose one off the end?
        if(iquehead == iquetail)
            iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
    }

    // unlink from sector and block lists
    P_UnsetThingPosition(mobj);

    // stop any playing sound
    S_StopSound(0, mobj);

    // free block
    P_RemoveThinker((thinker_t *) mobj);
}

void P_RespawnSpecials(void)
{
    fixed_t pos[3];
    subsector_t *ss;
    mobj_t *mo;
    spawnobj_t *sobj;
    int     i;

    // only respawn items in deathmatch 2 and optionally in coop.
    if(deathmatch != 2 && (!cfg.coopRespawnItems || !IS_NETGAME || deathmatch))
        return;

    // nothing left to respawn?
    if(iquehead == iquetail)
        return;

    // wait at least 30 seconds
    if(leveltime - itemrespawntime[iquetail] < 30 * 35)
        return;

    // Get the attributes of the mobj from spawn parameters.
    sobj = &itemrespawnque[iquetail];

    memcpy(pos, sobj->pos, sizeof(pos));
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    pos[VZ] = P_GetFixedp(ss, DMU_FLOOR_HEIGHT);

    // spawn a teleport fog at the new spot
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_IFOG);
    S_StartSound(sfx_itmbk, mo);

    // find which type to spawn
    for(i = 0; i < Get(DD_NUMMOBJTYPES); i++)
    {
        if(sobj->type == mobjinfo[i].doomednum)
            break;
    }

    // spawn it
    if(mobjinfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], i);
    mo->angle = sobj->angle;

    if(mo->flags2 & MF2_FLOORCLIP &&
       P_GetThingFloorType(mo) >= FLOOR_LIQUID &&
       mo->pos[VZ] == P_GetFixedp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        mo->floorclip = 10 * FRACUNIT;
    }
    else
    {
        mo->floorclip = 0;
    }

    // Copy spawn attributes to the new mobj
    memcpy(mo->spawninfo.pos, sobj->pos, sizeof(mo->spawninfo.pos));
    mo->spawninfo.angle = sobj->angle;
    mo->spawninfo.type = sobj->type;
    mo->spawninfo.options = sobj->thingflags;

    // pull it from the que
    iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
}

/*
 * Called when a player is spawned on the level.
 * Most of the player structure stays unchanged between levels.
 */
void P_SpawnPlayer(thing_t * mthing, int pnum)
{
    player_t *p;
    fixed_t pos[3];
    mobj_t *mobj;
    int     i;

    if(pnum < 0)
        pnum = 0;
    if(pnum >= MAXPLAYERS - 1)
        pnum = MAXPLAYERS - 1;

    // not playing?
    if(!players[pnum].plr->ingame)
        return;

    p = &players[pnum];

    if(p->playerstate == PST_REBORN)
        G_PlayerReborn(pnum);

    pos[VX] = mthing->x << FRACBITS;
    pos[VY] = mthing->y << FRACBITS;
    pos[VZ] = ONFLOORZ;
    mobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER);

    // With clients all player mobjs are remote, even the consoleplayer.
    if(IS_CLIENT)
    {
        /*P_UnsetThingPosition(mobj);
           mobj->flags |= MF_NOBLOCKMAP | MF_NOSECTOR;
           mobj->flags &= ~MF_SOLID;
           mobj->ddflags |= DDMF_REMOTE;
           //mobj->ddflags &= ~DDMF_SOLID; */

        mobj->flags &= ~MF_SOLID;
        mobj->ddflags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // set color translations for player sprites
    i = cfg.PlayerColor[pnum];
    if(i > 0)
        mobj->flags |= i << MF_TRANSSHIFT;

    p->plr->clAngle = mobj->angle = ANG45 * (mthing->angle / 45);
    p->plr->clLookDir = 0;
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    mobj->player = p;
    mobj->dplayer = p->plr;
    mobj->health = p->health;

    p->plr->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->plr->extralight = 0;
    p->plr->fixedcolormap = 0;
    p->plr->lookdir = 0;
    p->plr->viewheight = (cfg.plrViewHeight << FRACBITS);

    p->class = PCLASS_PLAYER;

    // setup gun psprite
    P_SetupPsprites(p);

    // give all cards in death match mode
    if(deathmatch)
        for(i = 0; i < NUMKEYS; i++)
            p->keys[i] = true;

    if(pnum == consoleplayer)
    {
        // wake up the status bar
        ST_Start();
        // wake up the heads up text
        HU_Start();
    }
}

/*
 * Spawns the passed thing into the world.
 */
void P_SpawnMapThing(thing_t *th)
{
    int     i;
    int     bit;
    mobj_t *mobj;
    fixed_t pos[3];

    //Con_Message("x = %i, y = %i, height = %i, angle = %i, type = %i, options = %i\n",
    //            th->x, th->y, th->height, th->angle, th->type, th->options);

    // Count deathmatch start positions
    if(th->type == 11)
    {
        if(deathmatch_p < &deathmatchstarts[MAX_DM_STARTS])
        {
            memcpy(deathmatch_p, th, sizeof(*th));
            deathmatch_p++;
        }
        return;
    }

    // Check for players specially.
    if(th->type >= 1 && th->type <= 4)
    {
        // Register this player start.
        P_RegisterPlayerStart(th);

        return;
    }

    // Don't spawn things flagged for Multiplayer if we're not in
    // a netgame.
    if(!IS_NETGAME && (th->options & MTF_NOTSINGLE))
        return;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching
    if(deathmatch && (th->options & MTF_NOTDM))
        return;

    // Don't spawn things flagged for Not Coop if we're coop'in
    if(IS_NETGAME && !deathmatch && (th->options & MTF_NOTCOOP))
        return;

    // check for apropriate skill level
    if(gameskill == sk_baby)
        bit = 1;
    else if(gameskill == sk_nightmare)
        bit = 4;
    else
        bit = 1 << (gameskill - 1);

    if(!(th->options & bit))
        return;

    // find which type to spawn

    for(i = 0; i < Get(DD_NUMMOBJTYPES); i++)
    {
        if(th->type == mobjinfo[i].doomednum)
            break;
    }

    // Clients only spawn local objects.
    if(IS_CLIENT)
    {
        if(!(mobjinfo[i].flags & MF_LOCAL))
            return;
    }

    if(i == Get(DD_NUMMOBJTYPES))
        return;

    // don't spawn keycards in deathmatch
    if(deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
        return;

    // Check for specific disabled objects.
    if(IS_NETGAME && (th->options & MTF_NOTSINGLE))    // multiplayer flag
    {
        // Cooperative weapons?
        if(cfg.noCoopWeapons && !deathmatch && i >= MT_CLIP // ammo...
           && i <= MT_SUPERSHOTGUN) // ...and weapons
        {
            // Don't spawn this.
            return;
        }
        // Don't spawn any special objects in coop?
        if(cfg.noCoopAnything && !deathmatch)
            return;
        // BFG disabled in netgames?
        if(cfg.noNetBFG && i == MT_MISC25)
            return;
    }

    // don't spawn any monsters if -nomonsters
    if(nomonsters && (i == MT_SKULL || (mobjinfo[i].flags & MF_COUNTKILL)))
    {
        return;
    }

    // spawn it
    pos[VX] = th->x << FRACBITS;
    pos[VY] = th->y << FRACBITS;

    if(mobjinfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else if(mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
        pos[VZ] = FLOATRANDZ;
    else
        pos[VZ] = ONFLOORZ;

    mobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], i);
    if(mobj->flags2 & MF2_FLOATBOB)
    {                           // Seed random starting index for bobbing motion
        mobj->health = P_Random();
    }
    mobj->angle = ANG45 * (th->angle / 45);
    if(mobj->tics > 0)
        mobj->tics = 1 + (P_Random() % mobj->tics);
    if(mobj->flags & MF_COUNTKILL)
        totalkills++;
    if(mobj->flags & MF_COUNTITEM)
        totalitems++;

    mobj->visangle = mobj->angle >> 16; // "angle-servo"; smooth actor turning
    if(th->options & MTF_DEAF)
        mobj->flags |= MTF_DEAF;

    // Set the spawn info for this mobj
    memcpy(mobj->spawninfo.pos, pos, sizeof(mobj->spawninfo.pos));
    mobj->spawninfo.angle = mobj->angle;
    mobj->spawninfo.type = mobjinfo[i].doomednum;
    mobj->spawninfo.options = th->options;
}

mobj_t *P_SpawnCustomPuff(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    mobj_t *th;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return NULL;

    z += ((P_Random() - P_Random()) << 10);

    th = P_SpawnMobj(x, y, z, type);
    th->momz = FRACUNIT;
    th->tics -= P_Random() & 3;

    // Make it last at least one tic.
    if(th->tics < 1)
        th->tics = 1;

    return th;
}

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    mobj_t *th = P_SpawnCustomPuff(x, y, z, MT_PUFF);

    // don't make punches spark on the wall
    if(th && attackrange == MELEERANGE)
        P_SetMobjState(th, S_PUFF3);
}

void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage)
{
    mobj_t *th;

    z += ((P_Random() - P_Random()) << 10);
    th = P_SpawnMobj(x, y, z, MT_BLOOD);
    th->momz = FRACUNIT * 2;
    th->tics -= P_Random() & 3;

    if(th->tics < 1)
        th->tics = 1;

    if(damage <= 12 && damage >= 9)
        P_SetMobjState(th, S_BLOOD2);
    else if(damage < 9)
        P_SetMobjState(th, S_BLOOD3);
}

/*
 * Moves the missile forward a bit and possibly explodes it right there.
 */
void P_CheckMissileSpawn(mobj_t *th)
{
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->pos[VX] += (th->momx >> 1);
    th->pos[VY] += (th->momy >> 1);
    th->pos[VZ] += (th->momz >> 1);

    if(!P_TryMove(th, th->pos[VX], th->pos[VY], false, false))
        P_ExplodeMissile(th);
}

mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    fixed_t z;
    mobj_t *th;
    angle_t an;
    int     dist, dist3;

    z = source->pos[VZ] + 4 * 8 * FRACUNIT;

    z -= source->floorclip;
    th = P_SpawnMobj(source->pos[VX], source->pos[VY], z, type);

    if(th->info->seesound)
        S_StartSound(th->info->seesound, th);

    th->target = source;        // where it came from
    an = R_PointToAngle2(source->pos[VX], source->pos[VY],
                         dest->pos[VX], dest->pos[VY]);

    // fuzzy player
    if(dest->flags & MF_SHADOW)
        an += (P_Random() - P_Random()) << 20;

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul(th->info->speed, finecosine[an]);
    th->momy = FixedMul(th->info->speed, finesine[an]);

    dist = P_ApproxDistance(dest->pos[VX] - source->pos[VX],
                            dest->pos[VY] - source->pos[VY]);
    //dist3 = P_ApproxDistance (dist, dest->pos[VZ] - source->pos[VZ]);
    dist = dist / th->info->speed;

    if(dist < 1)
        dist = 1;

    th->momz = (dest->pos[VZ] - source->pos[VZ]) / dist;

    // Make sure the speed is right (in 3D).
    dist3 = P_ApproxDistance(th->momx, th->momy);
    dist3 = P_ApproxDistance(dist3, th->momz);
    if(!dist3)
        dist3 = 1;
    dist = FixedDiv(th->info->speed, dist3);

    th->momx = FixedMul(th->momx, dist);
    th->momy = FixedMul(th->momy, dist);
    th->momz = FixedMul(th->momz, dist);

    P_CheckMissileSpawn(th);

    return th;
}

/*
 * Tries to aim at a nearby monster
 */
void P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type)
{
    mobj_t *th;
    angle_t an;
    fixed_t pos[3];
    fixed_t dist;
    fixed_t slope;              //, scale;

    //  float fSlope;

    // see which target is to be aimed at
    an = source->angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
    if(!cfg.noAutoAim)
        if(!linetarget)
        {
            an += 1 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);

            if(!linetarget)
            {
                an -= 2 << 26;
                slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
            }

            if(!linetarget)
            {
                an = source->angle;
                //if(!INCOMPAT_OK) slope = 0;
            }
        }

    memcpy(pos, source->pos, sizeof(pos));
    pos[VZ] += 4 * 8 * FRACUNIT;

    pos[VZ] -= source->floorclip;

    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);

    if(th->info->seesound)
        //S_NetAvoidStartSound(th, th->info->seesound, source->player);
        S_StartSound(th->info->seesound, th);

    th->target = source;
    th->angle = an;
    th->momx = FixedMul(th->info->speed, finecosine[an >> ANGLETOFINESHIFT]);
    th->momy = FixedMul(th->info->speed, finesine[an >> ANGLETOFINESHIFT]);

    // Allow free-aim with the BFG in deathmatch?
    if(deathmatch && cfg.netBFGFreeLook == 0 && type == MT_BFG)
        th->momz = 0;
    else
        th->momz = FixedMul(th->info->speed, slope);

    // Make sure the speed is right (in 3D).
    dist = P_ApproxDistance(P_ApproxDistance(th->momx, th->momy), th->momz);
    if(!dist)
        dist = 1;
    dist = FixedDiv(th->info->speed, dist);
    th->momx = FixedMul(th->momx, dist);
    th->momy = FixedMul(th->momy, dist);
    th->momz = FixedMul(th->momz, dist);

    P_CheckMissileSpawn(th);
}
