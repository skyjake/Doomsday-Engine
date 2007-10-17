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
 * Moving object handling. Spawn functions.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jheretic.h"

#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define CLAMP(v, min, max) (v < min? v=min : v > max? v=max : v)

#define VANISHTICS  (2*TICSPERSEC)

#define MAX_BOB_OFFSET  0x80000

#define FRICTION_NORMAL 0xe800
#define FRICTION_LOW    0xf900
#define FRICTION_FLY    0xeb00
#define STOPSPEED       0x1000
#define STANDSPEED      0x8000

// TYPES -------------------------------------------------------------------

/** \todoCurrently Heretic doesn't really use the spawn que or spawnpoint stuff.
* It uses quite a different method involving special STATEs. The respawn
* time is therefore set by manipulation of the DED frame->time param of
* each state. Furthermore, this process is divided in two parts. */
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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t attackrange;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobjtype_t PuffType;
mobj_t *MissileMobj;

spawnobj_t itemrespawnque[ITEMQUESIZE];
int     itemrespawntime[ITEMQUESIZE];
int     iquehead;
int     iquetail;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Returns true if the mobj is still present.
 */
boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
    state_t *st;

    if(state == S_NULL)
    {                           // Remove mobj
        mobj->state = (state_t *) S_NULL;
        P_RemoveMobj(mobj);
        return (false);
    }

    if(mobj->ddflags & DDMF_REMOTE)
    {
        Con_Error("P_SetMobjState: Can't set Remote state!\n");
    }

    st = &states[state];
    P_SetState(mobj, state);

    mobj->turntime = false;     // $visangle-facetarget
    if(st->action)
    {                           // Call action function
        st->action(mobj);
    }
    return (true);
}

/*
 * Same as P_SetMobjState, but does not call the state function.
 */
boolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
    //state_t *st;

    if(state == S_NULL)
    {                           // Remove mobj
        mobj->state = (state_t *) S_NULL;
        P_RemoveMobj(mobj);
        return (false);
    }

    mobj->turntime = false;     // $visangle-facetarget
    P_SetState(mobj, state);
    return (true);
}

void P_ExplodeMissile(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        // Clients won't explode missiles.
        P_SetMobjState(mo, S_NULL);
        return;
    }

    if(mo->type == MT_WHIRLWIND)
    {
        if(++mo->special2 < 60)
        {
            return;
        }
    }
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }
    if(mo->info->deathsound)
    {
        S_StartSound(mo->info->deathsound, mo);
    }
}

void P_FloorBounceMissile(mobj_t *mo)
{
    mo->mom[MZ] = -mo->mom[MZ];
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
}

void P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move)
{
    angle >>= ANGLETOFINESHIFT;
    mo->mom[MX] += FixedMul(move, finecosine[angle]);
    mo->mom[MY] += FixedMul(move, finesine[angle]);
}

/*
 * Returns 1 if 'source' needs to turn clockwise, or 0 if 'source' needs
 * to turn counter clockwise.  'delta' is set to the amount 'source'
 * needs to turn.
 */
int P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta)
{
    angle_t diff;
    angle_t angle1;
    angle_t angle2;

    angle1 = source->angle;
    angle2 = R_PointToAngle2(source->pos[VX], source->pos[VY],
                             target->pos[VX], target->pos[VY]);
    if(angle2 > angle1)
    {
        diff = angle2 - angle1;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return (0);
        }
        else
        {
            *delta = diff;
            return (1);
        }
    }
    else
    {
        diff = angle1 - angle2;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return (1);
        }
        else
        {
            *delta = diff;
            return (0);
        }
    }
}

/*
 * The missile tracer field must be the target.  Returns true if
 * target was tracked, false if not.
 */
boolean P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnMax)
{
    int     dir;
    int     dist;
    angle_t delta;
    angle_t angle;
    mobj_t *target;

    target = actor->tracer;
    if(target == NULL)
    {
        return false;
    }
    if(!(target->flags & MF_SHOOTABLE))
    {   // Target died
        actor->tracer = NULL;
        return false;
    }
    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta >>= 1;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }
    if(dir)
    {   // Turn clockwise
        actor->angle += delta;
    }
    else
    {   // Turn counter clockwise
        actor->angle -= delta;
    }
    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FixedMul(actor->info->speed, finecosine[angle]);
    actor->mom[MY] = FixedMul(actor->info->speed, finesine[angle]);
    if(actor->pos[VZ] + FLT2FIX(actor->height) < target->pos[VZ] ||
       target->pos[VZ] + FLT2FIX(target->height) < actor->pos[VZ])
    {                           // Need to seek vertically
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist = dist / actor->info->speed;
        if(dist < 1)
        {
            dist = 1;
        }
        actor->mom[MZ] = (target->pos[VZ] - actor->pos[VZ]) / dist;
    }
    return true;
}

