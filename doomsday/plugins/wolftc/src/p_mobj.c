/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
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
 * p_mobj.c: Moving object handling. Spawn functions.
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "wolftc.h"

#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS          (2*TICSPERSEC)

#define MAX_BOB_OFFSET      8

#define STOPSPEED           (1.0f/1.6/10)
#define STANDSPEED          (1.0f/2)

// TYPES -------------------------------------------------------------------

typedef struct spawnobj_s {
    float       pos[3];
    int         angle;
    int         type;
    int         thingflags;
} spawnobj_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_PlayerReborn(int player);
void    P_ApplyTorque(mobj_t *mo);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    P_SpawnMapThing(spawnspot_t *mthing);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float attackrange;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

spawnobj_t itemrespawnque[ITEMQUESIZE];
int     itemrespawntime[ITEMQUESIZE];
int     iquehead;
int     iquetail;

// CODE --------------------------------------------------------------------

/**
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

    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

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
    mo->mom[MZ] = -mo->mom[MZ];
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
}

/**
 * Returns the ground friction factor for the mobj.
 */
float P_GetMobjFriction(mobj_t *mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorz) && !mo->onmobj)
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

    xmove = mo->mom[MX] = CLAMP(mo->mom[MX], -FLT2FIX(MAXMOVE), FLT2FIX(MAXMOVE));
    ymove = mo->mom[MY] = CLAMP(mo->mom[MY], -FLT2FIX(MAXMOVE), FLT2FIX(MAXMOVE));

    if((xmove | ymove) == 0)
    {
        if(mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

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
        if(!cfg.moveBlock && (xmove < -FLT2FIX(MAXMOVE) / 2 || ymove < -FLT2FIX(MAXMOVE) / 2))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallrun))
                largeNegative = true;
        }

        if(xmove > FLT2FIX(MAXMOVE) / 2 || ymove > FLT2FIX(MAXMOVE) / 2 || largeNegative)
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
                                     DMU_CEILING_MATERIAL) == skyMaskMaterial)
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
                mo->mom[MX] = mo->mom[MY] = 0;
        }
    } while(xmove | ymove);

    // slow down
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // debug option for no sliding at all
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return;                 // no friction for missiles ever

    if(mo->pos[VZ] > FLT2FIX(mo->floorz) && !mo->onmobj && !(mo->flags2 & MF2_FLY))
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
            if(mo->mom[MX] > FRACUNIT / 4 || mo->mom[MX] < -FRACUNIT / 4 ||
               mo->mom[MY] > FRACUNIT / 4 || mo->mom[MY] < -FRACUNIT / 4)
            {
                if(mo->floorz !=
                   P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }

    // Stop player walking animation.
    if(player && !player->plr->cmd.forwardMove && !player->plr->cmd.sideMove &&
       mo->mom[MX] > -STANDSPEED && mo->mom[MX] < STANDSPEED &&
       mo->mom[MY] > -STANDSPEED && mo->mom[MY] < STANDSPEED)
    {
        // if in a walking frame, stop moving
        if((unsigned) ((player->plr->mo->state - states) - PCLASS_INFO(player->class)->runstate) < 4)
            P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->normalstate);
    }

    if((!player || (player->plr->cmd.forwardMove == 0 && player->plr->cmd.sideMove == 0)) &&
       mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED)
    {
        mo->mom[MX] = 0;
        mo->mom[MY] = 0;
    }
    else
    {
        if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= FLT2FIX(mo->floorz)) &&
           !mo->onmobj)
        {
            mo->mom[MX] *= FRICTION_FLY;
            mo->mom[MY] *= FRICTION_FLY;
        }
#if __JHERETIC__
        else if(P_ToXSector(P_GetPtrp(mo->subsector,
                                    DMU_SECTOR))->special == 15)
        {
            // Friction_Low
            mo->mom[MX] *= FRICTION_LOW;
            mo->mom[MY] *= FRICTION_LOW;
        }
#endif
        else
        {
            float       friction = P_GetMobjFriction(mo);

            mo->mom[MX] *= friction;
            mo->mom[MY] *= friction;
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
        //// \todo Play a sound, spawn a generator, etc.
    }

    // Continue checking.
    return true;
}
*/

