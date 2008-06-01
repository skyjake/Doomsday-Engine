/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

static thinker_t thinkerCap; // The head and tail of the thinker list.

// CODE --------------------------------------------------------------------

boolean P_IsMobjThinker(think_t thinker)
{
    think_t             altfunc;

    if(thinker == gx.MobjThinker)
        return true;

    altfunc = gx.GetVariable(DD_ALT_MOBJ_THINKER);
    return (altfunc && thinker == altfunc);
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

/**
 * Iterate the list of thinkers making a callback for each.
 *
 * @param type          If not @c NULL, only make a callback for thinkers
 *                      whose function matches this.
 * @param callback      The callback to make. Iteration will continue
 *                      until a callback returns a zero value.
 * @param context       Is passed to the callback function.
 */
boolean P_IterateThinkers(think_t type,
                          boolean (*callback) (thinker_t* thinker, void*),
                          void* context)
{
    boolean             result = true;
    thinker_t*          th, *next;

    th = thinkerCap.next;
    while(th != &thinkerCap)
    {
#ifdef FAKE_MEMORY_ZONE
        assert(th->next != NULL);
        assert(th->prev != NULL);
#endif

        next = th->next;
        if(!(type && th->function && th->function != type))
            if((result = callback(th, context)) == 0)
                break;
        th = next;
    }

    return result;
}

static boolean runThinker(thinker_t* th, void* context)
{
    // Thinker cannot think when in stasis.
    if(!th->inStasis)
    {
        if(th->function == (think_t) -1)
        {
            // Time to remove it.
            th->next->prev = th->prev;
            th->prev->next = th->next;
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

void P_RunThinkers(void)
{
    P_IterateThinkers(NULL, runThinker, NULL);
}

void P_InitThinkers(void)
{
    thinkerCap.prev = thinkerCap.next = &thinkerCap;
    P_ClearMobjIDs();
}

boolean P_ThinkerListInited(void)
{
    return (thinkerCap.next)? true : false;
}

/**
 * Adds a new thinker at the end of the list.
 */
void P_ThinkerAdd(thinker_t* th)
{
    // Link the thinker to the thinker list.
    thinkerCap.prev->next = th;
    th->next = &thinkerCap;
    th->prev = thinkerCap.prev;
    thinkerCap.prev = th;

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