/*
 * Wind pushes the mobj, if its sector special is a wind type.
 */
void P_WindThrust(mobj_t *mo)
{
    static int windTab[3] = { 2048 * 5, 2048 * 10, 2048 * 25 };

    int special = P_XSector(P_GetPtrp(mo->subsector,
                                      DMU_SECTOR))->special;
    switch (special)
    {
    case 40:
    case 41:
    case 42:                    // Wind_East
        P_ThrustMobj(mo, 0, windTab[special - 40]);
        break;
    case 43:
    case 44:
    case 45:                    // Wind_North
        P_ThrustMobj(mo, ANG90, windTab[special - 43]);
        break;
    case 46:
    case 47:
    case 48:                    // Wind_South
        P_ThrustMobj(mo, ANG270, windTab[special - 46]);
        break;
    case 49:
    case 50:
    case 51:                    // Wind_West
        P_ThrustMobj(mo, ANG180, windTab[special - 49]);
        break;
    }
}

fixed_t P_GetMobjFriction(mobj_t *mo)
{
    sector_t* sector = P_GetPtrp(mo->subsector, DMU_SECTOR);

    if(mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= FLT2FIX(mo->floorz)) &&
       !mo->onmobj)
    {
        return FRICTION_FLY;
    }
    else if(P_XSector(sector)->special == 15)
    {
        return FRICTION_LOW;
    }
    return XS_Friction(sector);
}

void P_XYMovement(mobj_t *mo)
{
    fixed_t ptryx, ptryy;
    player_t *player;
    fixed_t xmove, ymove;
    boolean largeNegative;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    xmove = mo->mom[MX] = CLAMP(mo->mom[MX], -MAXMOVE, MAXMOVE);
    ymove = mo->mom[MY] = CLAMP(mo->mom[MY], -MAXMOVE, MAXMOVE);

    if((xmove | ymove) == 0)
    {
        if(mo->flags & MF_SKULLFLY)
        {                       // A flying mobj slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
            P_SetMobjState(mo, mo->info->seestate);
        }
        return;
    }

    if(mo->flags2 & MF2_WINDTHRUST)
        P_WindThrust(mo);

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
        {                       // Blocked move
            if(mo->flags2 & MF2_SLIDE)
            {                   // Try to slide along it
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {   // Explode a missile
                if(ceilingline)
                {
                    sector_t* backsector =
                        P_GetPtrp(ceilingline, DMU_BACK_SECTOR);

                    if(backsector && P_GetIntp(backsector,
                                               DMU_CEILING_TEXTURE) == skyflatnum)
                    {
                        // Hack to prevent missiles exploding against the sky
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->mom[MX] = mo->mom[MY] = 0;
                            mo->mom[MZ] = -FRACUNIT;
                        }
                        else
                        {
                            P_RemoveMobj(mo);
                        }
                        return;
                    }
                }
                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
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
    {                           // No friction for missiles
        return;
    }
    if(mo->pos[VZ] > FLT2FIX(mo->floorz) && !mo->onmobj && !(mo->flags2 & MF2_FLY))
    {                           // No friction when falling
        return;
    }

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
    if(player && mo->mom[MX] > -STANDSPEED && mo->mom[MX] < STANDSPEED &&
       mo->mom[MY] > -STANDSPEED && mo->mom[MY] < STANDSPEED &&
       !player->plr->cmd.forwardMove && !player->plr->cmd.sideMove)
    {
        // if in a walking frame, stop moving
        if((unsigned) ((player->plr->mo->state - states) - PCLASS_INFO(player->class)->runstate) < 4)
            P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->normalstate);
    }

    if(mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED && mo->mom[MY] > -STOPSPEED
       && mo->mom[MY] < STOPSPEED && (!player ||
                                   (player->plr->cmd.forwardMove == 0 &&
                                    player->plr->cmd.sideMove == 0)))
    {
        mo->mom[MX] = 0;
        mo->mom[MY] = 0;
    }
    else
    {
        if(mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= FLT2FIX(mo->floorz)) &&
           !mo->onmobj)
        {
            mo->mom[MX] = FixedMul(mo->mom[MX], FRICTION_FLY);
            mo->mom[MY] = FixedMul(mo->mom[MY], FRICTION_FLY);
        }
#if __JHERETIC__
        else if(P_XSector(P_GetPtrp(mo->subsector,
                                    DMU_SECTOR))->special == 15)
        {
            // Friction_Low
            mo->mom[MX] = FixedMul(mo->mom[MX], FRICTION_LOW);
            mo->mom[MY] = FixedMul(mo->mom[MY], FRICTION_LOW);
        }
#endif
        else
        {
            fixed_t fric = P_GetMobjFriction(mo);

            mo->mom[MX] = FixedMul(mo->mom[MX], fric);
            mo->mom[MY] = FixedMul(mo->mom[MY], fric);
        }
    }
}

