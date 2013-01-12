/**\file p_mobj.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Map Objects
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
#include "de_misc.h"
#include "de_audio.h"

#include "def_main.h"
#include "render/r_main.h" // validCount, viewport
#include "render/r_things.h" // useSRVO

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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
mobj_t* P_MobjCreate(thinkfunc_t function, coord_t const pos[3], angle_t angle,
    coord_t radius, coord_t height, int ddflags)
{
    mobj_t* mo;

    if(!function)
        Con_Error("P_MobjCreateXYZ: Think function invalid, cannot create mobj.");

#ifdef _DEBUG
    if(isClient)
    {
        VERBOSE2( Con_Message("P_MobjCreate: Client creating mobj at [x:%f, y:%f, z:%f]\n", pos[VX], pos[VY], pos[VZ]) );
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
    {
        // No, we need to allocate another.
        mo = (mobj_t *) Z_Calloc(MOBJ_SIZE, PU_MAP, NULL);
    }

    V3d_Copy(mo->origin, pos);
    mo->angle = angle;
    mo->visAngle = mo->angle >> 16; // "angle-servo"; smooth actor turning.
    mo->radius = radius;
    mo->height = height;
    mo->ddFlags = ddflags;
    mo->thinker.function = function;
    if(mo->thinker.function)
    {
        GameMap_ThinkerAdd(theMap, &mo->thinker, true); // Make it public.
    }

    return mo;
}

#undef P_MobjCreateXYZ
DENG_EXTERN_C mobj_t* P_MobjCreateXYZ(thinkfunc_t function, coord_t x, coord_t y, coord_t z,
    angle_t angle, coord_t radius, coord_t height, int ddflags)
{
    coord_t pos[3];
    V3d_Set(pos, x, y, z);
    return P_MobjCreate(function, pos, angle, radius, height, ddflags);
}

/**
 * All mobjs must be destroyed through this routine. Part of the public API.
 *
 * @note Does not actually destroy the mobj. Instead, mobj is marked as
 * awaiting removal (which occurs when its turn for thinking comes around).
 */
#undef P_MobjDestroy
DENG_EXTERN_C void P_MobjDestroy(mobj_t* mo)
{
#ifdef _DEBUG
    if(mo->ddFlags & DDMF_MISSILE)
    {
        VERBOSE2( Con_Message("P_MobjDestroy: Destroying missile %i.\n", mo->thinker.id) );
    }
#endif

    // Unlink from sector and block lists.
    P_MobjUnlink(mo);

    S_StopSound(0, mo);

    GameMap_ThinkerRemove(theMap, (thinker_t *) mo);
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
#undef P_MobjSetState
DENG_EXTERN_C void P_MobjSetState(mobj_t* mobj, int statenum)
{
    state_t*            st = states + statenum;
    boolean             spawning = (mobj->state == 0);
    ded_ptcgen_t*       pg;

#if _DEBUG
    if(statenum < 0 || statenum >= defs.count.states.num)
        Con_Error("P_MobjSetState: statenum %i out of bounds.\n", statenum);
    /*
    if(mobj->ddFlags & DDMF_MISSILE)
    {
        Con_Message("P_MobjSetState: Missile %i going to state %i.\n", mobj->thinker.id, statenum);
    }
    */
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
            P_SpawnMobjParticleGen(pg, mobj);
        }
    }

    if(!(mobj->ddFlags & DDMF_REMOTE))
    {
        if(defs.states[statenum].execute)
            Con_Execute(CMDS_SCRIPT, defs.states[statenum].execute, true, false);
    }
}

boolean Mobj_SetOrigin(struct mobj_s* mo, coord_t x, coord_t y, coord_t z)
{
    if(!gx.MobjTryMoveXYZ)
    {
        return false;
    }
    return gx.MobjTryMoveXYZ(mo, x, y, z);
}

