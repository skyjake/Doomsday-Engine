/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_actor.c: Common code relating to mobjs.
 *
 * Mobj management, movement smoothing etc.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

// MACROS ------------------------------------------------------------------

#define MIN_STEP        ((10 * ANGLE_1) >> 16)  // degrees per tic
#define MAX_STEP        (ANG90 >> 16)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Removes the given mobj from the world.
 *
 * @param mo                The mobj to be removed.
 * @param noRespawn         Disable the automatical respawn which occurs
 *                          with mobjs of certain type(s) (also dependant on
 *                          the current gamemode).
 *                          Generally this should be @c false.
 */
void P_MobjRemove(mobj_t* mo, boolean noRespawn)
{
    if(!noRespawn)
    {
#if __JDOOM__ || __JDOOM64__
        if((mo->flags & MF_SPECIAL) && !(mo->flags & MF_DROPPED) &&
           (mo->type != MT_INV) && (mo->type != MT_INS))
        {
            P_RespawnEnqueue(&mo->spawnSpot);
        }
#elif __JHERETIC__
        if((mo->flags & MF_SPECIAL) && !(mo->flags & MF_DROPPED))
        {
            P_RespawnEnqueue(&mo->spawnSpot);
        }
#endif
    }

#if __JHEXEN__
    if((mo->flags & MF_COUNTKILL) && (mo->flags & MF_CORPSE))
    {
        A_DeQueueCorpse(mo);
    }

    P_MobjRemoveFromTIDList(mo);
#endif

    P_MobjDestroy(mo);
}

/**
 * Called after a move to link the mobj back into the world.
 */
void P_MobjSetPosition(mobj_t* mo)
{
    int                 flags = 0;

    if(!(mo->flags & MF_NOSECTOR))
        flags |= DDLINK_SECTOR;

    if(!(mo->flags & MF_NOBLOCKMAP))
        flags |= DDLINK_BLOCKMAP;

    P_MobjLink(mo, flags);
}

/**
 * Unlinks a mobj from the world so that it can be moved.
 */
void P_MobjUnsetPosition(mobj_t* mo)
{
    P_MobjUnlink(mo);
}

/**
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_MobjSetSRVO(mobj_t* mo, float stepx, float stepy)
{
    mo->srvo[VX] = -stepx;
    mo->srvo[VY] = -stepy;
}

/**
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_MobjSetSRVOZ(mobj_t* mo, float stepz)
{
    mo->srvo[VZ] = -stepz;
}

/**
 * Turn visual angle towards real angle. An engine cvar controls whether
 * the visangle or the real angle is used in rendering.
 * Real-life analogy: angular momentum (you can't suddenly just take a
 * 90 degree turn in zero time).
 */
void P_MobjAngleSRVOTicker(mobj_t* mo)
{
    short               target, step, diff;
    int                 lstep, hgt;

    // Check requirements.
    if(mo->flags & MF_MISSILE || !(mo->flags & MF_COUNTKILL))
    {
        mo->visAngle = mo->angle >> 16;
        return; // This is not for us.
    }

    target = mo->angle >> 16;
    diff = target - mo->visAngle;

    if(mo->turnTime)
    {
        if(mo->tics)
            step = abs(diff) / mo->tics;
        else
            step = abs(diff);
        if(!step)
            step = 1;
    }
    else
    {
        // Calculate a good step size.
        // Thing height and diff taken into account.
        hgt = (int) mo->height;
        if(hgt < 30)
            hgt = 30;
        if(hgt > 60)
            hgt = 60;

        lstep = abs(diff) * 8 / hgt;
        if(lstep < MIN_STEP)
            lstep = MIN_STEP;
        if(lstep > MAX_STEP)
            lstep = MAX_STEP;
        step = lstep;
    }

    // Do the step.
    if(abs(diff) <= step)
        mo->visAngle = target;
    else if(diff > 0)
        mo->visAngle += step;
    else if(diff < 0)
        mo->visAngle -= step;
}

/**
 * The thing's timer has run out, which means the thing has completed its
 * step. Or there has been a teleport.
 */
void P_MobjClearSRVO(mobj_t* mo)
{
    memset(mo->srvo, 0, sizeof(mo->srvo));
}

boolean P_MobjIsCamera(const mobj_t* mo)
{
    // Client mobjs do not have thinkers and thus cannot be cameras.
    return (mo && mo->thinker.function && mo->player &&
            (mo->player->plr->flags & DDPF_CAMERA));
}

/**
 * The first three bits of the selector special byte contain a relative
 * health level.
 */
void P_UpdateHealthBits(mobj_t* mobj)
{
    int                 i;

    if(mobj->info && mobj->info->spawnHealth > 0)
    {
        mobj->selector &= DDMOBJ_SELECTOR_MASK; // Clear high byte.
        i = (mobj->health << 3) / mobj->info->spawnHealth;
        if(i > 7)
            i = 7;
        if(i < 0)
            i = 0;
        mobj->selector |= i << DDMOBJ_SELECTOR_SHIFT;
    }
}

/**
 * Given a mobjtype, lookup the statenum associated to the named state.
 *
 * @param mobjType      Type of mobj.
 * @param name          State name identifier.
 *
 * @return              Statenum of the associated state ELSE @c, S_NULL.
 */
statenum_t P_GetState(mobjtype_t type, statename_t name)
{
    if(type < 0 || type >= Get(DD_NUMMOBJTYPES))
        return S_NULL;
    if(name < 0 || name >= NUM_STATE_NAMES)
        return S_NULL;

    return MOBJINFO[type].states[name];
}

void P_RipperBlood(mobj_t* actor)
{
    mobj_t*             mo;
    float               pos[3];

    pos[VX] = actor->pos[VX];
    pos[VY] = actor->pos[VY];
    pos[VZ] = actor->pos[VZ];

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VZ] += FIX2FLT((P_Random() - P_Random()) << 12);

    if((mo = P_SpawnMobj3fv(MT_BLOOD, pos, actor->angle, 0)))
    {
        mo->angle = mo->visAngle = 0;
        mo->moveDir = DI_EAST;

#if __JHERETIC__
        mo->flags |= MF_NOGRAVITY;
#endif
        mo->mom[MX] = actor->mom[MX] / 2;
        mo->mom[MY] = actor->mom[MY] / 2;
        mo->tics += P_Random() & 3;
    }
}