void P_ZMovement(mobj_t *mo)
{
    fixed_t     gravity;
    fixed_t     dist;
    fixed_t     delta;

    gravity = XS_Gravity(P_GetPtrp(mo->subsector, DMU_SECTOR));

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    // check for smooth step up
    if(mo->player && FIX2FLT(mo->pos[VZ]) < mo->floorz)
    {
        mo->dplayer->viewheight -= mo->floorz - FIX2FLT(mo->pos[VZ]);

        mo->dplayer->deltaviewheight =
            FIX2FLT(((cfg.plrViewHeight << FRACBITS) - FLT2FIX(mo->dplayer->viewheight)) >> 3);
    }

    // adjust height
    mo->pos[VZ] += mo->mom[MZ];
    if((mo->flags2 & MF2_FLY) &&
       mo->onmobj && mo->pos[VZ] > mo->onmobj->pos[VZ] + FLT2FIX(mo->onmobj->height))
        mo->onmobj = NULL; // We were on a mobj, we are NOT now.

    if(mo->flags & MF_FLOAT && mo->target && !P_IsCamera(mo->target))
    {
        // float down towards target if too close
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);

            //delta = (mo->target->pos[VZ] + (mo->height>>1)) - mo->z;
            delta = (mo->target->pos[VZ] + FLT2FIX(mo->target->height /2)) -
                    (mo->pos[VZ] + FLT2FIX(mo->height /2));

            if(dist < mo->radius + mo->target->radius &&
               ABS(delta) < FLT2FIX(mo->height + mo->target->height))
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
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->pos[VZ] > FLT2FIX(mo->floorz) &&
       !mo->onmobj && (leveltime & 2))
    {
        mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
    }

    // Clip movement. Another thing?
    if(mo->onmobj && mo->pos[VZ] <= mo->onmobj->pos[VZ] + FLT2FIX(mo->onmobj->height))
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dplayer->deltaviewheight = FIX2FLT(mo->mom[MZ] >> 3);

                if(mo->player->health > 0)
                    S_StartSound(sfx_plroof, mo);
            }
            mo->mom[MZ] = 0;
        }
        if(!mo->mom[MZ])
            mo->pos[VZ] = mo->onmobj->pos[VZ] + FLT2FIX(mo->onmobj->height);

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // The floor.
    if(mo->pos[VZ] <= FLT2FIX(mo->floorz))
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

        int correct_lost_soul_bounce = false;

        if(correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
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
                mo->player->plr->deltaviewheight = FIX2FLT(mo->mom[MZ] >> 3);
#if __JHERETIC__
                mo->player->jumptics = 12;  // can't jump in a while.
#endif
                // Fix DOOM bug - dead players grunting when hitting the ground
                // (e.g., after an archvile attack)
                if(mo->player->health > 0)
                    S_StartSound(sfx_plroof, mo);
            }
            P_HitFloor(mo);
            mo->mom[MZ] = 0;
        }
        mo->pos[VZ] = FLT2FIX(mo->floorz);

        // cph 2001/05/26 -
        // See lost soul bouncing comment above. We need this here for bug
        // compatibility with original Doom2 v1.9 - if a soul is charging and
        // hit by a raising floor this incorrectly reverses its Y momentum.
        //

        if(!correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ];

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
#if __JHERETIC__
            else if(mo->type == MT_MNTRFX2)
            {                   // Minotaur floor fire can go up steps
                return;
            }
            else
#endif
            {
                P_ExplodeMissile(mo);
                return;
            }
        }
#if __JHERETIC__
        if(mo->info->crashstate && (mo->flags & MF_CORPSE))
        {
            P_SetMobjState(mo, mo->info->crashstate);
            return;
        }