#undef Mobj_OriginSmoothed
DENG_EXTERN_C void Mobj_OriginSmoothed(mobj_t* mo, coord_t origin[3])
{
    if(!origin) return;

    V3d_Set(origin, 0, 0, 0);
    if(!mo) return;

    V3d_Copy(origin, mo->origin);

    // Apply a Short Range Visual Offset?
    if(useSRVO && mo->state && mo->tics >= 0)
    {
        const double mul = mo->tics / (float) mo->state->tics;
        vec3d_t srvo;

        V3d_Copy(srvo, mo->srvo);
        V3d_Scale(srvo, mul);
        V3d_Sum(origin, origin, srvo);
    }

    if(mo->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed origins for all local players.
        if(P_GetDDPlayerIdx(mo->dPlayer) == consolePlayer &&
           // $voodoodolls: Must be a real player to use the smoothed origin.
           mo->dPlayer->mo == mo)
        {
            const viewdata_t* vd = R_ViewData(consolePlayer);
            V3d_Copy(origin, vd->current.origin);
        }
        // The client may have a Smoother for this object.
        else if(isClient)
        {
            Smoother_Evaluate(clients[P_GetDDPlayerIdx(mo->dPlayer)].smoother, origin);
        }
    }
}

#undef Mobj_AngleSmoothed
DENG_EXTERN_C angle_t Mobj_AngleSmoothed(mobj_t* mo)
{
    if(!mo) return 0;

    if(mo->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed angles for all local players.
        if(P_GetDDPlayerIdx(mo->dPlayer) == consolePlayer &&
           // $voodoodolls: Must be a real player to use the smoothed angle.
           mo->dPlayer->mo == mo)
        {
            const viewdata_t* vd = R_ViewData(consolePlayer);
            return vd->current.angle;
        }
    }

#ifdef __CLIENT__
    // Apply a Short Range Visual Offset?
    if(useSRVOAngle && !netGame && !playback)
    {
        return mo->visAngle << 16;
    }
#endif

    return mo->angle;
}

coord_t Mobj_ApproxPointDistance(mobj_t* mo, coord_t const* point)
{
    if(!mo || !point) return 0;
    return M_ApproxDistance(point[VZ] - mo->origin[VZ],
                            M_ApproxDistance(point[VX] - mo->origin[VX],
                                             point[VY] - mo->origin[VY]));
}

D_CMD(InspectMobj)
{
    mobj_t* mo = 0;
    thid_t id = 0;
    char const *moType = "Mobj";
#ifdef __CLIENT__
    clmoinfo_t* info = 0;
#endif

    if(argc != 2)
    {
        Con_Printf("Usage: %s (mobj-id)\n", argv[0]);
        return true;
    }

    // Get the ID.
    id = strtol(argv[1], NULL, 10);

    // Find the mobj.
    mo = GameMap_MobjByID(theMap, id);
    if(!mo)
    {
        Con_Printf("Mobj with id %i not found.\n", id);
        return false;
    }

#ifdef __CLIENT__
    info = ClMobj_GetInfo(mo);
    if(info) moType = "CLMOBJ";
#endif

    Con_Printf("%s %i [%p] State:%s (%i)\n", moType, id, mo, Def_GetStateName(mo->state), (int)(mo->state - states));
    Con_Printf("Type:%s (%i) Info:[%p]", Def_GetMobjName(mo->type), mo->type, mo->info);
    if(mo->info)
    {
        Con_Printf(" (%i)\n", (int)(mo->info - mobjInfo));
    }
    else
    {
        Con_Printf("\n");
    }
    Con_Printf("Tics:%i ddFlags:%08x\n", mo->tics, mo->ddFlags);
#ifdef __CLIENT__
    if(info)
    {
        Con_Printf("Cltime:%i (now:%i) Flags:%04x\n", info->time, Timer_RealMilliseconds(), info->flags);
    }
#endif
    Con_Printf("Flags:%08x Flags2:%08x Flags3:%08x\n", mo->flags, mo->flags2, mo->flags3);
    Con_Printf("Height:%f Radius:%f\n", mo->height, mo->radius);
    Con_Printf("Angle:%x Pos:(%f,%f,%f) Mom:(%f,%f,%f)\n",
               mo->angle,
               mo->origin[0], mo->origin[1], mo->origin[2],
               mo->mom[0], mo->mom[1], mo->mom[2]);
    Con_Printf("FloorZ:%f CeilingZ:%f\n", mo->floorZ, mo->ceilingZ);
    if(mo->bspLeaf)
    {
        Con_Printf("Sector:%i (FloorZ:%f CeilingZ:%f)\n", P_ToIndex(mo->bspLeaf->sector),
                   mo->bspLeaf->sector->SP_floorheight,
                   mo->bspLeaf->sector->SP_ceilheight);
    }
    if(mo->onMobj)
    {
        Con_Printf("onMobj:%i\n", mo->onMobj->thinker.id);
    }

    return true;
}
