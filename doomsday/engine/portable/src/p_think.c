/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * p_think.c: Thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_defs.h"
#include "de_console.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         isPublic; /* @c true = all thinkers in this list are
                                 visible publically */
    thinker_t       thinkerCap;
} thinkerlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int idtable[2048]; // 65536 bits telling which IDs are in use.
unsigned short iddealer = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static size_t numThinkerLists;
static thinkerlist_t** thinkerLists;
static boolean inited = false;

// CODE --------------------------------------------------------------------

static thid_t newMobjID(void)
{
    // Increment the ID dealer until a free ID is found.
    // \fixme What if all IDs are in use? 65535 thinkers!?
    while(P_IsUsedMobjID(++iddealer));
    // Mark this ID as used.
    P_SetMobjID(iddealer, true);
    return iddealer;
}

void P_ClearMobjIDs(void)
{
    memset(idtable, 0, sizeof(idtable));
    idtable[0] |= 1; // ID zero is always "used" (it's not a valid ID).
}

boolean P_IsUsedMobjID(thid_t id)
{
    return idtable[id >> 5] & (1 << (id & 31) /*(id % 32) */ );
}

void P_SetMobjID(thid_t id, boolean state)
{
    int                 c = id >> 5, bit = 1 << (id & 31); //(id % 32);

    if(state)
        idtable[c] |= bit;
    else
        idtable[c] &= ~bit;
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

/**
 * Locates a mobj based on the identifier.
 * @todo  A hash table wouldn't hurt (see client's mobj id table).
 */
struct mobj_s* P_MobjForID(int id)
{
    mobjidlookup_t lookup;
    lookup.id = id;
    lookup.result = 0;
    DD_IterateThinkers(gx.MobjThinker, mobjIdLookup, &lookup);
    return lookup.result;
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

static thinkerlist_t* listForThinkFunc(think_t func, boolean isPublic,
                                       boolean canCreate)
{
    size_t              i;

    for(i = 0; i < numThinkerLists; ++i)
    {
        thinkerlist_t*      list = thinkerLists[i];

        if(list->thinkerCap.function == func && list->isPublic == isPublic)
            return list;
    }

    if(!canCreate)
        return NULL;

    // A new thinker type.
    {
    thinkerlist_t*      list;

    thinkerLists = Z_Realloc(thinkerLists, sizeof(thinkerlist_t*) *
                             ++numThinkerLists, PU_STATIC);
    thinkerLists[numThinkerLists-1] = list =
        Z_Calloc(sizeof(thinkerlist_t), PU_STATIC, 0);

    initThinkerList(list);
    list->isPublic = isPublic;
    list->thinkerCap.function = func;
    // Set the list sentinel to instasis (safety measure).
    list->thinkerCap.inStasis = true;

    return list;
    }
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

static int iterateThinkers(thinkerlist_t* list,
                               int (*callback) (thinker_t*, void*),
                               void* context)
{
    int              result = false;

    if(list)
    {
        thinker_t*          th, *next;

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

/**
 * @param th            Thinker to be added.
 * @param makePublic    If @c true, this thinker will be visible publically
 *                      via the Doomsday public API thinker interface(s).
 */
void P_ThinkerAdd(thinker_t* th, boolean makePublic)
{
    if(!th)
        return;

    if(!th->function)
    {
        Con_Error("P_ThinkerAdd: Invalid thinker function.");
    }

    // Will it need an ID?
    if(P_IsMobjThinker(th->function))
    {
        // It is a mobj, give it an ID (not for client mobjs, though, they
        // already have an id).
        if(!Cl_IsClientMobj((mobj_t*)th))
        {
            th->id = newMobjID();
        }
    }
    else
    {
        // Zero is not a valid ID.
        th->id = 0;
    }

    // Link the thinker to the thinker list.
    linkThinkerToList(th, listForThinkFunc(th->function, makePublic, true));
}

/**
 * Deallocation is lazy -- it will not actually be freed until its
 * thinking turn comes up.
 */
void P_ThinkerRemove(thinker_t* th)
{
    // Has got an ID?
    if(th->id)
    {
        // Then it must be a mobj.
        mobj_t* mo = (mobj_t *) th;

        // Flag the ID as free.
        P_SetMobjID(th->id, false);

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
    if(func && func == gx.MobjThinker)
        return true;

    return false;
}

/**
 * Init the thinker lists.
 *
 * @params flags        0x1 = Init public thinkers.
 *                      0x2 = Init private (engine-internal) thinkers.
 */
void P_InitThinkerLists(byte flags)
{
    if(!inited)
    {
        numThinkerLists = 0;
        thinkerLists = NULL;
    }
    else
    {
        size_t              i;

        for(i = 0; i < numThinkerLists; ++i)
        {
            thinkerlist_t* list = thinkerLists[i];

            if(list->isPublic && !(flags & 0x1))
                continue;
            if(!list->isPublic && !(flags & 0x2))
                continue;

            initThinkerList(list);
        }
    }

    P_ClearMobjIDs();
    inited = true;
}

boolean P_ThinkerListInited(void)
{
    return inited;
}

/**
 * Iterate the list of thinkers making a callback for each.
 *
 * @param func          If not @c NULL, only make a callback for thinkers
 *                      whose function matches this.
 * @param flags         Thinker filter flags.
 * @param callback      The callback to make. Iteration will continue
 *                      until a callback returns a non-zero value.
 * @param context       Is passed to the callback function.
 */
int P_IterateThinkers(think_t func, byte flags,
                          int (*callback) (thinker_t*, void*),
                          void* context)
{
    int result = false;
    size_t i;

    if(!inited) return false;

    if(func)
    {
        // We might have both public and shared lists for this func.
        if(flags & 0x1)
            result = iterateThinkers(listForThinkFunc(func, true, false),
                                     callback, context);
        if(!result && (flags & 0x2))
            result = iterateThinkers(listForThinkFunc(func, false, false),
                                     callback, context);
        return result;
    }

    for(i = 0; i < numThinkerLists; ++i)
    {
        thinkerlist_t* list = thinkerLists[i];

        if(list->isPublic && !(flags & 0x1))
            continue;
        if(!list->isPublic && !(flags & 0x2))
            continue;

        result = iterateThinkers(list, callback, context);
        if(result) break;
    }
    return result;
}

/**
 * Part of the Doomsday public API.
 */
void DD_InitThinkers(void)
{
    P_InitThinkerLists(0x1); // Init the public thinker lists.
}

/**
 * Part of the Doomsday public API.
 */
void DD_RunThinkers(void)
{
    P_IterateThinkers(NULL, 0x1 | 0x2, runThinker, NULL);
}

/**
 * Adds a new thinker to the thinker lists.
 * Part of the Doomsday public API.
 */
void DD_ThinkerAdd(thinker_t* th)
{
    P_ThinkerAdd(th, true); // This is a public thinker.
}

/**
 * Removes a thinker from the thinker lists.
 * Part of the Doomsday public API.
 */
void DD_ThinkerRemove(thinker_t* th)
{
    P_ThinkerRemove(th);
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

/**
 * Part of the Doomsday public API.
 */
int DD_IterateThinkers(think_t func, int (*callback) (thinker_t*, void*), void* context)
{
    return P_IterateThinkers(func, 0x1, callback, context);
}