#endif
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(gravity >> 3) * 2;
        else
            mo->mom[MZ] -= gravity >> 3;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(FIX2FLT(mo->pos[VZ]) + mo->height > mo->ceilingz)
    {
        // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = FLT2FIX(mo->ceilingz - mo->height);

        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(P_GetIntp(mo->subsector, DMU_CEILING_TEXTURE) == skyflatnum)
            {
#if __JHERETIC__
                if(mo->type == MT_BLOODYSKULL)
                {
                    mo->mom[MX] = mo->mom[MY] = 0;
                    mo->mom[MZ] = -FRACUNIT;
                }
                else
#endif
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
                     P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT) + TELEFOGHEIGHT,
                     MT_TFOG);
    // initiate teleport sound
    S_StartSound(sfx_telept, mo);

    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    mo = P_SpawnMobj(pos[VX], pos[VY],
                     P_GetFixedp(ss, DMU_FLOOR_HEIGHT) + TELEFOGHEIGHT,
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

    if(mobj->spawninfo.options & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->reactiontime = 18;

    // remove the old monster.
    P_RemoveMobj(mobj);
}

/*
 * Thinker for the ultra-fast blaster PL2 ripper-spawning missile.
 */
void P_BlasterMobjThinker(mobj_t *mobj)
{
    int     i;
    fixed_t xfrac;
    fixed_t yfrac;
    fixed_t zfrac;
    fixed_t z;
    boolean changexy;

    // Handle movement
    if(mobj->mom[MX] || mobj->mom[MY] || (mobj->pos[VZ] != FLT2FIX(mobj->floorz)) || mobj->mom[MZ])
    {
        xfrac = mobj->mom[MX] >> 3;
        yfrac = mobj->mom[MY] >> 3;
        zfrac = mobj->mom[MZ] >> 3;
        changexy = xfrac || yfrac;
        for(i = 0; i < 8; i++)
        {
            if(changexy)
            {
                if(!P_TryMove(mobj, mobj->pos[VX] + xfrac, mobj->pos[VY] + yfrac, false, false))
                {               // Blocked move
                    P_ExplodeMissile(mobj);
                    return;
                }
            }
            mobj->pos[VZ] += zfrac;
            if(mobj->pos[VZ] <= FLT2FIX(mobj->floorz))
            {                   // Hit the floor
                mobj->pos[VZ] = FLT2FIX(mobj->floorz);
                P_HitFloor(mobj);
                P_ExplodeMissile(mobj);
                return;
            }
            if(FIX2FLT(mobj->pos[VZ]) + mobj->height > mobj->ceilingz)
            {                   // Hit the ceiling
                mobj->pos[VZ] = FLT2FIX(mobj->ceilingz - mobj->height);
                P_ExplodeMissile(mobj);
                return;
            }
            if(changexy && (P_Random() < 64))
            {
                z = mobj->pos[VZ] - 8 * FRACUNIT;
                if(z < FLT2FIX(mobj->floorz))
                {
                    z = FLT2FIX(mobj->floorz);
                }
                P_SpawnMobj(mobj->pos[VX], mobj->pos[VY], z, MT_BLASTERSMOKE);
            }
        }
    }
    // Advance the state
    if(mobj->tics != -1)
    {
        mobj->tics--;
        while(!mobj->tics)
        {
            if(!P_SetMobjState(mobj, mobj->state->nextstate))
            {                   // mobj was removed
                return;
            }
        }
    }
}

void P_MobjThinker(mobj_t *mobj)
{
    if(mobj->ddflags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.
#if __JDOOM__
    // Spectres get selector = 1.
    if(mobj->type == MT_SHADOWS)
        mobj->selector = (mobj->selector & ~DDMOBJ_SELECTOR_MASK) | 1;
#endif

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

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
        //mobj->pos[VZ] = mobj->floorz+FloatBobOffsets[(mobj->health++)&63];

        // Keep it on the floor.
        mobj->pos[VZ] = FLT2FIX(mobj->floorz);

        // Negative floorclip raises the mobj off the floor.
        mobj->floorclip = FIX2FLT(-mobj->special1);
        if(mobj->floorclip < FIX2FLT(-MAX_BOB_OFFSET))
        {
            // We don't want it going through the floor.
            mobj->floorclip = FIX2FLT(-MAX_BOB_OFFSET);
        }

        // Old floatbob used health as index, let's still increase it
        // as before (in case somebody wants to use it).
        mobj->health++;
    }
    else if((mobj->pos[VZ] != FLT2FIX(mobj->floorz)) || mobj->mom[MZ]) // GMJ 02/02/02
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

        if(mobj->pos[VZ] > FLT2FIX(mobj->dropoffz) && // Only objects contacting dropoff
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

/*
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    mobj_t *mobj;

    //state_t *st;
    mobjinfo_t *info;
    fixed_t space;

#ifdef _DEBUG
    if(type < 0 || type >= Get(DD_NUMMOBJTYPES))
        Con_Error("P_SpawnMobj: Illegal mobj type %i.\n", type);
#endif

    mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
    memset(mobj, 0, sizeof(*mobj));
    info = &mobjinfo[type];
    mobj->type = type;
    mobj->info = info;
    mobj->pos[VX] = x;
    mobj->pos[VY] = y;
    mobj->radius = info->radius;
    mobj->height = FIX2FLT(info->height);
    mobj->flags = info->flags;
    mobj->flags2 = info->flags2;
    mobj->flags3 = info->flags3;

    // Let the engine know about solid objects.
    if(mobj->flags & MF_SOLID)
        mobj->ddflags |= DDMF_SOLID;
    if(mobj->flags2 & MF2_DONTDRAW)
        mobj->ddflags |= DDMF_DONTDRAW;

    mobj->damage = info->damage;

    mobj->health = info->spawnhealth
                   * (IS_NETGAME ? cfg.netMobHealthModifier : 1);

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

    // set subsector and/or block links.
    P_SetThingPosition(mobj);

    //killough $dropoff_fix
    mobj->floorz   = P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT);
    mobj->dropoffz = mobj->floorz;
    mobj->ceilingz = P_GetFloatp(mobj->subsector, DMU_CEILING_HEIGHT);

    if(z == ONFLOORZ)
    {
        mobj->pos[VZ] = FLT2FIX(mobj->floorz);
    }
    else if(z == ONCEILINGZ)
    {
        mobj->pos[VZ] = FLT2FIX(mobj->ceilingz) - mobj->info->height;
    }
    else if(z == FLOATRANDZ)
    {
        space = FLT2FIX(mobj->ceilingz) - mobj->info->height - FLT2FIX(mobj->floorz);
        if(space > 48 * FRACUNIT)
        {
            space -= 40 * FRACUNIT;
            mobj->pos[VZ] =
                ((space * P_Random()) >> 8) + FLT2FIX(mobj->floorz + 40);
        }
        else
        {
            mobj->pos[VZ] = FLT2FIX(mobj->floorz);
        }
    }
    else
    {
        mobj->pos[VZ] = z;
    }

    if(mobj->flags2 & MF2_FLOORCLIP &&
       P_GetThingFloorType(mobj) >= FLOOR_LIQUID &&
       mobj->pos[VZ] == P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT))
    {
        mobj->floorclip = 10;
    }
    else
    {
        mobj->floorclip = 0;
    }
    return (mobj);
}

void P_RemoveMobj(mobj_t *mobj)
{
    if((mobj->flags & MF_SPECIAL) && !(mobj->flags & MF_DROPPED))
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

/*
 * Called when a player is spawned on the level
 * Most of the player structure stays unchanged between levels
 */
void P_SpawnPlayer(thing_t * mthing, int plrnum)
{
    player_t *p;
    fixed_t pos[3];
    mobj_t *mobj;
    int     i;
    extern int playerkeys;

    if(!players[plrnum].plr->ingame)
        return;                 // not playing

    p = &players[plrnum];

    Con_Printf("P_SpawnPlayer: spawning player %i, col=%i.\n", plrnum,
               cfg.playerColor[plrnum]);

    if(p->playerstate == PST_REBORN)
        G_PlayerReborn(plrnum);

    pos[VX] = mthing->x << FRACBITS;
    pos[VY] = mthing->y << FRACBITS;

    pos[VZ] = ONFLOORZ;
    mobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER);

    // On clients all player mobjs are remote, even the consoleplayer.
    if(IS_CLIENT)
    {
        mobj->flags &= ~MF_SOLID;
        mobj->ddflags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // set color translations for player sprites
    i = cfg.playerColor[plrnum];
    if(i > 0)
        mobj->flags |= i << MF_TRANSSHIFT;
    mobj->angle = ANG45 * (mthing->angle / 45); /* $unifiedangles */
    p->plr->lookdir = 0; /* $unifiedangles */
    p->plr->lookdir = 0;
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    mobj->player = p;
    mobj->dplayer = p->plr;
    mobj->health = p->health;
    p->plr->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->morphTics = 0;
    p->rain1 = NULL;
    p->rain2 = NULL;
    p->plr->extraLight = 0;
    p->plr->fixedcolormap = 0;
    if(p->plr->flags & DDPF_CAMERA)
    {
        p->plr->mo->pos[VZ] += (cfg.plrViewHeight << FRACBITS);
        p->plr->viewheight = 0;
    }
    else
        p->plr->viewheight = (float) cfg.plrViewHeight;

    P_SetupPsprites(p);         // setup gun psprite

    p->class = PCLASS_PLAYER;

    if(deathmatch)
    {                           // Give all keys in death match mode
        for(i = 0; i < NUM_KEY_TYPES; i++)
        {
            p->keys[i] = true;
            if(p == &players[consoleplayer])
            {
                playerkeys = 7;
            }
        }
    }
    else if(p == &players[consoleplayer])
    {
        playerkeys = 0;
    }

    if(plrnum == consoleplayer)
    {
        // wake up the status bar
        ST_Start();

        // wake up the heads up text
        HU_Start();
    }
}

void P_SpawnMapThing(thing_t * th)
{
    int     i;
    int     bit;
    mobj_t *mobj;
    fixed_t pos[3];

    /*Con_Message("x = %i, y = %i, height = %i, angle = %i, type = %i, options = %i\n",
                mthing->x, mthing->y, mthing->height, mthing->angle, mthing->type, mthing->options);*/

    // count deathmatch start positions
    if(th->type == 11)
    {
        if(deathmatch_p < &deathmatchstarts[MAX_DM_STARTS])
        {
            memcpy(deathmatch_p, th, sizeof(*th));
            deathmatch_p++;
        }
        return;
    }

    // check for players specially
    if(th->type <= 4)
    {
        // Register this player start.
        P_RegisterPlayerStart(th);
        return;
    }

    // Ambient sound sequences
    if(th->type >= 1200 && th->type < 1300)
    {
        P_AddAmbientSfx(th->type - 1200);
        return;
    }

    // Check for boss spots
    if(th->type == 56)      // Monster_BossSpot
    {
        P_AddBossSpot(th->x << FRACBITS, th->y << FRACBITS, ANG45 * (th->angle / 45));
        return;
    }

    // check for apropriate skill level
    if(!IS_NETGAME && (th->options & 16))
        return;

    if(gameskill == SM_BABY)
        bit = 1;
    else if(gameskill == SM_NIGHTMARE)
        bit = 4;
    else
        bit = 1 << (gameskill - 1);
    if(!(th->options & bit))
        return;

    // find which type to spawn
    for(i = 0; i < Get(DD_NUMMOBJTYPES); i++)
        if(th->type == mobjinfo[i].doomednum)
            break;

    // Clients only spawn local objects.
    if(IS_CLIENT)
    {
        if(!(mobjinfo[i].flags & MF_LOCAL))
            return;
    }

    if(i == Get(DD_NUMMOBJTYPES))
    {
        Con_Error("P_SpawnMapThing: Unknown type %i at (%i, %i)", th->type,
                  th->x, th->y);
    }

    // don't spawn keys and players in deathmatch
    if(deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
        return;

    // don't spawn any monsters if -nomonsters
    if(nomonsters && (mobjinfo[i].flags & MF_COUNTKILL))
        return;

    // spawn it
    switch (i)
    {                           // Special stuff
    case MT_WSKULLROD:
    case MT_WPHOENIXROD:
    case MT_AMSKRDWIMPY:
    case MT_AMSKRDHEFTY:
    case MT_AMPHRDWIMPY:
    case MT_AMPHRDHEFTY:
    case MT_AMMACEWIMPY:
    case MT_AMMACEHEFTY:
    case MT_ARTISUPERHEAL:
    case MT_ARTITELEPORT:
    case MT_ITEMSHIELD2:
        if(gamemode == shareware)
        {                       // Don't place on map in shareware version
            return;
        }
        break;
    case MT_WMACE:
        if(gamemode != shareware)
        {                       // Put in the mace spot list
            P_AddMaceSpot(th);
            return;
        }
        return;
    default:
        break;
    }
    pos[VX] = th->x << FRACBITS;
    pos[VY] = th->y << FRACBITS;
    if(mobjinfo[i].flags & MF_SPAWNCEILING)
    {
        pos[VZ] = ONCEILINGZ;
    }
    else if(mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
    {
        pos[VZ] = FLOATRANDZ;
    }
    else
    {
        pos[VZ] = ONFLOORZ;
    }

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
    if(th->options & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;

    // Set the spawn info for this mobj
    memcpy(mobj->spawninfo.pos, pos, sizeof(mobj->spawninfo.pos));
    mobj->spawninfo.angle = mobj->angle;
    mobj->spawninfo.type = mobjinfo[i].doomednum;
    mobj->spawninfo.options = th->options;
}

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    mobj_t *puff;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return;

    z += ((P_Random() - P_Random()) << 10);
    puff = P_SpawnMobj(x, y, z, PuffType);
    if(puff->info->attacksound)
    {
        S_StartSound(puff->info->attacksound, puff);
    }
    switch (PuffType)
    {
    case MT_BEAKPUFF:
    case MT_STAFFPUFF:
        puff->mom[MZ] = FRACUNIT;
        break;
    case MT_GAUNTLETPUFF1:
    case MT_GAUNTLETPUFF2:
        puff->mom[MZ] = .8 * FRACUNIT;
    default:
        break;
    }
}

void P_BloodSplatter(fixed_t x, fixed_t y, fixed_t z, mobj_t *originator)
{
    mobj_t *mo;

    mo = P_SpawnMobj(x, y, z, MT_BLOODSPLATTER);
    mo->target = originator;
    mo->mom[MX] = (P_Random() - P_Random()) << 9;
    mo->mom[MY] = (P_Random() - P_Random()) << 9;
    mo->mom[MZ] = FRACUNIT * 2;
}

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
    th->mom[MX] = mo->mom[MX] >> 1;
    th->mom[MY] = mo->mom[MY] >> 1;
    th->tics += P_Random() & 3;
}

int P_GetThingFloorType(mobj_t *thing)
{
    return P_GetTerrainType(P_GetPtrp(thing->subsector, DMU_SECTOR), PLN_FLOOR);
}

#define SMALLSPLASHCLIP 12;

int P_HitFloor(mobj_t *thing)
{
    mobj_t *mo;

    if(thing->floorz != P_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT))
    {
        // don't splash if landing on the edge above water/lava/etc....
        return (FLOOR_SOLID);
    }

    // Things that don't splash go here
    switch (thing->type)
    {
    case MT_LAVASMOKE:
    case MT_SPLASH:
    case MT_SLUDGECHUNK:
        return (FLOOR_SOLID);
    default:
        break;
    }

    switch (P_GetThingFloorType(thing))
    {
    case FLOOR_WATER:
        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SPLASHBASE);
        if(mo)
            mo->floorclip += SMALLSPLASHCLIP;

        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SPLASH);
        if(mo)
        {
            mo->target = thing;
            mo->mom[MX] = (P_Random() - P_Random()) << 8;
            mo->mom[MY] = (P_Random() - P_Random()) << 8;
            mo->mom[MZ] = 2 * FRACUNIT + (P_Random() << 8);
        }
        S_StartSound(sfx_gloop, mo);
        return (FLOOR_WATER);

    case FLOOR_LAVA:
        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_LAVASPLASH);
        if(mo)
            mo->floorclip += SMALLSPLASHCLIP;

        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_LAVASMOKE);
        if(mo)
        {
            mo->mom[MZ] = FRACUNIT + (P_Random() << 7);
        }
        S_StartSound(sfx_burn, mo);
        return (FLOOR_LAVA);

    case FLOOR_SLUDGE:
        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SLUDGESPLASH);
        if(mo)
            mo->floorclip += SMALLSPLASHCLIP;

        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SLUDGECHUNK);
        if(mo)
        {
            mo->target = thing;
            mo->mom[MX] = (P_Random() - P_Random()) << 8;
            mo->mom[MY] = (P_Random() - P_Random()) << 8;
            mo->mom[MZ] = FRACUNIT + (P_Random() << 8);
        }
        return (FLOOR_SLUDGE);
    }
    return (FLOOR_SOLID);
}

