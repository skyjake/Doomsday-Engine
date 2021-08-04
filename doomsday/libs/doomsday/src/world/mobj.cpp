/** @file mobj.cpp  Base for world map objects.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/world/mobj.h"
#include "doomsday/world/mobjthinker.h"
#include "doomsday/world/mobjthinkerdata.h"
#include "doomsday/world/bspleaf.h"
#include "doomsday/world/map.h"
#include "doomsday/world/thinkers.h"
#include "doomsday/world/world.h"
#include "doomsday/defs/ded.h"
#include "doomsday/console/exec.h"
#include "doomsday/audio/audio.h"
#include "doomsday/doomsdayapp.h"

#include <de/legacy/vector1.h>
#include <de/logbuffer.h>

using namespace de;

size_t Mobj_Sizeof(void)
{
    return DoomsdayApp::app().plugins().gameExports().GetInteger(DD_MOBJ_SIZE);
}

/**
 * All mobjs must be allocated through this routine. Part of the public API.
 */
mobj_t *P_MobjCreate(thinkfunc_t function, const Vec3d &origin, angle_t angle,
                     coord_t radius, coord_t height, dint ddflags)
{
    DE_ASSERT(function);

    // Do we have any unused mobjs we can reuse?
    mobj_t *mob = world::World::get().takeUnusedMobj();
    if (!mob)
    {
        // No, we need to allocate another.
        mob = MobjThinker(Thinker::AllocateMemoryZone).take();
    }

    V3d_Set(mob->origin, origin.x, origin.y, origin.z);
    mob->angle    = angle;
    mob->visAngle = mob->angle >> 16; // "angle-servo"; smooth actor turning.
    mob->radius   = radius;
    mob->height   = height;
    mob->ddFlags  = ddflags;
    mob->lumIdx   = -1;
    mob->mapSpotNum = -1; // unknown
    mob->thinker.function = function;
    Mobj_Map(*mob).thinkers().add(mob->thinker);

    return mob;
}

/**
 * All mobjs must be destroyed through this routine. Part of the public API.
 *
 * @note Does not actually destroy the mobj. Instead, mobj is marked as
 * awaiting removal (which occurs when its turn for thinking comes around).
 */
void Mobj_Destroy(mobj_t *mo)
{
#ifdef _DEBUG
    if (mo->ddFlags & DDMF_MISSILE)
    {
        LOG_AS("Mobj_Destroy");
        LOG_MAP_XVERBOSE("Destroying missile %i", mo->thinker.id);
    }
#endif

    // Unlink from sector and block lists.
    Mobj_Unlink(mo);

    audio::Audio::get().stopSound(0, mo);

    Mobj_Map(*mo).thinkers().remove(reinterpret_cast<thinker_t &>(*mo));
}

void Mobj_SetState(mobj_t *mob, int statenum)
{
    if (!mob) return;

    const state_t *oldState = mob->state;

    DE_ASSERT(statenum >= 0 && statenum < DED_Definitions()->states.size());

    mob->state  = &runtimeDefs.states[statenum];
    mob->tics   = mob->state->tics;
    mob->sprite = mob->state->sprite;
    mob->frame  = mob->state->frame;

    if (!(mob->ddFlags & DDMF_REMOTE))
    {
        const String exec = DED_Definitions()->states[statenum].gets("execute");
        if (!exec.isEmpty())
        {
            Con_Execute(CMDS_SCRIPT, exec, true, false);
        }
    }

    // Notify private data about the changed state.
    if (!mob->thinker.d)
    {
        Thinker_InitPrivateData(&mob->thinker);
    }
    if (MobjThinkerData *data = THINKER_DATA_MAYBE(mob->thinker, MobjThinkerData))
    {
        data->stateChanged(oldState);
    }
}

void P_MobjRecycle(mobj_t* mo)
{
    // Release the private data.
    MobjThinker::zap(*mo);

    // The sector next link is used as the unused mobj list links.
    world::World::get().putUnusedMobj(mo);
}

Vec3d Mobj_Origin(const mobj_t &mob)
{
    return Vec3d(mob.origin);
}

Vec3d Mobj_Center(mobj_t &mob)
{
    return Vec3d(mob.origin[0], mob.origin[1], mob.origin[2] + mob.height / 2);
}

coord_t Mobj_Radius(const mobj_t &mobj)
{
    return mobj.radius;
}

AABoxd Mobj_Bounds(const mobj_t &mobj)
{
    const Vec2d origin = Mobj_Origin(mobj);
    const ddouble radius  = Mobj_Radius(mobj);
    return AABoxd(origin.x - radius, origin.y - radius,
                  origin.x + radius, origin.y + radius);
}

bool Mobj_IsLinked(const mobj_t &mob)
{
    return mob._bspLeaf != 0;
}

bool Mobj_IsSectorLinked(const mobj_t &mob)
{
    return (mob._bspLeaf != nullptr && mob.sPrev != nullptr);
}

world_Sector *Mobj_Sector(const mobj_t *mob)
{
    if (!mob || !Mobj_IsLinked(*mob)) return nullptr;
    return Mobj_BspLeafAtOrigin(*mob).sectorPtr();
}

world::Map &Mobj_Map(const mobj_t &mob)
{
    return Thinker_Map(mob.thinker);
}

world::BspLeaf &Mobj_BspLeafAtOrigin(const mobj_t &mob)
{
    if (Mobj_IsLinked(mob))
    {
        return *reinterpret_cast<world::BspLeaf *>(mob._bspLeaf);
    }
    throw Error("Mobj_BspLeafAtOrigin", "Mobj is not yet linked");
}
