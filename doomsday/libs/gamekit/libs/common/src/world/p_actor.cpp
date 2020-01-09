/** @file p_actor.cpp Common code relating to mobj management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cstdio>
#include <cstring>

#include "common.h"
#include "gamesession.h"
#include "p_tick.h"
#include "p_actor.h"

#include <doomsday/world/mobj.h>

#if __JDOOM64__
# define RESPAWNTICS            (4 * TICSPERSEC)
#else
# define RESPAWNTICS            (30 * TICSPERSEC)
#endif

typedef struct spawnqueuenode_s {
    int startTime;
    int minTics; ///< Minimum number of tics before respawn.
    void (*callback) (mobj_t *mo, void *context);
    void* context;

    coord_t pos[3];
    angle_t angle;
    mobjtype_t type;
    int spawnFlags; ///< MSF_* flags

    struct spawnqueuenode_s *next;
} spawnqueuenode_t;

static spawnqueuenode_t *spawnQueueHead, *unusedNodes;

void P_SpawnTelefog(mobj_t *mo, void * /*context*/)
{
#if __JDOOM__ || __JDOOM64__
    S_StartSound(SFX_ITMBK, mo);
#else
    S_StartSound(SFX_RESPAWN, mo);
#endif

# if __JDOOM64__
    mo->translucency = 255;
    mo->spawnFadeTics = 0;
    mo->intFlags |= MIF_FADE;
# elif __JDOOM__
    // Spawn the item teleport fog at the new spot.
    P_SpawnMobj(MT_IFOG, mo->origin, mo->angle, 0);
# endif
}

void P_MobjRemove(mobj_t *mo, dd_bool noRespawn)
{
#if !defined(__JDOOM__) && !defined(__JDOOM64__)
    DE_UNUSED(noRespawn);
#endif

    if (mo->ddFlags & DDMF_REMOTE)
        goto justDoIt;

#if __JDOOM__ || __JDOOM64__
    if (!noRespawn)
    {
        if (
# if __JDOOM__
            // Only respawn items in deathmatch 2 and optionally in coop.
            (gfw_Rule(deathmatch) == 2 ||
             (cfg.coopRespawnItems && IS_NETGAME && !gfw_Rule(deathmatch))) &&
# endif /*#elif __JDOOM64__
           (spot->flags & MTF_RESPAWN) &&
# endif*/
            (mo->flags & MF_SPECIAL) && !(mo->flags & MF_DROPPED)
# if __JDOOM__ || __JDOOM64__
            && (mo->type != MT_INV) && (mo->type != MT_INS)
# endif
           )
        {
            P_DeferSpawnMobj3fv(RESPAWNTICS, mobjtype_t(mo->type),
                                mo->spawnSpot.origin, mo->spawnSpot.angle,
                                mo->spawnSpot.flags, P_SpawnTelefog, NULL);
        }
    }
#endif

#if __JHEXEN__
    if ((mo->flags & MF_COUNTKILL) && (mo->flags & MF_CORPSE))
    {
        P_RemoveCorpseInQueue(mo);
    }

    P_MobjRemoveFromTIDList(mo);
#endif

justDoIt:
    Mobj_Destroy(mo);
}

void P_MobjLink(struct mobj_s *mobj)
{
    DE_ASSERT(mobj != 0);
    Mobj_Link(mobj, MLF_BLOCKMAP | (!(mobj->flags & MF_NOSECTOR)? MLF_SECTOR : 0));
}

void P_MobjUnlink(struct mobj_s *mobj)
{
    Mobj_Unlink(mobj);
}

void P_MobjSetSRVO(mobj_t *mo, coord_t stepx, coord_t stepy)
{
    DE_ASSERT(mo != 0);
    mo->srvo[VX] = -stepx;
    mo->srvo[VY] = -stepy;
}

void P_MobjSetSRVOZ(mobj_t *mo, coord_t stepz)
{
    DE_ASSERT(mo != 0);
    mo->srvo[VZ] = -stepz;
}