/*
 * Returns true if the missile is at a valid spawn point, otherwise
 * explodes it and returns false.
 */
boolean P_CheckMissileSpawn(mobj_t *missile)
{
    //missile->tics -= P_Random()&3;

    // move a little forward so an angle can be computed if it
    // immediately explodes
    if(missile->type == MT_BLASTERFX1)
    {                           // Ultra-fast ripper spawning missile
        missile->pos[VX] += (missile->mom[MX] >> 3);
        missile->pos[VY] += (missile->mom[MY] >> 3);
        missile->pos[VZ] += (missile->mom[MZ] >> 3);
    }
    else
    {
        missile->pos[VX] += (missile->mom[MX] >> 1);
        missile->pos[VY] += (missile->mom[MY] >> 1);
        missile->pos[VZ] += (missile->mom[MZ] >> 1);
    }
    if(!P_TryMove(missile, missile->pos[VX], missile->pos[VY], false, false))
    {
        P_ExplodeMissile(missile);
        return (false);
    }
    return (true);
}

/**
 * Tries to aim at a nearby monster if <code>source</code> is a player.
 * Else aim is taken at <code>dest</code>.
 * @param   source      The mobj doing the shooting.
 * @param   dest        The mobj being shot at. Can be <code>NULL</code>
 *                      if <code>source</code> is a player.
 * @param   type        The type of mobj to be shot.
 * @return              Pointer to the newly spawned missile.
 */
mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    fixed_t     pos[3];
    mobj_t     *th = 0;
    angle_t     an = 0;
    fixed_t     dist = 0;
    fixed_t     slope = 0;
    fixed_t     spawnZOff = 0;

    memcpy(pos, source->pos, sizeof(pos));

    if(source->player)
    {
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
                    slope =
                        FRACUNIT * (tan(LOOKDIR2RAD(source->dplayer->lookdir)) / 1.2);
                }
            }

        if(!(source->player->plr->flags & DDPF_CAMERA))
            spawnZOff = (cfg.plrViewHeight - 9) * FRACUNIT +
                   (((int) source->player->plr->lookdir) << FRACBITS) / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_MNTRFX1:        // Minotaur swing attack missile
            spawnZOff = 40 * FRACUNIT;
            break;

        case MT_SRCRFX1:        // Sorcerer Demon fireball
            spawnZOff = 48 * FRACUNIT;
            break;

        case MT_KNIGHTAXE:      // Knight normal axe
        case MT_REDAXE:         // Knight red power axe
            spawnZOff = 36 * FRACUNIT;
            break;

        default:
            spawnZOff = 32 * FRACUNIT;
            break;
        }
    }

    if(type == MT_MNTRFX2)      // always exactly on the floor.
        pos[VZ] = ONFLOORZ;
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= FLT2FIX(source->floorclip);
    }

    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);

    if(th->info->seesound)
        S_StartSound(th->info->seesound, th);

    if(!source->player)
    {
        an = R_PointToAngle2(pos[VX], pos[VY],
                             dest->pos[VX], dest->pos[VY]);
        // fuzzy player
        if(dest->flags & MF_SHADOW)
            an += (P_Random() - P_Random()) << 21; // note << 20 in jDoom
    }

    th->target = source;        // where it came from
    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = FixedMul(th->info->speed, finecosine[an]);
    th->mom[MY] = FixedMul(th->info->speed, finesine[an]);

    if(source->player)
    {
        th->mom[MZ] = FixedMul(th->info->speed, slope);
    }
    else
    {
        dist = P_ApproxDistance(dest->pos[VX] - pos[VX],
                                dest->pos[VY] - pos[VY]);
        dist = dist / th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]), th->mom[MZ]);
    if(!dist)
        dist = 1;
    dist = FixedDiv(th->info->speed, dist);
    th->mom[MX] = FixedMul(th->mom[MX], dist);
    th->mom[MY] = FixedMul(th->mom[MY], dist);
    th->mom[MZ] = FixedMul(th->mom[MZ], dist);

