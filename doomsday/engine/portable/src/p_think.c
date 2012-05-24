/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_defs.h"
#include "de_console.h"
#include "de_network.h"

typedef struct thinkerlist_s {
    boolean isPublic; ///< All thinkers in this list are visible publically.
    thinker_t thinkerCap;
} thinkerlist_t;

static thid_t newMobjID(GameMap* map)
{
    assert(map);
    // Increment the ID dealer until a free ID is found.
    // @todo What if all IDs are in use? 65535 thinkers!?
    while(GameMap_IsUsedMobjID(map, ++map->thinkers.iddealer));
    // Mark this ID as used.
    GameMap_SetMobjID(map, map->thinkers.iddealer, true);
    return map->thinkers.iddealer;
}

void GameMap_ClearMobjIDs(GameMap* map)
{
    assert(map);
    memset(map->thinkers.idtable, 0, sizeof(map->thinkers.idtable));
    map->thinkers.idtable[0] |= 1; // ID zero is always "used" (it's not a valid ID).
}

boolean GameMap_IsUsedMobjID(GameMap* map, thid_t id)
{
    assert(map);
    return map->thinkers.idtable[id >> 5] & (1 << (id & 31) /*(id % 32) */ );
}

void GameMap_SetMobjID(GameMap* map, thid_t id, boolean state)
{
    int c = id >> 5, bit = 1 << (id & 31); //(id % 32);
    assert(map);

    if(state)
        map->thinkers.idtable[c] |= bit;
    else
        map->thinkers.idtable[c] &= ~bit;
}

typedef struct mobjidlookup_s {
    thid_t id;
    mobj_t* result;
} mobjidlookup_t;

