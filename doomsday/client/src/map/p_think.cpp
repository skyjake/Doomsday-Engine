/** @file map/p_think.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DENG_NO_API_MACROS_THINKER

#include "de_base.h"
#include "de_defs.h"
#include "de_console.h"
#include "de_network.h"

#include "map/gamemap.h"

typedef struct thinkerlist_s {
    boolean isPublic; ///< All thinkers in this list are visible publically.
    thinker_t thinkerCap;
} thinkerlist_t;

boolean Thinker_IsMobjFunc(thinkfunc_t func)
{
    return (func && func == reinterpret_cast<thinkfunc_t>(gx.MobjThinker));
}

namespace de {

static thid_t newMobjID(GameMap &map)
{
    // Increment the ID dealer until a free ID is found.
    /// @todo fixme: What if all IDs are in use? 65535 thinkers!?
    while(map.isUsedMobjId(++map.thinkers.iddealer)) {}

    // Mark this ID as used.
    map.setMobjId(map.thinkers.iddealer, true);

    return map.thinkers.iddealer;
}

void GameMap::clearMobjIds()
{
    std::memset(thinkers.idtable, 0, sizeof(thinkers.idtable));
    thinkers.idtable[0] |= 1; // ID zero is always "used" (it's not a valid ID).
}

boolean GameMap::isUsedMobjId(thid_t id)
{
    return thinkers.idtable[id >> 5] & (1 << (id & 31) /*(id % 32) */ );
}

void GameMap::setMobjId(thid_t id, boolean inUse)
{
    int c = id >> 5, bit = 1 << (id & 31); //(id % 32);

    if(inUse) thinkers.idtable[c] |= bit;
    else      thinkers.idtable[c] &= ~bit;
}

typedef struct mobjidlookup_s {
    thid_t id;
    mobj_t *result;
} mobjidlookup_t;