#if __JHERETIC__
    //// \kludge Set this global ptr as we need access to the mobj even if it
    //// explodes instantly in order to assign values to it.
    //// This is a bit of a kludge really...
    MissileMobj = th;
#endif

    return (P_CheckMissileSpawn(th) ? th : NULL);
}

/**
 * Tries to aim at a nearby monster if <code>source</code> is a player.
 * Else aim is the angle of the source mobj. Z angle is specified with
 * <code>mom[MZ]</code>
 * @param   source      The mobj doing the shooting.
 * @param   type        The type of mobj to be shot.
 * @param   angle       The X/Y angle to shoot the missile in.
 * @param   mom[MZ]        The Z momentum of the missile to be spawned.
 * @return              Pointer to the newly spawned missile.
 */
mobj_t *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type, angle_t angle,
                            fixed_t momz)
{
    fixed_t     pos[3];
    mobj_t     *th = 0;
    angle_t     an = 0;
    fixed_t     dist = 0;
    fixed_t     slope = 0;
    fixed_t     spawnZOff = 0;

    memcpy(pos, source->pos, sizeof(pos));

    an = angle;
    if(source->player)
    {
        // Try to find a target
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
                    an = angle;
            }

        if(!(source->player->plr->flags & DDPF_CAMERA))
            spawnZOff = (cfg.plrViewHeight - 9) * FRACUNIT +
                        (((int) source->player->plr->lookdir) << FRACBITS) / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_MNTRFX1:        // Minotaur swing attack missile
            spawnZOff = 40 * FRACUNIT;
            break;

        case MT_SRCRFX1:        // Sorcerer Demon fireball
            spawnZOff = 48 * FRACUNIT;
            break;

        case MT_KNIGHTAXE:      // Knight normal axe
        case MT_REDAXE:         // Knight red power axe
            spawnZOff = 36 * FRACUNIT;
            break;

        default:
            spawnZOff = 32 * FRACUNIT;
            break;
        }
    }

    if(type == MT_MNTRFX2)      // always exactly on the floor.
        pos[VZ] = ONFLOORZ;
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= FLT2FIX(source->floorclip);
    }

    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);

    if(th->info->seesound)
        S_StartSound(th->info->seesound, th);

    th->target = source;        // where it came from
    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = FixedMul(th->info->speed, finecosine[an]);
    th->mom[MY] = FixedMul(th->info->speed, finesine[an]);

    if(source->player && momz == -12345)
    {
        th->mom[MZ] = FixedMul(th->info->speed, slope);

        // Make sure the speed is right (in 3D).
        dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]), th->mom[MZ]);
        if(!dist)
            dist = 1;
        dist = FixedDiv(th->info->speed, dist);
        th->mom[MX] = FixedMul(th->mom[MX], dist);
        th->mom[MY] = FixedMul(th->mom[MY], dist);
        th->mom[MZ] = FixedMul(th->mom[MZ], dist);
    }
    else
    {
        th->mom[MZ] = momz;
    }

#if __JHERETIC__
    //// \kludge Set this global ptr as we need access to the mobj even if it
    //// explodes instantly in order to assign values to it.
    //// This is a bit of a kludge really...
    MissileMobj = th;
#endif

    return (P_CheckMissileSpawn(th) ? th : NULL);
}

void C_DECL A_ContMobjSound(mobj_t *actor)
{
    switch (actor->type)
    {
    case MT_KNIGHTAXE:
        S_StartSound(sfx_kgtatk, actor);
        break;
    case MT_MUMMYFX1:
        S_StartSound(sfx_mumhed, actor);
        break;
    default:
        break;
    }
}