void P_MobjAngleSRVOTicker(mobj_t *mo)
{
#define MIN_STEP (10 * ANGLE_1) >> 16 ///< Degrees per tic
#define MAX_STEP ANG90 >> 16

    DE_ASSERT(mo != 0);

    // Check requirements.
    if(mo->flags & MF_MISSILE || !(mo->flags & MF_COUNTKILL))
    {
        mo->visAngle = mo->angle >> 16;
        return; // This is not for us.
    }

    short target = mo->angle >> 16;
    short diff = target - mo->visAngle;

    short step = 0;
    if(mo->turnTime)
    {
        if(mo->tics) step = de::abs(diff) / mo->tics;
        else         step = de::abs(diff);

        if(!step) step = 1;
    }
    else
    {
        // Calculate a good step size.
        // Thing height and diff taken into account.
        int hgt = (int) mo->height;
        hgt = MINMAX_OF(30, hgt, 60);

        int lstep = de::abs(diff) * 8 / hgt;
        lstep = MINMAX_OF(MIN_STEP, lstep, MAX_STEP);

        step = lstep;
    }

    // Do the step.
    if(de::abs(diff) <= step)
        mo->visAngle  = target;
    else if(diff > 0)
        mo->visAngle += step;
    else if(diff < 0)
        mo->visAngle -= step;

#undef MAX_STEP
#undef MIN_STEP
}

void P_MobjClearSRVO(mobj_t *mo)
{
    DE_ASSERT(mo != 0);
    std::memset(mo->srvo, 0, sizeof(mo->srvo));
}

dd_bool P_MobjIsCamera(const mobj_t *mo)
{
    // Client mobjs do not have thinkers and thus cannot be cameras.
    return (mo && mo->thinker.function && mo->player &&
            (mo->player->plr->flags & DDPF_CAMERA));
}

dd_bool Mobj_IsCrunchable(mobj_t *mobj)
{
    DE_ASSERT(mobj != 0);

#if __JDOOM__ || __JDOOM64__
    return mobj->health <= 0 && (cfg.gibCrushedNonBleeders || !(mobj->flags & MF_NOBLOOD));
#elif __JHEXEN__
    return mobj->health <= 0 && (mobj->flags & MF_CORPSE) != 0;
#else
    return mobj->health <= 0;
#endif
}

dd_bool Mobj_IsDroppedItem(mobj_t *mobj)
{
    DE_ASSERT(mobj != 0);
#if __JHEXEN__
    return (mobj->flags2 & MF2_DROPPED) != 0;
#else
    return (mobj->flags & MF_DROPPED) != 0;
#endif
}

const terraintype_t *P_MobjFloorTerrain(const mobj_t *mobj)
{
    return P_PlaneMaterialTerrainType(Mobj_Sector(mobj), PLN_FLOOR);
}

void P_UpdateHealthBits(mobj_t *mo)
{
    if(!mo || !mo->info) return;

    if(mo->info->spawnHealth > 0)
    {
        mo->selector &= DDMOBJ_SELECTOR_MASK; // Clear high byte.

        int sel = (mo->health << 3) / mo->info->spawnHealth;
        sel = MINMAX_OF(0, sel, 7);

        mo->selector |= sel << DDMOBJ_SELECTOR_SHIFT;
    }
}

statenum_t P_GetState(mobjtype_t type, statename_t name)
{
    if(type < MT_FIRST || type >= Get(DD_NUMMOBJTYPES)) return S_NULL;
    if(name < 0 || name >= STATENAMES_COUNT) return S_NULL;

    return statenum_t(MOBJINFO[type].states[name]);
}

void P_RipperBlood(mobj_t *actor)
{
    DE_ASSERT(actor != 0);

    coord_t pos[3];
    pos[VX] = actor->origin[VX];
    pos[VY] = actor->origin[VY];
    pos[VZ] = actor->origin[VZ];

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VZ] += FIX2FLT((P_Random() - P_Random()) << 12);

    if(mobj_t *mo = P_SpawnMobj(MT_BLOOD, pos, actor->angle, 0))
    {
#if __JHERETIC__
        mo->flags |= MF_NOGRAVITY;
#endif
        mo->mom[MX] = actor->mom[MX] / 2;
        mo->mom[MY] = actor->mom[MY] / 2;
        mo->tics += P_Random() & 3;
    }
}

