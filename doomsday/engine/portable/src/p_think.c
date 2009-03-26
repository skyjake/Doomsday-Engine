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
 * p_think.c: Thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_defs.h"
#include "de_console.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int idtable[2048]; // 65536 bits telling which IDs are in use.
unsigned short iddealer = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static size_t numThinkerLists;
static thinker_t* thinkerCaps; // The head and tail of the thinker lists.
static boolean inited = false;

// CODE --------------------------------------------------------------------

static void linkThinkerToList(thinker_t* th, thinker_t* list)
{
    // Link the thinker to the thinker list.
    list->prev->next = th;
    th->next = list;
    th->prev = list->prev;
    list->prev = th;
}

static void unlinkThinkerFromList(thinker_t* th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

static void initThinkerList(thinker_t* list)
{
    list->prev = list->next = list;
}

static thinker_t* listForThinkFunc(think_t func, boolean canCreate)
{
    size_t              i;

    for(i = 0; i < numThinkerLists; ++i)
    {
        thinker_t*          list = &thinkerCaps[i];

        if(list->function == func)
            return list;
    }

    if(!canCreate)
        return NULL;

    // A new thinker type.
    {
    thinker_t*          list;
    thinker_t*          newLists =
        Z_Calloc(sizeof(thinker_t) * ++numThinkerLists, PU_STATIC, 0);

    if(thinkerCaps)
    {
        for(i = 0; i < numThinkerLists-1; ++i)
        {
            thinker_t*          dst = &newLists[i];
            const thinker_t*    src = &thinkerCaps[i];

            memcpy(dst, src, sizeof(thinker_t));
            if(dst->next != dst->prev)
            {
                dst->next->prev = dst;
                dst->prev->next = dst;
            }
            else
            {
                dst->next = dst->prev = dst;
            }
        }

        Z_Free(thinkerCaps);
    }

    thinkerCaps = newLists;
    list = &thinkerCaps[numThinkerLists-1];

    initThinkerList(list);
    list->function = func;
    // Set the list sentinel to instasis (safety measure).
    list->inStasis = true;

    return list;
    }
}

static boolean runThinker(thinker_t* th, void* context)
{
    // Thinker cannot think when in stasis.
    if(!th->inStasis)
    {
        if(th->function == (think_t) -1)
        {
            // Time to remove it.
            unlinkThinkerFromList(th);

            if(th->id)
            {   // Its a mobj.
                P_MobjRecycle((mobj_t*) th);
            }
            else
            {
                Z_Free(th);
            }
        }
        else if(th->function)
        {
            th->function(th);
        }
    }

    return true; // Continue iteration.
}

static boolean iterateThinkers(thinker_t* list,
                               boolean (*callback) (thinker_t*, void*),
                               void* context)
{
    boolean             result = true;

    if(list)
    {
        thinker_t*          th, *next;

        th = list->next;
        while(th != list && th)
        {
#ifdef FAKE_MEMORY_ZONE
            assert(th->next != NULL);
            assert(th->prev != NULL);
#endif

            next = th->next;
            if((result = callback(th, context)) == 0)
                break;
            th = next;
        }
    }

    return result;
}

boolean P_IsMobjThinker(think_t func)
{
    if(func && func == gx.MobjThinker)
        return true;

    return false;
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

thid_t P_NewMobjID(void)
{
    // Increment the ID dealer until a free ID is found.
    // \fixme What if all IDs are in use? 65535 thinkers!?
    while(P_IsUsedMobjID(++iddealer));
    // Mark this ID as used.
    P_SetMobjID(iddealer, true);
    return iddealer;
}

void P_InitThinkers(void)
{
    if(!inited)
    {
        numThinkerLists = 0;
        thinkerCaps = NULL;
    }
    else
    {
        size_t              i;

        for(i = 0; i < numThinkerLists; ++i)
            initThinkerList(&thinkerCaps[i]);
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
 * @param type          If not @c NULL, only make a callback for thinkers
 *                      whose function matches this.
 * @param callback      The callback to make. Iteration will continue
 *                      until a callback returns a zero value.
 * @param context       Is passed to the callback function.
 */
boolean P_IterateThinkers(think_t func,
                          boolean (*callback) (thinker_t*, void*),
                          void* context)
{
    if(!inited)
        return true;

    if(func)
        return iterateThinkers(listForThinkFunc(func, false), callback,
                               context);

    {
    boolean             result = true;
    size_t              i;

    for(i = 0; i < numThinkerLists; ++i)
    {
        thinker_t*          list = &thinkerCaps[i];

        if((result = iterateThinkers(list, callback, context)) == 0)
            break;
    }
    return result;
    }
}

void P_RunThinkers(void)
{
    P_IterateThinkers(NULL, runThinker, NULL);
}

/**
 * Adds a new thinker at the end of the list.
 */
void P_ThinkerAdd(thinker_t* th)
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
        // It is a mobj, give it an ID.
        th->id = P_NewMobjID();
    }
    else
    {
        // Zero is not a valid ID.
        th->id = 0;
    }

    // Link the thinker to the thinker list.
    linkThinkerToList(th, listForThinkFunc(th->function, true));
}

/**
 * Deallocation is lazy -- it will not actually be freed until its
 * thinking turn comes up.
 */
void P_ThinkerRemove(thinker_t* th)
{
    // Has got an ID?
    if(th->id)
    {   // Then it must be a mobj.
        mobj_t*             mo = (mobj_t *) th;

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

    th->function = (think_t) - 1;
}

/**
 * Change the 'in stasis' state of a thinker (stop it from thinking).
 *
 * @param th            The thinker to change.
 * @param on            @c true, put into stasis.
 */
void P_ThinkerSetStasis(thinker_t* th, boolean on)
{
    if(th)
    {
        th->inStasis = on;
    }
}