static int mobjIdLookup(thinker_t *thinker, void *context)
{
    mobjidlookup_t *lookup = (mobjidlookup_t *) context;
    if(thinker->id == lookup->id)
    {
        lookup->result = (mobj_t *) thinker;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

struct mobj_s *GameMap::mobjById(int id)
{
    /// @todo  A hash table wouldn't hurt (see client's mobj id table).
    mobjidlookup_t lookup;
    lookup.id = id;
    lookup.result = 0;
    iterateThinkers(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                    0x1/*mobjs are public*/, mobjIdLookup, &lookup);
    return lookup.result;
}

static void linkThinkerToList(thinker_t *th, thinkerlist_t *list)
{
    // Link the thinker to the thinker list.
    list->thinkerCap.prev->next = th;
    th->next = &list->thinkerCap;
    th->prev = list->thinkerCap.prev;
    list->thinkerCap.prev = th;
}

static void unlinkThinkerFromList(thinker_t *th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

static void initThinkerList(thinkerlist_t *list)
{
    list->thinkerCap.prev = list->thinkerCap.next = &list->thinkerCap;
}

static thinkerlist_t *listForThinkFunc(GameMap &map, thinkfunc_t func, boolean isPublic,
    boolean canCreate)
{
    thinkerlist_t* list;
    size_t i;

    for(i = 0; i < map.thinkers.numLists; ++i)
    {
        list = map.thinkers.lists[i];

        if(list->thinkerCap.function == func && list->isPublic == isPublic)
            return list;
    }

    if(!canCreate) return NULL;

    // A new thinker type.
    map.thinkers.lists = (thinkerlist_t **) Z_Realloc(map.thinkers.lists, sizeof(thinkerlist_t*) * ++map.thinkers.numLists, PU_APPSTATIC);
    map.thinkers.lists[map.thinkers.numLists-1] = list = (thinkerlist_t *) Z_Calloc(sizeof(thinkerlist_t), PU_APPSTATIC, 0);

    initThinkerList(list);
    list->isPublic = isPublic;
    list->thinkerCap.function = func;
    // Set the list sentinel to instasis (safety measure).
    list->thinkerCap.inStasis = true;

    return list;
}

static int runThinker(thinker_t *th, void *context)
{
    DENG_UNUSED(context);

    // Thinker cannot think when in stasis.
    if(!th->inStasis)
    {
        // Time to remove it?
        if(th->function == (thinkfunc_t) -1)
        {
            unlinkThinkerFromList(th);

            if(th->id)
            {
                mobj_t *mo = (mobj_t *) th;
#ifdef __CLIENT__
                if(!Cl_IsClientMobj(mo))
                {
                    // It's a regular mobj: recycle for reduced allocation overhead.
                    P_MobjRecycle(mo);
                }
                else
                {
                    // Delete the client mobj.
                    ClMobj_Destroy(mo);
                }
#else
                P_MobjRecycle(mo);
#endif
            }
            else
            {
                // Non-mobjs are just deleted right away.
                Z_Free(th);
            }
        }
        else if(th->function)
        {
            th->function(th);
        }
    }

    return false; // Continue iteration.
}

static int iterateThinkerList(thinkerlist_t *list, int (*callback) (thinker_t *, void *),
    void *context)
{
    int result = false;

    if(list)
    {
        thinker_t *th, *next;

        th = list->thinkerCap.next;
        while(th != &list->thinkerCap && th)
        {
#ifdef LIBDENG_FAKE_MEMORY_ZONE
            DENG_ASSERT(th->next != 0);
            DENG_ASSERT(th->prev != 0);
#endif

            next = th->next;
            result = callback(th, context);
            if(result) break;
            th = next;
        }
    }

    return result;
}

void GameMap::thinkerAdd(thinker_t &th, boolean makePublic)
{
    if(!th.function)
        throw Error("GameMap::thinkerAdd", "Invalid thinker function");

    // Will it need an ID?
    if(Thinker_IsMobjFunc(th.function))
    {
        // It is a mobj, give it an ID (not for client mobjs, though, they
        // already have an id).
#ifdef __CLIENT__
        if(!Cl_IsClientMobj(reinterpret_cast<mobj_t *>(&th)))
#endif
        {
            th.id = newMobjID(*this);
        }
    }
    else
    {
        // Zero is not a valid ID.
        th.id = 0;
    }

    // Link the thinker to the thinker list.
    linkThinkerToList(&th, listForThinkFunc(*this, th.function, makePublic, true));
}

void GameMap::thinkerRemove(thinker_t &th)
{
    // Has got an ID?
    if(th.id)
    {
        // Flag the identifier as free.
        setMobjId(th.id, false);

#ifdef __SERVER__
        // Then it must be a mobj.
        mobj_t *mo = reinterpret_cast<mobj_t *>(&th);

        // If the state of the mobj is the NULL state, this is a
        // predictable mobj removal (result of animation reaching its
        // end) and shouldn't be included in netGame deltas.
        if(!mo->state || mo->state == states)
        {
            Sv_MobjRemoved(th.id);
        }
#endif
    }

    th.function = (thinkfunc_t) -1;
}

void GameMap::initThinkerLists(byte flags)
{
    if(!thinkers.inited)
    {
        thinkers.numLists = 0;
        thinkers.lists = 0;
    }
    else
    {
        for(size_t i = 0; i < thinkers.numLists; ++i)
        {
            thinkerlist_t *list = thinkers.lists[i];

            if(list->isPublic && !(flags & 0x1)) continue;
            if(!list->isPublic && !(flags & 0x2)) continue;

            initThinkerList(list);
        }
    }

    clearMobjIds();
    thinkers.inited = true;
}

boolean GameMap::thinkerListInited() const
{
    return thinkers.inited;
}

int GameMap::iterateThinkers(thinkfunc_t func, byte flags,
    int (*callback) (thinker_t *, void *), void *context)
{
    if(!thinkers.inited) return false;

    int result = false;
    if(func)
    {
        // We might have both public and shared lists for this func.
        if(flags & 0x1)
            result = iterateThinkerList(listForThinkFunc(*this, func, true, false),
                                        callback, context);
        if(!result && (flags & 0x2))
            result = iterateThinkerList(listForThinkFunc(*this, func, false, false),
                                        callback, context);
        return result;
    }

    for(size_t i = 0; i < thinkers.numLists; ++i)
    {
        thinkerlist_t *list = thinkers.lists[i];

        if(list->isPublic && !(flags & 0x1)) continue;
        if(!list->isPublic && !(flags & 0x2)) continue;

        result = iterateThinkerList(list, callback, context);
        if(result) break;
    }
    return result;
}

} // namespace de

using namespace de;

/**
 * Locates a mobj by it's unique identifier in the CURRENT map.
 */
#undef P_MobjForID
DENG_EXTERN_C struct mobj_s *P_MobjForID(int id)
{
    if(!theMap) return 0;
    return theMap->mobjById(id);
}

#undef Thinker_Init
void Thinker_Init()
{
    if(!theMap) return;
    theMap->initThinkerLists(0x1); // Init the public thinker lists.
}

#undef Thinker_Run
void Thinker_Run()
{
    if(!theMap) return;
    theMap->iterateThinkers(NULL, 0x1 | 0x2, runThinker, NULL);
}

#undef Thinker_Add
void Thinker_Add(thinker_t *th)
{
    if(!th || !theMap) return;
    theMap->thinkerAdd(*th, true); // This is a public thinker.
}

#undef Thinker_Remove
void Thinker_Remove(thinker_t *th)
{
    if(!th || !theMap) return;
    theMap->thinkerRemove(*th);
}

#undef Thinker_SetStasis
void Thinker_SetStasis(thinker_t *th, boolean on)
{
    if(th)
    {
        th->inStasis = on;
    }
}

#undef Thinker_Iterate
int Thinker_Iterate(thinkfunc_t func, int (*callback) (thinker_t *, void *), void *context)
{
    if(!theMap) return false; // Continue iteration.
    return theMap->iterateThinkers(func, 0x1, callback, context);
}

DENG_DECLARE_API(Thinker) =
{
    { DE_API_THINKER },
    Thinker_Init,
    Thinker_Run,
    Thinker_Add,
    Thinker_Remove,
    Thinker_SetStasis,
    Thinker_Iterate
};