static spawnqueuenode_t *allocateNode()
{
#define SPAWNQUEUENODE_BATCHSIZE 32

    spawnqueuenode_t *node;

    if(unusedNodes)
    {
        // There are existing nodes we can re-use.
        node = unusedNodes;
        unusedNodes = unusedNodes->next;
        node->next = 0;
    }
    else
    {
        // We need to allocate more.
        spawnqueuenode_t *storage = (spawnqueuenode_t *)
            Z_Malloc(sizeof(*node) * SPAWNQUEUENODE_BATCHSIZE, PU_GAMESTATIC, 0);

        // Add all but one to the unused node list.
        for(int i = 0; i < SPAWNQUEUENODE_BATCHSIZE-1; ++i)
        {
            node = storage++;
            node->next = unusedNodes;
            unusedNodes = node;
        }

        node = storage;
    }

    return node;

#undef SPAWNQUEUENODE_BATCHSIZE
}

static void freeNode(spawnqueuenode_t *node, bool recycle)
{
    if(!node) return;

    // Find this node in the spawn queue and unlink it if found.
    if(spawnQueueHead)
    {
        if(spawnQueueHead == node)
        {
            spawnQueueHead = spawnQueueHead->next;
        }
        else
        {
            for(spawnqueuenode_t *other = spawnQueueHead; other->next; other = other->next)
            {
                if(other->next == node)
                    other->next = other->next->next;
            }
        }
    }

    if(recycle)
    {
        // Recycle this node for later use.
        node->next = unusedNodes;
        unusedNodes = node;
        return;
    }

    Z_Free(node);
}

static spawnqueuenode_t *dequeueSpawn()
{
    spawnqueuenode_t *node = spawnQueueHead;
    if(spawnQueueHead)
        spawnQueueHead = spawnQueueHead->next;
    return node;
}

static void emptySpawnQueue(bool recycle)
{
    if(spawnQueueHead)
    {
        while(spawnqueuenode_t *node = dequeueSpawn())
        {
            freeNode(node, recycle);
        }
    }
    spawnQueueHead = 0;
}

static void enqueueSpawn(int minTics, mobjtype_t type, coord_t x, coord_t y,
    coord_t z, angle_t angle, int spawnFlags,
    void (*callback) (mobj_t *mo, void *context), void *context)
{
    spawnqueuenode_t *spawn = allocateNode();

    spawn->type = type;
    spawn->pos[VX] = x;
    spawn->pos[VY] = y;
    spawn->pos[VZ] = z;
    spawn->angle = angle;
    spawn->spawnFlags = spawnFlags;

    spawn->startTime = mapTime;
    spawn->minTics = minTics;

    spawn->callback = callback;
    spawn->context = context;

    if(spawnQueueHead)
    {
        // Find the correct insertion point.
        if(spawnQueueHead->next)
        {
            spawnqueuenode_t *other = spawnQueueHead;

            while(other->next && other->next->minTics - (mapTime - other->next->startTime) <= minTics)
            {
                other = other->next;
            }

            spawn->next = (other->next? other->next : 0);
            other->next = spawn;
        }
        else
        {
            // After or before the head?
            if(spawnQueueHead->minTics - (mapTime - spawnQueueHead->startTime) <= minTics)
            {
                spawn->next = 0;
                spawnQueueHead->next = spawn;
            }
            else
            {
                spawn->next = spawnQueueHead;
                spawnQueueHead = spawn;
            }
        }
    }
    else
    {
        spawn->next = 0;
        spawnQueueHead = spawn;
    }
}

void P_DeferSpawnMobj3f(int minTics, mobjtype_t type, coord_t x, coord_t y, coord_t z,
    angle_t angle, int spawnFlags,  void (*callback) (mobj_t *mo, void *context),
    void* context)
{
    if(minTics > 0)
    {
        enqueueSpawn(minTics, type, x, y, z, angle, spawnFlags, callback,
                     context);
    }
    // Spawn immediately.
    else if(mobj_t *mo = P_SpawnMobjXYZ(type, x, y, z, angle, spawnFlags))
    {
        if(callback)
            callback(mo, context);
    }
}