void P_RipperBlood(mobj_t *mo)
{
    mobj_t         *th;
    float           pos[3];

    pos[VX] = mo->pos[VX];
    pos[VY] = mo->pos[VY];
    pos[VZ] = mo->pos[VZ];

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VZ] += FIX2FLT((P_Random() - P_Random()) << 12);

    th = P_SpawnMobj3fv(MT_BLOOD, pos);
    th->flags |= MF_NOGRAVITY;
    th->mom[MX] = mo->mom[MX] / 2;
    th->mom[MY] = mo->mom[MY] / 2;
    th->tics += P_Random() & 3;
}

int P_GetMobjFloorType(mobj_t *thing)
{
    return P_GetTerrainType(P_GetPtrp(thing->subsector, DMU_SECTOR), PLN_FLOOR);
}

void P_HitFloor(mobj_t *mo)
{
    //P_MobjSectorsIterator(mo, PIT_Splash, mo);
}

void P_ZMovement(mobj_t *mo)
{
    float       gravity;
    float       dist;
    float       delta;

    gravity = XS_Gravity(P_GetPtrp(mo->subsector, DMU_SECTOR));

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    // check for smooth step up
    if(mo->player && mo->pos[VZ] < mo->floorz)
    {
        mo->dplayer->viewheight -= mo->floorz - mo->pos[VZ];

        mo->dplayer->deltaviewheight =
            (cfg.plrViewHeight - mo->dplayer->viewheight) / 8;
    }

    // adjust height
    mo->pos[VZ] += mo->mom[MZ];
    if((mo->flags2 & MF2_FLY) &&
       mo->onmobj && mo->pos[VZ] > mo->onmobj->pos[VZ] + mo->onmobj->height)
        mo->onmobj = NULL; // We were on a mobj, we are NOT now.

    if((mo->flags & MF_FLOAT) && mo->target && !P_IsCamera(mo->target))
    {
        // float down towards target if too close
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);

            //delta = (mo->target->pos[VZ] + (mo->height / 2)) - mo->pos[VZ];
            delta = (mo->target->pos[VZ] + mo->target->height/2) -
                    (mo->pos[VZ] + mo->height /2);

            if(dist < mo->radius + mo->target->radius &&
               fabs(delta) < mo->height + mo->target->height)
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
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK]);
    }

    // Clip movement. Another thing?
    if(mo->onmobj && mo->pos[VZ] <= mo->onmobj->pos[VZ] + mo->onmobj->height)
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dplayer->deltaviewheight = mo->mom[MZ] / 8;

                if(mo->player->health > 0)
                    S_StartSound(sfx_blockd, mo);
            }
            mo->mom[MZ] = 0;
        }

        if(mo->mom[MZ] == 0)
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
        //  somebody left this after the setting mom[MZ] to 0,
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
                  && gamemission != GM_DOOM2;

        if(correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
        {
            // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dplayer->deltaviewheight = mo->mom[MZ] / 8;

                // Fix DOOM bug - dead players grunting when hitting the ground
                // (e.g., after an archvile attack)
                if(mo->player->health > 0)
                    S_StartSound(sfx_blockd, mo);
            }
            P_HitFloor(mo);
            mo->mom[MZ] = 0;
        }

        mo->pos[VZ] = mo->floorz;

        // cph 2001/05/26 -
        // See lost soul bouncing comment above. We need this here for bug
        // compatibility with original Doom2 v1.9 - if a soul is charging and
        // hit by a raising floor this incorrectly reverses its Y momentum.
        //
        if(!correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
            mo->mom[MZ] = -mo->mom[MZ];

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
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(gravity / 8) * 2;
        else
            mo->mom[MZ] -= gravity / 8;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingz)
    {
        // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = mo->ceilingz - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(P_GetIntp(mo->subsector, DMU_CEILING_MATERIAL) == skyMaskMaterial)
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
    float       pos[3];
    subsector_t *ss;
    mobj_t     *mo;

    pos[VX] = FIX2FLT(mobj->spawnspot.pos[VX]);
    pos[VY] = FIX2FLT(mobj->spawnspot.pos[VY]);
    pos[VZ] = FIX2FLT(mobj->spawnspot.pos[VZ]);

    // somthing is occupying it's position?
    if(!P_CheckPosition2f(mobj, pos[VX], pos[VY]))
        return;                 // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?
    mo = P_SpawnMobj3f(FLT2FIX(mobj->pos[VX]), FLT2FIX(mobj->pos[VY]),
                     P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT),
                     MT_TFOG);
    // initiate teleport sound
    S_StartSound(sfx_nmrrsp, mo);

    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    mo = P_SpawnMobj3f(FLT2FIX(pos[VX]), FLT2FIX(pos[VY]),
                     P_GetFixedp(ss, DMU_FLOOR_HEIGHT),
                     MT_TFOG);

    S_StartSound(sfx_nmrrsp, mo);

    // spawn it
    if(mobj->info->flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    // inherit attributes from deceased one
    mo = P_SpawnMobj3fv(pos, mobj->type);
    memcpy(mo->spawnspot.pos, mobj->spawnspot.pos, sizeof(mo->spawnspot.pos));
    mo->spawnspot.angle = mobj->spawnspot.angle;
    mo->spawnspot.type = mobj->spawnspot.type;
    mo->spawnspot.options = mobj->spawnspot.options;
    mo->angle = mobj->spawnspot.angle;

    if(mobj->spawnspot.options & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

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
    if(mobj->mom[MX] || mobj->mom[MY] || (mobj->flags & MF_SKULLFLY))
    {
        P_XYMovement(mobj);

        //// \fixme decent NOP/NULL/Nil function pointer please.
        if(mobj->thinker.function == NOPFUNC)
            return;             // mobj was removed
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {                           // Floating item bobbing motion
        // Keep it on the floor.
        mobj->pos[VZ] = FLT2FIX(mobj->floorz);
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
    else if(mobj->pos[VZ] != FLT2FIX(mobj->floorz) || mobj->mom[MZ]) // GMJ 02/02/02
    {
        P_ZMovement(mobj);
        if(mobj->thinker.function != P_MobjThinker) // cph - Must've been removed
            return;             // killough - mobj was removed
    }
    // non-sentient objects at rest
    else if(!(mobj->mom[MX] | mobj->mom[MY]) && !sentient(mobj) && !(mobj->player) &&
            !((mobj->flags & MF_CORPSE) && cfg.slidingCorpses))
    {
        // killough 9/12/98: objects fall off ledges if they are hanging off
        // slightly push off of ledge if hanging more than halfway off

        if(FIX2FLT(mobj->pos[VZ]) > mobj->dropoffz && // Only objects contacting dropoff
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
        if((mobj->flags & MF_CORPSE ? FIX2FLT(mobj->pos[VZ]) > mobj->dropoffz :
                                      FIX2FLT(mobj->pos[VZ]) - mobj->dropoffz > 24) && // Only objects contacting drop off
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

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t *P_SpawnMobj3f(mobjtype_t type, float x, float y, float z)
{
    mobj_t     *mobj;
    mobjinfo_t *info;
    float       space;

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
    mobj->flags3 = info->flags3;
    mobj->damage = info->damage;

    mobj->health = info->spawnhealth
                   * (IS_NETGAME ? cfg.netMobHealthModifier : 1);

    // Let the engine know about solid objects.
    //if(mobj->flags & MF_SOLID) mobj->ddflags |= DDMF_SOLID;
    P_SetDoomsdayFlags(mobj);

    if(gameskill != SM_NIGHTMARE)
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
    P_SetMobjPosition(mobj);

    //killough $dropoff_fix
    mobj->floorz   = P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT);
    mobj->dropoffz = mobj->floorz;
    mobj->ceilingz = P_GetFloatp(mobj->subsector, DMU_CEILING_HEIGHT);

    if(z == ONFLOORZ)
        mobj->pos[VZ] = mobj->floorz;
    else if(z == ONCEILINGZ)
        mobj->pos[VZ] = mobj->ceilingz - mobj->info->height;
    else if(z == FLOATRANDZ)
    {
        space = mobj->ceilingz - mobj->info->height - mobj->floorz;
        if(space > 48)
        {
            space -= 40;
            mobj->pos[VZ] =
                (space * P_Random()) / 256) + mobj->floorz + 40;
        }
        else
        {
            mobj->pos[VZ] = mobj->floorz;
        }
    }
    else
    {
        mobj->pos[VZ] = z;
    }

    if((mobj->flags2 & MF2_FLOORCLIP) &&
       P_GetMobjFloorType(mobj) >= FLOOR_LIQUID &&
       mobj->pos[VZ] == P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT))
    {
        mobj->floorclip = 10;
    }
    else
    {
        mobj->floorclip = 0;
    }

    return mobj;
}

mobj_t *P_SpawnMobj3fv(mobjtype_t type, float pos[3])
{
    return P_SpawnMobj3f(type, pos[VX], pos[VY], pos[VZ]);
}

void P_RemoveMobj(mobj_t *mobj)
{
    if((mobj->flags & MF_SPECIAL) && !(mobj->flags & MF_DROPPED) &&
       (mobj->type != MT_INV) && (mobj->type != MT_INS))
    {
        // Copy the mobj's info to the respawn que
        spawnobj_t *spawnobj = &itemrespawnque[iquehead];

        memcpy(spawnobj->pos, mobj->spawnspot.pos, sizeof(spawnobj->pos));
        spawnobj->angle = mobj->spawnspot.angle;
        spawnobj->type = mobj->spawnspot.type;
        spawnobj->thingflags = mobj->spawnspot.options;

        itemrespawntime[iquehead] = leveltime;
        iquehead = (iquehead + 1) & (ITEMQUESIZE - 1);

        // lose one off the end?
        if(iquehead == iquetail)
            iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
    }

    // unlink from sector and block lists
    P_UnsetMobjPosition(mobj);

    // stop any playing sound
    S_StopSound(0, mobj);

    // free block
    P_RemoveThinker((thinker_t *) mobj);
}

void P_CheckRespawnQueue(void)
{
    float       pos[3];
    subsector_t *ss;
    mobj_t     *mo;
    spawnobj_t *sobj;
    int         i;

    // Only respawn items in deathmatch 2 and optionally in coop.
    if(deathmatch != 2 && (!cfg.coopRespawnItems || !IS_NETGAME || deathmatch))
        return;

    // Nothing left to respawn?
    if(iquehead == iquetail)
        return;

    // Wait at least 30 seconds
    if(leveltime - itemrespawntime[iquetail] < 30 * 35)
        return;

    // Get the attributes of the mobj from spawn parameters.
    sobj = &itemrespawnque[iquetail];

    memcpy(pos, sobj->pos, sizeof(pos));
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    pos[VZ] = P_GetFloatp(ss, DMU_FLOOR_HEIGHT);

    // Spawn a teleport fog at the new spot.
    mo = P_SpawnMobj3fv(MT_IFOG, pos);
    S_StartSound(sfx_nmrrsp, mo);

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(sobj->type == mobjinfo[i].doomednum)
            break;
    }

    if(mobjinfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    mo = P_SpawnMobj3fv(pos, i);
    mo->angle = sobj->angle;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       P_GetMobjFloorType(mo) >= FLOOR_LIQUID &&
       mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        mo->floorclip = 10;
    }
    else
    {
        mo->floorclip = 0;
    }

    // Copy spawn attributes to the new mobj.
    memcpy(mo->spawnspot.pos, sobj->pos, sizeof(mo->spawnspot.pos));
    mo->spawnspot.angle = sobj->angle;
    mo->spawnspot.type = sobj->type;
    mo->spawnspot.options = sobj->thingflags;

    // Pull it from the que.
    iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
}

/**
 * Called when a player is spawned on the level.
 * Most of the player structure stays unchanged between levels.
 */
void P_SpawnPlayer(spawnspot_t *spot, int pnum)
{
    player_t   *p;
    float       pos[3];
    mobj_t     *mobj;
    int         i;

    if(pnum < 0)
        pnum = 0;
    if(pnum >= MAXPLAYERS - 1)
        pnum = MAXPLAYERS - 1;

    // Not playing?
    if(!players[pnum].plr->ingame)
        return;

    p = &players[pnum];

    if(p->playerstate == PST_REBORN)
        G_PlayerReborn(pnum);

    if(spot)
    {
        pos[VX] = spot->pos[VX];
        pos[VY] = spot->pos[VY];
        pos[VZ] = ONFLOORZ;
    }
    else
    {
        pos[VX] = pos[VY] = pos[VZ] = 0;
    }

    mobj = P_SpawnMobj3fv(pos, MT_PLAYER);

    // With clients all player mobjs are remote, even the consoleplayer.
    if(IS_CLIENT)
    {
        mobj->flags &= ~MF_SOLID;
        mobj->ddflags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // Set color translations for player sprites.
    i = cfg.playerColor[pnum];
    if(i > 0)
        mobj->flags |= i << MF_TRANSSHIFT;

    mobj->angle = (spot? spot->angle : 0); /* $unifiedangles */
    p->plr->lookdir = 0; /* $unifiedangles */
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    mobj->player = p;
    mobj->dplayer = p->plr;
    mobj->health = p->health;

    p->plr->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->plr->extraLight = 0;
    p->plr->fixedcolormap = 0;
    p->plr->lookdir = 0;

    if(!spot)
        p->plr->flags |= DDPF_CAMERA;

    if(p->plr->flags & DDPF_CAMERA)
    {
        p->plr->mo->pos[VZ] += (float) cfg.plrViewHeight;
        p->plr->viewheight = 0;
    }
    else
        p->plr->viewheight = (float) cfg.plrViewHeight;

    p->class = PCLASS_PLAYER;

    // Setup gun psprite.
    P_SetupPsprites(p);

    // Give all cards in death match mode.
    if(deathmatch)
        for(i = 0; i < NUM_KEY_TYPES; i++)
            p->keys[i] = true;

    if(pnum == consoleplayer)
    {
        // Wake up the status bar.
        ST_Start();
        // Wake up the heads up text.
        HU_Start();
    }
}

/*
 * Spawns the passed thing into the world.
 */
void P_SpawnMapThing(spawnspot_t *th)
{
    int         i, bit;
    mobj_t     *mobj;
    float       pos[3];

    //Con_Message("x = %i, y = %i, height = %i, angle = %i, type = %i, options = %i\n",
    //            th->x, th->y, th->height, th->angle, th->type, th->options);

    // Count deathmatch start positions.
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

    // Don't spawn things flagged for Multiplayer if we're not in a netgame.
    if(!IS_NETGAME && (th->options & MTF_NOTSINGLE))
        return;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching.
    if(deathmatch && (th->options & MTF_NOTDM))
        return;

    // Don't spawn things flagged for Not Coop if we're coop'in.
    if(IS_NETGAME && !deathmatch && (th->options & MTF_NOTCOOP))
        return;

    // Check for apropriate skill level.
    if(gameskill == SM_BABY)
        bit = 1;
    else if(gameskill == SM_NIGHTMARE)
        bit = 4;
    else
        bit = 1 << (gameskill - 1);

    if(!(th->options & bit))
        return;

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
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

    // Don't spawn keycards in deathmatch.
    if(deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
        return;

    // Check for specific disabled objects.
    if(IS_NETGAME && (th->options & MTF_NOTSINGLE)) // Multiplayer flag.
    {
        if(cfg.noCoopWeapons && !deathmatch && i >= MT_CLIP
           && i <= MT_SUPERSHOTGUN)
            return;

        // Don't spawn any special objects in coop?
        if(cfg.noCoopAnything && !deathmatch)
            return;

        // BFG disabled in netgames?
        if(cfg.noNetBFG && i == MT_MISC25)
            return;
    }

    // Don't spawn any monsters if -nomonsters.
    if(nomonsters && (i == MT_SKULL || (mobjinfo[i].flags & MF_COUNTKILL)))
    {
        return;
    }

    pos[VX] = (float) th->x;
    pos[VY] = (float) th->y;

    if(mobjinfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else if(mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
        pos[VZ] = FLOATRANDZ;
    else
        pos[VZ] = ONFLOORZ;

    mobj = P_SpawnMobj3fv(pos, i);
    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Seed random starting index for bobbing motion.
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
    if(th->options & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;

    // Set the spawn info for this mobj
    memcpy(mobj->spawnspot.pos, pos, sizeof(mobj->spawnspot.pos));
    mobj->spawnspot.angle = mobj->angle;
    mobj->spawnspot.type = mobjinfo[i].doomednum;
    mobj->spawnspot.options = th->options;
}

mobj_t *P_SpawnCustomPuff(mobjtype_t type, float x, float y, float z)
{
    mobj_t *th;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return NULL;

    z += FIX2FLT((P_Random() - P_Random()) << 10);

    th = P_SpawnMobj3f(type, x, y, z);
    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;

    // Make it last at least one tic.
    if(th->tics < 1)
        th->tics = 1;

    return th;
}

void P_SpawnPuff(float x, float y, float z)
{
    mobj_t     *th = P_SpawnCustomPuff(MT_PUFF, x, y, z);

    // Don't make punches spark on the wall.
    if(th && attackrange == MELEERANGE)
        P_SetMobjState(th, S_PUFF3);
}

void P_SpawnBlood(float x, float y, float z, int damage)
{
    mobj_t     *th;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    th = P_SpawnMobj3f(MT_BLOOD, x, y, z);
    th->mom[MZ] = 2;
    th->tics -= P_Random() & 3;

    if(th->tics < 1)
        th->tics = 1;

    if(damage <= 12 && damage >= 9)
        P_SetMobjState(th, S_BLOOD2);
    else if(damage < 9)
        P_SetMobjState(th, S_BLOOD3);
}

/**
 * Moves the missile forward a bit and possibly explodes it right there.
 * @param   th  The missile to be checked.
 * @return      @c true, if the missile is at a valid location
 *              else @c false,.
 */
boolean P_CheckMissileSpawn(mobj_t *th)
{
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->pos[VX] += (th->mom[MX] >> 1);
    th->pos[VY] += (th->mom[MY] >> 1);
    th->pos[VZ] += (th->mom[MZ] >> 1);

    if(!P_TryMove(th, th->pos[VX], th->pos[VY], false, false))
    {
        P_ExplodeMissile(th);
        return false;
    }

    return true;
}

/**
 * Tries to aim at a nearby monster if source is a player. Else aim is
 * taken at dest.
 *
 * @param   source      The mobj doing the shooting.
 * @param   dest        The mobj being shot at. Can be @c NULL if source
 *                      is a player.
 * @param   type        The type of mobj to be shot.
 * @return              Pointer to the newly spawned missile.
 */
mobj_t *P_SpawnMissile(mobjtype_t type, mobj_t *source, mobj_t *dest)
{
    float       pos[3];
    mobj_t     *th;
    angle_t     an;
    float       dist;
    float       slope;
    float       spawnZOff = 0;

    memcpy(pos, source->pos, sizeof(pos));

    if(source->player)
    {
        // see which target is to be aimed at
        an = source->angle;
        slope = P_AimLineAttack(source, an, 16 * 64);
        if(!cfg.noAutoAim)
            if(!linetarget)
            {
                an += 1 << 26;
                slope = P_AimLineAttack(source, an, 16 * 64);

                if(!linetarget)
                {
                    an -= 2 << 26;
                    slope = P_AimLineAttack(source, an, 16 * 64);
                }

                if(!linetarget)
                {
                    an = source->angle;
                    slope =
                        tan(LOOKDIR2RAD(source->dplayer->lookdir) / 1.2f;
                }
            }

        if(!(source->player->plr->flags & DDPF_CAMERA))
            spawnZOff = (cfg.plrViewHeight - 9) +
                        (source->player->plr->lookdir) / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_TRACER:            // Revenant Tracer Missile.
            spawnZOff = 16 + 32;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorclip;

    th = P_SpawnMobj3f(FLT2FIX(pos[VX]), FLT2FIX(pos[VY]), FLT2FIX(pos[VZ]), type);

    if(th->info->seesound)
        S_StartSound(th->info->seesound, th);

    if(!source->player)
    {
        an = R_PointToAngle2(pos[VX], pos[VY],
                             dest->pos[VX], dest->pos[VY]);
        // fuzzy player
        if(dest->flags & MF_SHADOW)
            an += (P_Random() - P_Random()) << 20;
    }

    th->target = source;        // where it came from
    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = FIX2FLT(FixedMul(th->info->speed, finecosine[an]));
    th->mom[MY] = FIX2FLT(FixedMul(th->info->speed, finesine[an]));

    if(source->player)
    {   // Allow free-aim with the BFG in deathmatch?
        if(deathmatch && cfg.netBFGFreeLook == 0 && type == MT_BFG)
            th->mom[MZ] = 0;
        else
            th->mom[MZ] = FIX2FLT(th->info->speed) * slope;
    }
    else
    {
        dist = P_ApproxDistance(dest->pos[VX] - pos[VX],
                                dest->pos[VY] - pos[VY]);
        dist /= FIX2FLT(th->info->speed);
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]), th->mom[MZ]);
    if(!dist)
        dist = 1;
    dist = FIX2FLT(th->info->speed) / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}