static int mobjIdLookup(thinker_t* thinker, void* context)
{
    mobjidlookup_t* lookup = (mobjidlookup_t*) context;
    if(thinker->id == lookup->id)
    {
        lookup->result = (mobj_t*) thinker;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

struct mobj_s* GameMap_MobjByID(GameMap* map, int id)
{
    // @todo  A hash table wouldn't hurt (see client's mobj id table).
    mobjidlookup_t lookup;
    lookup.id = id;
    lookup.result = 0;
    GameMap_IterateThinkers(map, gx.MobjThinker, 0x1/*mobjs are public*/, mobjIdLookup, &lookup);
    return lookup.result;
}

/**
 * Locates a mobj by it's unique identifier in the CURRENT map.
 */
struct mobj_s* P_MobjForID(int id)
{
    if(!theMap) return NULL;
    return GameMap_MobjByID(theMap, id);
}

static void linkThinkerToList(thinker_t* th, thinkerlist_t* list)
{
    // Link the thinker to the thinker list.
    list->thinkerCap.prev->next = th;
    th->next = &list->thinkerCap;
    th->prev = list->thinkerCap.prev;
    list->thinkerCap.prev = th;
}

static void unlinkThinkerFromList(thinker_t* th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

static void initThinkerList(thinkerlist_t* list)
{
    list->thinkerCap.prev = list->thinkerCap.next = &list->thinkerCap;
}

static thinkerlist_t* listForThinkFunc(GameMap* map, think_t func, boolean isPublic,
    boolean canCreate)
{
    thinkerlist_t* list;
    size_t i;
    assert(map);

    for(i = 0; i < map->thinkers.numLists; ++i)
    {
        list = map->thinkers.lists[i];

        if(list->thinkerCap.function == func && list->isPublic == isPublic)
            return list;
    }

    if(!canCreate) return NULL;

    // A new thinker type.
    map->thinkers.lists = Z_Realloc(map->thinkers.lists, sizeof(thinkerlist_t*) * ++map->thinkers.numLists, PU_APPSTATIC);
    map->thinkers.lists[map->thinkers.numLists-1] = list = Z_Calloc(sizeof(thinkerlist_t), PU_APPSTATIC, 0);

    initThinkerList(list);
    list->isPublic = isPublic;
    list->thinkerCap.function = func;
    // Set the list sentinel to instasis (safety measure).
    list->thinkerCap.inStasis = true;

    return list;
}

static int runThinker(thinker_t* th, void* context)
{
    // Thinker cannot think when in stasis.
    if(!th->inStasis)
    {
        // Time to remove it?
        if(th->function == (think_t) -1)
        {
            unlinkThinkerFromList(th);

            if(th->id)
            {
                mobj_t* mo = (mobj_t*) th;
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

static int iterateThinkers(thinkerlist_t* list, int (*callback) (thinker_t*, void*),
    void* context)
{
    int result = false;

    if(list)
    {
        thinker_t* th, *next;

        th = list->thinkerCap.next;
        while(th != &list->thinkerCap && th)
        {
#ifdef FAKE_MEMORY_ZONE
            assert(th->next != NULL);
            assert(th->prev != NULL);
#endif

            next = th->next;
            result = callback(th, context);
            if(result) break;
            th = next;
        }
    }

    return result;
}

void GameMap_ThinkerAdd(GameMap* map, thinker_t* th, boolean makePublic)
{
    assert(map);
    if(!th) return;

    if(!th->function)
    {
        Con_Error("GameMap_ThinkerAdd: Invalid thinker function.");
    }

    // Will it need an ID?
    if(P_IsMobjThinker(th->function))
    {
        // It is a mobj, give it an ID (not for client mobjs, though, they
        // already have an id).
        if(!Cl_IsClientMobj((mobj_t*)th))
        {
            th->id = newMobjID(map);
        }
    }
    else
    {
        // Zero is not a valid ID.
        th->id = 0;
    }

    // Link the thinker to the thinker list.
    linkThinkerToList(th, listForThinkFunc(map, th->function, makePublic, true));
}

void GameMap_ThinkerRemove(GameMap* map, thinker_t* th)
{
    assert(map);

    // Has got an ID?
    if(th->id)
    {
        // Then it must be a mobj.
        mobj_t* mo = (mobj_t *) th;

        // Flag the ID as free.
        GameMap_SetMobjID(map, th->id, false);

        // If the state of the mobj is the NULL state, this is a
        // predictable mobj removal (result of animation reaching its
        // end) and shouldn't be included in netGame deltas.
        if(!isClient)
        {
            if(!mo->state || mo->state == states)
            {
                Sv_MobjRemoved(th->id);
            }
        }
    }

    th->function = (think_t) -1;
}

boolean P_IsMobjThinker(think_t func)
{
    return (func && func == gx.MobjThinker);
}

void GameMap_InitThinkerLists(GameMap* map, byte flags)
{
    assert(map);
    if(!map->thinkers.inited)
    {
        map->thinkers.numLists = 0;
        map->thinkers.lists = NULL;
    }
    else
    {
        size_t i;

        for(i = 0; i < map->thinkers.numLists; ++i)
        {
            thinkerlist_t* list = map->thinkers.lists[i];

            if(list->isPublic && !(flags & 0x1)) continue;
            if(!list->isPublic && !(flags & 0x2)) continue;

            initThinkerList(list);
        }
    }

    GameMap_ClearMobjIDs(map);
    map->thinkers.inited = true;
}

boolean GameMap_ThinkerListInited(GameMap* map)
{
    assert(map);
    return map->thinkers.inited;
}

int GameMap_IterateThinkers(GameMap* map, think_t func, byte flags,
    int (*callback) (thinker_t*, void*), void* context)
{
    int result = false;
    size_t i;
    assert(map);

    if(!map->thinkers.inited) return false;

    if(func)
    {
        // We might have both public and shared lists for this func.
        if(flags & 0x1)
            result = iterateThinkers(listForThinkFunc(map, func, true, false),
                                     callback, context);
        if(!result && (flags & 0x2))
            result = iterateThinkers(listForThinkFunc(map, func, false, false),
                                     callback, context);
        return result;
    }

    for(i = 0; i < map->thinkers.numLists; ++i)
    {
        thinkerlist_t* list = map->thinkers.lists[i];

        if(list->isPublic && !(flags & 0x1)) continue;
        if(!list->isPublic && !(flags & 0x2)) continue;

        result = iterateThinkers(list, callback, context);
        if(result) break;
    }
    return result;
}

/// @note Part of the Doomsday public API.
void DD_InitThinkers(void)
{
    if(!theMap) return;
    GameMap_InitThinkerLists(theMap, 0x1); // Init the public thinker lists.
}

/// @note Part of the Doomsday public API.
void DD_RunThinkers(void)
{
    if(!theMap) return;
    GameMap_IterateThinkers(theMap, NULL, 0x1 | 0x2, runThinker, NULL);
}

/// @note Part of the Doomsday public API.
void DD_ThinkerAdd(thinker_t* th)
{
    if(!theMap) return;
    GameMap_ThinkerAdd(theMap, th, true); // This is a public thinker.
}

/// @note Part of the Doomsday public API.
void DD_ThinkerRemove(thinker_t* th)
{
    if(!theMap) return;
    GameMap_ThinkerRemove(theMap, th);
}

/**
 * Change the 'in stasis' state of a thinker (stop it from thinking).
 *
 * @param th            The thinker to change.
 * @param on            @c true, put into stasis.
 */
void DD_ThinkerSetStasis(thinker_t* th, boolean on)
{
    if(th)
    {
        th->inStasis = on;
    }
}

/// @note Part of the Doomsday public API.
int DD_IterateThinkers(think_t func, int (*callback) (thinker_t*, void*), void* context)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_IterateThinkers(theMap, func, 0x1, callback, context);
}