void P_DeferSpawnMobj3fv(int minTics, mobjtype_t type, coord_t const pos[3], angle_t angle,
    int spawnFlags, void (*callback) (mobj_t* mo, void* context), void* context)
{
    if(minTics > 0)
    {
        enqueueSpawn(minTics, type, pos[VX], pos[VY], pos[VZ], angle,
                     spawnFlags, callback, context);
    }
    // Spawn immediately.
    else if(mobj_t *mo = P_SpawnMobj(type, pos, angle, spawnFlags))
    {
        if(callback)
            callback(mo, context);
    }
}

static mobj_t *processOneSpawnTask()
{
    mobj_t *mo = 0;

    // Anything due to spawn?
    if(spawnQueueHead && mapTime - spawnQueueHead->startTime >= spawnQueueHead->minTics)
    {
        spawnqueuenode_t *spawn = dequeueSpawn();
        DE_ASSERT(spawn != 0);

        // Spawn it.
        if((mo = P_SpawnMobj(spawn->type, spawn->pos, spawn->angle, spawn->spawnFlags)))
        {
            if(spawn->callback)
                spawn->callback(mo, spawn->context);
        }

        freeNode(spawn, true);
    }

    return mo;
}

/// @note Called 35 times per second by P_DoTick.
void P_ProcessDeferredSpawns()
{
    while(processOneSpawnTask())
    {}
}

void P_PurgeDeferredSpawns()
{
    emptySpawnQueue(true);
}

#ifdef __JHEXEN__

/// @todo Remove fixed limit.
#define MAX_TID_COUNT           200

static int TIDList[MAX_TID_COUNT + 1];  // +1 for termination marker
static mobj_t *TIDMobj[MAX_TID_COUNT];

static int insertThinkerInIdListWorker(thinker_t *th, void *context)
{
    mobj_t *mo = (mobj_t *)th;
    int *count = (int *) context;

    if(mo->tid != 0)
    {
        // Add to list.
        if(*count == MAX_TID_COUNT)
        {
            Con_Error("P_CreateTIDList: MAX_TID_COUNT (%d) exceeded.", MAX_TID_COUNT);
        }

        TIDList[*count] = mo->tid;
        TIDMobj[(*count)++] = mo;
    }

    return false; // Continue iteration.
}

void P_CreateTIDList()
{
    int count = 0;
    Thinker_Iterate(P_MobjThinker, insertThinkerInIdListWorker, &count);

    // Add termination marker
    TIDList[count] = 0;
}

void P_MobjInsertIntoTIDList(mobj_t *mo, int tid)
{
    DE_ASSERT(mo != 0);

    int index = -1;
    int i = 0;
    for(; TIDList[i] != 0; ++i)
    {
        if(TIDList[i] == -1)
        {
            // Found empty slot
            index = i;
            break;
        }
    }

    if(index == -1)
    {
        // Append required
        if(i == MAX_TID_COUNT)
        {
            Con_Error("P_MobjInsertIntoTIDList: MAX_TID_COUNT (%d) exceeded.", MAX_TID_COUNT);
        }
        index = i;
        TIDList[index + 1] = 0;
    }

    mo->tid = tid;
    TIDList[index] = tid;
    TIDMobj[index] = mo;
}

void P_MobjRemoveFromTIDList(mobj_t *mo)
{
    if(!mo || !mo->tid)
        return;

    for(int i = 0; TIDList[i] != 0; ++i)
    {
        if(TIDMobj[i] == mo)
        {
            TIDList[i] = -1;
            TIDMobj[i] = 0;
            mo->tid = 0;
            return;
        }
    }

    mo->tid = 0;
}

mobj_t *P_FindMobjFromTID(int tid, int *searchPosition)
{
    DE_ASSERT(searchPosition != 0);

    for(int i = *searchPosition + 1; TIDList[i] != 0; ++i)
    {
        if(TIDList[i] == tid)
        {
            *searchPosition = i;
            return TIDMobj[i];
        }
    }

    *searchPosition = -1;
    return 0;
}

#endif // __JHEXEN__
